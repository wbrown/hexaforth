#!/usr/bin/env python3
"""Regions: the idea, and the machine against it.

    region_test.py [SEED]                   the model alone
    region_test.py VM IMAGE.hex [SEED [COUNT]]   region.fs on the reference VM

A region is a count of the cells that follow, and then bands.  A band is

    n  x0 .. x(n-1)  T

n crossings, and T, where the band ends: the row it goes down to, plus
AS_END, so that it is further along than any crossing and ends them.  The
first band has no crossings and ends at the region's top; the last has none
and ends at NEVER.  A band is an even number of cells and a region starts
on an even cell, so a crossing in is at an even address and a crossing out
at an odd one: a place in a list is inside the region if its address is odd.

The model is region.fs's merge and sweep with memory an array: merge has
two places to read and one to write and nothing else, and the sweep has the
band of each region and the last band written.  Alone, it is run on random
rectangles combined, and every pixel and the form of every region checked.

A set of pixels has one form as a region (`canon`), so the machine is
checked a cell at a time: random rectangles are made, combined, moved and
boxed at the prompt of the reference VM, and every cell of every region
made is compared with the one form of what it should hold.
"""
import random
import subprocess
import sys

AS_END = 1 << 32                        # a row, as the end of a band
END_MIN = 1 << 31                       # every crossing is below this and every end above
NEVER = 1 << 33
mem = [0] * 200000
top = [2]


def new_rgn():
    a = top[0]
    top[0] += 258
    assert a % 2 == 0
    return a


def rect_rgn(x, y, w, h, r):
    if w <= 0 or h <= 0:
        mem[r:r + 3] = [2, 0, NEVER]
        return
    mem[r:r + 9] = [8, 0, y + AS_END, 2, x, x + w, y + h + AS_END, 0, NEVER]


OPS = {"and": lambda a, b: a & b, "or": lambda a, b: a | b, "diff": lambda a, b: a & ~b}
turns = [0]


def merge(qa, qb, qo, op):
    """Two places to read and one to write.  Returns where the band's end was written."""
    while True:
        ha, hb = mem[qa], mem[qb]
        x = min(ha, hb)
        mem[qo] = x                     # where the next crossing goes, if this is one
        if x >= END_MIN:                # the end of both: and it is the end of the answer's band
            return qo
        turns[0] += 1
        if ha == x:
            qa += 1
        if hb == x:
            qb += 1
        qo += (op(qa, qb) ^ qo) & 1     # it is one if the answer is not what it was


def nxt(c):
    return c + mem[c] + 2


def rgn_op(a, b, to, op):
    ca, cb, pl = a + 1, b + 1, to + 1
    mem[pl] = 0                         # a band of nothing, to be the one above the first
    while True:
        po = nxt(pl)
        qo = merge(ca + 1, cb + 1, po + 1, op)
        mem[po] = qo - po - 1
        if mem[pl:pl + mem[pl] + 1] == mem[po:po + mem[po] + 1]:
            mem[po - 1] = mem[qo]       # the same crossings: the band above goes on down
        else:
            pl = po
        t = mem[nxt(pl) - 1]
        if mem[nxt(ca) - 1] == t:
            ca = nxt(ca)
        if mem[nxt(cb) - 1] == t:
            cb = nxt(cb)
        if t == NEVER:
            break
    mem[to] = nxt(pl) - to - 1


def canon(pixels):
    """The one form of a set of pixels: the cells of the region, count and all."""
    rows = {}
    for x, y in pixels:
        rows.setdefault(y, set()).add(x)
    cells = []
    if rows:
        cells += [0, min(rows) + AS_END]
        last = None
        for y in range(min(rows), max(rows) + 1):
            xs = rows.get(y, set())
            cross = sorted({x for x in xs if x - 1 not in xs} ^ {x + 1 for x in xs if x + 1 not in xs})
            if cross == last:
                cells[-1] = y + 1 + AS_END
            else:
                cells += [len(cross)] + cross + [y + 1 + AS_END]
                last = cross
    cells += [0, NEVER]
    return [len(cells)] + cells


def model(seed):
    rng = random.Random(seed)
    checks = 0
    for _ in range(4000):
        top[0] = 2
        pool = []
        for _ in range(3):
            r = new_rgn()
            x, y = rng.randrange(-6, 24), rng.randrange(-6, 24)
            w, h = rng.randrange(-1, 14), rng.randrange(-1, 14)
            rect_rgn(x, y, w, h, r)
            want = {(xx, yy) for xx in range(x, x + w) for yy in range(y, y + h)}
            assert mem[r:r + mem[r] + 1] == canon(want)
            pool.append((r, want))
        for _ in range(8):
            (a, pa), (b, pb) = rng.choice(pool), rng.choice(pool)
            how = rng.choice(list(OPS))
            to = new_rgn()
            rgn_op(a, b, to, OPS[how])
            want = {"and": pa & pb, "or": pa | pb, "diff": pa - pb}[how]
            assert mem[to:to + mem[to] + 1] == canon(want), (how, mem[to:to + mem[to] + 1], canon(want))
            pool.append((to, want))
            checks += 1
    print(f"the model: {checks} operations on regions of up to eleven rectangles, every region the one "
          f"form of its pixels; {turns[0]} turns of merge")


REGIONS = 8


def machine(vm, image, seed, count):
    rng = random.Random(seed)
    lines = [f"new-rgn constant r{i}" for i in range(REGIONS)]
    lines.append(": d swap . dup @ 1+ over + swap do i @ . loop ;")     # ( n rgn -- )
    held = [set() for _ in range(REGIONS)]
    want = []                                           # what each line that prints should print
    for n in range(count):
        kind = rng.choice(["rect"] * 3 + ["and", "or", "diff"] * 3 + ["offset", "box", "copy"])
        to = rng.randrange(REGIONS)
        if kind == "rect":
            x, y = rng.randrange(-40, 800), rng.randrange(-40, 600)
            w, h = rng.randrange(-2, 300), rng.randrange(-2, 200)
            if rng.random() < 0.3:                      # edges that others have, so that crossings meet
                x, y = x // 50 * 50, y // 50 * 50
                w, h = w // 50 * 50, h // 50 * 50
            lines.append(f"{x} {y} {w} {h} r{to} rect-rgn")
            held[to] = {(xx, yy) for xx in range(x, x + w) for yy in range(y, y + h)}
        elif kind in OPS:
            a = rng.choice([i for i in range(REGIONS) if i != to])
            b = rng.choice([i for i in range(REGIONS) if i != to])
            lines.append(f"r{a} r{b} r{to} rgn-{kind}")
            held[to] = {"and": held[a] & held[b], "or": held[a] | held[b], "diff": held[a] - held[b]}[kind]
        elif kind == "offset":
            dx, dy = rng.randrange(-60, 60), rng.randrange(-60, 60)
            lines.append(f"{dx} {dy} r{to} rgn-offset")
            held[to] = {(x + dx, y + dy) for x, y in held[to]}
        elif kind == "copy":
            a = rng.choice([i for i in range(REGIONS) if i != to])
            lines.append(f"r{a} r{to} rgn-copy")
            held[to] = set(held[a])
        elif kind == "box":
            if not held[to]:
                continue
            xs, ys = [x for x, _ in held[to]], [y for _, y in held[to]]
            lines.append(f"{n} . r{to} rgn-box . . . .")
            want.append((lines[-1], [n, max(ys) + 1 - min(ys), max(xs) + 1 - min(xs), min(ys), min(xs)]))
            continue
        if len(canon(held[to])) > 250:                  # more than a region holds: start it again
            lines[-1] = f"r{to} rgn-none"
            held[to] = set()
        lines.append(f"{n} r{to} d")
        want.append((lines[-1], [n] + canon(held[to])))
        lines.append(f"{n} . r{to} rgn-empty? .")
        want.append((lines[-1], [n, -1 if not held[to] else 0]))
    typed = "".join(line + "\n" for line in lines) + "8000 execute\n"
    out = subprocess.run([vm, image], input=typed, capture_output=True, text=True, timeout=600).stdout
    failed = 0
    for line, cells in want:
        at = out.find(line + " ")
        got = out[at + len(line):].split("\n")[0].split() if at >= 0 else ["(not echoed)"]
        if got != [str(c) for c in cells] + ["ok"]:
            failed += 1
            if failed <= 5:
                print(f"FAIL {line}\n  wanted {' '.join(map(str, cells))}\n  got    {' '.join(got)}")
    print(f"the machine, seed {seed}: {len(want)} regions and boxes looked at, {failed} wrong")
    return 1 if failed else 0


def main():
    args = sys.argv[1:]
    if len(args) < 2:
        model(int(args[0]) if args else 1)
        return 0
    vm, image = args[:2]
    seed = int(args[2]) if len(args) > 2 else 1
    count = int(args[3]) if len(args) > 3 else 400
    return machine(vm, image, seed, count)


if __name__ == "__main__":
    sys.exit(main())
