#!/usr/bin/env python3
"""Find fewer instructions that do what a loop does, wherever its values are kept.

    shorter.py IMAGE.hex COUNTS [REPORT]

The peephole joins instructions that are next to each other, and keeps
every value where the source put it.  A loop has more to give: what it
keeps from one turn to the next (a pointer, an end, a distance) can be
anywhere, as long as it is in the same place at the start of every turn;
and the end of a turn is next to the start of the next.

For each loop of the image that ran and calls nothing, this takes one turn
of it as a whole (the test, and the body) and looks for the fewest
instructions that do the same, trying each way of keeping the loop's
values in the top cells of the two stacks.  The turn it looks for ends
with the test, so that one conditional branch at the bottom goes back and
no jump is needed: `begin body test until`, with the test made the other
way round.

The search is over every ALU instruction, the literals and the variables
the loop uses.  An instruction is tried only if what it computes is part
of what the turn must leave (or is a value moved), and a line of search is
given up when what is left to do cannot be done in the instructions left.
What it finds is checked with the model of insn_map.py, as the peephole's
rules are.

A turn of more than a few instructions is too much to search whole, and
the search is given up after NODES states (the report says when); then the
turn is also shortened a part at a time, with the values where they are.
Turns of more than LONGEST instructions (12) are not looked at.

The report is build/shorter.txt: for each loop, what a turn costs now, the
fewest instructions found with the values where they are, and the fewest
found with them anywhere, with where each value is kept.
"""
import itertools
import os
import struct
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import insn_map as im                               # noqa: E402
import loops as lp                                  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEPTH = im.State.DEPTH
DELTAS = (0, 1, -1, -2)
VAR_TOP = im.VAR_BASE + 0x1000
NODES = int(os.environ.get("NODES", 20000))         # states looked at, for each length tried
LONGEST = int(os.environ.get("LONGEST", 12))        # instructions in a turn that is looked at


class Budget(Exception):
    pass


def leaves(e, memo={}):
    """The cells of the stacks, as they were at the start, that a value is made of."""
    if isinstance(e, int) or isinstance(e, str):
        return frozenset()
    got = memo.get(e)
    if got is None:
        if e[0] in ("d", "r"):
            got = frozenset([e])
        elif e[0] == "sum":
            got = frozenset().union(*(leaves(t) for t in e[2]))
        else:
            got = frozenset().union(*(leaves(x) for x in e[1:]))
        memo[e] = got
    return got


def parts(e, out):
    """Everything a value is made from, and each part of a sum."""
    if e in out or isinstance(e, str):
        return
    out.add(e)
    if isinstance(e, int) or e[0] in ("d", "r"):
        return
    if e[0] == "sum":
        terms = e[2]
        for mask in range(1, 1 << len(terms)):
            part = 0
            for i, t in enumerate(terms):
                if mask >> i & 1:
                    part = im.add(part, t)
            out.add(part)
            out.add(im.add(part, e[1]))
        out.add(e[1])
        for t in terms:
            parts(t, out)
    elif e[0] == "load":
        parts(e[2], out)
    else:
        for x in e[1:]:
            parts(x, out)


def chain(mem):
    """The stores that made a memory, first first."""
    out = []
    while mem != "mem0":
        out.append(mem)
        mem = mem[1]
    return out[::-1]


class Target:
    """What a sequence must leave: the two stacks, memory, and (with fact) the
    top of the data stack only as far as whether it is zero."""

    def __init__(self, d, r, mem, loads, fact=None):
        self.d, self.r, self.mem, self.fact = tuple(d), tuple(r), mem, fact
        self.loads = set(loads)
        stores = chain(mem)
        self.prefix = {"mem0": 0}
        for i, m in enumerate(stores):
            self.prefix[m] = i + 1
        self.nstores = len(stores)
        self.needed = set()
        body = self.d[:-1] if fact else self.d
        for e in body + self.r:
            parts(e, self.needed)
        for m in stores:
            parts(m[2], self.needed)
            parts(m[3], self.needed)
        for l in self.loads:
            parts(l[2], self.needed)
        if fact:
            for x in fact[1:]:
                parts(x, self.needed)
        self.final = set(body) | set(self.r)
        self.need_leaves = frozenset().union(*(leaves(e) for e in self.final),
                                             *(leaves(m[2]) | leaves(m[3]) for m in stores),
                                             *(leaves(x) for x in (fact[1:] if fact else ())))
        ints = {e for e in self.needed if isinstance(e, int)}
        self.consts = sorted(c for c in ints if literal(c) is not None)
        self.var_loads = sorted({l[2] for l in self.loads
                                 if isinstance(l[2], int) and im.VAR_BASE <= l[2] < VAR_TOP})
        self.var_stores = sorted({m[2] for m in stores
                                  if isinstance(m[2], int) and im.VAR_BASE <= m[2] < VAR_TOP})

    def done(self, state):
        d, r, mem = state
        if r != self.r or mem != self.mem or len(d) != len(self.d):
            return False
        if self.fact:
            return d[:-1] == self.d[:-1] and im.is_zero(d[-1]) == self.fact
        return d == self.d

    def left(self, state):
        """No fewer instructions than this are still needed."""
        d, r, mem = state
        present = set(d) | set(r)
        have = frozenset().union(*(leaves(e) for e in present))
        if not self.need_leaves <= have:
            return 99                               # something needed is gone for good
        values = self.nstores - self.prefix[mem] + sum(1 for v in self.final if v not in present)
        if self.fact and not any(im.is_zero(v) == self.fact for v in present):
            values += 1
        td = self.d[:-1] if self.fact else self.d
        extra = 1 if self.fact else 0
        return max(values, shape(d, td, extra), shape(r, self.r, 0))


def shape(s, t, extra):
    """Instructions needed to make stack s into t (and `extra` more cells on it):
    a cell can be written only when it is the top, or just under it."""
    want = len(t) + extra
    i = next((i for i, (a, b) in enumerate(zip(s, t)) if a != b), None)
    if i is None:
        if len(s) < len(t):
            return want - len(s)
        return (len(s) - want + 1) // 2 if len(s) > want else want - len(s)
    size = len(s)
    if size <= i:
        first = i + 1 - size
    elif size <= i + 3:
        first = 1
    else:
        first = (size - i - 2) // 2 + 1
    return first + max(0, want - (i + 2))


def literal(c):
    """The instruction that pushes c, if one does."""
    for s in range(3):
        if c >> 12 * s << 12 * s == c and c >> 12 * s < 4096:
            return 0x8000 | s << 12 | c >> 12 * s
    return None


def successors(state, T):
    """Every instruction worth trying from a state, and the state it gives."""
    d, r, mem = state
    if len(d) < 2:
        return
    t, n, r0 = d[-1], d[-2], r[-1]
    present = set(d) | set(r)
    needed, fact = T.needed, T.fact
    for src in range(4):
        if src == 2:
            inp = ("load", mem, t)
            if inp not in T.loads:
                continue
        else:
            inp = (n, t, None, r0)[src]
        for op in range(15):
            nd = d
            if op == 0:
                out = inp
            elif op == 1:
                out, nd = inp, d[:-2] + (t, n)
            elif op == 2:
                out, nd = inp, d[:-2] + (t, t)
            elif op == 3:
                out = im.add(inp, n)
            elif op in (4, 5, 6, 7, 9):
                out = im.commutative(im.ALU_OPS[op], inp, n)
            elif op == 8:
                out = im.invert(inp)
            elif op in (10, 11):
                out = (im.ALU_OPS[op], n, inp)
            elif op in (12, 13):
                out = (im.ALU_OPS[op], inp, t)
            else:
                out = ("load", mem, inp)
                if out not in T.loads:
                    continue
            if not (out in needed or out in present or (fact and im.is_zero(out) == fact)):
                continue
            addr = nd[-1]
            for dd in DELTAS:
                d2 = nd + (nd[-1],) if dd > 0 else nd[:dd] if dd else nd
                if not d2 or len(d2) > DEPTH + 3:
                    continue
                for rd in DELTAS:
                    r2 = r + (r0,) if rd > 0 else r[:rd] if rd else r
                    if not r2 or len(r2) > DEPTH + 3:
                        continue
                    yield im.encode(src, op, 0, dd, rd, 0), (d2[:-1] + (out,), r2, mem)
                    yield im.encode(src, op, 1, dd, rd, 0), (d2, r2[:-1] + (out,), mem)
                    m2 = ("store", mem, addr, out)
                    if m2 in T.prefix:
                        yield im.encode(src, op, 3, dd, rd, 0), (d2, r2, m2)
    for c in T.consts:
        ins = literal(c)
        if len(d) < DEPTH + 3:
            yield ins, (d + (c,), r, mem)
        if c:
            v = im.add(t, c)
            if v in needed or (fact and im.is_zero(v) == fact):
                yield ins | 0x4000, (d[:-1] + (v,), r, mem)
    for a in T.var_loads:
        v = ("load", mem, a)
        if v in T.loads and len(d) < DEPTH + 3:
            yield 0xB000 | a - im.VAR_BASE, (d + (v,), r, mem)
    for a in T.var_stores:
        m2 = ("store", mem, a, t)
        if m2 in T.prefix and len(d) > 1:
            yield 0xF000 | a - im.VAR_BASE, (d[:-1], r, m2)


def exactly(T, length, nodes, kept=None):
    """A sequence of `length` instructions that leaves T, or None.  With
    kept, a number that is on top of the return stack at the start."""
    start = (tuple(("d", i) for i in reversed(range(DEPTH))),
             tuple(("r", i) for i in reversed(range(DEPTH))) + (() if kept is None else (kept,)),
             "mem0")
    seen = {}
    count = [0]

    def go(state, left, path):
        if left == 0:
            return list(path) if T.done(state) else None
        count[0] += 1
        if count[0] > nodes:
            raise Budget()
        for ins, s in successors(state, T):
            if seen.get(s, -1) >= left - 1:
                continue
            seen[s] = left - 1
            if T.left(s) > left - 1:
                continue
            path.append(ins)
            found = go(s, left - 1, path)
            if found:
                return found
            path.pop()
        return None

    if T.left(start) > length:
        return None
    return go(start, length, [])


def check(seq, T):
    """The model of insn_map.py agrees that seq leaves T."""
    key = im.behaviour(seq)
    if key is None:
        return False
    return T.done((key[0], key[1], key[2])) and set(key[4]) <= T.loads and not key[3]


# --------------------------------------------------------------------------
# One turn of a loop, and the same with its values kept elsewhere.

def substitute(e, m):
    """A value, with the cells it is made of renamed."""
    if isinstance(e, int):
        return e
    if isinstance(e, str):
        return e
    tag = e[0]
    if tag in ("d", "r"):
        return m.get(e, e)
    if tag == "sum":
        out = e[1]
        for t in e[2]:
            out = im.add(out, substitute(t, m))
        return out
    if tag == "~":
        return im.invert(substitute(e[1], m))
    if tag in ("IN&N", "IN|N", "IN^N", "IN*N", "IN==N"):
        return im.commutative(tag, substitute(e[1], m), substitute(e[2], m))
    return (tag,) + tuple(substitute(x, m) for x in e[1:])


def turn(test, body):
    """One turn as the model has it: the state after the body and the test
    again, or None if the model cannot follow it or the stacks move."""
    st = im.State()
    try:
        if test is not None:
            for ins in body:
                st.step(ins)
            for ins in test:
                st.step(ins)
        else:
            for ins in body:
                st.step(ins)
    except (im.Underflow, im.Barrier):
        return None
    if st.io or st.pc is not None or len(st.d) != DEPTH + 1 or len(st.r) != DEPTH:
        return None
    return st


def live(st):
    """How many cells at the top of each stack a turn reads or changes."""
    used = set(leaves(st.d[-1]))                    # the test
    for m in chain(st.mem):
        used |= leaves(m[2]) | leaves(m[3])
    for l in st.loads:
        used |= leaves(l[2])
    for tag, stack in (("d", st.d[:-1]), ("r", st.r)):
        for i in range(DEPTH):
            e = stack[DEPTH - 1 - i]
            if e != (tag, i):
                used |= leaves(e) | {(tag, i)}
    kd = max([e[1] + 1 for e in used if e[0] == "d"] + [0])
    kr = max([e[1] + 1 for e in used if e[0] == "r"] + [0])
    return kd, kr


def layouts(kd, kr):
    """Every way of keeping kd+kr values in the top cells of the two stacks:
    a map from where a value is now to where it would be."""
    values = [("d", i) for i in range(kd)] + [("r", i) for i in range(kr)]
    k = len(values)
    for a in range(0, k + 1):
        if a > DEPTH - 2 or k - a > DEPTH - 2:
            continue
        slots = [("d", i) for i in range(a)] + [("r", i) for i in range(k - a)]
        for perm in itertools.permutations(slots):
            yield dict(zip(values, perm))


def retarget(st, m, continues_on_zero):
    """What a turn must leave when its values are kept as m says."""
    back = {v: k for k, v in m.items()}
    d = [("d", i) for i in reversed(range(DEPTH))]
    r = [("r", i) for i in reversed(range(DEPTH))]

    def was(slot):
        stack = st.d if slot[0] == "d" else st.r
        return stack[DEPTH - 1 - slot[1]]

    for slot, old in back.items():
        (d if slot[0] == "d" else r)[DEPTH - 1 - slot[1]] = substitute(was(old), m)
    mem = substitute(st.mem, m)
    loads = {substitute(l, m) for l in st.loads}
    fact = im.is_zero(substitute(st.d[-1], m))
    if continues_on_zero:
        fact = im._flip(fact)
    return Target(d + [0], r, mem, loads, fact)


def where(m):
    """A layout, in words."""
    name = lambda s: {("d", 0): "T", ("d", 1): "N", ("r", 0): "R"}.get(s, f"{s[0]}{s[1]}")
    moved = [f"{name(a)} in {name(b)}" for a, b in sorted(m.items()) if a != b]
    return ", ".join(moved) if moved else "as they are"


def turned(ins):
    """A test that is one instruction, the other way round (as cross.fs, `turned`)."""
    if ins & 0xF300 == 0x6000 and ins >> 4 & 15 in (6, 9):
        return ins ^ 0xF0
    return None


def by_parts(turn_code):
    """Shorten a turn a part at a time: any run of up to six of its
    instructions that fewer do the same as, with the values where they are."""
    X = list(turn_code)
    again = True
    while again:
        again = False
        for size in range(min(6, len(X)), 1, -1):
            for i in range(len(X) - size + 1):
                key = im.behaviour(X[i:i + size])
                if key is None or key[3]:
                    continue
                last = i + size == len(X)           # the test is at the end
                T = Target(key[0], key[1], key[2], key[4], im.is_zero(key[0][-1]) if last else None)
                for length in range(1, size):
                    try:
                        new = exactly(T, length, NODES // 4)
                    except Budget:
                        new = None
                    if new and check(new, T):
                        X[i:i + size] = new
                        again = True
                        break
                if again:
                    break
            if again:
                break
    return X


def structure(code, head, tail):
    """(test, body) of a loop that tests at the top, (None, body) of one that
    tests at the bottom, or None for anything else."""
    span = code[head:tail]
    last = code[tail]
    ctl = [i for i, w in enumerate(span) if not w & 0x8000 and w >> 13 != 3]
    if any(not w & 0x8000 and w >> 13 == 3 and w & 0x1000 for w in span):
        return None
    if last >> 13 == 0 and len(ctl) == 1:
        b = ctl[0]
        if span[b] >> 13 == 1 and im.flat_target(head + b, span[b]) == tail + 1:
            return span[:b], span[b + 1:]
    if last >> 13 == 1 and not ctl:
        return None, span
    return None


def main():
    image, counts_path = sys.argv[1], sys.argv[2]
    report = sys.argv[3] if len(sys.argv) > 3 else f"{ROOT}/build/shorter.txt"
    code, _ = im.load_image(image)
    raw = b"".join(struct.pack("<I", int(l, 16)) for l in open(image) if l.strip())
    raw += bytes(0x20000 - len(raw))
    import deskprof
    names = deskprof.word_names(raw)
    starts = sorted(names)
    data = open(counts_path, "rb").read()
    half = len(data) // 16
    work = list(struct.unpack(f"<{half}d", data[:8 * half]))[:len(code)]
    work += [0.0] * (len(code) - len(work))
    words = im.load_words(f"{ROOT}/vm_opcodes.h")
    single = {}
    for n, c in words.items():
        if len(c) == 1:
            single.setdefault(c[0], n)

    def show(w):
        if w & 0x8000:
            if w >> 12 & 3 == 3:
                return f"[var {w & 0xFFF:x} {'!' if w & 0x4000 else '@'}]"
            v = (w & 0xFFF) << 12 * (w >> 12 & 3)
            return f"{v} +" if w & 0x4000 else f"{v}"
        return single.get(w) or f"({im.describe(w)})"

    def word_of(a):
        return names[max(s for s in starts if s <= a)]

    out, saved_all, began = [], 0.0, time.time()
    rows = []
    for head, tail in lp.find_loops(code, starts):
        if work[tail] >= 1:
            rows.append((sum(work[head:tail + 1]), head, tail))
    only = os.environ.get("ONLY")
    for ran, head, tail in sorted(rows, reverse=True):
        turns = work[tail]
        if only and word_of(head) not in only.split(","):
            continue
        s = structure(code, head, tail)
        title = f"{word_of(head)}  {head:04x}-{tail:04x}: {turns:,.0f} turns"
        if s is None:
            out.append(f"{title}: not looked at (it calls a word, or branches inside)")
            continue
        test, body = s
        st = turn(test, body)
        if st is None:
            out.append(f"{title}: not looked at (the model cannot follow a turn)")
            continue
        now = len(body) + (len(test) + 2 if test is not None else 1)
        text = " ".join(show(w) for w in body)
        if now > LONGEST:
            out.append(f"{title}, {now} instructions a turn: not looked at (more than {LONGEST})")
            continue
        out.append(f"{title}, {now} instructions a turn")
        out.append(f"    now:  {'begin ' + ' '.join(show(w) for w in test) + ' while ' if test is not None else 'begin '}"
                   f"{text} {'repeat' if test is not None else 'until'}")
        kd, kr = live(st)
        best = {}
        # A turn that ends with the test, going round on nothing: the loop as
        # it is tests at the top and goes round on something.
        flip = test is not None
        same = {v: v for v in [("d", i) for i in range(kd)] + [("r", i) for i in range(kr)]}
        ways = [m for m in layouts(kd, kr) if m != same] if kd + kr <= 4 else []
        most = now - 1                              # instructions before the branch, at most
        for kept, ways in (("where they are", [same]), ("anywhere", ways)):
            found, gave_up = None, 0
            for length in range(1, most):
                for m in ways:
                    T = retarget(st, m, flip)
                    try:
                        seq = exactly(T, length, NODES if m == same else NODES // 10)
                    except Budget:
                        seq = None
                        gave_up += 1
                    if seq and check(seq, T):
                        found = (length, seq, m)
                        break
                if found:
                    break
            if gave_up:
                out.append(f"    (values {kept}: {gave_up} searches were given up, so fewer may do)")
            elif not found:
                out.append(f"    (values {kept}: no fewer instructions do it)")
            best[kept] = found
            if found:
                most = found[0]
            if found and (kept == "where they are" or found[0] < (best["where they are"] or (now,))[0]):
                length, seq, m = found
                out.append(f"    {length + 1} with the values {kept}"
                           + (f" ({where(m)})" if m != same else "") + ":  begin "
                           + " ".join(show(w) for w in seq) + " until")
        # What is known to do a turn with the values where they are: what was
        # found, or the loop itself with its test at the bottom.
        base = best["where they are"][1] if best["where they are"] else \
            list(body) if test is None else \
            body + [turned(test[0])] if len(test) == 1 and turned(test[0]) else None
        if base:
            if not best["where they are"] and test is not None:
                out.append(f"    {len(base) + 1} with the test at the bottom:  begin "
                           + " ".join(show(w) for w in base) + " until")
            part = by_parts(base)
            if len(part) < len(base):
                out.append(f"    {len(part) + 1} a part at a time:  begin "
                           + " ".join(show(w) for w in part) + " until")
            best["by parts"] = (len(part), part, same)
        # A number the turn pushes can be kept on the return stack instead,
        # put there before the loop: it is then an input of the ALU.
        most = min([f[0] for f in best.values() if f] + [now - 1])
        for c in sorted({w & 0xFFF for w in (test or []) + body if w & 0xF000 == 0x8000}):
            T = retarget(st, same, flip)
            T = Target(T.d, T.r + (c,), T.mem, T.loads, T.fact)
            for length in range(1, most):
                try:
                    seq = exactly(T, length, NODES, c)
                except Budget:
                    break
                if seq and check([literal(c), im.encode(1, 0, 1, -1, 1, 0)] + seq, T):
                    out.append(f"    {length + 1} with {c} kept in R:  {c} >r begin "
                               + " ".join(show(w) for w in seq) + " until rdrop")
                    best[f"kept {c}"] = (length, seq, same)
                    most = length
                    break
        fewest = min([f[0] + 1 for f in best.values() if f] + [now])
        if fewest < now:
            saved_all += (now - fewest) * turns
        print(f"{title}: {now} -> {fewest}   ({time.time() - began:.0f}s)", flush=True)
    out.insert(0, f"{image}: loops that call nothing, by how often they ran.  "
                  f"Instructions that the fewest found would save: {saved_all:,.0f} of {sum(work):,.0f}.\n")
    with open(report, "w") as f:
        f.write("\n".join(out) + "\n")
    print(f"\nall of it: {report}")


if __name__ == "__main__":
    sys.exit(main())
