#!/bin/sh
# How often each rule of the nucleus's peephole is used while the desktop
# is worked.
#
#   test/hits.sh
#
# join.fs keeps a count for each of its rules.  This builds the first
# machine of util/machines, runs the desktop's test on the reference VM
# (test/desktop_test.py: its windows worked with keys and a pointer, and
# what is typed in the Forth window), has the VM write its memory when it
# stops, and reads the counts out of that (test/hits.c).  What the test
# itself types at the prompt to drive the desktop is compiled by the
# nucleus too, and is counted with the rest.
#
# Everything is kept in build/hits/; the exit status is 0 only if the
# desktop's test passed and the counts were read.
cd "$(dirname "$0")/.."
OUT=build/hits
mkdir -p $OUT/desktop
PICTURE=baremetal/build/mandrill.tiff       # the Mandrill, which baremetal/Makefile fetches,
MKDISK=baremetal/build/mkdisk               # and what makes a disk, which it builds

stop() { # what failed, and the log that says why
    echo "FAIL $1"
    tail -5 $2
    exit 1
}

make -C baremetal build/mandrill.tiff build/mkdisk > $OUT/picture.log 2>&1 || stop "the picture and mkdisk" $OUT/picture.log
set -- $(grep -v '^#' util/machines | head -1)
C=$1 A=$2 W=$3 H=$4 BASE=$5
EXTRA="-DSTRICT" util/build-machine.sh $C $A $W $H $BASE > $OUT/build.log 2>&1 || stop "the build" $OUT/build.log
IMAGE=build/nuc-$C-$A.hex
TABLES=
[ -f build/nuc-$C-$A.tables ] && TABLES=build/nuc-$C-$A.tables

# The test starts the VM with an image and a disk.  This is the VM told,
# besides, where to write its memory when it stops.
rm -f $OUT/memory.out
printf '#!/bin/sh\nexec %s "$1" "$2" %s\n' "$PWD/build/hexaforth-$C-$A" "$PWD/$OUT/memory.out" > $OUT/vm
chmod +x $OUT/vm

$MKDISK $PICTURE $OUT/disk.img baremetal/block1.fs $TABLES > $OUT/disk.log 2>&1 || stop "the disk" $OUT/disk.log
cp $PICTURE $OUT/disk.tiff                  # where the test looks for the picture that is on the disk
python3 test/desktop_test.py $OUT/vm $IMAGE $OUT/desktop $OUT/disk.img $C $W $H $BASE $TABLES > $OUT/desktop.log 2>&1
passed=$?
git checkout util/__pycache__ baremetal/__pycache__ test/__pycache__ 2>/dev/null
[ $passed = 0 ] || stop "the desktop's test" $OUT/desktop.log
echo "the desktop, cells of $C bits: $(tail -1 $OUT/desktop.log)"
[ -f $OUT/memory.out ] || stop "the VM wrote no memory" $OUT/desktop.log

gcc -O2 -w -fcommon $(cat build/machine-$C-$A.flags) -o $OUT/hits test/hits.c vm_opcodes.c vm_debug.c -lm || exit 1
$OUT/hits $OUT/memory.out forth/joins.fs > $OUT/hits.txt || stop "the counts" $OUT/hits.txt
cat $OUT/hits.txt
