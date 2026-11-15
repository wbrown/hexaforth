#!/bin/sh
# The system and the desktop, on every size of machine.
#
#   test/sizes.sh [SEED ...]
#
# Builds the machine with cells of 64 bits, of 32 and of 16, each with a
# display and each stopping at a fetch or store outside memory (STRICT),
# and on each runs the tests that type at the prompt of the reference VM:
# the nucleus; the desktop, its windows worked with keys and a pointer and
# its screen looked at; the drawing words against a model of them; regions
# a cell at a time against the one form of their pixels; the dither
# against a dither a pixel at a time; and the TIFF reader.  The same
# sources and the same tests: what is different is the cell.
#
# The machine of 16 bits has 128KB, so its display is 512 by 342, and the
# desktop lays its windows out for that.
#
# Everything a test prints is kept in build/sizes/; one line a test is
# printed, and the exit status is 0 only if all of them passed.
cd "$(dirname "$0")/.."
OUT=build/sizes
mkdir -p $OUT
SEEDS=${*:-3 4}
fail=0
PICTURE=baremetal/build/mandrill.tiff       # the Mandrill, which baremetal/Makefile fetches,
MKDISK=baremetal/build/mkdisk               # and what makes a disk, which it builds
make -C baremetal build/mandrill.tiff build/mkdisk > $OUT/picture.log 2>&1

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
    mv build/dither-test-$C-$A $OUT/dither-test-$C
    mv build/nuc-$C-$A.hex $OUT/nuc-$C.hex
    rm -f $OUT/nuc-$C.tables                # the names that go with the image, if it has any
    [ -f build/nuc-$C-$A.tables ] && mv build/nuc-$C-$A.tables $OUT/nuc-$C.tables
    run prompt-$C python3 test/prompt_test.py $OUT/vm-$C $OUT/nuc-$C.hex $C
    # The desktop, with a disk made as QEMU's is: block 1 for the editor, the
    # Mandrill, and the tables of names that go with this image, if it has any.
    T=
    [ -f $OUT/nuc-$C.tables ] && T=$OUT/nuc-$C.tables
    if $MKDISK $PICTURE $OUT/disk-$C.img baremetal/block1.fs $T > $OUT/disk-$C.log 2>&1; then
        cp $PICTURE $OUT/disk-$C.tiff       # where the test looks for the picture that is on the disk
        run desktop-$C python3 test/desktop_test.py $OUT/vm-$C $OUT/nuc-$C.hex $OUT/desktop-$C \
            $OUT/disk-$C.img $C $W $H $BASE $T
    else
        printf 'FAIL %-18s %s\n' "disk-$C" "$(tail -1 $OUT/disk-$C.log | cut -c1-110)"
        fail=1
    fi
    for s in $SEEDS; do
        run display-$C-$s python3 test/display_test.py $OUT/vm-$C $OUT/nuc-$C.hex $s 150 $C $W $H
        run regions-$C-$s python3 test/region_test.py $OUT/vm-$C $OUT/nuc-$C.hex $s 400 $C
        run dither-$C-$s $OUT/dither-test-$C $OUT/nuc-$C.hex $s
        run tiff-$C-$s python3 test/tiff_test.py $OUT/vm-$C $OUT/nuc-$C.hex $s $C
    done
done
git checkout util/__pycache__ baremetal/__pycache__ test/__pycache__ 2>/dev/null
exit $fail
