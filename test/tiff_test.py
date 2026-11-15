#!/usr/bin/env python3
"""Check the TIFF reader in forth/tiff.fs, which reads a row at a time.

    tiff_test.py VM IMAGE.hex [SEED [CELL_BITS]]

The reference VM has no disk, so it is given one at the prompt: a word in
'blk-read that makes a block's bytes from the block's number, by a
generator that is the same here, and then lays over them the bytes of a
file's header and directory where the block has any.  A disk of any size is
so had with nothing kept, and a file's pixels are whatever the disk holds
where its strips say they are.

Each file is opened and dithered there (tiff-open, atkinson), and the
bitmap compared with a dither done here of the same bytes.  The files are
of either byte order, of one sample, two and three, with strips that are
and are not where blocks and cells start, next to each other, apart and
out of order, their places given as LONGs and SHORTs, in the entry and
out of it, and one has its directory across the end of a block.  One is
dithered twice, two are dithered a row of each in turn, and the last is
512 by 342 of three samples: 525KB, on a machine that with cells of 16
bits has 128KB.  The machine may have cells of 64 bits, 32 or 16.
"""
import os
import random
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dither_test import reference  # noqa: E402

BLOCK = 1024
SPACING = 1024          # a file every so many blocks of the disk


def disk_block(blk):
    """The bytes of a block of the disk with no file's header in it."""
    x = (blk * 40503 + 12345) & 0xFFFF
    out = bytearray()
    for _ in range(BLOCK):
        x = (x * 25173 + 13849) & 0xFFFF
        out.append(x >> 8)
    return out


class File:
    """A TIFF on the disk: only its header and directory are written."""

    def __init__(self, n, w, h, spp, rps, offsets, be=False, photo=None, short=False, ifd_at=8, base=None):
        self.n, self.w, self.h, self.spp, self.rps = n, w, h, spp, rps
        self.base = (n + 1) * SPACING if base is None else base
        self.photo = photo if photo is not None else (2 if spp == 3 else 1)
        e = ">" if be else "<"
        strips = (h + rps - 1) // rps if rps else 1
        assert len(offsets) == strips
        self.offsets = offsets
        entries = [(256, 4, [w]), (257, 3, [h]), (258, 3, [8] * spp), (259, 3, [1]),
                   (262, 3, [self.photo]), (273, 3 if short else 4, offsets), (277, 3, [spp])]
        if rps:
            entries.append((278, 4, [rps]))
        row = w * spp
        entries.append((279, 4, [row * min(rps or h, h - i * (rps or h)) for i in range(strips)]))
        after = ifd_at + 2 + 12 * len(entries) + 4
        body, extra = struct.pack(e + "H", len(entries)), b""
        for tag, kind, values in sorted(entries):
            data = struct.pack(e + str(len(values)) + ("H" if kind == 3 else "I"), *values)
            if len(data) <= 4:
                field = data.ljust(4, b"\0")
            else:
                field = struct.pack(e + "I", after + len(extra))
                extra += data + bytes(len(data) & 1)
            body += struct.pack(e + "HHI", tag, kind, len(values)) + field
        body += struct.pack(e + "I", 0) + extra
        self.explicit = dict(enumerate((b"MM" if be else b"II") + struct.pack(e + "HI", 42, ifd_at)))
        self.explicit.update((ifd_at + i, b) for i, b in enumerate(body))
        assert max(self.explicit) < min(offsets), "a strip in the directory"
        self.blocks = {}

    def byte(self, off):
        if off in self.explicit:
            return self.explicit[off]
        blk = self.base + off // BLOCK
        if blk not in self.blocks:
            self.blocks[blk] = disk_block(blk)
        return self.blocks[blk][off % BLOCK]

    def patches(self, cellb):
        """(block of the disk, cell in it, the cells): where the header's bytes go."""
        out = []
        for fb in sorted({off // BLOCK for off in self.explicit}):
            offs = [off for off in self.explicit if off // BLOCK == fb]
            lo, hi = min(offs) // cellb * cellb, max(offs) // cellb * cellb + cellb
            cells = [sum(self.byte(a + i) << 8 * i for i in range(cellb)) for a in range(lo, hi, cellb)]
            out.append((self.base + fb, (lo % BLOCK) // cellb, cells))
        return out

    def rows(self):
        """Its dither: rows of 0 and 1, 1 for white."""
        w, h, spp = self.w, self.h, self.spp
        gray = []
        for y in range(h):
            at = self.offsets[y // self.rps if self.rps else 0] + (y % self.rps if self.rps else y) * w * spp
            for x in range(w):
                s = [self.byte(at + x * spp + i) for i in range(spp)]
                if spp == 3:
                    gray.append((77 * s[0] + 150 * s[1] + 29 * s[2]) >> 8)
                else:
                    gray.append(s[0] if self.photo else 255 - s[0])
        return reference(gray, w, h)

    def bitmap(self, bits):
        """The cells its dither should be."""
        w = self.w
        stride = (w + bits - 1) // bits
        return [sum(1 << b for b in range(bits) if c * bits + b < w and not row[c * bits + b])
                for row in self.rows() for c in range(stride)]


def session(vm, image, bits, files, script):
    """Type the disk, the files' headers and a script; what the VM printed."""
    cellb = bits // 8
    lines = ["variable sd  variable bf  variable bk",
             ": rnd sd @ 25173 * 13849 + 65535 and dup sd ! 8 rshift ;",
             f": cl 0 {cellb} 0 do rnd i 8 * lshift or loop ;",
             f": gen 40503 * 12345 + 65535 and sd !  {BLOCK // cellb} over + swap do cl i ! loop ;",
             ": d rot . hex over 1 + @ * swap @ swap over + swap do i @ . loop decimal ;"]
    data, table = [], []
    for f in files:
        for blk, cell, cells in f.patches(cellb):
            table.append((blk, cell, len(cells), len(data)))
            data += cells
    # The headers' bytes are a table of cells, and go where there is room for one: above the display.
    lines += [f"{len(data) * cellb} halloc constant pd  variable np  pd np !", ": l, np @ ! 1 np +! ;", "hex"]
    for i in range(0, len(data), 6):
        lines.append(" ".join(f"{c:x} l," for c in data[i:i + 6]))
    lines.append("decimal create pt")
    for blk, cell, n, src in table:
        lines.append(f"{blk} , {cell} , {n} , pd {src} + ,")
    lines += ["0 , 0 , 0 , 0 ,",
              ": lay dup >r 3 + @  r@ 1 + @ bf @ +  r@ 2 + @ cells-move r> ;",
              ": rd 2dup gen  bk ! bf !  pt begin dup 2 + @ while dup @ bk @ = if lay then 4 + repeat drop ;",
              "' rd 'blk-read !"]
    lines += script
    typed = "".join(line + "\n" for line in lines) + "8000 execute\n"
    return subprocess.run([vm, image], input=typed, capture_output=True, text=True, timeout=1200).stdout


def check(out, bits, want):
    failed = 0
    for line, f, what in want:
        cells = f.bitmap(bits)
        at = out.find(line + " ")
        got = out[at + len(line):].split("\n")[0].split() if at >= 0 else ["(not echoed)"]
        try:
            got_cells = [int(v, 16) & (1 << bits) - 1 for v in got[1:-1]] if got[-1:] == ["ok"] else None
        except ValueError:
            got_cells = None
        if got_cells != cells:
            failed += 1
            if failed <= 4:
                n = 0 if got_cells is None else sum(a != b for a, b in zip(got_cells, cells))
                print(f"FAIL {what}: {' '.join(got)[:200]} ({n} of {len(cells)} cells differ)")
    return failed


def main():
    vm, image = sys.argv[1:3]
    seed = int(sys.argv[3]) if len(sys.argv) > 3 else 1
    bits = int(sys.argv[4]) if len(sys.argv) > 4 else 64
    rng = random.Random(seed)
    odd = rng.randrange(257, 1000) | 1

    def along(first, sizes, gap):
        out, at = [], first
        for s in sizes:
            out.append(at)
            at += s + gap
        return out

    def strips(w, h, spp, rps):
        return [w * spp * min(rps, h - y) for y in range(0, h, rps)]

    shapes = [  # what, w, h, spp, rps, first, gap, and the rest
        ("strips on blocks and cells", 64, 20, 3, 4, 1024, 0, {}),
        ("strips at odd bytes", 64, 12, 3, 5, odd, 7, {}),
        ("the first strip on a cell, the rest not", 64, 12, 3, 4, 1024, 5, {}),
        ("big-endian, white is zero, a strip a row", 37, 11, 1, 1, 300 + rng.randrange(40), 3,
         dict(be=True, photo=0)),
        ("one strip and no RowsPerStrip, its place a SHORT", 50, 9, 1, 0, 2048 + rng.randrange(64), 0,
         dict(short=True)),
        ("big-endian, places an array of SHORTs", 33, 7, 3, 3, 700 + rng.randrange(99), 0,
         dict(be=True, short=True)),
        ("two places, SHORTs in the entry", 40, 6, 1, 3, 600 + rng.randrange(99), 1, dict(short=True)),
        ("two samples", 24, 5, 2, 2, 512, 0, {}),
        ("the directory across the end of a block", 48, 8, 3, 8, 4096, 0, dict(ifd_at=1017)),
    ]
    files, script, want = [], [], []
    for what, w, h, spp, rps, first, gap, more in shapes:
        offs = along(first, strips(w, h, spp, rps) if rps else [w * spp * h], gap)
        files.append(File(len(files), w, h, spp, rps, offs, **more))
    # Strips in the file in the other order from the rows they are.
    sizes = strips(48, 9, 3, 3)
    files.append(File(len(files), 48, 9, 3, 3, along(1100, sizes, 2000 - sizes[0])[::-1]))
    shapes.append(("strips out of order",))
    for f, shape in zip(files, shapes):
        script += [f"create b{f.n} 0 , 0 ,  {f.base} tiff-open constant i{f.n}",
                   f"b{f.n} i{f.n} atkinson", f"{f.n} b{f.n} {f.h} d"]
        want.append((script[-1], f, shape[0]))
    # Again, from the first pixel: a new bitmap of the same image.
    script += ["create c1 0 , 0 ,  c1 i1 atkinson", f"101 c1 {files[1].h} d"]
    want.append((script[-1], files[1], "an image dithered a second time"))
    # Two at once, a row of each in turn: each has its own window.
    fa, fb = files[0], files[3]
    fb2 = File(len(files), fb.w, fa.h, 1, 2, along(900, strips(fb.w, fa.h, 1, 2), 11))
    files.append(fb2)
    script += [f"create ta 0 , 0 ,  create tb 0 , 0 ,  {fb2.base} tiff-open constant i{fb2.n}",
               f"ta i{fa.n} dither-start constant da  tb i{fb2.n} dither-start constant db",
               f": two {fa.h} 0 do da dither-row db dither-row loop ; two",
               f"102 ta {fa.h} d", f"103 tb {fa.h} d"]
    want.append((script[-2], fa, "two at once: the first"))
    want.append((script[-1], fb2, "two at once: the second"))
    failed = check(session(vm, image, bits, files, script), bits, want)
    count = len(want)

    # One that is bigger than a small machine: in a session of its own, since nothing gives memory back.
    w, h = 512, 342
    big = File(0, w, h, 3, 64, along(1024 + (0 if seed % 2 else 3), strips(w, h, 3, 64), 0))
    # And then more memory is asked for than the machine has left, which is refused.
    more = {16: 50000, 32: 600000, 64: 40000000}[bits]
    script = [f"create b0 0 , 0 ,  {big.base} tiff-open constant i0", "b0 i0 atkinson", f"0 b0 {h} d",
              f": more {more} halloc ; ' more catch ."]
    out = session(vm, image, bits, [big], script)
    failed += check(out, bits, [(script[-2], big, f"an image of {w * h * 3} bytes")])
    if script[-1] + " -8  ok" not in out:
        failed += 1
        print(f"FAIL asking for {more} bytes that are not there: "
              f"{out[out.find(script[-1]):].splitlines()[0][:120] if script[-1] in out else '(not echoed)'}")
    count += 2
    print(f"the TIFF reader, cells of {bits} bits, seed {seed}: {count} checks, {failed} failed: files "
          f"dithered as the same bytes are here, and memory that is not there refused")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
