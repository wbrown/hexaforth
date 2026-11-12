#!/usr/bin/env python3
"""Convert a cross.fs hex dump (one 32-bit little-endian word per line) to a
flat binary, dropping trailing zeros."""
import struct
import sys

words = [int(line, 16) for line in open(sys.argv[1]) if line.strip()]
data = b"".join(struct.pack("<I", w) for w in words).rstrip(b"\0")
data += b"\0" * (-len(data) % 8)
open(sys.argv[2], "wb").write(data)
