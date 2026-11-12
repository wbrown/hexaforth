#!/usr/bin/env python3
"""Split a cross.fs hex image into the eight byte lanes of hf_mem.

    lanes.py IMAGE.hex OUT_PREFIX     writes OUT_PREFIX0.hex .. OUT_PREFIX7.hex
"""
import struct
import sys

data = b"".join(struct.pack("<I", int(l, 16)) for l in open(sys.argv[1]) if l.strip())
for k in range(8):
    with open(f"{sys.argv[2]}{k}.hex", "w") as f:
        f.write("\n".join(f"{b:02x}" for b in data[k::8]) + "\n")
