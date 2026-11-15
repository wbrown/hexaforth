#!/usr/bin/env python3
"""What each word costs, with all that it calls.

    paths.py IMAGE.hex COUNTS [REPORT]

COUNTS is how often each instruction ran (build/desktop.counts, from
`make -C baremetal demo-profile`).  That says what a word did itself, but
not whose work it was: a fill is a dozen words, none of which looks like
much, and what it costs is the sum of them.  Here each word is charged with
the work of what it calls: a call that ran c times is charged c times what
the word called costs each time it is entered, with that word's own calls
charged to it in the same way.  So a word's figure is what would be saved
if it were never called, and a way of doing something is judged by the
figure of the word that is asked to do it.

Read this before saying what a change will save.  How often a thing is
done, times what one of them is thought to cost, leaves out the loop that
does them and whatever is rare and dear: both are in these figures.

For the words that cost most, the report says where the cost is: what the
word did itself, and what each word it calls was charged to it.

What a call costs is taken to be the same whoever makes it: what the word
called costs in all, over how often it is entered.  A word that does more
for one caller than another is charged to them evenly.  And a word reached
by `execute` of something that is not a number in the code (a vector, a
window's draw routine, a task) has a caller that cannot be seen: what it
costs then is charged to no one, and such words are listed at the end.

Cycles are those of rtl/hexaforth.v, as loops.py has them.  The report is
build/paths.txt.
"""
import bisect
import os
import struct
import sys
from collections import Counter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import insn_map as im                               # noqa: E402
import loops                                        # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, f"{ROOT}/baremetal")
import deskprof                                     # noqa: E402


def pushed(code, at):
    """The number the instructions before `at` push, if they are a push of
    one: the execution token of a call to another page."""
    k, value = at - 1, 0
    while k >= 0 and code[k] & 0x8000 and code[k] >> 12 & 3 != 3:
        value += (code[k] & 0xFFF) << 12 * (code[k] >> 12 & 3)
        if not code[k] & 0x4000:
            return value
        k -= 1
    return None


def main():
    image, counts_path = sys.argv[1], sys.argv[2]
    report = sys.argv[3] if len(sys.argv) > 3 else f"{ROOT}/build/paths.txt"
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
        return mods[max((k for k, s in enumerate(mod_at) if s <= a), default=0)][1]

    def start_of(a):
        return starts[bisect.bisect_right(starts, a) - 1] if a >= starts[0] else None

    # What each word did itself, in instructions and in cycles; the calls it
    # makes, with how often each ran; and whether it goes round to its own
    # start, which is then no measure of how often it is entered.
    own = {s: [0.0, 0.0] for s in starts}
    calls = {s: Counter() for s in starts}
    round_to_start = set()
    unseen = 0.0                                    # calls by execute of what is not a number
    for a, w in enumerate(code):
        s = start_of(a)
        if s is None or not w:
            continue
        own[s][0] += work[a]
        own[s][1] += work[a] * loops.cycles(w)
        if w & 0x8000 or w >> 13 == 3:
            continue
        kind, to = w >> 13, im.flat_target(a, w)
        if kind != 1 and to == im.EXECUTE:
            to = pushed(code, a)
            if to is None:
                unseen += work[a]
                continue
        if to == s:
            round_to_start.add(s)
        elif kind != 1 and to in own:               # a call, or a jump to another word, which is one
            calls[s][to] += work[a]

    called = Counter()
    for s in starts:
        for to, c in calls[s].items():
            called[to] += c
    entered = {s: called[s] if s in round_to_start else max(called[s], work[s]) for s in starts}

    memo, busy = {}, set()

    def whole(s):
        """(instructions, cycles) of a word and all it calls."""
        if s in memo:
            return memo[s]
        if s in busy:                               # a word that comes round to itself: its own, once
            return tuple(own[s])
        busy.add(s)
        total = list(own[s])
        for to, c in calls[s].items():
            if entered[to]:
                t = whole(to)
                total[0] += c * t[0] / entered[to]
                total[1] += c * t[1] / entered[to]
        busy.discard(s)
        memo[s] = tuple(total)
        return memo[s]

    all_work = sum(work)
    all_cycles = sum(c * loops.cycles(w) for c, w in zip(work, code))
    rows = sorted(((whole(s), s) for s in starts if own[s][0] or calls[s]), reverse=True)

    def line(s):
        t = whole(s)
        each = f"{t[0] / entered[s]:10,.1f}" if entered[s] else f"{'':10}"
        return (f"{names[s]:16} {file_of(s):16} {t[0]:13,.0f} {100 * t[0] / all_work:5.1f}% "
                f"{t[1]:13,.0f} {own[s][0]:12,.0f} {entered[s]:11,.0f} {each}")

    head = (f"{'word':16} {'file':16} {'instructions':>13} {'':6} {'cycles':>13} {'its own':>12} "
            f"{'entered':>11} {'each time':>10}")
    out = [f"{image}: {all_work:,.0f} instructions of work, {all_cycles:,.0f} cycles.",
           "Each word with all it calls: what would be saved if it were never called.",
           "", head]
    out += [line(s) for _, s in rows]
    summary = len(out)

    out += ["", "Where it goes, for the words that cost most: what the word did itself, and what",
            "each word it calls was charged to it.", ""]
    for t, s in rows[:60]:
        parts = [(own[s][0], "itself")]
        for to, c in calls[s].items():
            if entered[to]:
                parts.append((c * whole(to)[0] / entered[to], f"{names[to]} ({c:,.0f} times)"))
        parts.sort(reverse=True)
        out.append(f"{names[s]}  {t[0]:,.0f}")
        out += [f"    {v:13,.0f} {100 * v / t[0]:5.1f}%  {n}" for v, n in parts if v >= t[0] / 200]

    out += ["", "Entered more often than any call that can be seen: by execute of a vector, or as a",
            "task, or from the prompt.  What they cost then is charged to no one above them.", ""]
    hidden = []
    for s in starts:
        if s not in round_to_start and entered[s] > called[s] and entered[s]:
            share = whole(s)[0] * (entered[s] - called[s]) / entered[s]
            if share >= all_work / 2000:
                hidden.append((share, s))
    for share, s in sorted(hidden, reverse=True):
        out.append(f"    {names[s]:16} {share:13,.0f} {100 * share / all_work:5.1f}%  "
                   f"{entered[s] - called[s]:,.0f} of {entered[s]:,.0f} times")
    out.append(f"\nCalls by execute of what is not a number in the code: {unseen:,.0f}")

    with open(report, "w") as f:
        f.write("\n".join(out) + "\n")
    print("\n".join(out[:min(summary, 34)]))
    print(f"\nall of it, where each one's cost goes, and what is reached by execute: {report}")


if __name__ == "__main__":
    sys.exit(main())
