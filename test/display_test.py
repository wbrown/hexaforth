#!/usr/bin/env python3
"""Check the drawing words in forth/display.fs against a model of them.

    display_test.py VM IMAGE.hex [SEED [COUNT]]
    OPS="scroll region rect text" display_test.py ...   only those operations

Draws the same random sequence of fills, text, copies and scrolls twice,
in ports with random origins, clip rectangles and clip regions:
on the reference VM, by typing Forth at its prompt, and here, a pixel at a
time.  The VM then prints its bitmap and the two are compared.  The display
is in the VM's memory, so nothing has to show it.
"""
import os
import random
import re
import struct
import subprocess
import sys

HERE = __file__.rsplit("/", 2)[0]
W, H, STRIDE = 768, 576, 12
PATTERNS = {"p-white": [0] * 8, "p-black": [0xFF] * 8, "p-gray": [0xAA, 0x55] * 4,
            "p-ltgray": [0x88, 0x22] * 4, "p-dkgray": [0x77, 0xDD] * 4}
MODES = {"copy-mode": lambda d, s: s, "or-mode": lambda d, s: d | s,
         "xor-mode": lambda d, s: d ^ s, "bic-mode": lambda d, s: d & ~s & 1}


sys.path.insert(0, f"{HERE}/test")
import fontfile                                     # noqa: E402

FONTS = fontfile.fonts()


class Model:
    def __init__(self):
        self.px = [[0] * W for _ in range(H)]       # 1 is black
        self.clip = (0, 0, W, H)
        self.ox = self.oy = 0                       # where the port's coordinates start
        self.pattern = PATTERNS["p-black"]
        self.mode = MODES["copy-mode"]
        self.style = {"inverse": 0, "bold": 0, "italic": 0}
        self.text_or = 0                            # text only adds its black
        self.font = "mono"
        self.region = None                          # rows of 0 and 1, or None

    def visible(self, x, y):
        x0, y0, x1, y1 = self.clip
        return x0 <= x < x1 and y0 <= y < y1 and (self.region is None or self.region[y][x])

    def set_clip(self, x, y, w, h):
        x, y = x + self.ox, y + self.oy
        self.clip = (max(x, 0), max(y, 0), min(x + w, W), min(y + h, H))

    def clip_to(self, x, y, w, h):
        x, y = x + self.ox, y + self.oy
        x0, y0, x1, y1 = self.clip
        self.clip = (max(x, x0), max(y, y0), min(x + w, x1), min(y + h, y1))

    def pat_rect(self, x, y, w, h):
        x, y = x + self.ox, y + self.oy
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                if self.visible(xx, yy):
                    src = self.pattern[yy & 7] >> (7 - (xx & 7)) & 1
                    self.px[yy][xx] = self.mode(self.px[yy][xx], src)

    def draw_text(self, text, x, y):
        pen, y = x + self.ox, y + self.oy
        for ch in text:
            code = ord(ch) - 32
            width, rows = FONTS[self.font][code if 0 <= code <= 94 else 0]
            # Not drawn: wholly off either side, or cut by the screen's left edge.
            if not (pen + width <= self.clip[0] or pen < 0 or pen >= self.clip[2]):
                for r in range(16):
                    bits = rows[r][:]
                    if self.style["italic"] and r < 8:
                        bits = [0] + bits[:-1]
                    if self.style["bold"]:
                        bits = [b | (bits[i - 1] if i else 0) for i, b in enumerate(bits)]
                    if self.style["inverse"]:
                        bits = [b ^ 1 for b in bits]
                    adds = self.text_or and not self.style["inverse"]
                    for k, b in enumerate(bits):
                        if self.visible(pen + k, y + r):
                            self.px[y + r][pen + k] = self.px[y + r][pen + k] | b if adds else b
            pen += width

    def copy_bits(self, sx, sy, dx, dy, w, h):
        sx, sy, dx, dy = sx + self.ox, sy + self.oy, dx + self.ox, dy + self.oy
        old = [row[:] for row in self.px]
        for j in range(h):
            for i in range(w):
                if self.visible(dx + i, dy + j):
                    self.px[dy + j][dx + i] = self.mode(self.px[dy + j][dx + i], old[sy + j][sx + i])

    def scroll_rect(self, x, y, w, h, dx, dy):
        # A pixel moves to a place if the place and where it comes from can
        # both be drawn in and are both in the rectangle; a place that can be
        # drawn in and has nothing to come to it is made white.
        x, y = x + self.ox, y + self.oy

        def movable(i, j):
            return x <= i < x + w and y <= j < y + h and self.visible(i, j)

        old = [row[:] for row in self.px]
        for j in range(max(y, 0), min(y + h, H)):
            for i in range(max(x, 0), min(x + w, W)):
                if movable(i, j):
                    self.px[j][i] = old[j - dy][i - dx] if movable(i - dx, j - dy) else 0
        self.mode = MODES["copy-mode"]
        self.pattern = PATTERNS["p-white"]


def session(seed, count):
    """The Forth to type, and the model after doing the same."""
    rng = random.Random(seed)
    only = os.environ.get("OPS", "").split()        # to try some operations harder
    m = Model()
    lines = ["display-init", ": tb here 64 + >c ;", ": ch tb + c! ;",
             "new-rgn constant ra", "new-rgn constant rb", "new-rgn constant rc",
             "new-rgn constant rd", "new-rgn constant re",
             ": dump hex 576 0 do i row 12 0 do dup i + @ . loop drop cr loop decimal ;"]
    for _ in range(count):
        op = rng.choice(only or ["rect"] * 4 + ["text"] * 4 + [
            "copy", "scroll", "clip", "noclip", "mode", "origin", "font", "region", "region",
            "noregion", "pattern", "style", "textor"])
        if op == "rect":
            x, y = rng.randint(-40, W), rng.randint(-40, H)
            w, h = rng.choice([1, 7, 64, 130, 300]), rng.choice([1, 3, 16, 60])
            lines.append(f"{x} {y} {w} {h} pat-rect")
            m.pat_rect(x, y, w, h)
        elif op == "text":
            text = "".join(chr(rng.randint(33, 126)) for _ in range(rng.choice([1, 2, 9, 20])))
            x, y = rng.randint(-30, W), rng.randint(-20, H)
            for i, ch in enumerate(text):
                lines.append(f"{ord(ch)} {i} ch")
            lines.append(f"tb {len(text)} {x} {y} draw-text")
            m.draw_text(text, x, y)
        elif op == "copy":
            w, h = rng.randint(1, 200), rng.randint(1, 80)
            sx, sy = rng.randint(0, W - w), rng.randint(0, H - h)
            dx, dy = rng.choice([(rng.randint(-50, W), rng.randint(-30, H)),
                                 (sx + rng.randint(-9, 9), sy + rng.randint(-3, 3))])
            sx, sy, dx, dy = sx - m.ox, sy - m.oy, dx - m.ox, dy - m.oy
            lines.append(f"screen {sx} {sy} {dx} {dy} {w} {h} copy-bits")
            m.copy_bits(sx, sy, dx, dy, w, h)
        elif op == "scroll":
            w, h = rng.randint(10, 300), rng.randint(10, 120)
            x, y = rng.randint(0, W - w), rng.randint(0, H - h)
            dx, dy = rng.choice([(0, -16), (0, 16), (8, 0), (-3, 0), (5, -7)])
            x, y = x - m.ox, y - m.oy
            lines.append(f"{x} {y} {w} {h} {dx} {dy} scroll-rect")
            m.scroll_rect(x, y, w, h, dx, dy)
        elif op == "clip":
            x, y = rng.randint(-20, W - 50), rng.randint(-20, H - 50)
            w, h = rng.randint(20, 500), rng.randint(20, 400)
            lines.append(f"{x} {y} {w} {h} clip!")
            m.set_clip(x, y, w, h)
        elif op == "origin":
            m.ox, m.oy = rng.choice([(0, 0), (rng.randint(-30, 300), rng.randint(-30, 300))])
            lines.append(f"{m.ox} {m.oy} origin!")
        elif op == "font":
            m.font = rng.choice(list(FONTS))
            lines.append(f"{m.font} font !")
        elif op == "region":
            # Two or three rectangles, combined; ra to re are regions.
            def rect():
                x, y = rng.randint(-30, W - 20), rng.randint(-30, H - 20)
                w, h = rng.randint(1, 500), rng.randint(1, 400)
                return x, y, w, h

            def inside(r):
                x, y, w, h = r
                return [[1 if x <= i < x + w and y <= j < y + h else 0 for i in range(W)]
                        for j in range(H)]

            def combine(a, b, how):
                f = {"and": lambda p, q: p & q, "or": lambda p, q: p | q,
                     "diff": lambda p, q: p & ~q & 1}[how]
                return [[f(p, q) for p, q in zip(u, v)] for u, v in zip(a, b)]

            a, b, how = rect(), rect(), rng.choice(["and", "or", "diff", "diff"])
            lines.append("%d %d %d %d ra rect-rgn" % a)
            lines.append("%d %d %d %d rb rect-rgn" % b)
            lines.append(f"ra rb rc rgn-{how}")
            m.region, name = combine(inside(a), inside(b), how), "rc"
            if rng.random() < 0.6:
                c, how = rect(), rng.choice(["and", "or", "diff"])
                lines.append("%d %d %d %d rd rect-rgn" % c)
                lines.append(f"rc rd re rgn-{how}")
                m.region, name = combine(m.region, inside(c), how), "re"
            lines.append(f"{name} rgn-clip!")
        elif op == "noregion":
            lines.append("0 rgn-clip!")
            m.region = None
        elif op == "noclip":
            lines.append("noclip")
            m.clip = (0, 0, W, H)
            m.region = None
        elif op == "textor":
            m.text_or = rng.randint(0, 1)
            lines.append(f"{-m.text_or} text-or !")
        elif op == "mode":
            name = rng.choice(list(MODES))
            lines.append(name)
            m.mode = MODES[name]
        elif op == "pattern":
            name = rng.choice(list(PATTERNS))
            lines.append(f"{name} pattern !")
            m.pattern = PATTERNS[name]
        else:
            name = rng.choice(list(m.style))
            m.style[name] ^= 1
            lines.append(f"{-m.style[name]} {name} !")
    lines += ["dump", "8000 execute"]
    return lines, m


def main():
    vm, image = sys.argv[1:3]
    seed = int(sys.argv[3]) if len(sys.argv) > 3 else 1
    count = int(sys.argv[4]) if len(sys.argv) > 4 else 300
    lines, m = session(seed, count)
    out = subprocess.run([vm, image], input="\n".join(lines) + "\n", capture_output=True,
                         text=True, errors="replace").stdout
    if "error:" in out:
        print("the VM reported an error:", out[out.index("error:") - 80:][:200])
        return 1
    rows = [l.split() for l in out[out.rindex("dump") + 4:].splitlines()]
    rows = [r for r in rows if len(r) == STRIDE][:H]
    if len(rows) != H:
        print(f"expected {H} rows of the bitmap, got {len(rows)}")
        return 1
    bad = 0
    for y, row in enumerate(rows):
        for i, word in enumerate(row):
            cell = int(word, 16) & (1 << 64) - 1
            for b in range(64):
                if (cell >> b & 1) != m.px[y][64 * i + b]:
                    bad += 1
                    if bad <= 5:
                        print(f"pixel {64 * i + b},{y} is {cell >> b & 1}, should be {m.px[y][64 * i + b]}")
    print(f"seed {seed}: {count} drawing operations, {bad} pixels differ")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
