#!/usr/bin/env python3
"""Say where the desktop's instructions go.

    deskprof.py KERNEL-profile.elf DISK IMAGE.hex SIZES

Boots a kernel built with -DPROFILE, which counts how often the instruction
at each address is executed, and drives the desktop through a few things a
person would do.  After each, the counts are read out of the machine and
the difference from the last reading is that activity's.

The desktop never stops: its event loop goes round whether anything is
happening or not.  So one reading is taken with nothing happening, to learn
what a turn of the loop costs, and that much is taken off each activity for
every turn made while it ran.  What is left is the work the activity
caused.
"""
import bisect
import os
import struct
import subprocess
import sys
import time
from collections import Counter

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, f"{HERE}/../util")
import guitest                                      # noqa: E402
import insn_map                                     # noqa: E402

PROFILE_AT = 0x400000       # as hexaforth.S
ADDR_BITS = 22
CODE = 0x10000              # instruction addresses
# Each task's part of the two stacks, as it is spawned with (wm.fs, view.fs, term.fs).
TASKS = (("the desktop", 0, 96), ("the picture", 96, 128), ("the Forth window", 128, 256))


def symbols(elf):
    out = {}
    for line in subprocess.check_output(["llvm-nm", elf], text=True).splitlines():
        addr, _, name = line.split()
        out[name] = int(addr, 16)
    return out


def chain(raw, link):
    """(xt, name, is-data) for each header from link back."""
    out = []
    while link:
        xt = struct.unpack_from("<Q", raw, link + 8)[0]
        out.append((xt & 0xFFFF, insn_map.name_at(raw, link + 16), xt >> 63))
        link = 8 * struct.unpack_from("<Q", raw, link)[0]
    return out


def word_names(raw, image):
    """Where each word starts: the dictionary in the image's bytes, and for
    the rest the labels, which are in the file of names beside the image."""
    cells = struct.unpack(f"<{0x10000 // 2}H", raw[:0x10000])     # the first 64KB
    head = 8 * cells[0x2000]                # the first cell of the data: cross.fs
    names = {}
    for value, n in insn_map.labels_of(image):
        if not value >> 63:
            names.setdefault(value & 0xFFFF, n)
    for a, n, _ in chain(raw, head):
        names[a] = n
    return names


def modules(sizes_path):
    """(first instruction, file) for each stretch of code, from the build's
    sizes: each line has the bytes of code a file compiled and the
    instruction it starts at.  A file that includes others has their code
    in its own, so an instruction is the smallest file's that has it; and
    a file may be in several pages, its code in each ending where the
    page's does."""
    spans = []
    for line in open(sizes_path):
        f = line.split()
        if f[0].startswith("(") or len(f) < 6 or not int(f[1]):
            continue
        spans.append((int(f[5]), int(f[5]) + int(f[1]) // 2, f[0]))
    edges = sorted({0} | {a for a, _, _ in spans} | {b for _, b, _ in spans})
    out = []
    for a in edges:
        over = [(b - s, n) for s, b, n in spans if s <= a < b]
        owner = min(over)[1] if over else "nuc.fs (kernel)" if a < 0x1000 else "(nothing)"
        if not out or out[-1][1] != owner:
            out.append((a, owner))
    return out


def kind(w):
    if w & 0x8000:
        if w >> 12 & 3 == 3:
            return "variable fetch or store"
        return "literal add" if w & 0x4000 else "literal push"
    return ["jump", "conditional branch", "call", "ALU"][w >> 13]


class Profiler:
    def __init__(self, elf, disk):
        self.m = guitest.Machine(elf, disk)
        syms = symbols(elf)
        self.mem = syms["image"]
        self.table = self.mem + (8 << ADDR_BITS) + 0x1000 + PROFILE_AT
        self.last = [0] * CODE

    def dump(self, addr, size):
        path = f"{self.m.tmp}/dump"
        self.m.command("pmemsave", val=addr, size=size, filename=path)
        time.sleep(0.2)
        return open(path, "rb").read()

    def reading(self):
        """Counts since the last reading."""
        now = struct.unpack(f"<{CODE}Q", self.dump(self.table, 8 * CODE))
        delta = [a - b for a, b in zip(now, self.last)]
        self.last = list(now)
        return delta


def main():
    elf, disk, image, sizes = sys.argv[1:5]
    p = Profiler(elf, disk)
    m = p.m
    phases = []

    def phase(name, settle=1.5):
        time.sleep(settle)
        phases.append((name, p.reading()))
        if os.environ.get("SHOTS"):                 # a picture after each, to see what was done
            m.screenshot(f"{os.environ['SHOTS']}/{len(phases):02d}.png")

    try:
        m.serial(b"desktop")
        phase("booting, with the picture dithered", settle=2.0)
        time.sleep(3.0)
        phase("nothing happening", settle=0)
        m.type("2 3 + .\r")
        m.type(": sq dup * ; 12 sq .\r")
        phase("typing two lines in the Forth window")
        m.type(": p 40 0 do i . cr loop ; p\r")
        phase("printing 40 lines, which scroll")
        for _ in range(4):
            m.key("w", "ctrl")
            time.sleep(0.4)
        phase("bringing each window to the front in turn")
        m.click(100, 40)                            # the Forth window, by its title
        m.drag(100, 40, 160, 90)
        m.drag(160, 90, 100, 40)
        phase("dragging the Forth window away and back")
        m.click(700, 308)                           # the calculator
        phase("raising the calculator")
        m.type("12\r34+")
        for bx, by in ((0, 0), (3, 1)):
            m.click(627 + 34 * bx + 17, 319 + 96 + 24 * by + 12)
        phase("the calculator: keys and buttons")
        m.click(190, 388)                           # the Words window, by a line of its list
        phase("raising the Words window and choosing a word")
        for _ in range(6):
            m.key("down")
        m.key("pgdn")
        phase("the Words window: six words down, and a page")
        m.click(480, 278)                           # the editor
        phase("raising the editor")
        m.type("hello there")
        phase("typing eleven characters in the editor")
        for x in range(100, 700, 40):
            m.move(x, 150 + x // 10)
        phase("moving the pointer across the screen")
        raw = p.dump(p.mem, 0x20000)
        stacks = p.dump(p.mem + (8 << ADDR_BITS), 0x1000)
    finally:
        m.stop()

    code = struct.unpack(f"<{len(raw) // 2}H", raw)
    built = b"".join(struct.pack("<I", int(l, 16)) for l in open(image) if l.strip())
    names = word_names(built + bytes(0x20000 - len(built)), image)
    starts = sorted(names)
    mods = modules(sizes)
    mod_at = [a for a, _ in mods]

    def word(a):
        i = bisect.bisect_right(starts, a) - 1
        if a >= 0xa000:
            return "(defined at the prompt)"
        return names[starts[i]] if i >= 0 else "(start)"

    def module(a):
        return mods[bisect.bisect_right(mod_at, a) - 1][1] if a < 0xa000 else "made at run time"

    # A turn of the event loop: the busiest address in event-loop is executed once a turn.
    loop = [a for a in range(CODE) if word(a) == "event-loop"]
    idle = next(c for n, c in phases if n == "nothing happening")
    turns = max(idle[a] for a in loop)
    per_turn = [c / turns for c in idle]
    print(f"A turn of the event loop with nothing happening: {sum(idle) / turns:.0f} instructions")
    by = Counter()
    for a, c in enumerate(idle):
        by[word(a)] += c
    print("  " + ", ".join(f"{n} {v / turns:.0f}" for n, v in by.most_common(9)))

    work = [0.0] * CODE                             # every activity's work, by address
    for name, counts in phases:
        if name == "nothing happening":
            continue
        t = max(counts[a] for a in loop)
        net = [max(0.0, c - t * r) for c, r in zip(counts, per_turn)]
        work = [a + b for a, b in zip(work, net)]
        total = sum(net)
        print(f"\n{name}: {total:,.0f} instructions of work "
              f"({sum(counts):,} executed, {t:,} turns of the loop)")
        bm, bw, bk = Counter(), Counter(), Counter()
        for a, c in enumerate(net):
            if c:
                bm[module(a)] += c
                bw[word(a)] += c
                bk[kind(code[a])] += c
        print("  by file:  " + ", ".join(f"{n} {100 * v / total:.0f}%" for n, v in bm.most_common(6)))
        print("  by word:  " + ", ".join(f"{n} {100 * v / total:.0f}%" for n, v in bw.most_common(9)))
        print("  by kind:  " + ", ".join(f"{n} {100 * v / total:.0f}%" for n, v in bk.most_common()))
        if os.environ.get("DETAIL") and os.environ["DETAIL"] in name:   # each word: instructions, calls
            for n, v in bw.most_common(40):
                calls = sum(net[a] for a in starts if names[a] == n)
                print(f"    {n:14} {v:12,.0f} {calls:10,.0f} calls {v / max(calls, 1):8.1f} each")
    # How far up its part of each stack every task got.  The kernel clears the
    # stacks at boot and nothing clears a cell that is dropped, so the highest
    # cell of a task's part that is not zero is as far as it reached (or one
    # short, if the last thing it put there was a zero).
    cells = struct.unpack("<512Q", stacks)
    print("\nThe deepest each task went, of the cells it has (data stack, return stack):")
    for task, lo, hi in TASKS:
        deep = [max((i - lo + 1 for i in range(lo, hi) if cells[base + i]), default=0) for base in (0, 256)]
        print(f"  {task}: {deep[0]} and {deep[1]} of {hi - lo}")
    # For util/survey.py: how often each instruction ran, as work and as a turn of the idle loop.
    with open(os.path.join(os.path.dirname(image), "desktop.counts"), "wb") as f:
        f.write(struct.pack(f"<{CODE}d", *work))
        f.write(struct.pack(f"<{CODE}d", *per_turn))


if __name__ == "__main__":
    sys.exit(main())
