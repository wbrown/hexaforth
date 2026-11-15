#!/usr/bin/env python3
"""Build a block device image for hexaforth.

    mkdisk.py IMAGE OUT.img [SOURCE.fs [NAMES]]

Blocks are 1024 bytes.  Layout:
    block 1     Forth source, space padded, for `1 load`
    block 16..  IMAGE as an uncompressed little-endian RGB TIFF, written
                here in several strips so the reader has to follow the strip
                tables
    block 1024.. NAMES, the tables of names the build made for the
                decompiler (build/nuc.tables), which see.fs reads in when
                the Words window comes to the front
    block 1152.. nothing: HexaPaint keeps its canvas here, a block that says
                it is there and then up to fifteen blocks of its rows
                (paint.fs), and the image is made long enough to have them

Also writes OUT.tiff beside the image, for reference.
"""
import struct
import sys

from PIL import Image

BLOCK = 1024
TIFF_BLOCK = 16
NAMES_BLOCK = 1024
CANVAS_BLOCK = 1152
CANVAS_BLOCKS = 16
ROWS_PER_STRIP = 64


def tiff(im):
    w, h = im.size
    rgb = im.convert("RGB").tobytes()
    strips = [rgb[y * w * 3:(y + ROWS_PER_STRIP) * w * 3] for y in range(0, h, ROWS_PER_STRIP)]
    n = len(strips)
    entries = 9
    ifd_at = 8
    bits_at = ifd_at + 2 + entries * 12 + 4
    offsets_at = bits_at + 6
    counts_at = offsets_at + 4 * n
    data_at = (counts_at + 4 * n + 1023) & ~1023    # start the pixels on a block
    offsets, at = [], data_at
    for s in strips:
        offsets.append(at)
        at += len(s)

    def entry(tag, kind, count, value):
        return struct.pack("<HHII", tag, kind, count, value)

    SHORT, LONG = 3, 4
    out = b"II" + struct.pack("<HI", 42, ifd_at)
    out += struct.pack("<H", entries)
    out += entry(256, LONG, 1, w)                   # ImageWidth
    out += entry(257, LONG, 1, h)                   # ImageLength
    out += entry(258, SHORT, 3, bits_at)            # BitsPerSample
    out += entry(259, SHORT, 1, 1)                  # Compression: none
    out += entry(262, SHORT, 1, 2)                  # Photometric: RGB
    out += entry(273, LONG, n, offsets_at)          # StripOffsets
    out += entry(277, SHORT, 1, 3)                  # SamplesPerPixel
    out += entry(278, LONG, 1, ROWS_PER_STRIP)      # RowsPerStrip
    out += entry(279, LONG, n, counts_at)           # StripByteCounts
    out += struct.pack("<I", 0)
    out += struct.pack("<3H", 8, 8, 8)
    out += struct.pack(f"<{n}I", *offsets)
    out += struct.pack(f"<{n}I", *(len(s) for s in strips))
    out += bytes(data_at - len(out))
    out += b"".join(strips)
    return out


def main():
    image, out = sys.argv[1], sys.argv[2]
    source = open(sys.argv[3]).read() if len(sys.argv) > 3 else ""
    text = " ".join(source.split()).encode()
    assert len(text) <= BLOCK, "source does not fit one block"
    data = tiff(Image.open(image))
    open(out.rsplit(".", 1)[0] + ".tiff", "wb").write(data)
    disk = bytearray(BLOCK * TIFF_BLOCK)
    disk[BLOCK:2 * BLOCK] = text.ljust(BLOCK)
    disk += data
    if len(sys.argv) > 4:                           # the tables of names, for the decompiler
        names = open(sys.argv[4], "rb").read()
        assert len(disk) <= BLOCK * NAMES_BLOCK, "the picture runs into the tables of names"
        disk += bytes(BLOCK * NAMES_BLOCK - len(disk)) + names
    assert len(disk) <= BLOCK * CANVAS_BLOCK, "what is on the disk runs into HexaPaint's canvas"
    disk += bytes(BLOCK * (CANVAS_BLOCK + CANVAS_BLOCKS) - len(disk))
    disk += bytes(-len(disk) % (64 * BLOCK))
    open(out, "wb").write(disk)
    print(f"{out}: {len(disk) // BLOCK} blocks, TIFF at block {TIFF_BLOCK} "
          f"({len(data)} bytes)")


if __name__ == "__main__":
    main()
