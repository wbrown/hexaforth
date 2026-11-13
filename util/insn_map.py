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
  insn_map.py --fused build/nuc.hex forth/fused.fs
                                   write the table the decompiler uses to
                                   show a fused instruction as its words
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

    if "--fused" in args:
        i = args.index("--fused")
        return write_fused(args[i + 1], args[i + 2], words, classes, canonical)

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
                claims.append((code, [canonical(key)]))
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
    exists for each pair that equals one instruction and has a base
    instruction on at least one side: the new instruction is a base one
    when it is first emitted, and may be anything when it is the result of
    a fusion being offered to the instruction before it.  (Pairs of two
    fused instructions are left out: there are 140,000 of them.)  A triple
    entry exists only where neither adjacent pair fuses, since cross.fs
    retries after each fusion.  IO instructions are left out.

    A third table is of three instructions that two can replace: the first
    two base instructions, and the third a base instruction or one with a
    return folded into it.  (With anything reachable as the first there are
    450,000.)  It has an entry only
    where no pair in the three fuses and the three are not one instruction.
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
                for x, y in ((a, b), (b, a)):
                    if (x, y) in pairs:
                        continue
                    key = behaviour([x, y])
                    if key in classes:
                        c = canonical(key)
                        pairs[(x, y)] = c
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

    # Three instructions that two can replace.
    def foldable(ins):          # as fold-exit in cross.fs: a return can be folded in
        return ins & 0xF003 == 0x6000 and ins & 0x0300 not in (0x0100, 0x0200)

    tails = base + sorted({b | 0x1003 for b in base if foldable(b)} - set(base))
    want = defaultdict(list)
    for a in base:
        for b in base:
            if (a, b) in pairs:
                continue
            for c in tails:
                if (b, c) in pairs or (a, b, c) in triples:
                    continue
                key = behaviour([a, b, c])
                if key is not None and key not in classes:
                    want[key].append((a, b, c))
    firsts = [encode(i, o, m, d, r, 0) for i in range(4) for o in range(15) for m in (0, 1, 3)
              for d in DELTA_BITS for r in DELTA_BITS]
    seconds = firsts + [w | 0x1000 for w in firsts]
    two = {}
    for x in firsts:
        for y in seconds:
            key = behaviour([x, y])
            if key in want:
                # Prefer a result that later instructions can fuse with.
                score = (y in reach) * 2 + (x in reach)
                if key not in two or score > two[key][0]:
                    two[key] = (score, x, y)
    threes = {t: two[key][1:] for key, ts in want.items() if key in two for t in ts}

    with open(path, "w") as f:
        f.write("\\ Instruction fusion tables for the cross.fs peephole.\n"
                "\\ GENERATED by `python3 util/insn_map.py --peephole forth/peephole.fs`\n"
                "\\ from the symbolic model of vm.c.  Do not edit.\n\\\n"
                "\\ Each entry is two cells: the instructions to match, packed 16 bits\n"
                "\\ each with the most recent lowest, then what they equal: one\n"
                "\\ instruction, or in the last table two, packed the same way.  Each\n"
                "\\ table is in order of what it matches, and is searched by halving.\n\n")
        f.write(f"\\ {len(pairs)} pairs\n#{len(pairs)} constant #fuse2\ncreate fuse2\n")
        for (a, b), c in sorted(pairs.items()):
            f.write(f"${a << 16 | b:x} , ${c:x} ,\n")
        f.write(f"\n\\ {len(triples)} triples\n#{len(triples)} constant #fuse3\ncreate fuse3\n")
        for (a, b, c), d in sorted(triples.items()):
            f.write(f"${a << 32 | b << 16 | c:x} , ${d:x} ,\n")
        f.write(f"\n\\ {len(threes)} threes that two instructions replace\n"
                f"#{len(threes)} constant #fuse32\ncreate fuse32\n")
        for (a, b, c), (x, y) in sorted(threes.items()):
            f.write(f"${a << 32 | b << 16 | c:x} , ${x << 16 | y:x} ,\n")
    print(f"{path}: {len(base)} base instructions, {len(reach)} reachable, "
          f"{len(pairs)} pairs, {len(triples)} triples, {len(threes)} threes for two")
    if check:
        claims = [(list(k), [v]) for k, v in pairs.items()] + \
                 [(list(k), [v]) for k, v in triples.items()] + \
                 [(list(k), list(v)) for k, v in threes.items()]
        return verify(root, claims)
    return 0


def dictionary_names(path):
    """The names in an image's dictionary."""
    raw = b"".join(struct.pack("<I", int(l, 16)) for l in open(path) if l.strip())
    cells = struct.unpack(f"<{len(raw) // 2}H", raw)
    names = set()
    link = 8 * cells[max(i for i in range(0x2000, 0x8000) if cells[i])]
    while link:
        names.add(raw[link + 17:link + 17 + raw[link + 16]].decode("latin-1"))
        link = 8 * struct.unpack_from("<Q", raw, link)[0]
    return names


def write_fused(image, path, words, classes, canonical):
    """The table see.fs uses to show a fused instruction as the words it stands for.

    For every ALU instruction in the image's code, the shortest sequence of
    two or three words that does the same and is made only of primitives
    with a dictionary entry, so that each can be typed and looked up.
    Instructions that are a single word need no entry; see.fs finds those
    in the dictionary.  A return folded into an instruction is taken off
    first, as see.fs does.
    """
    code, _ = load_image(image)
    names = dictionary_names(image)
    singles = {c[0] for n, c in words.items() if len(c) == 1 and n in names}
    wanted = set()
    for ins in code:
        if ins >> 13 == 3:
            if ins & 0x1000:
                if ins & 3 != 3:
                    continue
                ins &= ~0x1003
            if ins not in singles and ins != 0x6400:
                wanted.add(ins)

    vocab = {n: c for n, c in words.items()
             if c and modelled(c) and n in names and n not in ("noop", "halt")}
    best = {}                                   # behaviour -> (words, instructions, sequence)
    for length in (2, 3):
        for seq in itertools.product(vocab, repeat=length):
            seq_code = [i for w in seq for i in vocab[w]]
            key = behaviour(seq_code)
            if key in classes:
                rank = (length, len(seq_code), sum(len(w) for w in seq))
                if key not in best or rank < best[key][0]:
                    best[key] = (rank, seq)

    rows, missing = [], []
    for ins in sorted(wanted):
        key = behaviour([ins])
        if key in best and len(" ".join(best[key][1])) <= 23:
            rows.append((ins, " ".join(best[key][1])))
        else:
            missing.append(ins)
    with open(path, "w") as f:
        f.write("\\ fused.fs - fused instructions as the words they stand for, for see.fs\n"
                "\\ GENERATED by `python3 util/insn_map.py --fused build/nuc.hex forth/fused.fs`\n"
                "\\ from the instructions in a built image.  Do not edit.\n\\\n"
                "\\ Each entry is four cells: the instruction, then a counted string in 24\n"
                "\\ bytes.  A zero ends the table.\n\n"
                "header fused create fused\n")
        for ins, text in rows:
            field = bytes([len(text)]) + text.encode() + bytes(23 - len(text))
            f.write(f"${ins:04x} ,  " + " ".join(f"${b:02x} c," for b in field) + f"  \\ {text}\n")
        f.write("0 ,\n")
    print(f"{len(rows)} fused instructions written to {path}; "
          f"{len(missing)} have no sequence of known words"
          + (": " + " ".join(f"{i:04x}" for i in missing) if missing else ""))


def load_image(path):
    """Code words and the entry points named by the dictionary."""
    raw = b"".join(struct.pack("<I", int(l, 16)) for l in open(path) if l.strip())
    cells = list(struct.unpack(f"<{len(raw) // 2}H", raw))
    code = cells[:0x2000] + [0] * 0x6000 + cells[0x8000:0x10000]
    code = code[:max(i for i, w in enumerate(code) if w) + 1]
    # Headers are 64-bit cells: link, then xt with the immediate flag on top.
    # A link is the address of a cell, which is 8 bytes.
    entries = set()
    labels = 0
    link = 8 * cells[max(i for i in range(0x2000, 0x8000) if cells[i])]
    while link:
        xt = struct.unpack_from("<Q", raw, link + 8)[0] & 0xFFFF
        entries.add(xt)
        if raw[link + 16:link + 23] == b"\x06labels":
            # The word pushes the address of the variable that heads the label chain.
            where = sum((w & 0xFFF) << 12 * (w >> 12 & 3) for w in cells[xt:xt + 2] if w & 0x8000)
            labels = 8 * struct.unpack_from("<Q", raw, 8 * where)[0]
        link = 8 * struct.unpack_from("<Q", raw, link)[0]
    # Labels name the definitions that have no header.  One for code holds
    # its execution token; one for data has the top bit set.
    global LABELLED
    LABELLED = bool(labels)
    while labels:
        value = struct.unpack_from("<Q", raw, labels + 8)[0]
        if not value >> 63:
            entries.add(value & 0xFFFF)
        labels = 8 * struct.unpack_from("<Q", raw, labels)[0]
    return code, entries


LABELLED = False            # the image last loaded names every definition


def flat_target(at, ins):
    """Word address a branch at `at` goes to: the kernel, or its own page."""
    field = ins & 0x1FFF
    return (at & ~0xFFF) | (field & 0xFFF) if field & 0x1000 else field


def split_runs(code, entries):
    """Cut code into control instructions and straight-line runs.  A run ends
    at a RET or an IO write, and before anything a branch can land on."""
    targets = set(entries)
    for i, w in enumerate(code):
        if w and not w & 0x8000 and w >> 13 != 3:
            targets.add(flat_target(i, w))
        # A two-instruction literal that points into code past the kernel is
        # an execution token: its word is an entry point too.  An image with
        # labels names every definition, so its entry points are all known
        # already; a number that only looks like one is the address of data.
        nxt = code[i + 1] if i + 1 < len(code) else 0
        if not LABELLED and w & 0xF000 == 0x9000 and nxt & 0xF000 == 0xC000:
            value = (w & 0xFFF) << 12 | nxt & 0xFFF
            if 0x1000 <= value < 0x2000 or 0x8000 <= value < len(code):
                targets.add(value)
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
            if w == 0 and i and code[i - 1] == 0:
                continue                # padding between code regions
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

    def translations(run):
        """The run with its code-address literals rewritten for the other
        image, every way that can be done.  A number that is the address of
        code in one image may just as well be an ordinary constant or the
        address of data, so each such literal is tried both ways."""
        parts, i = [], 0                        # (as written, rewritten or None)
        while i < len(run):
            w = run[i]
            nxt = run[i + 1] if i + 1 < len(run) else 0
            if w & 0xF000 == 0x9000 and nxt & 0xF000 == 0xC000 and \
                    ((w & 0xFFF) << 12 | nxt & 0xFFF) in moved:
                to = moved[(w & 0xFFF) << 12 | nxt & 0xFFF]
                parts.append(([w, nxt], [0x9000 | to >> 12, 0xC000 | to & 0xFFF]))
                i += 1
            elif w & 0xF000 == 0x9000 and (w & 0xFFF) << 12 in moved and \
                    moved[(w & 0xFFF) << 12] & 0xFFF == 0:
                parts.append(([w], [0x9000 | moved[(w & 0xFFF) << 12] >> 12]))
            elif w & 0xF000 == 0x8000 and moved.get(w & 0xFFF, 0x1000) < 0x1000 \
                    and moved[w & 0xFFF] != w & 0xFFF:
                parts.append(([w], [0x8000 | moved[w & 0xFFF]]))
            else:
                parts.append(([w], None))
            i += 1
        open_ = [n for n, (_, alt) in enumerate(parts) if alt and alt != parts[n][0]][:12]
        for choice in itertools.product((True, False), repeat=len(open_)):
            pick = dict(zip(open_, choice))
            yield [w for n, (same, alt) in enumerate(parts) for w in (alt if pick.get(n) else same)]

    bad = runs = 0
    for (kind, at_a, a), (_, at_b, b) in zip(plain, opt):
        if kind == "ctl":
            same = a >> 13 == b >> 13 and (
                a == b == 0 or moved.get(flat_target(at_a, a)) == flat_target(at_b, b))
        else:
            runs += 1
            want = behaviour(b)
            same = want is not None and (
                want == behaviour(a) or any(want == behaviour(t) for t in translations(a)))
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
// Run an instruction sequence and the shorter one claimed to equal it on
// vm.c, from a state where every cell that can be reached holds the address
// of one of sixteen cells of data.  The two differ as code, so the code is
// put where nothing computed from those addresses can point: at an odd cell
// high in memory.  (A shift of one address by another used to land on it.)
#include "vm.h"
#include <string.h>
#define DATA (0x400 / CELL_BYTES)
#define CODE (0x3001 * CELL_BYTES / 2)          // the instruction the code starts at
static const uint16_t prefix[] = {
    0x8000 | (DATA + 8), 0x8000 | (DATA + 9), 0x8000 | (DATA + 10),
    0x8000 | (DATA + 11), 0x8000 | (DATA + 12), 0x8000 | (DATA + 13),
    0x650d, 0x650d, 0x650d, 0x650d, 0x650d, 0x650d,     // >r x6
    0x8000 | DATA, 0x8000 | (DATA + 1), 0x8000 | (DATA + 2),
    0x8000 | (DATA + 3), 0x8000 | (DATA + 4), 0x8000 | (DATA + 5),
};
static context ctx;
static void run(const uint16_t *code, int n, context *out) {
    memset(&ctx, 0, sizeof ctx);
    memcpy(ctx.memory + CODE, prefix, sizeof prefix);
    memcpy(ctx.memory + CODE + sizeof prefix / 2, code, n * 2);
    ctx.EIP = CODE;
    int64_t *cells = (int64_t *)((uint8_t *)ctx.memory + 0x400);
    for (int k = 0; k < 16; k++) cells[k] = DATA + (k * 5 + 3) % 16;
    ctx.OUT = stdout; ctx.IN = stdin;
    vm(&ctx);
    if (ctx.EIP >= CODE) ctx.EIP = 0;   // only a RET moves the PC somewhere comparable
    *out = ctx;
}
int main(void) {
    static context a, b;
    int n, m, bad = 0, total = 0;
    while (scanf("%d", &n) == 1) {
        uint16_t code[64], other[64];
        for (int i = 0; i < n; i++) scanf("%hx", &code[i]);
        scanf("%d", &m);
        for (int i = 0; i < m; i++) scanf("%hx", &other[i]);
        run(code, n, &a);
        run(other, m, &b);
        total++;
        if (a.SP != b.SP || a.RSP != b.RSP || a.EIP != b.EIP ||
            memcmp(a.DSTACK, b.DSTACK, 8 * a.SP) || memcmp(a.RSTACK, b.RSTACK, 8 * a.RSP) ||
            memcmp((uint8_t *)a.memory + 0x400, (uint8_t *)b.memory + 0x400, 0x100)) {
            bad++;
            printf("MISMATCH:");
            for (int i = 0; i < m; i++) printf(" %04x", other[i]);
            printf(" vs");
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
        # A small memory: it is cleared for every sequence that is run.
        subprocess.check_call(["cc", "-O1", "-w", "-fcommon", "-DADDR_BITS=14", f"-I{root}", "-o", exe, src,
                               f"{root}/vm.c", f"{root}/vm_opcodes.c", f"{root}/vm_debug.c",
                               f"{root}/util/stack.c", "-lm"])
        lines = "".join(f"{len(c)} {' '.join(f'{w:04x}' for w in c)} "
                        f"{len(o)} {' '.join(f'{w:04x}' for w in o)}\n" for c, o in claims)
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
