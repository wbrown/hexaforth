#!/usr/bin/env python3
"""Peephole rules that have a literal in them, for cross.fs.

    litrules.py forth/litrules.fs [--verify]

peephole.fs is about ALU instructions next to each other.  A literal between
them, or before them, stops it: `16 -` is a literal, an invert, an add of 1
and an add, and nothing in those tables says it is an add of -16.

Here a literal is a token with a value that is not known until the rule is
used: L pushes a number (of either sign), A adds one (not negative) to the
top of the stack, and V and W fetch a variable (the instruction that does
it in one: any variable, and the two may be the same one).  A shape is a few tokens, the last of them the one being
compiled: ALU instructions, written a, b, and X for the last, and at most
two literals.  Every shape is tried with every instruction the peephole can
have left in each place, and for each the shortest thing that does the same
is looked for among: nothing, one or two ALU instructions, and one literal
with up to two ALU instructions round it, where the literal's value is the
shape's literals added or subtracted and one more or less.  A shape that
fetches variables is replaced only by something that fetches the same ones,
in either order: fetching is all the same whichever is first, so the
fetches a sequence makes are compared as a set.  What is found
with one pair of values is then checked with others, large and small and
negative, and (with --verify) run on vm.c.

A shape may also end at a conditional branch, B.  The branch takes the top
of the stack and asks only whether it is zero, so what comes before it may
be replaced by anything that leaves the same answer to that: `<> if` is
`xor if`, and `0<> if` is `if`.

A rule says only that two things do the same.  cross.fs uses it when what
it would compile is shorter than what is there, which depends on the value:
a number that does not fit one instruction costs more.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import insn_map as im                               # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
INVERT = 0x6480

# The value of a rule's literal: sk*K + sj*J + c, where K and J are the
# shape's literals in order.  cross.fs has the same numbering (lit-fn).
FNS = [(sk, sj, c) for sk in (-1, 0, 1) for sj in (-1, 0, 1) for c in (-1, 0, 1)]


def fn_id(f):
    return (f[0] + 1) * 9 + (f[1] + 1) * 3 + f[2] + 1


def lit(v):
    """Push v, as cross.fs does."""
    if v < 0:
        return lit(~v) + [INVERT]
    out = []
    for sh in (2, 1, 0):
        c = v >> 12 * sh & 0xFFF
        if c:
            out.append((0xC000 if out else 0x8000) | sh << 12 | c)
    return out or [0x8000]


def addlit(v):
    return [0xC000 | sh << 12 | (v >> 12 * sh & 0xFFF) for sh in (2, 1, 0) if v >> 12 * sh & 0xFFF]


def tables():
    """What peephole.fs has: what it can leave in the code, and what it joins."""
    reach, pairs, table = set(), set(), None
    for line in open(f"{ROOT}/forth/peephole.fs"):
        m = re.match(r"create (\w+)", line)
        if m:
            table = m.group(1)
        m = re.match(r"\$([0-9a-f]+) , \$([0-9a-f]+) ,", line)
        if m:
            k, v = int(m.group(1), 16), int(m.group(2), 16)
            reach |= {v >> 16, v & 0xFFFF} if table == "fuse32" else {v}
            if table == "fuse2":
                pairs.add((k >> 16, k & 0xFFFF))
    return reach, pairs


beh = im.effect         # behaviour, with the fetches it makes in no order


def build(shape, alus, k, j, va=0xB111, vb=0xB222):
    """The instructions of a shape, or None if a value cannot be written."""
    out, a, lits = [], iter(alus), iter((k, j))
    for t in shape:
        if t == "B":
            continue
        if t == "V":
            out.append(va)
        elif t == "W":
            out.append(vb)
        elif t == "L":
            out += lit(next(lits))
        elif t == "A":
            v = next(lits)
            if v < 0:
                return None
            out += addlit(v)
        else:
            out.append(next(a))
    return out


def instantiate(rshape, x, y, v, va=0xB111, vb=0xB222):
    out = []
    for e in rshape:
        if e == "V":
            out.append(va)
        elif e == "W":
            out.append(vb)
        elif e == "x":
            out.append(x)
        elif e == "y":
            out.append(y)
        elif e == "L":
            out += lit(v)
        else:
            if v < 0:
                return None
            out += addlit(v)
    return out


RSHAPES = ["", "x", "L", "A", "Lx", "xL", "Ax", "xA", "xy", "Lxy", "xLy", "xyL", "Axy", "xAy", "xyA"]


def with_vars(vs):
    """What may replace a shape that fetches the variables vs: up to three
    tokens, each of the fetches once, and the rest as RSHAPES has."""
    out = set()
    for rest in ("", "x", "L", "A", "xy", "xL", "Lx", "xA", "Ax"):
        if len(rest) + len(vs) > 3:
            continue
        for order in ([vs] if len(vs) == 1 else [vs, vs[::-1]]):
            toks = list(rest)

            def place(done, left, vars_):
                if not vars_:
                    out.add("".join(done + left))
                    return
                for n in range(len(left) + 1):
                    place(done + left[:n] + [vars_[0]], left[n:], vars_[1:])
            place([], toks, list(order))
    return sorted(out, key=lambda r: (len(r), r))


VARS = [(0xB111, 0xB222), (0xB005, 0xB005), (0xB222, 0xB111), (0xB7FF, 0xB000)]
PRIMARY = [(1000, 37), (37, 1000)]
CHECKS = [(5, 3), (0, 0), (4095, 1), (70000, 64), (-77, 12), (63, 5000), (-1, 1), (1, -4096)]


def main():
    path = sys.argv[1]
    words = im.load_words(f"{ROOT}/vm_opcodes.h")
    plain = lambda i: i >> 13 == 3 and not i & 0x8000 and i >> 4 & 15 != 15 and i >> 8 & 3 != 2 \
        and not i & 0x1000
    base = sorted({i for code in words.values() for i in code if plain(i)})
    reach, pairs = tables()
    reach = sorted(i for i in reach | set(base) if plain(i))
    started = time.time()

    # ---- what a shape may be replaced by, for each pair of values tried first
    def candidates(k, j, two, vs, br):
        fns = [f for f in FNS if (two or f[1] == 0) and (f[0] or f[1])]
        table = {}

        def offer(seq, entry):
            key = beh(seq, br)
            if key is None:
                return
            h = hash(key)
            old = table.get(h)
            if old is None or entry[0] < old[0]:
                table[h] = entry

        for rs in (with_vars(vs) if vs else RSHAPES):
            xs = [0] if "x" not in rs else (reach if "y" not in rs else base)
            ys = [0] if "y" not in rs else base
            fs = [None] if not ("L" in rs or "A" in rs) else fns
            for f in fs:
                v = 0 if f is None else f[0] * k + f[1] * j + f[2]
                for x in xs:
                    for y in ys:
                        seq = instantiate(rs, x, y, v)
                        if seq is not None:
                            offer(seq, (len(rs), rs, x, y, f))
        return table

    def shapes():
        """(shape, the places for instructions) for every shape tried."""
        one, three = (reach, base), (base, base, base)
        yield "LX", (reach,)
        yield "AX", (reach,)
        for s in ("LaX", "aLX", "AaX", "aAX"):
            yield s, one
        yield "LLX", (base,)
        for s in ("LaLX", "LaAX"):
            yield s, one
        for s in ("LabX", "aLbX", "abLX", "AabX", "aAbX", "abAX"):
            yield s, three
        yield "LaA", (reach,)
        yield "AaA", (reach,)
        for s in ("LabA", "AabA"):
            yield s, (base, base)
        # with a variable fetched, or two
        yield "VX", (reach,)
        for s in ("VaX", "aVX", "VLaX", "LVaX", "VaLX", "LaVX", "aVLX", "aLVX", "VAaX",
                  "VWaX", "VaWX", "aVWX"):
            yield s, one
        for s in ("VLX", "LVX", "VAX", "VWX", "VWLX", "VLWX", "LVWX"):
            yield s, (base,)
        for s in ("VabX", "aVbX", "abVX", "VWabX", "VWaAX", "VaAbX", "VLabX", "LVabX"):
            yield s, three if s.count("a") + s.count("b") == 2 else (base, base)
        yield "VaA", (reach,)
        yield "aVA", (reach,)
        yield "VWaA", (reach,)
        # before a conditional branch
        for s in ("aB", "LaB", "AaB", "VaB", "LVaB", "VLaB", "VWaB"):
            yield s, (reach,)
        for s in ("abB", "LabB", "aLbB", "AabB", "VabB", "aVbB"):
            yield s, one
        yield "abcB", three
        yield "LabcB", three

    def combos(places):
        if len(places) == 1:
            for a in places[0]:
                yield (a,)
        elif len(places) == 2:
            for a in places[0]:
                for b in places[1]:
                    yield (a, b)
        else:
            for a in places[0]:
                for b in places[1]:
                    for c in places[2]:
                        yield (a, b, c)

    def adjacent_ok(shape, alus):
        """No two instructions next to each other that peephole.fs would join."""
        toks, it = [], iter(alus)
        for t in shape:
            toks.append(next(it) if t in "abcX" else None)  # a literal or a fetch is not one
        return not any(p is not None and q is not None and (p, q) in pairs
                       for p, q in zip(toks, toks[1:]))

    def holds(shape, alus, rs, x, y, f, k, j, va=0xB111, vb=0xB222):
        """None if the rule cannot be tried with these values, else whether it holds."""
        br = shape[-1] == "B"
        before = build(shape, alus, k, j, va, vb)
        v = 0 if f is None else f[0] * k + f[1] * j + f[2]
        after = instantiate(rs, x, y, v, va, vb)
        if before is None or after is None:
            return None
        want = beh(before, br)
        if want is None:
            return None
        return want == beh(after, br)

    rules = {}                                      # (shape, alus) -> (rshape, x, y, fn)
    cand = {}
    for shape, places in shapes():
        two = sum(t in "LA" for t in shape[:-1]) + (shape[-1] == "A") == 2
        vs = "".join(t for t in shape if t in "VW")
        br = shape[-1] == "B"
        length = len(shape) - br                    # the branch stays: it is not replaced
        found = 0
        for k, j in (PRIMARY if two else PRIMARY[:1]):
            if (k, j, two, vs, br) not in cand:
                cand[(k, j, two, vs, br)] = candidates(k, j, two, vs, br)
            table = cand[(k, j, two, vs, br)]
            for alus in combos(places):
                if (shape, alus) in rules or not adjacent_ok(shape, alus):
                    continue
                # what is shorter whatever follows was dealt with when its last
                # instruction was compiled: only what the branch allows is new
                if br and (shape[:-2] + "X", alus) in rules:
                    continue
                seq = build(shape, alus, k, j)
                key = beh(seq, br)
                if key is None:
                    continue
                entry = table.get(hash(key))
                if entry is None or entry[0] >= length:
                    continue
                _, rs, x, y, f = entry
                tried = [holds(shape, alus, rs, x, y, f, a, b, *VARS[n % len(VARS)])
                         for n, (a, b) in enumerate([(k, j)] + CHECKS)]
                if tried[0] and False not in tried and tried.count(True) >= 3:
                    rules[(shape, alus)] = (rs, x, y, f)
                    found += 1
        print(f"  {shape:5} {found:6} rules   ({time.time() - started:.0f}s)", flush=True)

    # ---- write them
    kind = {"a": 1, "b": 1, "c": 1, "L": 2, "A": 3, "V": 4, "W": 4}
    relem = {"x": 1, "y": 2, "L": 3, "A": 4, "V": 5, "W": 6}
    rows = []
    for (shape, alus), (rs, x, y, f) in rules.items():
        before = shape[:-1][::-1]                   # nearest the one being compiled first
        sid = "XAB".index(shape[-1]) << 12 | sum(kind[t] << 3 * n for n, t in enumerate(before))
        key = sid
        for a in (list(alus) + [0, 0, 0])[:3]:
            key = key << 16 | a
        rid = sum(relem[e] << 3 * n for n, e in enumerate(rs))
        value = rid << 40 | (fn_id(f) if f else 0) << 32 | x << 16 | y
        rows.append((key, value))
    rows.sort()
    with open(path, "w") as f:
        f.write("\\ Peephole rules that have a literal in them.\n"
                "\\ GENERATED by `python3 util/litrules.py forth/litrules.fs`: do not edit.\n"
                "\\\n"
                "\\ Each entry is two cells.  The first is the shape: which of the tokens\n"
                "\\ before the one being compiled are instructions, pushes and adds, and\n"
                "\\ the instructions.  The second is what does the same: its tokens, how\n"
                "\\ its literal's value is made from the shape's, and its instructions.\n"
                "\\ See fuse-lit in cross.fs.\n\n")
        f.write(f"#{len(rows)} constant #fuseL\ncreate fuseL\n")
        for key, value in rows:
            f.write(f"${key:x} , ${value:x} ,\n")
    print(f"{path}: {len(rows)} rules, {time.time() - started:.0f}s")

    if "--verify" in sys.argv:
        claims = []
        for (shape, alus), (rs, x, y, f) in rules.items():
            for n, (k, j) in enumerate([(1000, 37), (5, 3), (70000, 64), (-77, 12)]):
                va, vb = VARS[n]
                before = build(shape, alus, k, j, va, vb)
                v = 0 if f is None else f[0] * k + f[1] * j + f[2]
                after = instantiate(rs, x, y, v, va, vb)
                if before and after is not None and im.behaviour(before) is not None:
                    # before a branch only whether the top is zero is the same: ask that
                    tail = [0x8000, 0x649C] if shape[-1] == "B" else []
                    claims.append((before + tail, after + tail))
        return im.verify(ROOT, claims)
    return 0


if __name__ == "__main__":
    sys.exit(main())
