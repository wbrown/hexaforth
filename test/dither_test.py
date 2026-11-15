#!/usr/bin/env python3
"""Check the dither in forth/atkinson.fs against a dither a pixel at a time.

    dither_test.py VM IMAGE.hex [SEED [CELL_BITS]]

Images are made in the reference VM's memory, of bytes from a generator
that is the same here, and dithered there (image-is, atkinson); the bitmap
is printed and compared with the dither done here the plain way, every
pixel.  They are of one sample and of three, and of widths that are and
are not whole cells of lanes, of pixels and of the image's own bytes, so
that both ways the grays are had and every end of a row are tried.  Two
are dithered a row of each in turn, three have a bitmap made of a part of
them (dither-part), and four one at half their size (dither-half).  The
machine may have cells of 64 bits, 32 or 16.
"""
import random
import subprocess
import sys


def reference(gray, w, h):
    """Atkinson's dither, a pixel at a time: rows of 1 for white."""
    err = [[0] * (w + 4) for _ in range(h + 2)]
    rows = []
    for y in range(h):
        row = []
        for x in range(w):
            old = gray[y * w + x] + err[y][x + 2]
            new = 255 if old > 127 else 0
            row.append(1 if new else 0)
            e = ((old - new + 1024) >> 3) - 128
            for dx, dy in ((1, 0), (2, 0), (-1, 1), (0, 1), (1, 1), (0, 2)):
                err[y + dy][x + 2 + dx] += e
        rows.append(row)
    return rows


def main():
    vm, image = sys.argv[1:3]
    seed = int(sys.argv[3]) if len(sys.argv) > 3 else 1
    bits = int(sys.argv[4]) if len(sys.argv) > 4 else 64
    rng = random.Random(seed)
    shapes = [(64, 10, 3), (40, 9, 3), (37, 11, 3), (50, 8, 1), (1, 3, 1), (3, 5, 3), (130, 6, 3), (16, 4, 1)]
    shapes += [(rng.choice([8, 24, 33, 72, 100]), rng.randrange(1, 9), rng.choice([1, 3])) for _ in range(6)]
    lines = ["variable sd",
             ": rnd sd @ 25173 * 13849 + 65535 and dup sd ! 8 rshift ;",
             # A cell at a time, its lowest byte first: where a cell is 16 bits the address of a
             # byte cannot say the upper half of memory, which is where halloc's memory is.
             f": cl 0 {bits // 8} 0 do rnd i 8 * lshift or loop ;",
             f": fill {bits // 8 - 1} + {(bits // 8).bit_length() - 1} rshift over + swap do cl i ! loop ;",
             ": d rot . hex over 1 + @ * swap @ swap over + swap do i @ . loop decimal ;",    # ( n bm h )
             # What an image and its dither took is given back when its bitmap has been
             # printed, so that the images together want no more memory than the biggest.
             "himem @ constant hm"]
    want = []
    done = "hm himem !"

    def make(n, w, h, spp, part=None, half=False):
        """Make image n in the VM; the cells its bitmap should be, or those of
        a bitmap of a part of it: (x, y, w, h), x being where a cell starts.
        At half size a pixel is the gray of four, to the nearest, and the
        part is of that."""
        start = rng.randrange(1, 65536)
        x, data = start, []
        for _ in range(w * h * spp):
            x = (x * 25173 + 13849) & 0xFFFF
            data.append(x >> 8)
        gray = data if spp == 1 else [(77 * data[i] + 150 * data[i + 1] + 29 * data[i + 2]) >> 8
                                      for i in range(0, len(data), 3)]
        gw, gh = w, h
        if half:
            gw, gh = w // 2, h // 2
            gray = [(gray[2 * y * w + 2 * x] + gray[2 * y * w + 2 * x + 1]
                     + gray[(2 * y + 1) * w + 2 * x] + gray[(2 * y + 1) * w + 2 * x + 1] + 2) >> 2
                    for y in range(gh) for x in range(gw)]
        px, py, pw, ph = part or (0, 0, gw, gh)
        stride = (pw + bits - 1) // bits
        cells = []
        for row in reference(gray, gw, gh)[py:py + ph]:
            for c in range(stride):                 # whole cells of the row, as far as the image goes
                cells.append(sum(1 << b for b in range(bits)
                                 if px + c * bits + b < gw and not row[px + c * bits + b]))
        lines.extend([f"{w * h * spp} halloc constant r{n}",
                      f"{start} sd !  r{n} {w * h * spp} fill",
                      f"create b{n} 0 , 0 ,",
                      f"r{n} {w} {h} {spp} image-is constant i{n}"])
        return cells

    for n, (w, h, spp) in enumerate(shapes):
        cells = make(n, w, h, spp)
        lines += [f"b{n} i{n} atkinson", f"{n} b{n} {h} d"]
        want.append((lines[-1], (w, h, spp), cells))
        lines.append(done)
    # Two at once: a row of one and then a row of the other, to the end of
    # both.  Each has its own image and its own dither.
    n = len(shapes)
    for (wa, spa), (wb, spb), h in (((48, 3), (30, 1), 7), ((64, 3), (64, 3), 5)):
        ca, cb = make(n, wa, h, spa), make(n + 1, wb, h, spb)
        lines += [f"b{n} i{n} dither-start constant d{n}  b{n + 1} i{n + 1} dither-start constant d{n + 1}",
                  f": two{n} {h} 0 do d{n} dither-row d{n + 1} dither-row loop ; two{n}",
                  f"{n} b{n} {h} d", f"{n + 1} b{n + 1} {h} d"]
        want.append((lines[-2], (wa, h, spa), ca))
        want.append((lines[-1], (wb, h, spb), cb))
        lines.append(done)
        n += 2
    # A bitmap of a part of an image: every row is dithered as far as the last
    # that is kept, and the rows and the cells of the part are what is kept.
    for (w, h, spp), part in (((3 * bits + 7, 12, 3), (bits, 3, bits + 9, 5)),
                              ((4 * bits, 10, 3), (2 * bits, 2, bits, 6)),
                              ((2 * bits + 3, 9, 1), (0, 4, bits + 5, 5))):
        cells = make(n, w, h, spp, part)
        px, py, pw, ph = part
        lines += [f"b{n} i{n} {px} {py} {pw} {ph} dither-part constant d{n}",
                  f": part{n} {py + ph} 0 do d{n} dither-row loop ; part{n}",
                  f"{n} b{n} {ph} d"]
        want.append((lines[-1], (pw, ph, spp), cells))
        lines.append(done)
        n += 1
    # A bitmap of an image at half its size, all of it or a part: two rows of
    # the image are read for each row that is dithered.  Widths of three
    # samples that are a multiple of eight are those whose grays the machine
    # with cells of 64 bits has a cell at a time; an odd width has a pixel
    # over in every row, and an odd height a row over.
    for (w, h, spp), part in (((4 * bits, 10, 3), (0, 0, 2 * bits, 5)),
                              ((4 * bits + 6, 9, 3), (bits, 1, bits + 3, 3)),
                              ((2 * bits + 5, 8, 1), (0, 0, bits + 2, 4)),
                              ((8 * bits, 6, 3), (2 * bits, 1, bits, 2))):
        cells = make(n, w, h, spp, part, half=True)
        px, py, pw, ph = part
        lines += [f"b{n} i{n} {px} {py} {pw} {ph} dither-half constant d{n}",
                  f": half{n} {py + ph} 0 do d{n} dither-row loop ; half{n}",
                  f"{n} b{n} {ph} d"]
        want.append((lines[-1], (pw, ph, spp), cells))
        lines.append(done)
        n += 1
    typed = "".join(line + "\n" for line in lines) + "8000 execute\n"
    out = subprocess.run([vm, image], input=typed, capture_output=True, text=True, timeout=600).stdout
    failed = 0
    for line, shape, cells in want:
        at = out.find(line + " ")
        got = out[at + len(line):].split("\n")[0].split() if at >= 0 else ["(not echoed)"]
        try:
            got_cells = [int(v, 16) & (1 << bits) - 1 for v in got[1:-1]] if got[-1:] == ["ok"] else None
        except ValueError:
            got_cells = None
        if got_cells != cells:
            failed += 1
            if failed <= 3:
                print(f"FAIL an image {shape[0]} by {shape[1]}, {shape[2]} to a pixel: {' '.join(got)[:300]}")
    print(f"the dither, cells of {bits} bits, seed {seed}: {len(want)} images, {failed} not as the "
          f"dither a pixel at a time makes them")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
