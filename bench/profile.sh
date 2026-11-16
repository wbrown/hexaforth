#!/bin/sh
# Where the desktop's instructions go.
#
#   bench/profile.sh
#   DETAIL=scroll bench/profile.sh      each word of an activity whose name has that in it
#
# The first machine of util/machines is built, its desktop is run on the
# reference VM that counts how often the instruction at each place is run
# (counting.c), and worked through a few things a person would do
# (profile.c, which says at its top what it prints).
#
# Everything is kept in build/profile/: what profile prints is
# build/profile/profile.txt, and build/profile/desktop.counts is the work
# by place.
cd "$(dirname "$0")/.."
OUT=build/profile
mkdir -p $OUT
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
IMAGE=build/nuc-$C-$A
[ -f $IMAGE.tables ] || stop "the build made no names" $OUT/build.log

# The desktop's machine, with the VM that counts in it in place of vm.c.
gcc -O2 -w -fcommon $(cat build/machine-$C-$A.flags) -DCOUNTING_A_TEST -o $OUT/profile bench/profile.c \
    test/deskvm.c test/deskcheck.c test/tables.c test/dither.c bench/tiff.c test/nucleus.c bench/counting.c \
    vm_opcodes.c vm_debug.c util/stack.c util/image.c -lm || exit 1
# (The machine has a copy of the disk, which it keeps as disk.img.)
$MKDISK $PICTURE $OUT/made.img baremetal/block1.fs $IMAGE.tables > $OUT/disk.log 2>&1 || stop "the disk" $OUT/disk.log
$OUT/profile $IMAGE.hex $IMAGE.tables $IMAGE.sizes $OUT/made.img $PICTURE forth $OUT > $OUT/profile.txt 2> $OUT/profile.log \
    || stop "the profile" $OUT/profile.log
cat $OUT/profile.txt
