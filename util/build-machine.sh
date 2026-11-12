#!/bin/sh
# Build the toolchain and nucleus for a machine of a given size.
#
#   util/build-machine.sh CELL_BITS [ADDR_BITS]
#
# CELL_BITS is 64, 32 or 16.  ADDR_BITS defaults to 17 (128KB), or 16 for a
# 16-bit cell.  Writes, under build/:
#   basewords-C-A.fs   the machine definition cross.fs reads
#   nuc-C-A.hex        the nucleus compiled for that machine
#   hexaforth-C-A      the reference VM built for it
set -e
cd "$(dirname "$0")/.."
C=$1
A=${2:-$([ "$C" -lt 17 ] && echo "$C" || echo 17)}
D="-DCELL_BITS=$C -DADDR_BITS=$A"
mkdir -p build
gcc -w -fcommon $D -o build/bwgen-$C-$A util/baseword_fs_gen.c vm_opcodes.c vm_debug.c -lm
build/bwgen-$C-$A build/basewords-$C-$A.fs > /dev/null
# cross.fs always writes build/nuc.hex and build/nuc.lst; keep any that are
# there, since they belong to the default machine.
for f in hex lst; do [ -f build/nuc.$f ] && mv build/nuc.$f build/nuc.$f.keep; done
(cd forth && gforth ./cross.fs ../build/basewords-$C-$A.fs ./nuc.fs > ../build/cross-$C-$A.log 2>&1) || true
if grep -q "tdp" build/cross-$C-$A.log; then
    mv build/nuc.hex build/nuc-$C-$A.hex
    rm -f build/nuc.lst
fi
for f in hex lst; do [ -f build/nuc.$f.keep ] && mv build/nuc.$f.keep build/nuc.$f; done
[ -f build/nuc-$C-$A.hex ] || { tail -8 build/cross-$C-$A.log; exit 1; }
gcc -O2 -w -fcommon $D -o build/hexaforth-$C-$A main.c vm.c vm_opcodes.c vm_debug.c util/stack.c -lm
echo "build/nuc-$C-$A.hex: $(tr -c 'a-zA-Z0-9 ' ' ' < build/cross-$C-$A.log | grep -o 'tdp [0-9A-F]* tcp [0-9A-F]*')"
