#!/usr/bin/env python3
"""Check the drawing words in forth/display.fs against a model of them.

    display_test.py VM IMAGE.hex [SEED [COUNT]]

Draws the same random sequence of fills, text, copies and scrolls twice:
on the reference VM, by typing Forth at its prompt, and here, a pixel at a
time.  The VM then prints its bitmap and the two are compared.  The display
is in the VM's memory, so nothing has to show it.
"""
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


def load_font():
    text = open(f"{HERE}/forth/font.fs").read()
    data = b"".join(struct.pack("<Q", int(v, 16)) for v in re.findall(r"\$([0-9a-f]+) glyph,", text))
    return [data[i * 16:i * 16 + 16] for i in range(95)]


FONT = load_font()


class Model:
    def __init__(self):
        self.px = [[0] * W for _ in range(H)]       # 1 is black
        self.clip = (0, 0, W, H)
        self.pattern = PATTERNS["p-black"]
        self.mode = MODES["copy-mode"]
        self.style = {"inverse": 0, "bold": 0, "italic": 0}

    def visible(self, x, y):
        x0, y0, x1, y1 = self.clip
        return x0 <= x < x1 and y0 <= y < y1

    def set_clip(self, x, y, w, h):
        self.clip = (max(x, 0), max(y, 0), min(x + w, W), min(y + h, H))

    def clip_to(self, x, y, w, h):
        x0, y0, x1, y1 = self.clip
        self.clip = (max(x, x0), max(y, y0), min(x + w, x1), min(y + h, y1))

    def pat_rect(self, x, y, w, h):
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                if self.visible(xx, yy):
                    src = self.pattern[yy & 7] >> (7 - (xx & 7)) & 1
                    self.px[yy][xx] = self.mode(self.px[yy][xx], src)

    def draw_text(self, text, x, y):
        for i, ch in enumerate(text):
            cx = x + 8 * i
            if cx < 0:                              # cut by the screen's edge: not drawn
                continue
            code = ord(ch) - 32
            glyph = FONT[code if 0 <= code <= 94 else 0]
            for r in range(16):
                b = glyph[r]
                if self.style["italic"] and r < 8:
                    b >>= 1
                if self.style["bold"]:
                    b |= b >> 1
                if self.style["inverse"]:
                    b ^= 0xFF
                for k in range(8):
                    if self.visible(cx + k, y + r):
                        self.px[y + r][cx + k] = b >> (7 - k) & 1

    def copy_bits(self, sx, sy, dx, dy, w, h):
        old = [row[:] for row in self.px]
        for j in range(h):
            for i in range(w):
                if self.visible(dx + i, dy + j):
                    self.px[dy + j][dx + i] = self.mode(self.px[dy + j][dx + i], old[sy + j][sx + i])

    def scroll_rect(self, x, y, w, h, dx, dy):
        saved = self.clip
        self.clip_to(x, y, w, h)
        self.mode = MODES["copy-mode"]
        self.copy_bits(x, y, x + dx, y + dy, w, h)
        self.pattern = PATTERNS["p-white"]
        if dy > 0:
            self.pat_rect(x, y, w, dy)
        if dy < 0:
            self.pat_rect(x, y + h + dy, w, -dy)
        if dx > 0:
            self.pat_rect(x, y, dx, h)
        if dx < 0:
            self.pat_rect(x + w + dx, y, -dx, h)
        self.clip = saved


def session(seed, count):
    """The Forth to type, and the model after doing the same."""
    rng = random.Random(seed)
    m = Model()
    lines = ["display-init", ": tb here 64 + >c ;", ": ch tb + c! ;",
             ": dump hex 576 0 do i row 12 0 do dup i + @ . loop drop cr loop decimal ;"]
    for _ in range(count):
        op = rng.choice(["rect"] * 4 + ["text"] * 4 + ["copy", "scroll", "clip", "noclip", "mode",
                                                      "pattern", "style"])
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
            lines.append(f"screen {sx} {sy} {dx} {dy} {w} {h} copy-bits")
            m.copy_bits(sx, sy, dx, dy, w, h)
        elif op == "scroll":
            w, h = rng.randint(10, 300), rng.randint(10, 120)
            x, y = rng.randint(0, W - w), rng.randint(0, H - h)
            dx, dy = rng.choice([(0, -16), (0, 16), (8, 0), (-3, 0), (5, -7)])
            lines.append(f"{x} {y} {w} {h} {dx} {dy} scroll-rect")
            m.scroll_rect(x, y, w, h, dx, dy)
        elif op == "clip":
            x, y = rng.randint(-20, W - 50), rng.randint(-20, H - 50)
            w, h = rng.randint(20, 500), rng.randint(20, 400)
            lines.append(f"{x} {y} {w} {h} clip!")
            m.set_clip(x, y, w, h)
        elif op == "noclip":
            lines.append("noclip")
            m.set_clip(0, 0, W, H)
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
