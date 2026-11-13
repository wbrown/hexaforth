#!/usr/bin/env python3
"""Turn a cross.fs hex image into what the Verilog memory is loaded from.

    memimage.py IMAGE.hex OUTDIR [CELL_BITS [ADDR_BITS]]

Writes OUTDIR/mem.hex, a cell on each line, cut to the size of the memory
(2^ADDR_BITS cells).  Also writes OUTDIR/krom.vh, the first 4KB as a Verilog
function, for the CPU's kernel ROM, and says so if the kernel's code does not
fit in it.
"""
import struct
import sys

data = b"".join(struct.pack("<I", int(l, 16)) for l in open(sys.argv[1]) if l.strip())
size = (int(sys.argv[3]) if len(sys.argv) > 3 else 64) // 8
if len(sys.argv) > 4:
    data = data[:size << int(sys.argv[4])]
data += bytes(-len(data) % size)
with open(f"{sys.argv[2]}/mem.hex", "w") as f:
    for at in range(0, len(data), size):
        f.write(f"{int.from_bytes(data[at:at + size], 'little'):0{2 * size}x}\n")

KROM = 4096                         # bytes
with open(f"{sys.argv[2]}/krom.vh", "w") as f:
    f.write("// The first 4KB of the kernel, for the CPU's kernel ROM.  Written by memimage.py.\n")
    f.write("function [15:0] kromf(input [10:0] a);\n    case (a)\n")
    for i, (w,) in enumerate(struct.iter_unpack("<H", data[:KROM])):
        if w:
            f.write(f"        11'd{i}: kromf = 16'h{w:04x};\n")
    f.write("        default: kromf = 16'h0000;\n    endcase\nendfunction\n")
if any(data[KROM:0x2000]):
    print(f"memimage.py: kernel code runs past {KROM} bytes; it will not work from the kernel ROM",
          file=sys.stderr)
