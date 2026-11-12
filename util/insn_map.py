#!/usr/bin/env python3
"""Map the hexaforth ALU instruction space.

Every ALU instruction is an (input mux, ALU op, output mux, data stack delta,
return stack delta, RET) tuple: 4*16*4*4*4*2 = 8192 encodings.  This script
executes each one symbolically, following vm.c, and uses the result three ways:

  1. groups the encodings by what they do, to see how many distinct
     behaviours the instruction set really has;
  2. finds the sequences of existing Forth words (from FORTH_OPS in
     vm_opcodes.h) that one instruction could replace;
  3. scans a compiled image for adjacent instructions that could be fused,
     which is what a peephole pass in cross.fs would save on real code.

Usage:
  insn_map.py                      summary + word sequences
  insn_map.py --image build/nuc.hex   also scan a compiled image
  insn_map.py --tsv                dump every encoding and its behaviour
  insn_map.py --verify             run every claimed equivalence on vm.c
  insn_map.py --peephole forth/peephole.fs
                                   write the fusion tables cross.fs loads
                                   (add --verify to run them on vm.c too)
  insn_map.py --compare plain.hex optimised.hex
                                   check a peephole build against one made
                                   with PEEPHOLE=off, run by run
"""
import itertools
import re
import os
import struct
import subprocess
import sys
import tempfile
from collections import Counter, defaultdict

MASK = (1 << 64) - 1

IN_MUX = ["N->IN", "T->IN", "[T]->IN", "R->IN"]
ALU_OPS = ["IN->", "T<>N,IN->", "T->N,IN->", "IN+N", "IN&N", "IN|N", "IN^N", "IN*N",
           "~IN", "IN==N", "N<IN", "Nu<IN", "IN>>T", "IN<<T", "[IN]", "io[IN]"]
OUT_MUX = ["->T", "->R", "->io[T]", "->[T]"]
DELTA = {0: 0, 1: 1, 3: -1, 2: -2}          # 2-bit signed field -> delta
DELTA_BITS = {v: k for k, v in DELTA.items()}

FIELDS = {"d+0": 0, "d+1": 4, "d-1": 0xC, "d-2": 8, "r+0": 0, "r+1": 1, "r-1": 3, "r-2": 2,
          "RET": 0x1000, "imm+": 0x4000, "imm<<12": 0x1000, "imm<<24": 0x2000, "imm<<36": 0x3000,
          "T|N": 0x400 | 5 << 4, "~T": 0x400 | 8 << 4, "N<<T": 13 << 4}
FIELDS.update({n: i << 10 for i, n in enumerate(IN_MUX)})
FIELDS.update({n: i << 4 for i, n in enumerate(ALU_OPS)})
FIELDS.update({n: i << 8 for i, n in enumerate(OUT_MUX)})
TERMS = {"imm": 0x8000, "alu": 0x6000, "ubranch": 0, "0branch": 0x2000, "scall": 0x4000}


def encode(in_mux, alu, out, d, r, ret):
    return 0x6000 | ret << 12 | in_mux << 10 | out << 8 | alu << 4 | DELTA_BITS[d] << 2 | DELTA_BITS[r]


def describe(ins):
    """Field-by-field text for an ALU instruction."""
    parts = [IN_MUX[ins >> 10 & 3], ALU_OPS[ins >> 4 & 15], OUT_MUX[ins >> 8 & 3]]
    d, r = DELTA[ins >> 2 & 3], DELTA[ins & 3]
    if d:
        parts.append(f"d{d:+d}")
    if r:
        parts.append(f"r{r:+d}")
    if ins & 0x1000:
        parts.append("RET")
    return " ".join(parts)


# --------------------------------------------------------------------------
# Symbolic values.  Sums carry a folded constant so literal adds compare equal.

def add(a, b):
    const, terms = 0, []
    for x in (a, b):
        if isinstance(x, int):
            const += x
        elif x[0] == "sum":
            const += x[1]
            terms += x[2]
        else:
            terms.append(x)
    const &= MASK
    terms = tuple(sorted(terms, key=repr))
    if not terms:
        return const
    if const == 0 and len(terms) == 1:
        return terms[0]
    return ("sum", const, terms)


def commutative(op, a, b):
    return (op,) + tuple(sorted((a, b), key=repr))


def invert(a):
    if isinstance(a, int):
        return ~a & MASK
    if a[0] == "~":
        return a[1]
    return ("~", a)


class Underflow(Exception):
    pass


class Barrier(Exception):
    pass


class State:
    """Windows onto the two stacks (top last), memory, IO events and PC."""
    DEPTH = 6

    def __init__(self):
        self.d = [("d", i) for i in reversed(range(self.DEPTH))]
        self.r = [("r", i) for i in reversed(range(self.DEPTH))]
        self.mem = "mem0"
        self.io = ()
        self.loads = ()         # every read, used or not: fusing must not drop one
        self.pc = None
        self.blocked = False

    def key(self):
        return (tuple(self.d), tuple(self.r), self.mem, self.io, self.loads, self.pc)

    def load(self, addr):
        value = ("load", self.mem, addr)
        self.loads += (value,)
        return value

    def step(self, ins):
        if self.blocked or self.pc is not None:
            raise Barrier()                 # nothing may follow a RET or an IO write
        try:
            if ins & 0x8000:
                self.literal(ins)
            else:
                self.alu(ins)
        except IndexError:
            raise Underflow()
        if len(self.d) < 1 or len(self.r) < 1:
            raise Underflow()

    def literal(self, ins):
        value = (ins & 0xFFF) << 12 * (ins >> 12 & 3)
        if ins & 0x4000:
            self.d[-1] = add(self.d[-1], value)
        else:
            self.d.append(value)

    def alu(self, ins):
        d, r = self.d, self.r
        t, n = d[-1], d[-2]
        src = ins >> 10 & 3
        inp = self.load(t) if src == 2 else [n, t, None, r[-1]][src]
        op = ins >> 4 & 15
        if op == 0:
            out = inp
        elif op == 1:
            d[-2], d[-1] = t, n
            out = inp
        elif op == 2:
            d[-2] = t
            out = inp
        elif op == 3:
            out = add(inp, n)
        elif op in (4, 5, 6, 7, 9):
            out = commutative(ALU_OPS[op], inp, n)
        elif op == 8:
            out = invert(inp)
        elif op in (10, 11):
            out = (ALU_OPS[op], n, inp)
        elif op in (12, 13):
            out = (ALU_OPS[op], inp, t)
        elif op == 14:
            out = self.load(inp)
        else:
            out = ("ioread", inp, len(self.io), len(d), len(r))
            self.io += (out,)
        if ins & 0x1000:
            self.pc = r[-1]
        addr = d[-1]                        # after any T<>N swap, before the pop
        for stack, delta in ((d, DELTA[ins >> 2 & 3]), (r, DELTA[ins & 3])):
            if delta > 0:
                stack.append(stack[-1])
            for _ in range(-delta):
                stack.pop()
        dest = ins >> 8 & 3
        if dest == 0:
            d[-1] = out
        elif dest == 1:
            r[-1] = out
        elif dest == 2:
            self.io += (("iowrite", addr, out, len(d), len(r)),)
            self.blocked = True             # may have changed a stack depth
        else:
            self.mem = ("store", self.mem, addr, out)


def behaviour(instructions):
    """Symbolic effect of an instruction sequence, or None if it cannot be modelled."""
    state = State()
    try:
        for ins in instructions:
            state.step(ins)
    except (Underflow, Barrier):
        return None
    return state.key()


# --------------------------------------------------------------------------
# The Forth words the toolchain already knows, assembled as init_opcodes does.

def load_words(path):
    words = {}
    text = open(path).read()
    text = text[text.index("static forth_define FORTH_OPS[]"):]
    for name, code in re.findall(r'\{"([^"]+)",\s*"([^"]*)"', text):
        out, acc = [], 0
        for token in code.split():
            if token in TERMS:
                out.append(acc | TERMS[token])
                acc = 0
            elif token in FIELDS:
                acc |= FIELDS[token]
            elif token in words:
                out += words[token]
            else:
                acc |= int(token)
        words[name] = out
    return words


def modelled(instructions):
    """Only literals and ALU instructions, and no IO."""
    for ins in instructions:
        if not ins & 0x8000:
            if ins >> 13 != 3 or ins >> 4 & 15 == 15 or ins >> 8 & 3 == 2:
                return False
    return True


def main():
    args = sys.argv[1:]
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    words = load_words(f"{root}/vm_opcodes.h")
    single_names = {}
    for name, code in words.items():
        if len(code) == 1:
            single_names.setdefault(code[0], name)

    # ---- 1. every encoding
    classes = defaultdict(list)
    for fields in itertools.product(range(4), range(16), range(4), DELTA_BITS, DELTA_BITS, (0, 1)):
        ins = encode(*fields)
        classes[behaviour([ins])].append(ins)
    classes.pop(None, None)

    def canonical(key):
        members = classes[key]
        named = [i for i in members if i in single_names]
        return named[0] if named else min(members)

    def name_of(ins):
        return single_names.get(ins) or f"({describe(ins)})"

    if "--compare" in args:
        i = args.index("--compare")
        return compare_images(args[i + 1], args[i + 2])

    if "--peephole" in args:
        return write_peephole(root, args[args.index("--peephole") + 1], words, classes,
                              canonical, "--verify" in args)

    if "--tsv" in args:
        for key, members in classes.items():
            rep = canonical(key)
            for ins in sorted(members):
                print(f"{ins:04x}\t{describe(ins)}\t{rep:04x}\t{single_names.get(rep, '')}")
        return

    named_classes = {k for k, m in classes.items() if any(i in single_names for i in m)}
    print(f"ALU encodings:        {sum(len(m) for m in classes.values())}")
    print(f"distinct behaviours:  {len(classes)}")
    print(f"already a named word: {len(named_classes)}")

    # ---- 2. word sequences that one instruction can replace
    vocab = {n: c for n, c in words.items() if c and modelled(c) and n not in ("noop", "halt")}
    found = defaultdict(list)                   # canonical instruction -> sequences
    claims = []                                 # (instruction sequence, single instruction)
    for length in (2, 3):
        for seq in itertools.product(vocab, repeat=length):
            code = [i for w in seq for i in vocab[w]]
            key = behaviour(code)
            if key in classes:
                found[canonical(key)].append((len(code), seq))
                claims.append((code, canonical(key)))
    if "--verify" in args:
        return verify(root, claims)
    print(f"\nSequences of 2-3 known words that equal ONE instruction "
          f"({len(vocab)} words tried):")
    print("  * = no word for this instruction yet\n")
    rows = []
    for ins, seqs in found.items():
        seqs.sort(key=lambda s: (len(s[1]), s[0]))
        shortest = [" ".join(s[1]) for s in seqs if len(s[1]) == len(seqs[0][1])]
        rows.append((ins not in single_names, len(seqs[0][1]), ins, shortest))
    for new, _, ins, shortest in sorted(rows, key=lambda r: (not r[0], r[1], r[2])):
        mark = "*" if new else " "
        label = single_names.get(ins, "")
        print(f"  {mark} {ins:04x} {label:9s} {describe(ins):34s} = {'  |  '.join(shortest[:4])}")

    # ---- 3. what a peephole pass would save on a real image
    if "--image" in args:
        scan_image(args[args.index("--image") + 1], classes, canonical, name_of)


def write_peephole(root, path, words, classes, canonical, check):
    """Fusion tables for cross.fs.

    cross.fs emits one instruction at a time and asks whether it fuses with
    the one or two before it.  `base` is every ALU instruction a baseword can
    emit; `reach` adds everything fusion can turn them into.  A pair entry
    exists for each (reach, base) that equals one instruction.  A triple
    entry exists only where neither adjacent pair fuses, since cross.fs
    retries after each fusion.  IO instructions are left out.
    """
    def plain(ins):
        return ins >> 13 == 3 and ins >> 4 & 15 != 15 and ins >> 8 & 3 != 2

    base = sorted({i for code in words.values() for i in code if plain(i)})
    reach, pairs, triples = set(base), {}, {}
    todo = list(base)
    while todo:
        fresh = []
        for a in todo:
            for b in base:
                key = behaviour([a, b])
                if key in classes:
                    c = canonical(key)
                    pairs[(a, b)] = c
                    if c not in reach:
                        reach.add(c)
                        fresh.append(c)
        for a in todo:
            for b in base:
                if (a, b) in pairs:
                    continue
                for c in base:
                    if (b, c) in pairs:
                        continue
                    key = behaviour([a, b, c])
                    if key in classes:
                        d = canonical(key)
                        triples[(a, b, c)] = d
                        if d not in reach:
                            reach.add(d)
                            fresh.append(d)
        todo = fresh

    with open(path, "w") as f:
        f.write("\\ Instruction fusion tables for the cross.fs peephole.\n"
                "\\ GENERATED by `python3 util/insn_map.py --peephole forth/peephole.fs`\n"
                "\\ from the symbolic model of vm.c.  Do not edit.\n\\\n"
                "\\ Each entry is two cells: the instructions to match, packed 16 bits\n"
                "\\ each with the most recent lowest, then the one instruction they equal.\n\n")
        f.write(f"\\ {len(pairs)} pairs\ncreate fuse2\n")
        for (a, b), c in sorted(pairs.items()):
            f.write(f"${a << 16 | b:x} , ${c:x} ,\n")
        f.write(f"0 ,\n\n\\ {len(triples)} triples\ncreate fuse3\n")
        for (a, b, c), d in sorted(triples.items()):
            f.write(f"${a << 32 | b << 16 | c:x} , ${d:x} ,\n")
        f.write("0 ,\n")
    print(f"{path}: {len(base)} base instructions, {len(reach)} reachable, "
          f"{len(pairs)} pairs, {len(triples)} triples")
    if check:
        claims = [(list(k), v) for k, v in pairs.items()] + [(list(k), v) for k, v in triples.items()]
        return verify(root, claims)
    return 0


def load_image(path):
    """Code words and the entry points named by the dictionary."""
    raw = b"".join(struct.pack("<I", int(l, 16)) for l in open(path) if l.strip())
    cells = list(struct.unpack(f"<{len(raw) // 2}H", raw))
    code = cells[:0x2000]
    code = code[:max(i for i, w in enumerate(code) if w) + 1]
    entries = set()
    link = cells[max(i for i in range(0x2000, len(cells)) if cells[i])]
    while link:
        link &= ~1
        name_len = raw[link + 2]
        entries.add(struct.unpack_from("<H", raw, (link + 3 + name_len + 1) & ~1)[0])
        link = struct.unpack_from("<H", raw, link)[0]
    return code, entries


def split_runs(code, entries):
    """Cut code into control instructions and straight-line runs.  A run ends
    at a RET or an IO write, and before anything a branch can land on."""
    targets = set(entries)
    for w in code:
        if w and not w & 0x8000 and w >> 13 != 3:
            targets.add(w & 0x1FFF)
    items, run, start = [], [], 0
    for i, w in enumerate(code):
        if run and i in targets:
            items.append(("run", start, run))
            run = []
        if w & 0x8000 or w >> 13 == 3:
            if not run:
                start = i
            run.append(w)
            if not w & 0x8000 and (w & 0x1000 or w >> 8 & 3 == 2):
                items.append(("run", start, run))
                run = []
        else:
            if run:
                items.append(("run", start, run))
                run = []
            items.append(("ctl", i, w))
    if run:
        items.append(("run", start, run))
    return items


def compare_images(plain_path, opt_path):
    """Every straight-line run of the optimised image must behave exactly like
    the matching run of the unoptimised one, and every branch must go to the
    matching place.  Literals that are code addresses are translated."""
    State.DEPTH = 64
    plain, opt = (split_runs(*load_image(p)) for p in (plain_path, opt_path))
    if [k for k, _, _ in plain] != [k for k, _, _ in opt]:
        print("images do not have the same control structure")
        return 1
    moved = {a[1]: b[1] for a, b in zip(plain, opt)}

    def translate(run):
        return [0x8000 | moved[w & 0xFFF] if w & 0xF000 == 0x8000 and (w & 0xFFF) in moved
                and moved[w & 0xFFF] < 0x1000 else w for w in run]

    bad = runs = 0
    for (kind, at_a, a), (_, at_b, b) in zip(plain, opt):
        if kind == "ctl":
            same = a >> 13 == b >> 13 and (a == b == 0 or moved.get(a & 0x1FFF) == b & 0x1FFF)
        else:
            runs += 1
            want = behaviour(b)
            same = want is not None and want in (behaviour(a), behaviour(translate(a)))
        if not same:
            bad += 1
            text = lambda x: " ".join(f"{w:04x}" for w in x) if isinstance(x, list) else f"{x:04x}"
            print(f"DIFFERENT at {at_a:04x} / {at_b:04x}\n  plain:     {text(a)}\n  optimised: {text(b)}")
    count = lambda items: sum(len(x) if k == "run" else 1 for k, _, x in items)
    print(f"{runs} straight-line runs and {len(plain) - runs} branches compared: "
          f"{bad} differ.  {count(plain)} -> {count(opt)} instructions")
    return 1 if bad else 0


def scan_image(path, classes, canonical, name_of):
    raw = b"".join(struct.pack("<I", int(l, 16)) for l in open(path) if l.strip())
    code = list(struct.unpack(f"<{len(raw) // 2}H", raw))[:0x2000]
    end = max(i for i, w in enumerate(code) if w) + 1
    code = code[:end]

    # Instructions that must stay where they are: anything a branch can land
    # on, and the instruction after a call to a word that reads its own return
    # address (inline:, does>, jumptable, the DO runtime).
    targets, takes_return = set(), set()
    for w in code:
        if not w & 0x8000 and w >> 13 != 3 and w:
            targets.add(w & 0x1FFF)
    for t in targets:
        if t < end and code[t] == 0x6C07:       # starts with r>
            takes_return.add(t)
    pinned = {i + 1 for i, w in enumerate(code) if w >> 13 == 2 and (w & 0x1FFF) in takes_return}

    def fusable(i):
        w = code[i]
        return bool(w & 0x8000) or w >> 13 == 3

    saved, patterns = 0, Counter()
    i = 0
    while i < end:
        if not fusable(i):
            i += 1
            continue
        best = None
        for length in (4, 3, 2):
            group = range(i, i + length)
            if i + length > end or not all(fusable(j) for j in group):
                continue
            if any(j in targets for j in group[1:]) or any(j in pinned for j in group[:-1]):
                continue
            seq = [code[j] for j in group]
            key = behaviour(seq)
            if key in classes:
                best = (length, seq, name_of(canonical(key)))
                break
            lit = fold_literal(seq)
            if lit:
                best = (length, seq, lit)
                break
        if best:
            length, seq, result = best
            label = " ".join("lit" if w & 0x8000 else name_of(w) for w in seq)
            patterns[(label, result)] += 1
            saved += length - 1
            i += length
        else:
            i += 1

    total = sum(1 for w in code if w)
    print(f"\nImage {path}: {total} instructions")
    print(f"fusable by a peephole pass: {saved} instructions saved "
          f"({100 * saved / total:.1f}%), {sum(patterns.values())} sites\n")
    for (label, result), count in patterns.most_common():
        print(f"  {count:4d}  {label:40s} -> {result}")


VERIFY_C = r"""
// Run an instruction sequence and the single instruction claimed to equal it
// on vm.c, from a state where every reachable cell is a valid address.
#include "vm.h"
#include <string.h>
static const uint16_t prefix[] = {
    0x8440, 0x8448, 0x8450, 0x8458, 0x8460, 0x8468,
    0x650d, 0x650d, 0x650d, 0x650d, 0x650d, 0x650d,     // >r x6
    0x8400, 0x8408, 0x8410, 0x8418, 0x8420, 0x8428,
};
static context ctx;
static void run(const uint16_t *code, int n, context *out) {
    memset(&ctx, 0, sizeof ctx);
    memcpy(ctx.memory, prefix, sizeof prefix);
    memcpy(ctx.memory + sizeof prefix / 2, code, n * 2);
    int64_t *cells = (int64_t *)((uint8_t *)ctx.memory + 0x400);
    for (int k = 0; k < 16; k++) cells[k] = 0x400 + 8 * ((k * 5 + 3) % 16);
    ctx.OUT = stdout; ctx.IN = stdin;
    vm(&ctx);
    if (ctx.EIP < 0x400) ctx.EIP = 0;   // only a RET moves the PC somewhere comparable
    *out = ctx;
}
int main(void) {
    static context a, b;
    int n, bad = 0, total = 0;
    while (scanf("%d", &n) == 1) {
        uint16_t code[64], one;
        for (int i = 0; i < n; i++) scanf("%hx", &code[i]);
        scanf("%hx", &one);
        run(code, n, &a);
        run(&one, 1, &b);
        total++;
        if (a.SP != b.SP || a.RSP != b.RSP || a.EIP != b.EIP ||
            memcmp(a.DSTACK, b.DSTACK, 8 * a.SP) || memcmp(a.RSTACK, b.RSTACK, 8 * a.RSP) ||
            memcmp((uint8_t *)a.memory + 0x400, (uint8_t *)b.memory + 0x400, 0x100)) {
            bad++;
            printf("MISMATCH: %04x vs", one);
            for (int i = 0; i < n; i++) printf(" %04x", code[i]);
            printf("\n");
        }
    }
    printf("%d/%d equivalences confirmed on vm.c\n", total - bad, total);
    return bad != 0;
}
"""


def verify(root, claims):
    with tempfile.TemporaryDirectory() as tmp:
        src, exe = f"{tmp}/verify.c", f"{tmp}/verify"
        open(src, "w").write(VERIFY_C)
        subprocess.check_call(["cc", "-O1", "-w", "-fcommon", f"-I{root}", "-o", exe, src,
                               f"{root}/vm.c", f"{root}/vm_opcodes.c", f"{root}/vm_debug.c",
                               f"{root}/util/stack.c", "-lm"])
        lines = "".join(f"{len(c)} {' '.join(f'{w:04x}' for w in c)} {one:04x}\n"
                        for c, one in claims)
        return subprocess.run([exe], input=lines, text=True).returncode


def fold_literal(seq):
    """`lit +` and friends: a sequence that just adds one encodable constant to T."""
    key = behaviour(seq)
    if key is None:
        return None
    blank = State()
    t = key[0][-1] if len(key[0]) == State.DEPTH else None
    if t is None or key[1:] != blank.key()[1:] or key[0][:-1] != blank.key()[0][:-1]:
        return None
    if isinstance(t, tuple) and t[0] == "sum" and t[2] == (("d", 0),):
        const = t[1]
        for shift in range(4):
            if const and const == (const >> 12 * shift & 0xFFF) << 12 * shift:
                return f"lit imm+  ({const} +)"
    return None


if __name__ == "__main__":
    sys.exit(main())
