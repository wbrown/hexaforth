#!/bin/sh
# The files that are made from the machine and from an image, and checked
# in, made again.
#
#   util/tables.sh
#
#   forth/folds.fs   what the nucleus's peephole is told of numbers, asked
#                    of the machine (util/joins.c)
#   forth/fused.fs   the words the decompiler shows an instruction as, made
#                    from a built image (util/fused.c).  The table is in
#                    the image, so the image is built with the table there
#                    is, the table made of that image, and the image built
#                    again, until the table stops changing
#
# The image is the default machine's (build/nuc.hex).  It says what it
# made, and the exit status is 0 only if the table settled.
cd "$(dirname "$0")/.."
mkdir -p build
if command -v brew > /dev/null 2>&1; then       # as build-machine.sh: where Homebrew's headers are
    CPATH="$(brew --prefix)/include${CPATH:+:$CPATH}"
    export CPATH
fi

gcc -O2 -w -fcommon -o build/joins util/joins.c -lm || exit 1
gcc -O2 -w -fcommon -Iutil -o build/fused util/fused.c util/image.c -lm || exit 1
build/joins forth/folds.fs || exit 1

image() { # build it, and say whether it was built
    (cd forth && gforth ./cross.fs ./basewords.fs ./nuc.fs) > build/cross.log 2>&1
    grep -q "tdp" build/cross.log
}

for turn in 1 2 3 4 5 6; do
    image || { echo "FAIL the image was not built"; tail -8 build/cross.log; exit 1; }
    cp forth/fused.fs build/fused.before
    build/fused build/nuc.hex build/nuc.tables forth/fused.fs || exit 1
    if cmp -s forth/fused.fs build/fused.before; then
        echo "forth/fused.fs settled after $turn"
        exit 0
    fi
done
echo "FAIL forth/fused.fs was still changing after $turn images"
exit 1
