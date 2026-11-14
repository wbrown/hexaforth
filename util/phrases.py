#!/usr/bin/env python3
"""What to write to get each instruction: a phrase book.

    phrases.py [REPORT]

An instruction is a control word, and there are 7,712 that do different
things.  Few have a name, and the rest are reached by writing words the
peephole joins: `over +` is one instruction, and so is `over @ swap !`.
This finds, for every instruction that some run of up to three of the
machine's words equals (LONGEST=4 in the environment for four, which
takes much longer), the shortest such run: the phrase to write.

It then asks two things of each phrase.

Does cross.fs make one instruction of it?  Every phrase is compiled, and
those that come out longer are places the peephole does not reach.

Is the name standard?  The words of vm_opcodes.h that gforth also has are
run in gforth and in the model on the same stacks, to see that a name
means here what it means there; and a list of words that ANS Forth, gforth
and the older Forths have, and this machine has no name for, is run in
gforth to see which are a single instruction here.

The report is build/phrases.txt.
"""
import os
import random
import re
import struct
import subprocess
import sys
import time
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import insn_map as im                               # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, f"{ROOT}/baremetal")
MASK = (1 << 64) - 1
LONGEST = int(os.environ.get("LONGEST", 3))

# Words other Forths have that work on the data stack alone.  Each is run in
# gforth if gforth has it; the definitions are for those it has not, and
# say what the word does in the Forth it comes from.
OTHERS = {
    # ANS and gforth
    "third": None, "fourth": None, "under+": None, "2nip": None, "2tuck": None, "2rot": None,
    "0<=": None, "0>=": None, "<=": None, ">=": None, "u<=": None, "u>=": None,
    "umin": None, "umax": None, "min": None, "max": None, "abs": None, "within": None,
    "cell+": None, "cell-": None, "cells": None, "char+": None, "chars": None,
    "d>s": None, "s>d": None, "2*": None, "2/": None, "1+": None, "1-": None,
    "true": None, "false": None, "bounds": None, "-rot": None, "nip": None, "tuck": None,
    "2dup": None, "2drop": None, "2swap": None, "2over": None, "rot": None,
    "negate": None, "invert": None, "0=": None, "0<": None, "0>": None, "0<>": None,
    "=": None, "<>": None, "<": None, ">": None, "u<": None, "u>": None,
    "+": None, "-": None, "*": None, "and": None, "or": None, "xor": None,
    "lshift": None, "rshift": None, "dup": None, "drop": None, "swap": None, "over": None,
    # fig-Forth, F83 and others, by what they are in words gforth has
    "2+": "2 +", "2-": "2 -", "not": "0=", "minus": "negate", "-dup": None,
    "swap-": "swap -", "over-": "over -", "tuck+": "tuck +", "under": "swap over",
    "nand": "and invert", "nor": "or invert", "andn": "invert and", "orn": "invert or",
    "xnor": "xor invert", "3dup": "third third third", "2dup=": "2dup =", "2dup+": "2dup +",
    "dup+": "dup +", "over+": "over +", "overand": "over and", "over=": "over =",
    "swap<": "swap <", "bic": "invert and",
}


def encodings():
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
                        ins = im.encode(i, o, m, d, r, 0)
                        classes[im.behaviour([ins])].append(ins)
    classes.pop(None, None)
    return words, names, classes


def phrases(vocab, classes):
    """Shortest run of words for each behaviour a run of up to LONGEST gives."""
    seen = {hash(im.behaviour([])): True}
    found = {}
    frontier = [((), [])]
    for depth in range(1, LONGEST + 1):
        nxt = []
        for phrase, code in frontier:
            for w, c in vocab.items():
                seq = code + c
                key = im.behaviour(seq)
                if key is None:
                    continue
                if depth == LONGEST:                # nothing is built on the longest: keep none
                    if key in classes and key not in found:
                        found[key] = phrase + (w,)
                    continue
                h = hash(key)
                if h in seen:
                    continue
                seen[h] = True
                nxt.append((phrase + (w,), seq))
                if key in classes and key not in found:
                    found[key] = phrase + (w,)
        frontier = nxt
        print(f"  {depth} words: {len(found):5} instructions have a phrase", flush=True)
    return found


def value(e, stack):
    """What a model's expression is, for a stack of numbers (top last)."""
    if isinstance(e, int):
        return e & MASK
    if e[0] == "d":
        return stack[-1 - e[1]] & MASK
    if e[0] == "sum":
        return (e[1] + sum(value(t, stack) for t in e[2])) & MASK
    if e[0] == "~":
        return ~value(e[1], stack) & MASK
    a, b = value(e[1], stack), value(e[2], stack)
    signed = lambda x: x - (1 << 64) if x >> 63 else x
    flag = lambda t: MASK if t else 0
    return {"IN&N": a & b, "IN|N": a | b, "IN^N": a ^ b, "IN*N": a * b & MASK,
            "IN==N": flag(a == b), "N<IN": flag(signed(a) < signed(b)), "Nu<IN": flag(a < b),
            "IN>>T": a >> (b & 63), "IN<<T": a << (b & 63) & MASK}[e[0]]


# Stacks to try a word on: few different numbers, so that two of a stack are
# often the same, and nothing and less than nothing are often on top.
_rng = random.Random(1984)
STACKS = [[_rng.choice([0, 1, -1, 2, 3, -7, 5, 62, 1 << 40, -(1 << 62)]) for _ in range(6)]
          for _ in range(24)]
STACKS += [s[:4] + [x, x] for s in STACKS[:6] for x in (s[0], s[1])]       # the top two the same
STACKS += [s[:3] + [x, s[4], x] for s in STACKS[:6] for x in (s[0],)]      # the top and the third


def in_model(code):
    """What a sequence leaves of each stack of STACKS, or None if it cannot be said."""
    key = im.behaviour(code)
    if key is None or key[2] != "mem0" or key[3] or key[4] or key[5] is not None:
        return None
    if key[1] != tuple(("r", i) for i in reversed(range(im.State.DEPTH))):
        return None
    try:
        return [[value(e, s) for e in key[0]] for s in STACKS]
    except (KeyError, IndexError, TypeError):
        return None


def in_gforth(text):
    """What gforth leaves of each stack of STACKS, or None if it has not the word."""
    script = " ".join(f"{' '.join(map(str, s))} {text} .s clearstack cr" for s in STACKS) + " bye"
    r = subprocess.run(["gforth", "-e", script], capture_output=True, text=True)
    if r.returncode or "Undefined word" in r.stderr or r.stderr.strip():
        return None
    out = []
    for line in r.stdout.strip().splitlines():
        m = re.match(r"<(\d+)>(.*)", line.strip())
        if not m:
            return None
        out.append([int(x) & MASK for x in m.group(2).split()])
    return out if len(out) == len(STACKS) else None


def compiled_sizes(found):
    """Each phrase as a definition, compiled by cross.fs: instructions it came to."""
    sizes = {}
    items = list(found.items())
    room = 16                                       # instructions kept for each phrase
    for start in range(0, len(items), 200):
        chunk = items[start:start + 200]
        src = ["meta 4 org target", "header execute : execute >r ;"]
        for n, (_, phrase) in enumerate(chunk):
            src.append(f"meta ${0x2000 + 2 * room * n:x} org target : p{n} {' '.join(phrase)} ;")
        src += ["meta $1000 org target", "header main : main halt ;", "header quit : quit halt ;"]
        open(f"{ROOT}/forth/phrasetest.fs", "w").write("\n".join(src) + "\n")
        r = subprocess.run(["gforth", "./cross.fs", "./basewords.fs", "./phrasetest.fs"],
                           cwd=f"{ROOT}/forth", capture_output=True, text=True)
        if "tdp" not in r.stdout:
            print("    a batch of phrases did not build:", (r.stdout + r.stderr).strip()[-200:])
            continue
        raw = b"".join(struct.pack("<I", int(l, 16)) for l in open(f"{ROOT}/build/phrasetest.hex")
                       if l.strip())
        code = struct.unpack(f"<{len(raw) // 2}H", raw)
        for n, (key, _) in enumerate(chunk):
            a = 0x1000 + room * n
            words = [w for w in code[a:a + room] if w]
            if words and words[-1] == 0x7403:       # an exit that did not fold is not the phrase's
                words = words[:-1]
            sizes[key] = len(words)
    os.remove(f"{ROOT}/forth/phrasetest.fs")
    return sizes


def main():
    report = sys.argv[1] if len(sys.argv) > 1 else f"{ROOT}/build/phrases.txt"
    started = time.time()
    words, names, classes = encodings()
    rep = {k: ([i for i in m if i in names] or [min(m)])[0] for k, m in classes.items()}
    vocab = {n: c for n, c in words.items()
             if c and im.modelled(c) and n not in ("noop", "halt", "exit", "r@;")}
    out = [f"{len(classes)} instructions that do different things, not counting those that also "
           f"return; {sum(1 for k in classes if rep[k] in names)} have a name."]

    print(f"phrases of up to {LONGEST} of {len(vocab)} words:")
    found = phrases(vocab, classes)
    out.append(f"{len(found)} are what some run of up to {LONGEST} of the machine's {len(vocab)} "
               f"words does; {len(classes) - len(found)} are not.")

    print("compiling each phrase:")
    sizes = compiled_sizes(found)
    longer = {k: n for k, n in sizes.items() if n > 1}
    out.append(f"cross.fs makes one instruction of {len(sizes) - len(longer)} of the {len(sizes)} "
               f"phrases it was given, and more than one of {len(longer)}.")

    print("names, against gforth:")
    agree, differ, absent = [], [], []
    for n, c in vocab.items():
        ours = in_model(c)
        if ours is None:
            continue
        theirs = in_gforth(n)
        (absent if theirs is None else agree if theirs == ours else differ).append(n)
    out.append(f"of the machine's words that only use the data stack, gforth has {len(agree) + len(differ)}"
               f" and {len(agree)} mean the same there; {len(absent)} it has not: {' '.join(absent)}")
    if differ:
        out.append(f"NOT THE SAME IN GFORTH: {' '.join(differ)}")

    by_result = defaultdict(list)                   # what an instruction leaves -> the instruction
    for key in classes:
        if rep[key] in names:
            continue
        r = in_model([rep[key]])
        if r is not None:
            by_result[repr(r)].append(key)
    known = []
    for w, definition in OTHERS.items():
        if w in words:
            continue
        theirs = in_gforth(w) or (in_gforth(definition) if definition else None)
        if theirs is None:
            continue
        for key in by_result.get(repr(theirs), []):
            known.append((w, key))
    out.append("")
    out.append("Words other Forths have, that are one instruction here and have no name here:")
    for w, key in known:
        phrase = " ".join(found[key]) if key in found else "(no phrase)"
        out.append(f"    {w:8} {rep[key]:04x}  {im.describe(rep[key]):34} = {phrase}")

    out.append("")
    out.append("Phrases cross.fs does not make one instruction of:")
    for key, n in sorted(longer.items(), key=lambda kv: (len(found[kv[0]]), kv[1])):
        out.append(f"    {rep[key]:04x}  {im.describe(rep[key]):34} = {' '.join(found[key]):28} {n} instructions")

    out.append("")
    out.append("Every instruction a phrase gives, and the phrase:")
    for key, phrase in sorted(found.items(), key=lambda kv: rep[kv[0]]):
        ins = rep[key]
        out.append(f"    {ins:04x}  {names.get(ins, ''):9} {im.describe(ins):34} = {' '.join(phrase)}")

    open(report, "w").write("\n".join(out) + "\n")
    first = out.index("Phrases cross.fs does not make one instruction of:")
    print("\n".join(out[:first + 26]))
    print(f"...\nall of it: {report}   ({time.time() - started:.0f}s)")


if __name__ == "__main__":
    sys.exit(main())
