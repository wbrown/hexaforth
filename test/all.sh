#!/bin/sh
# Every suite, one after another, and a line for each.
#
#   test/all.sh
#
# What CLAUDE.md lists under Testing, as it is run after a change to
# cross.fs, nuc.fs or the machine:
#
#   the instruction tests, each compiled by the C compiler and by cross.fs
#     and run on the VM (CMake's target `tests`)
#   the system on every size of machine (test/sizes.sh)
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

instructions() {
    cmake -S . -B build/cmake && cmake --build build/cmake --target tests
}

run instructions instructions
run sizes sh test/sizes.sh
run kernel-small make -C baremetal test
run kernel-fast make -C baremetal test KERNEL=hexaforth-fast
run desktop-small-strict make -C baremetal demo-test STRICT=1
run desktop-fast make -C baremetal demo-test KERNEL=hexaforth-fast
for w in 64 32 16; do
    run rtl-$w make -C rtl test W=$w LAT=2
done
run rtl-krom make -C rtl test W=64 LAT=2 KROM=1
exit $fail
