#!/bin/sh
# The image built with cross.fs's peephole, and built without.
#
#   util/both-builds.sh OUT
#
# For the first machine of util/machines.  The build without the peephole
# reads the calls the build with it compiled in place (build/nuc.inline),
# so that the two have the same calls: they are built one after the other.
# Leaves in OUT:
#
#   image.hex image.tables image.sizes   the image, built with the peephole
#   plain.hex plain.tables               and without
#   machine                              the machine's line of util/machines
#
# build-machine.sh is run for the machine first, so its reference VM, its
# basewords and what the C compiler is told for it are in build/ as that
# leaves them.  cross.fs writes build/nuc.*, which belong to the default
# machine: any that are there are kept.
cd "$(dirname "$0")/.."
OUT=$1
[ -n "$OUT" ] || { echo "usage: util/both-builds.sh OUT"; exit 1; }
mkdir -p $OUT
if command -v brew > /dev/null 2>&1; then       # as build-machine.sh: where Homebrew's headers are
    CPATH="$(brew --prefix)/include${CPATH:+:$CPATH}"
    export CPATH
fi
set -- $(grep -v '^#' util/machines | head -1)
C=$1 A=$2
echo "$*" > $OUT/machine
util/build-machine.sh "$@" > $OUT/machine.log 2>&1 || { echo "FAIL the machine"; tail -5 $OUT/machine.log; exit 1; }

KEEP="hex lst tables inline sizes"
for f in $KEEP; do [ -f build/nuc.$f ] && mv build/nuc.$f build/nuc.$f.keep; done
rm -f $OUT/image.hex $OUT/plain.hex
(cd forth && gforth ./cross.fs ../build/basewords-$C-$A.fs ./nuc.fs) > $OUT/cross.log 2>&1
if grep -q "tdp" $OUT/cross.log; then
    cp build/nuc.hex $OUT/image.hex; cp build/nuc.tables $OUT/image.tables; cp build/nuc.sizes $OUT/image.sizes
    (cd forth && PEEPHOLE=off gforth ./cross.fs ../build/basewords-$C-$A.fs ./nuc.fs) > $OUT/cross-plain.log 2>&1
    grep -q "tdp" $OUT/cross-plain.log && { cp build/nuc.hex $OUT/plain.hex; cp build/nuc.tables $OUT/plain.tables; }
fi
for f in $KEEP; do rm -f build/nuc.$f; [ -f build/nuc.$f.keep ] && mv build/nuc.$f.keep build/nuc.$f; done
[ -f $OUT/image.hex ] || { echo "FAIL the image"; tail -5 $OUT/cross.log; exit 1; }
[ -f $OUT/plain.hex ] || { echo "FAIL the image with the peephole off"; tail -5 $OUT/cross-plain.log; exit 1; }
