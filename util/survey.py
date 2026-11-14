#!/usr/bin/env python3
"""Lay the instruction encoding out against what a program does with it.

    survey.py IMAGE.hex [COUNTS] [REPORT]

An ALU instruction is fields: where the ALU's input comes from, the
operation, where the result goes, how each stack moves, and whether it
returns.  Every setting of them is an instruction the machine executes.
This says, for all of them and with no opinion about which are useful:

  1. the space: for each source, operation and destination, how many
     distinct behaviours the stack movements give, how many of those have a
     name, how many the peephole can produce, how many the image uses, and
     how often they run
  2. every behaviour the image uses, by how often it runs
  3. what is not used and could be: every run of instructions in the image
     that one instruction, or two, would do the work of, by how many
     instructions that would save when the program runs

COUNTS is how often each instruction ran, as `make -C baremetal
demo-profile` writes it (build/desktop.counts): the desktop's work, and a
turn of its idle loop.  Without it the counts are of places in the image.
The whole report goes to REPORT (build/survey.txt); the top of each part is
printed.
"""
import hashlib
import os
import pickle
import re
import struct
import sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import insn_map as im                               # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LONGEST = 6                 # instructions in a run that is tried


def encodings():
    """Every ALU encoding by behaviour, and the names there are."""
    words = im.load_words(f"{ROOT}/vm_opcodes.h")
    names = {}
    for name, code in words.items():
        if len(code) == 1:
            names.setdefault(code[0], name)
    classes = defaultdict(list)
    for i in range(4):
        for o in range(16):
            for m in range(4):
                for d in im.DELTA_BITS:
                    for r in im.DELTA_BITS:
                        for ret in (0, 1):
                            ins = im.encode(i, o, m, d, r, ret)
                            classes[im.behaviour([ins])].append(ins)
    classes.pop(None, None)
    return classes, names


def peephole_reach():
    """Every instruction the peephole's tables can leave in the code."""
    out = set()
    table = None
    for line in open(f"{ROOT}/forth/peephole.fs"):
        m = re.match(r"create (\w+)", line)
        if m:
            table = m.group(1)
        m = re.match(r"\$([0-9a-f]+) , \$([0-9a-f]+) ,", line)
        if m:
            v = int(m.group(2), 16)
            out |= {v >> 16, v & 0xFFFF} if table == "fuse32" else {v}
    return out


def digest(key):
    """A behaviour, small enough to keep a million of."""
    return hashlib.blake2b(repr(key).encode(), digest_size=8).digest()


def pairs_table(classes):
    """Behaviour of every two instructions (its digest) -> the two."""
    firsts = [im.encode(i, o, m, d, r, 0) for i in range(4) for o in range(15) for m in (0, 1, 3)
              for d in im.DELTA_BITS for r in im.DELTA_BITS]
    two = {}
    for x in firsts:
        for y in firsts + [w | 0x1000 for w in firsts]:
            key = im.behaviour([x, y])
            if key is not None and key not in classes:
                two.setdefault(digest(key), x << 16 | y)
    return two


def unfold(ins):
    """An instruction with a return folded into it (cross.fs, fold-exit) is the
    instruction and then exit: give the instruction, and whether it returns."""
    if ins & 0x1000 and ins & 3 == 3 and ins != 0x7403:
        return ins & ~0x1003, True
    return ins, False


def main():
    image = sys.argv[1]
    counts_path = sys.argv[2] if len(sys.argv) > 2 else None
    report = sys.argv[3] if len(sys.argv) > 3 else f"{ROOT}/build/survey.txt"
    classes, names = encodings()
    rep = {}                                        # behaviour -> the encoding that stands for it
    of = {}                                         # encoding -> behaviour
    for key, members in classes.items():
        named = [i for i in members if i in names]
        rep[key] = named[0] if named else min(members)
        for ins in members:
            of[ins] = key
    reach = {of[i] for i in peephole_reach() if i in of}

    code, entries = im.load_image(image)
    n = len(code)
    if counts_path:
        raw = open(counts_path, "rb").read()
        half = len(raw) // 16
        work = list(struct.unpack(f"<{half}d", raw[:8 * half]))[:n]
        idle = list(struct.unpack(f"<{half}d", raw[8 * half:]))[:n]
        unit = "runs"
    else:
        work, idle, unit = [1.0] * n, [0.0] * n, "places"
    work += [0.0] * (n - len(work))
    idle += [0.0] * (n - len(idle))

    def show(ins):
        if ins & 0x8000:
            if ins >> 12 & 3 == 3:
                return "var!" if ins & 0x4000 else "var@"
            return "lit+" if ins & 0x4000 else "lit"
        base, returns = unfold(ins)
        key = of.get(base)
        name = names.get(rep[key]) if key in rep else None
        return (name or f"({im.describe(base)})") + (";" if returns else "")

    # ---- what the image uses
    places, runs, turn = defaultdict(int), defaultdict(float), defaultdict(float)
    total_runs = total_alu = folded = 0.0
    for a, w in enumerate(code):
        total_runs += work[a]
        if w >> 13 == 3 and not w & 0x8000:
            base, returns = unfold(w)
            key = of.get(base)
            if key is None:
                continue
            folded += work[a] * returns
            places[key] += 1
            runs[key] += work[a]
            turn[key] += idle[a]
            total_alu += work[a]

    # ---- what is not used and could be
    cache = f"{ROOT}/build/survey.pairs"            # a minute's work, and the same every time
    two = None
    if os.path.exists(cache) and os.path.getmtime(cache) > os.path.getmtime(im.__file__):
        try:
            two = pickle.load(open(cache, "rb"))
        except (EOFError, pickle.UnpicklingError):
            pass                                    # one that was not finished
    if two is None:
        two = pairs_table(classes)
        with open(cache + ".new", "wb") as f:
            pickle.dump(two, f)
        os.replace(cache + ".new", cache)
    targets = set(entries)
    for i, w in enumerate(code):
        if w and not w & 0x8000 and w >> 13 != 3:
            targets.add(im.flat_target(i, w))
    takes_return = {t for t in targets if t < n and code[t] == 0x6C07}
    pinned = {i + 1 for i, w in enumerate(code)
              if w >> 13 == 2 and im.flat_target(i, w) in takes_return}
    found = defaultdict(lambda: [0, 0.0, 0.0])      # (run, what does it) -> places, saved, idle saved
    for kind, start, run in im.split_runs(code, entries):
        if kind != "run":
            continue
        i = 0
        while i < len(run):
            best = None
            for length in range(min(LONGEST, len(run) - i), 1, -1):
                if any(start + i + j in pinned for j in range(length - 1)):
                    continue
                seq = run[i:i + length]
                key = im.behaviour(seq)
                if key is None:
                    continue
                if key in classes:
                    result = (rep[key],)
                elif length > 2 and digest(key) in two:
                    result = (two[digest(key)] >> 16, two[digest(key)] & 0xFFFF)
                elif im.fold_literal(seq):
                    result = (0xC000,)
                else:
                    continue
                if best is None or length - len(result) > best[0] - len(best[2]):
                    best = (length, seq, result)
            if best:
                length, seq, result = best
                label = (" ".join(show(w) for w in seq), " ".join(show(w) for w in result))
                gain = length - len(result)
                found[label][0] += 1
                found[label][1] += gain * work[start + i]
                found[label][2] += gain * idle[start + i]
                i += length
            else:
                i += 1

    out = []
    used = {k for k in places}
    named = {k for k, m in classes.items() if any(i in names for i in m)}
    out.append(f"{image}: {sum(1 for w in code if w)} instructions")
    out.append(f"behaviours {len(classes)}: named {len(named)}, the peephole can produce {len(reach)}, "
               f"the image uses {len(used)}, of which {len(used - named)} have no name")
    if counts_path:
        unnamed = sum(runs[k] for k in used - named)
        out.append(f"instructions run as work: {total_runs:,.0f}, of them ALU {total_alu:,.0f}, "
                   f"and by behaviours with no name {unnamed:,.0f} ({100 * unnamed / total_alu:.0f}% of ALU); "
                   f"{folded:,.0f} had a return folded in")
    saved = sum(v[1] for v in found.values())
    out.append(f"runs of instructions that fewer would do: {sum(v[0] for v in found.values())} places, "
               f"{saved:,.0f} {unit} saved" + (f" ({100 * saved / total_runs:.1f}% of the work), "
               f"{sum(v[2] for v in found.values()):.1f} a turn of the idle loop" if counts_path else ""))

    # ---- 1. the space
    out.append("")
    out.append("1. The space.  For each source and operation, and each destination: behaviours / named /")
    out.append(f"   peephole can produce / used in the image, and {unit}.")
    out.append("")
    out.append(f"{'':22}" + "".join(f"{m:>30}" for m in im.OUT_MUX))
    cell = defaultdict(lambda: [0, 0, 0, 0, 0.0])
    for key in classes:
        ins = rep[key]
        c = cell[(ins >> 10 & 3, ins >> 4 & 15, ins >> 8 & 3)]
        c[0] += 1
        c[1] += key in named
        c[2] += key in reach
        c[3] += key in used
        c[4] += runs[key]
    for i in range(4):
        for o in range(16):
            row = f"{im.IN_MUX[i]:8} {im.ALU_OPS[o]:12} "
            for m in range(4):
                c = cell[(i, o, m)]
                row += f"{c[0]:5}/{c[1]:2}/{c[2]:3}/{c[3]:3} {c[4]:13,.0f}" if c[0] else f"{'-':>30}"
            out.append(row)
        out.append("")

    # ---- 2. what the image uses
    out.append(f"2. Every behaviour the image uses, by {unit}.  * has no name.")
    out.append("")
    out.append(f"   {'places':>6} {unit:>14} {'idle':>7}  instruction")
    part2 = len(out)
    for key in sorted(used, key=lambda k: -runs[k]):
        ins = rep[key]
        mark = " " if key in named else "*"
        out.append(f" {mark} {places[key]:6} {runs[key]:14,.0f} {turn[key]:7.1f}  "
                   f"{names.get(ins, ''):9} {im.describe(ins)}")

    # ---- 3. what could be used
    out.append("")
    out.append(f"3. Runs of instructions that fewer would do, by {unit} saved.")
    out.append("")
    out.append(f"   {'places':>6} {'saved':>14} {'idle':>7}  instructions -> what does the same")
    part3 = len(out)
    for (seq, result), (count, gain, igain) in sorted(found.items(), key=lambda kv: -kv[1][1]):
        out.append(f"   {count:6} {gain:14,.0f} {igain:7.1f}  {seq}  ->  {result}")

    with open(report, "w") as f:
        f.write("\n".join(out) + "\n")
    for line in out[:5]:
        print(line)
    print("\n".join(out[part2 - 3:part2 + 30]))
    print("\n".join(out[part3 - 3:part3 + 40]))
    print(f"\nall of it: {report}")


if __name__ == "__main__":
    sys.exit(main())
