#!/usr/bin/env python3
"""What a program's variables cost it: fetches and stores, by where and of what.

    vars.py IMAGE.hex COUNTS [FILE.fs ...] [--report REPORT]

A variable in the cells one instruction reaches is fetched or stored by
one instruction, so it looks as cheap as a cell of a stack.  It is not:

  a store is an instruction that a value left on a stack would not need
  a fetch reads memory, which is a cycle more than a cell of a stack
  a fetched value is on top of the stack, where the next instruction has
    to take it from; a value on the return stack, or second on the data
    stack, is an input of the ALU, and using it is part of the instruction
    that does the work (R->IN IN+N ->T is `r@ +`)

COUNTS is how often each instruction ran (build/desktop.counts).  For the
code of the files named (all of it if none is), this says how much of the
work is fetches and stores of variables; which variables, with how often
each is fetched and stored and in how many words; and for each word, by
what it spends on them, the variables it uses.  A variable that is stored
in a word and fetched in the same word, and nowhere else, is one that word
keeps for itself, and could keep on a stack.

It also counts the fetches that are followed at once by an operation on
what was fetched and the cell under it: each is two instructions that one
would do if the value were on the return stack.

The report is build/vars.txt.
"""
import os
import struct
import sys
from collections import Counter, defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import insn_map as im                               # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, f"{ROOT}/baremetal")
import deskprof                                     # noqa: E402


def main():
    args = sys.argv[1:]
    report = f"{ROOT}/build/vars.txt"
    if "--report" in args:
        i = args.index("--report")
        report = args[i + 1]
        del args[i:i + 2]
    image, counts_path, files = args[0], args[1], args[2:]
    code, _ = im.load_image(image)
    raw = b"".join(struct.pack("<I", int(l, 16)) for l in open(image) if l.strip())
    raw += bytes(0x20000 - len(raw))
    names = deskprof.word_names(raw, image)
    starts = sorted(names)
    data = open(counts_path, "rb").read()
    half = len(data) // 16
    work = list(struct.unpack(f"<{half}d", data[:8 * half]))[:len(code)]
    work += [0.0] * (len(code) - len(work))
    mods = deskprof.modules(os.path.splitext(image)[0] + ".sizes")
    mod_at = [a for a, _ in mods]

    def file_of(a):
        i = max((k for k, s in enumerate(mod_at) if s <= a), default=0)
        return mods[i][1]

    def word_of(a):
        at = max((s for s in starts if s <= a), default=None)
        return names[at] if at is not None else "(before any word)"

    # The names of the variables: a label for data holds its address with the
    # top bit set, and a word of the dictionary that only pushes one is one.
    var_name = {}
    for value, n in im.labels_of(image):
        if value >> 63:
            var_name.setdefault(value & 0xFFFF, n)
    cells = struct.unpack(f"<{len(raw) // 2}H", raw)
    link = 8 * cells[max(i for i in range(0x2000, 0x8000) if cells[i])]
    while link:
        xt = struct.unpack_from("<Q", raw, link + 8)[0] & 0xFFFF
        w = code[xt] if xt < len(code) else 0
        if w & 0xC000 == 0x8000 and w >> 12 & 3 != 3:
            value, k = (w & 0xFFF) << 12 * (w >> 12 & 3), xt + 1
            while code[k] & 0xC000 == 0xC000 and code[k] >> 12 & 3 != 3:
                value += (code[k] & 0xFFF) << 12 * (code[k] >> 12 & 3)
                k += 1
            if code[k] == 0x7403:
                var_name[value] = raw[link + 17:link + 17 + raw[link + 16]].decode("latin-1")
        link = 8 * struct.unpack_from("<Q", raw, link)[0]

    def var(off):
        return var_name.get(im.VAR_BASE + off, f"cell {im.VAR_BASE + off:x}")

    wanted = lambda a: not files or file_of(a) in files
    total = fetched = stored = joined = 0.0
    by_var = defaultdict(lambda: [0.0, 0.0, set(), set()])     # fetches, stores, words fetching, words storing
    by_word = defaultdict(lambda: [0.0, 0.0, 0.0, Counter()])  # all, fetches, stores, by variable
    for a, w in enumerate(code):
        if not w or not work[a] or not wanted(a):
            continue
        total += work[a]
        word = word_of(a)
        by_word[word][0] += work[a]
        if w & 0xB000 != 0xB000:
            continue
        v = var(w & 0xFFF)
        if w & 0x4000:
            stored += work[a]
            by_var[v][1] += work[a]
            by_var[v][3].add(word)
            by_word[word][2] += work[a]
        else:
            fetched += work[a]
            by_var[v][0] += work[a]
            by_var[v][2].add(word)
            by_word[word][1] += work[a]
            n = code[a + 1] if a + 1 < len(code) else 0
            # an operation on what was fetched and the cell under it, that
            # leaves one result: T->IN, an operation of two, to T, d-1
            if n >> 13 == 3 and n >> 10 & 3 == 1 and n >> 8 & 3 == 0 and n >> 2 & 3 == 3 \
                    and n >> 4 & 15 in (3, 4, 5, 6, 7, 9, 10, 11) and not n & 3:
                joined += work[a]
        by_word[word][3][v] += work[a]

    out = [f"{image}: the code of {', '.join(files) if files else 'every file'}",
           f"instructions run: {total:,.0f}",
           f"  fetches of variables: {fetched:,.0f} ({100 * fetched / total:.1f}%), each a cycle more "
           f"than a cell of a stack",
           f"  stores to variables:  {stored:,.0f} ({100 * stored / total:.1f}%), each an instruction a "
           f"value left on a stack would not need",
           f"  fetches followed at once by an operation of two on what was fetched: {joined:,.0f} "
           f"({100 * joined / total:.1f}%),",
           f"    each two instructions that one would do with the value on the return stack",
           "",
           "The variables, by how often they are fetched and stored.  `own` is a variable",
           "that one word alone stores and fetches: it is that word's, and could be on a stack.",
           "",
           f"{'fetched':>12} {'stored':>12}  {'words':>5}  variable"]
    for v, (f, s, wf, ws) in sorted(by_var.items(), key=lambda kv: -(kv[1][0] + kv[1][1])):
        users = wf | ws
        own = "  own: " + next(iter(users)) if len(users) == 1 else ""
        out.append(f"{f:12,.0f} {s:12,.0f}  {len(users):5}  {v}{own}")
    summary = len(out)
    out += ["", "The words, by what they spend on variables: all they run, fetches, stores, and the",
            "variables with how often each is used there.", ""]
    for word, (allw, f, s, c) in sorted(by_word.items(), key=lambda kv: -(kv[1][1] + kv[1][2])):
        if f + s == 0:
            continue
        out.append(f"{word}: {allw:,.0f} run, {f:,.0f} fetches and {s:,.0f} stores "
                   f"({100 * (f + s) / allw:.0f}%)")
        out.append("    " + ", ".join(f"{v} {n:,.0f}" for v, n in c.most_common()))
    with open(report, "w") as fh:
        fh.write("\n".join(out) + "\n")
    print("\n".join(out[:min(summary, 46)]))
    print(f"\nall of it, and every word: {report}")


if __name__ == "__main__":
    sys.exit(main())
