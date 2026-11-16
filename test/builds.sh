#!/bin/sh
# The image cross.fs makes with its peephole, against the image it makes
# without.
#
#   test/builds.sh
#
# After any change to a compiler or a peephole: every straight-line run of
# the image must leave what the same run leaves as it was typed, and every
# call, jump and branch go to the same place.  Both images are built
# (util/both-builds.sh) and compared by test/builds_test.c, which says how
# it does it.  Everything is kept in build/builds/; what the test prints is
# printed, and the exit status is its.
cd "$(dirname "$0")/.."
OUT=build/builds
sh util/both-builds.sh $OUT || exit 1
set -- $(cat $OUT/machine)
gcc -O2 -w -fcommon $(cat build/machine-$1-$2.flags) -o $OUT/builds-test test/builds_test.c util/image.c -lm || exit 1
$OUT/builds-test $OUT/plain.hex $OUT/plain.tables $OUT/image.hex $OUT/image.tables
