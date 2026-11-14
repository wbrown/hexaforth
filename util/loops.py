#!/usr/bin/env python3
"""Where a program's instructions go round: its loops, by what they cost.

    loops.py IMAGE.hex COUNTS [REPORT]

A loop is the instructions from where a branch goes back to, up to the
branch.  COUNTS is how often each instruction ran (build/desktop.counts,
from `make -C baremetal demo-profile`).  For every loop that ran: the word
it is in, how many times it went round, the instructions and the cycles
each turn took, and how much of all the work was spent in it; then the
loop itself, each instruction with how often it ran.

Cycles are those of rtl/hexaforth.v: one for an instruction, another for
each read of memory and for a shift, and two more for a multiply.  A call
in a loop is counted as one instruction: what the word called costs is
that word's, and it is shown as a call.

The report is build/loops.txt.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import insn_map as im                               # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, f"{ROOT}/baremetal")
import deskprof                                     # noqa: E402


def cycles(w):
    """Cycles the Verilog CPU takes for an instruction."""
    if w & 0x8000:
        return 2 if w >> 12 == 0xB else 1           # the fetch of a variable reads memory
    if w >> 13 != 3:
        return 1
    op = w >> 4 & 15
    return 1 + (w >> 10 & 3 == 2) + (op == 14) + (op in (12, 13)) + 2 * (op == 7)


def find_loops(code, starts):
    """(head, tail) for every branch that goes back, within a word.  A jump
    back to the start of a word, or into another, is a call that will not
    come back and no loop."""
    loops = []
    begins = set(starts)
    for at, w in enumerate(code):
        if w and not w & 0x8000 and w >> 13 in (0, 1):
            head = im.flat_target(at, w)
            if head <= at and not any(a in begins for a in range(head, at + 1)):
                loops.append((head, at))
    return loops


def main():
    image, counts_path = sys.argv[1], sys.argv[2]
    report = sys.argv[3] if len(sys.argv) > 3 else f"{ROOT}/build/loops.txt"
    code, _ = im.load_image(image)
    raw = b"".join(struct.pack("<I", int(l, 16)) for l in open(image) if l.strip())
    raw += bytes(0x20000 - len(raw))
    names = deskprof.word_names(raw)
    starts = sorted(names)
    data = open(counts_path, "rb").read()
    half = len(data) // 16
    work = list(struct.unpack(f"<{half}d", data[:8 * half]))[:len(code)]
    work += [0.0] * (len(code) - len(work))
    total = sum(work)
    total_cycles = sum(c * cycles(w) for c, w in zip(work, code))

    words = im.load_words(f"{ROOT}/vm_opcodes.h")
    single = {}
    for n, c in words.items():
        if len(c) == 1:
            single.setdefault(c[0], n)

    def word_of(a):
        i = max((s for s in starts if s <= a), default=None)
        return names[i] if i is not None else "?"

    def show(at, w):
        if w & 0x8000:
            if w >> 12 & 3 == 3:
                return f"variable {w & 0xFFF:x} {'!' if w & 0x4000 else '@'}"
            v = (w & 0xFFF) << 12 * (w >> 12 & 3)
            return f"{'+ ' if w & 0x4000 else ''}{v}"
        kind = w >> 13
        if kind != 3:
            t = im.flat_target(at, w)
            if kind == 2:
                return f"call {names.get(t, hex(t))}"
            return f"{'0branch' if kind else 'jump'} {t:04x}"
        base, ret = (w & ~0x1003, True) if w & 0x1000 and w & 3 == 3 and w != 0x7403 else (w, False)
        return f"{single.get(base, ''):9} {im.describe(base)}" + ("  ;" if ret else "")

    loops = find_loops(code, starts)
    rows = []
    for head, tail in loops:
        turns = work[tail]
        if turns < 1:
            continue
        ran = sum(work[head:tail + 1])
        cyc = sum(work[a] * cycles(code[a]) for a in range(head, tail + 1))
        inner = not any((h, t) != (head, tail) and head <= h and t <= tail for h, t in loops)
        calls = sum(1 for a in range(head, tail + 1)
                    if code[a] >> 13 == 2 and not code[a] & 0x8000 and work[a] >= turns / 2)
        rows.append((cyc, ran, turns, head, tail, inner, calls))
    rows.sort(reverse=True)

    out = [f"{image}: {total:,.0f} instructions of work, {total_cycles:,.0f} cycles; "
           f"{len(rows)} loops ran.",
           "A loop with another inside it has that one's cost in its own.", "",
           f"{'cycles':>12} {'of all':>6} {'turns':>11} {'ins/turn':>8} {'cyc/turn':>8} {'calls':>5}  "
           f"{'word':16} where"]
    for cyc, ran, turns, head, tail, inner, calls in rows:
        out.append(f"{cyc:12,.0f} {100 * cyc / total_cycles:5.1f}% {turns:11,.0f} {ran / turns:8.1f} "
                   f"{cyc / turns:8.1f} {calls:5}  {word_of(head):16} {head:04x}-{tail:04x}"
                   + ("" if inner else "  (has a loop inside)"))
    summary = len(out)
    for cyc, ran, turns, head, tail, inner, calls in rows:
        out.append("")
        out.append(f"{word_of(head)}  {head:04x}-{tail:04x}: {turns:,.0f} turns, {ran / turns:.1f} "
                   f"instructions and {cyc / turns:.1f} cycles a turn, "
                   f"{100 * cyc / total_cycles:.1f}% of all cycles")
        for a in range(head, tail + 1):
            out.append(f"    {a:04x} {code[a]:04x} {work[a]:12,.0f}  {show(a, code[a])}")
    with open(report, "w") as f:
        f.write("\n".join(out) + "\n")
    print("\n".join(out[:summary]))
    print(f"\nall of it, and each loop's instructions: {report}")


if __name__ == "__main__":
    sys.exit(main())
