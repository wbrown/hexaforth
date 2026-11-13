#!/usr/bin/env python3
"""Split a cross.fs hex image into the byte lanes of hf_mem.

    lanes.py IMAGE.hex OUT_PREFIX [CELL_BITS [MEMORY_BITS]]

Writes OUT_PREFIX0.hex and up: one file for each byte of a cell, cut to the
size of the memory (2^MEMORY_BITS bytes).  Also writes OUT_PREFIXkrom.hex,
the first 4KB as 2048 instructions, for the CPU's kernel ROM, and says so if
the kernel's code does not fit in it.
"""
import struct
import sys

data = b"".join(struct.pack("<I", int(l, 16)) for l in open(sys.argv[1]) if l.strip())
lanes = (int(sys.argv[3]) if len(sys.argv) > 3 else 64) // 8
if len(sys.argv) > 4:
    data = data[:1 << int(sys.argv[4])]
for k in range(lanes):
    with open(f"{sys.argv[2]}{k}.hex", "w") as f:
        f.write("\n".join(f"{b:02x}" for b in data[k::lanes]) + "\n")

KROM = 4096                         # bytes
with open(f"{sys.argv[2]}krom.hex", "w") as f:
    f.write("\n".join(f"{w:04x}" for (w,) in struct.iter_unpack("<H", data[:KROM])) + "\n")
if any(data[KROM:0x2000]):
    print(f"lanes.py: kernel code runs past {KROM} bytes; it will not work from the kernel ROM",
          file=sys.stderr)
