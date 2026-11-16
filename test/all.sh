#!/bin/sh
# Every suite, one after another, and a line for each.
#
#   test/all.sh
#
# What CLAUDE.md lists under Testing, as it is run after a change to
# cross.fs, nuc.fs or the machine:
#
#   what CMake builds and runs (its target `check`): the instruction tests,
#     the image built with the peephole against the image built without,
#     and the system on every size of machine
#   each AArch64 kernel against vm.c, and the desktop on QEMU with each
#   the Verilog against vm.c, with cells of 64, 32 and 16 bits, and with
#     the kernel in a ROM
#
# Everything a suite prints is kept in build/all/, a file a suite; the last
# line of each is printed here, and the exit status is 0 only if all of
# them passed.
cd "$(dirname "$0")/.."
OUT=build/all
mkdir -p $OUT
fail=0

run() { # name, command...
    name=$1; shift
    if "$@" > $OUT/$name.log 2>&1; then r=ok; else r=FAIL; fail=1; fi
    printf '%-4s %-22s %s\n' "$r" "$name" "$(grep -v '^make' $OUT/$name.log | tail -1 | cut -c1-110)"
}

check() {
    cmake -S . -B build/cmake && cmake --build build/cmake --target check
}

run check check
run kernel-small make -C baremetal test
run kernel-fast make -C baremetal test KERNEL=hexaforth-fast
run desktop-small-strict make -C baremetal demo-test STRICT=1
run desktop-fast make -C baremetal demo-test KERNEL=hexaforth-fast
for w in 64 32 16; do
    run rtl-$w make -C rtl test W=$w LAT=2
done
run rtl-krom make -C rtl test W=64 LAT=2 KROM=1
exit $fail
