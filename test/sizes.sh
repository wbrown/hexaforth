#!/bin/sh
# The system and the desktop, on every size of machine.
#
#   test/sizes.sh [SEED ...]
#
# Builds the machine with cells of 64 bits, of 32 and of 16 (util/machines
# says what each is), each with a display and each stopping at a fetch or
# store outside memory (STRICT),
# and on each runs the tests that type at the prompt of the reference VM:
# the nucleus; comments, and a block of a disk read as source; the desktop,
# its windows worked with keys and a pointer and
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

while read C A W H BASE <&3; do
    case $C in (\#*) continue;; esac
    if ! EXTRA="-DSTRICT" util/build-machine.sh $C $A $W $H $BASE > $OUT/build-$C.log 2>&1; then
        printf 'FAIL %-18s %s\n' "build-$C" "$(tail -1 $OUT/build-$C.log | cut -c1-110)"
        fail=1
        continue
    fi
    mv build/hexaforth-$C-$A $OUT/vm-$C
    mv build/dither-test-$C-$A $OUT/dither-test-$C
    mv build/source-test-$C-$A $OUT/source-test-$C
    mv build/prompt-test-$C-$A $OUT/prompt-test-$C
    mv build/region-test-$C-$A $OUT/region-test-$C
    mv build/tiff-test-$C-$A $OUT/tiff-test-$C
    mv build/display-test-$C-$A $OUT/display-test-$C
    mv build/nuc-$C-$A.hex $OUT/nuc-$C.hex
    rm -f $OUT/nuc-$C.tables                # the names that go with the image, if it has any
    [ -f build/nuc-$C-$A.tables ] && mv build/nuc-$C-$A.tables $OUT/nuc-$C.tables
    run prompt-$C $OUT/prompt-test-$C $OUT/nuc-$C.hex
    # Comments, and a block read as source: the block is a file that is
    # there for this, laid down as a disk's source is.
    if $MKDISK $PICTURE $OUT/source-$C.img test/source.fs > $OUT/source-disk-$C.log 2>&1; then
        run source-$C $OUT/source-test-$C $OUT/nuc-$C.hex $OUT/source-$C.img
    else
        printf 'FAIL %-18s %s\n' "source-disk-$C" "$(tail -1 $OUT/source-disk-$C.log | cut -c1-110)"
        fail=1
    fi
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
        run display-$C-$s $OUT/display-test-$C $OUT/nuc-$C.hex forth $s
        run regions-$C-$s $OUT/region-test-$C $OUT/nuc-$C.hex $s
        run dither-$C-$s $OUT/dither-test-$C $OUT/nuc-$C.hex $s
        run tiff-$C-$s $OUT/tiff-test-$C $OUT/nuc-$C.hex $s
    done
done 3< util/machines
git checkout util/__pycache__ baremetal/__pycache__ test/__pycache__ 2>/dev/null
exit $fail
