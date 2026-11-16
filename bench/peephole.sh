#!/bin/sh
# What the nucleus's compiler is worth (forth/join.fs): what it makes of
# the system's code beside what cross.fs makes of it, and what each of its
# rules saves, in the code and when the code is run.
#
#   bench/peephole.sh
#
# The nucleus compiles what is typed and what is read from blocks, and
# cross.fs compiles the image; the system's own source is the code there is
# to try the nucleus's compiler on.  For the first machine of
# util/machines:
#
#   the image is built with the peephole and without, and the two are cut
#     into the same straight-line runs (runs.c): a run as it is typed, and
#     what cross.fs made of it
#   each run as typed is given to the nucleus's emit1 at the prompt of the
#     reference VM, an instruction at a time, and the nucleus says what it
#     made of it and which of its rules it used (typed.c)
#   the work is done on a VM that counts how often the instruction at each
#     place is run (counting.c): the desktop's test (test/desktop_test.c,
#     built with that VM in it), and the drawing benchmark (draw.fs)
#   and the runs are weighed by that (weigh.c), which says at its top what
#     it prints
#
# Everything is kept in build/peephole/, and what weigh prints is
# build/peephole/peephole.txt.  The exit status is 0 only if every step
# was done and the desktop's test passed.
cd "$(dirname "$0")/.."
OUT=build/peephole
mkdir -p $OUT/desktop
PICTURE=baremetal/build/mandrill.tiff       # the Mandrill, which baremetal/Makefile fetches,
MKDISK=baremetal/build/mkdisk               # and what makes a disk, which it builds

stop() { # what failed, and the log that says why
    echo "FAIL $1"
    tail -5 $2
    exit 1
}

make -C baremetal build/mandrill.tiff build/mkdisk > $OUT/picture.log 2>&1 || stop "the picture and mkdisk" $OUT/picture.log
# The machine, and its image with the peephole and without.
sh util/both-builds.sh $OUT || exit 1
set -- $(cat $OUT/machine)
C=$1 A=$2 W=$3 H=$4 BASE=$5
FLAGS=$(cat build/machine-$C-$A.flags)
VM=build/hexaforth-$C-$A

MACHINE="vm_opcodes.c vm_debug.c util/stack.c"
gcc -O2 -w -fcommon $FLAGS -o $OUT/counting bench/counting.c $MACHINE -lm || exit 1
gcc -O2 -w -fcommon $FLAGS -o $OUT/runs bench/runs.c $MACHINE util/image.c -lm || exit 1
gcc -O2 -w -fcommon $FLAGS -o $OUT/typed bench/typed.c vm_opcodes.c vm_debug.c util/image.c -lm || exit 1
gcc -O2 -w -o $OUT/weigh bench/weigh.c || exit 1
# The desktop's test, with the VM that counts in it in place of vm.c.
gcc -O2 -w -fcommon $FLAGS -DCOUNTING_A_TEST -o $OUT/desktop-test test/desktop_test.c test/deskcheck.c \
    test/tables.c test/dither.c bench/tiff.c test/nucleus.c bench/counting.c vm_opcodes.c vm_debug.c \
    util/stack.c util/image.c -lm || exit 1

# The runs, and what the nucleus makes of them.
$OUT/runs $OUT/plain.hex $OUT/plain.tables $OUT/image.hex $OUT/image.tables > $OUT/runs.txt 2> $OUT/runs.log || stop "the runs" $OUT/runs.log
$OUT/typed type $OUT/runs.txt $OUT/image.hex $OUT/image.tables > $OUT/typed.fs 2> $OUT/typed.log || stop "what is typed" $OUT/typed.log
$VM $OUT/image.hex < $OUT/typed.fs > $OUT/typed.out 2>&1
$OUT/typed read $OUT/runs.txt $OUT/typed.out > $OUT/typed.txt 2> $OUT/typed.log || stop "what the nucleus made" $OUT/typed.log

# The work, counted.  The disk is made as the desktop's test wants it.
$MKDISK $PICTURE $OUT/disk.img baremetal/block1.fs $OUT/image.tables > $OUT/disk.log 2>&1 || stop "the disk" $OUT/disk.log
printf '8000 execute\n' | RUN_COUNTS=$OUT/idle.counts $OUT/counting $OUT/image.hex $OUT/disk.img > $OUT/idle.log 2>&1
RUN_COUNTS=$OUT/draw.counts $OUT/counting $OUT/image.hex $OUT/disk.img < bench/draw.fs > $OUT/draw.log 2>&1
rm -f $OUT/desktop.counts
RUN_COUNTS=$OUT/desktop.counts $OUT/desktop-test $OUT/image.hex $OUT/disk.img $PICTURE forth $OUT/desktop names \
    > $OUT/desktop.log 2>&1 || stop "the desktop's test" $OUT/desktop.log
for piece in idle draw desktop; do
    [ -f $OUT/$piece.counts ] || stop "nothing was counted for $piece" $OUT/$piece.log
done

$OUT/weigh $OUT/typed.txt forth/joins.fs $OUT/image.sizes $OUT desktop draw > $OUT/peephole.txt 2> $OUT/weigh.log || stop "the weighing" $OUT/weigh.log
cat $OUT/peephole.txt
