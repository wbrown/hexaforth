#!/bin/sh
# Build the toolchain and nucleus for a machine of a given size.
#
#   util/build-machine.sh CELL_BITS [ADDR_BITS [WIDTH HEIGHT BASE]]
#
# CELL_BITS is 64, 32 or 16.  Memory is 2^ADDR_BITS cells; the default is 22
# for a 64-bit cell, 17 for 32 and 16 for 16.  WIDTH, HEIGHT and BASE are a
# display for it, in pixels and the cell it starts at (util/machines has
# the ones the system is tried on).  Writes, under build/:
#   basewords-C-A.fs   the machine definition cross.fs reads
#   nuc-C-A.hex        the nucleus compiled for that machine
#   hexaforth-C-A      the reference VM built for it
#   dither-test-C-A    the dither's test, which has that VM in it
#   source-test-C-A    the test of comments and of a block read as source,
#                      which has it too
#   prompt-test-C-A    the test of the nucleus at its prompt, which has it too
#   region-test-C-A    the test of regions, which has it too
#   tiff-test-C-A      the test of the TIFF reader, which has it too
#   display-test-C-A   the test of the drawing words, which has it too, if
#                      the machine has a display
#   machine-C-A.flags  what the C compiler is told for it, for whatever else
#                      is built for the machine
set -e
cd "$(dirname "$0")/.."
C=$1
# The patterns have both brackets: the sh of macOS (bash 3.2) cannot read a
# case inside $( ) without them.
A=${2:-$(case $C in (64) echo 22;; (32) echo 17;; (*) echo 16;; esac)}
D="-DCELL_BITS=$C -DADDR_BITS=$A $EXTRA"
[ -n "$3" ] && D="$D -DDISP_W=$3 -DDISP_H=$4 -DDISP_BASE=$5"
mkdir -p build
echo "$D" > build/machine-$C-$A.flags
# cross.fs has gforth build C (util/fuse.c).  Where gforth is Homebrew's,
# the compiler has to be told where Homebrew's headers are, as
# CMakeLists.txt tells it.
if command -v brew > /dev/null 2>&1; then
    CPATH="$(brew --prefix)/include${CPATH:+:$CPATH}"
    export CPATH
fi
gcc -w -fcommon $D -o build/bwgen-$C-$A util/baseword_fs_gen.c vm_opcodes.c vm_debug.c -lm
build/bwgen-$C-$A build/basewords-$C-$A.fs > /dev/null
# cross.fs always writes build/nuc.hex, build/nuc.lst, build/nuc.sizes and
# build/nuc.inline (the calls it compiled in place, which a build with the
# peephole off reads), and with cells of 32 bits or more build/nuc.tables,
# the names that go with the image; keep any that are there, since they
# belong to the default machine.
KEEP="hex lst tables inline sizes"
for f in $KEEP; do [ -f build/nuc.$f ] && mv build/nuc.$f build/nuc.$f.keep; done
(cd forth && gforth ./cross.fs ../build/basewords-$C-$A.fs ./nuc.fs > ../build/cross-$C-$A.log 2>&1) || true
if grep -q "tdp" build/cross-$C-$A.log; then
    mv build/nuc.hex build/nuc-$C-$A.hex
    rm -f build/nuc.lst build/nuc.inline
    for f in tables sizes; do
        if [ -f build/nuc.$f ]; then mv build/nuc.$f build/nuc-$C-$A.$f; fi
    done
fi
for f in $KEEP; do [ -f build/nuc.$f.keep ] && mv build/nuc.$f.keep build/nuc.$f; done
[ -f build/nuc-$C-$A.hex ] || { tail -8 build/cross-$C-$A.log; exit 1; }
VM="vm.c vm_opcodes.c vm_debug.c util/stack.c"
gcc -O2 -w -fcommon $D -o build/hexaforth-$C-$A main.c $VM -lm
TYPED="test/nucleus.c $VM util/image.c"        # the nucleus on that VM, typed at
gcc -O2 -w -fcommon $D -o build/dither-test-$C-$A test/dither_test.c test/dither.c $TYPED -lm
gcc -O2 -w -fcommon $D -o build/source-test-$C-$A test/source_test.c $TYPED -lm
gcc -O2 -w -fcommon $D -o build/prompt-test-$C-$A test/prompt_test.c $TYPED -lm
gcc -O2 -w -fcommon $D -o build/region-test-$C-$A test/region_test.c $TYPED -lm
gcc -O2 -w -fcommon $D -o build/tiff-test-$C-$A test/tiff_test.c test/dither.c $TYPED -lm
[ -n "$3" ] && gcc -O2 -w -fcommon $D -o build/display-test-$C-$A test/display_test.c test/tables.c $TYPED -lm
echo "build/nuc-$C-$A.hex: $(tr -c 'a-zA-Z0-9 ' ' ' < build/cross-$C-$A.log | grep -o 'tdp [0-9A-F]* thp [0-9A-F]* tcp [0-9A-F]*')"
