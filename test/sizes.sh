#!/bin/sh
# What draws, on every size of machine.
#
#   test/sizes.sh [SEED ...]
#
# Builds the machine with cells of 64 bits, of 32 and of 16, each with a
# display and each stopping at a fetch or store outside memory (STRICT),
# and on each runs the tests that type at the prompt of the reference VM:
# the nucleus, the drawing words against a model of them, regions a cell
# at a time against the one form of their pixels, and the dither against a
# dither a pixel at a time.  The same sources and the same tests: what is
# different is the cell.
#
# The machine of 16 bits has 128KB, so its display is 512 by 342.
#
# Everything a test prints is kept in build/sizes/; one line a test is
# printed, and the exit status is 0 only if all of them passed.
cd "$(dirname "$0")/.."
OUT=build/sizes
mkdir -p $OUT
SEEDS=${*:-3 4}
fail=0

run() { # name, command...
    name=$1; shift
    if "$@" > $OUT/$name.log 2>&1; then r=ok; else r=FAIL; fail=1; fi
    printf '%-4s %-18s %s\n' "$r" "$name" "$(tail -1 $OUT/$name.log | cut -c1-110)"
}

for m in "64 22 768 576 0x4000" "32 17 768 576 0x8000" "16 16 512 342 0x8000"; do
    set -- $m
    C=$1 A=$2 W=$3 H=$4 BASE=$5
    if ! EXTRA="-DSTRICT -DDISP_W=$W -DDISP_H=$H -DDISP_BASE=$BASE" \
            util/build-machine.sh $C $A > $OUT/build-$C.log 2>&1; then
        printf 'FAIL %-18s %s\n' "build-$C" "$(tail -1 $OUT/build-$C.log | cut -c1-110)"
        fail=1
        continue
    fi
    mv build/hexaforth-$C-$A $OUT/vm-$C
    mv build/nuc-$C-$A.hex $OUT/nuc-$C.hex
    [ -f build/nuc-$C-$A.tables ] && mv build/nuc-$C-$A.tables $OUT/nuc-$C.tables
    run prompt-$C python3 test/prompt_test.py $OUT/vm-$C $OUT/nuc-$C.hex $C
    if [ $C != 16 ]; then
        run desktop-$C python3 test/desktop_test.py $OUT/vm-$C $OUT/nuc-$C.hex $OUT/desktop-$C \
            $C $W $H $BASE $OUT/nuc-$C.tables
    fi
    for s in $SEEDS; do
        run display-$C-$s python3 test/display_test.py $OUT/vm-$C $OUT/nuc-$C.hex $s 150 $C $W $H
        run regions-$C-$s python3 test/region_test.py $OUT/vm-$C $OUT/nuc-$C.hex $s 400 $C
        run dither-$C-$s python3 test/dither_test.py $OUT/vm-$C $OUT/nuc-$C.hex $s $C
        run tiff-$C-$s python3 test/tiff_test.py $OUT/vm-$C $OUT/nuc-$C.hex $s $C
    done
done
git checkout util/__pycache__ baremetal/__pycache__ test/__pycache__ 2>/dev/null
exit $fail
