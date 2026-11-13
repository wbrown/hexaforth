#!/usr/bin/env python3
"""Check the nucleus at its prompt, on the reference VM.

    prompt_test.py VM IMAGE.hex [CELL_BITS]

Types lines of Forth and checks what each prints.  These are things the
other tests do not reach: what the nucleus's own compiler produces, and the
tasks.
"""
import subprocess
import sys

BITS = int(sys.argv[3]) if len(sys.argv) > 3 else 64
VAR_BASE = 0x4000 // (BITS // 8)        # as vm_constants.h

CHECKS = [
    # (what is typed, what must follow it on the line)
    ("1 2 + .", "3  ok"),
    (": sq dup * ; 12 sq .", "144  ok"),
    (": f dup 1 > if dup 1- recurse * then ; 7 f .", "5040  ok"),
    (": g 0 swap begin dup while tuck + swap 1- repeat drop ; 10 g .", "55  ok"),
    (": h 0= if 7 else 9 then ; 0 h . 1 h .", "7 9  ok"),
    ("hex ff . decimal", "FF  ok"),
    # A number wider than 36 bits is made with a shift.
    *([(": big 81985529216486895 ; big hex . decimal", "123456789ABCDEF  ok"),
       (": neg -81985529216486895 ; neg big + .", "0  ok")] if BITS == 64 else []),
    # A variable is in the cells that one instruction fetches and stores,
    # and the compiler uses that instruction: 5 x ! x @ is three.
    ("variable x  variable y", " ok"),
    (": t 5 x ! x @ 1+ y ! y @ x @ + ; t .", "11  ok"),
    (f"x {VAR_BASE} - 4096 u< .", "-1  ok"),
    (": t2 5 x ! x @ ;", " ok"),
    ("' t2 1+ i@ 12 rshift .  ' t2 2 + i@ 12 rshift .  ' t2 3 + i@ hex . decimal",
     "15 11 7403  ok"),
    # Nothing is joined across the start of a loop: here the fetch is done
    # twice, of y and then of x, and only the first follows the address.
    (": u 5 x ! x y ! y begin @ dup 100 u< until ; u .", "5  ok"),
    # Tasks: one that counts, and a loop that keeps its index across pauses.
    ("create rec 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,", " ok"),
    ("create n 0 ,", " ok"),
    (":noname 1 n +! ; 128 128 rec spawn pause pause pause n @ .", "3  ok"),
    (": lp 3 0 do i . pause loop ; lp n @ .", "0 1 2 6  ok"),
]


def main():
    vm, image = sys.argv[1:3]
    typed = "".join(line + "\n" for line, _ in CHECKS) + "8000 execute\n"
    out = subprocess.run([vm, image], input=typed, capture_output=True, text=True, timeout=60).stdout
    failed = 0
    for line, want in CHECKS:
        at = out.find(line + " ")
        got = out[at + len(line):].split("\n")[0].strip() if at >= 0 else "(not echoed)"
        if got != want.strip():
            failed += 1
            print(f"FAIL {line!r}: {got!r}, expected {want.strip()!r}")
    print(f"{len(CHECKS) - failed}/{len(CHECKS)} prompt checks pass")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
