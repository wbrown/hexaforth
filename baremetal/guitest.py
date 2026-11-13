#!/usr/bin/env python3
"""Drive the hexaforth desktop in QEMU and check what is on the screen.

    guitest.py ELF DISK.img TIFF OUTDIR

Boots with a display, disk, keyboard and tablet, sends key and pointer
events through QMP, and takes screenshots.  Text is read back from a
screenshot by matching 8x16 cells against the font, and the image viewer's
content is compared with a dither of the same TIFF computed here.
"""
import json
import os
import re
import socket
import struct
import subprocess
import sys
import tempfile
import time

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
W, H = 768, 576
QCODE = {" ": "spc", "\r": "ret", ".": "dot", ",": "comma", "/": "slash", "-": "minus",
         "=": "equal", ";": "semicolon", "'": "apostrophe", "[": "bracket_left",
         "]": "bracket_right", "\\": "backslash", "`": "grave_accent"}
SHIFTED = dict(zip('!@#$%^&*()_+:"<>?{}|~', "1234567890-=;',./[]\\`"))


sys.path.insert(0, f"{HERE}/../test")
import fontfile                                     # noqa: E402

FONTS = fontfile.fonts()
# The fixed-width font, as the 16 bytes of each glyph: the windows that hold
# text on a grid are read back a character cell at a time.
FONT = {bytes(sum(b << (7 - i) for i, b in enumerate(row)) for row in rows): chr(32 + n)
        for n, (width, rows) in enumerate(FONTS["mono"])}
UNDERLINED = {glyph[:15]: c for glyph, c in FONT.items()}   # the bottom row is the underline
BOLD = {bytes(r | r >> 1 for r in glyph[:15]): c for glyph, c in FONT.items()}
ITALIC = {bytes(r >> 1 for r in glyph[:8]) + glyph[8:15]: c for glyph, c in FONT.items()}


def dither(tiff_path):
    """Atkinson dithering exactly as atkinson.fs does it: rows of 0/1."""
    im = Image.open(tiff_path).convert("RGB")
    w, h = im.size
    px = im.tobytes()
    gray = [(77 * px[i] + 150 * px[i + 1] + 29 * px[i + 2]) >> 8 for i in range(0, len(px), 3)]
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


class Machine:
    def __init__(self, elf, disk):
        self.tmp = tempfile.mkdtemp()
        sock = f"{self.tmp}/qmp"
        self.qemu = subprocess.Popen(
            ["qemu-system-aarch64", "-M", "virt", "-cpu", "cortex-a53", "-m", "256",
             "-display", "none", "-serial", "stdio", "-nic", "none",
             "-qmp", f"unix:{sock},server,nowait",
             "-device", "ramfb",
             "-global", "virtio-mmio.force-legacy=false",
             "-drive", f"file={disk},if=none,format=raw,id=hd0",
             "-device", "virtio-blk-device,drive=hd0",
             "-device", "virtio-keyboard-device",
             "-device", "virtio-tablet-device",
             "-kernel", elf],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, bufsize=0)
        os.set_blocking(self.qemu.stdout.fileno(), False)
        for _ in range(100):
            if os.path.exists(sock):
                break
            time.sleep(0.05)
        self.qmp = socket.socket(socket.AF_UNIX)
        self.qmp.connect(sock)
        self.file = self.qmp.makefile("rw")
        self.file.readline()
        self.command("qmp_capabilities")

    def command(self, name, **arguments):
        self.file.write(json.dumps({"execute": name, "arguments": arguments}) + "\n")
        self.file.flush()
        while True:
            reply = json.loads(self.file.readline())
            if "return" in reply or "error" in reply:
                return reply

    def serial(self, until, timeout=120):
        seen, start = b"", time.time()
        while until not in seen and time.time() - start < timeout:
            chunk = self.qemu.stdout.read()
            if chunk:
                seen += chunk
            else:
                time.sleep(0.02)
        return seen.decode(errors="replace")

    def events(self, *events):
        self.command("input-send-event", events=list(events))
        time.sleep(0.05)

    def key(self, qcode, *modifiers):
        keys = list(modifiers) + [qcode]
        for k in keys:
            self.events({"type": "key", "data": {"down": True, "key": {"type": "qcode", "data": k}}})
        for k in reversed(keys):
            self.events({"type": "key", "data": {"down": False, "key": {"type": "qcode", "data": k}}})

    def type(self, text):
        for ch in text:
            if ch in SHIFTED:
                base = SHIFTED[ch]
                self.key(QCODE.get(base, base), "shift")
            elif ch.isupper():
                self.key(ch.lower(), "shift")
            else:
                self.key(QCODE.get(ch, ch))

    def move(self, x, y):
        self.events({"type": "abs", "data": {"axis": "x", "value": x * 0x8000 // W + 1}},
                    {"type": "abs", "data": {"axis": "y", "value": y * 0x8000 // H + 1}})

    def button(self, down):
        self.events({"type": "btn", "data": {"down": down, "button": "left"}})

    def click(self, x, y):
        self.move(x, y)
        self.button(True)
        self.button(False)
        time.sleep(1.2)             # a click may repaint the whole screen

    def drag(self, x0, y0, x1, y1):
        self.move(x0, y0)
        self.button(True)
        self.move(x1, y1)
        self.button(False)
        time.sleep(1.2)

    def screenshot(self, path, settle=1.5):
        time.sleep(settle)
        ppm = f"{self.tmp}/screen.ppm"
        self.command("screendump", filename=ppm)
        time.sleep(0.3)
        shot = Image.open(ppm).convert("L").point(lambda v: 255 if v else 0)
        shot.save(path)
        return shot

    def stop(self):
        self.qemu.kill()


def read_text(shot, x0, y0, cols, rows):
    """The characters in a grid of 8x16 cells; '?' where nothing matches."""
    px = shot.load()
    lines = []
    for r in range(rows):
        line = ""
        for c in range(cols):
            glyph = bytes(
                sum((0 if px[x0 + c * 8 + b, y0 + r * 16 + y] else 1) << (7 - b) for b in range(8))
                for y in range(16))
            inverse = bytes(v ^ 0xFF for v in glyph)
            line += (FONT.get(glyph) or FONT.get(inverse)
                     or UNDERLINED.get(glyph[:15])           # a link is underlined
                     or BOLD.get(glyph[:15])                 # a fused word is bold
                     or ITALIC.get(glyph[:15]) or "?")       # and a label leans
        lines.append(line.rstrip())
    return lines


def shows(shot, text, x, y, font="sans", inverse=False):
    """Is this text on the screen at x, y?  For text that is not on a grid."""
    px = shot.load()
    for ch in text:
        width, rows = FONTS[font][ord(ch) - 32]
        for r, row in enumerate(rows):
            for k, ink in enumerate(row):
                if bool(px[x + k, y + r]) == bool(ink ^ inverse):   # a white pixel where ink should be
                    return False
        x += width
    return True


def main():
    elf, disk, tiff, out = sys.argv[1:5]
    os.makedirs(out, exist_ok=True)
    failures = []

    def check(name, ok, detail=""):
        print(f"{'ok  ' if ok else 'FAIL'} {name}{': ' + detail if detail else ''}")
        if not ok:
            failures.append(name)

    m = Machine(elf, disk)
    try:
        boot = m.serial(b"desktop")
        check("desktop comes up", "desktop" in boot)
        shot = m.screenshot(f"{out}/1-desktop.png")

        # Forth window: content starts at 12+1, 32+19; text is inset by 2.
        m.type("2 3 + .\r")
        m.type(": sq dup * ; 12 sq .\r")
        m.type("nosuch\r")
        shot = m.screenshot(f"{out}/2-forth.png")
        text = read_text(shot, 15, 53, 50, 16)
        check("Forth window opens with a prompt", text[0] == "ok", repr(text[0]))
        check("Forth window evaluates", text[1] == "2 3 + ." and text[2] == "5  ok", repr(text[1:3]))
        check("Forth window compiles", text[4] == "144  ok", repr(text[3:5]))
        check("Forth window reports errors", text[6].startswith("? -13"), repr(text[5:7]))

        # Calculator: click its title to bring it forward, then use keys and buttons.
        m.click(700, 308)
        m.type("12\r34+")
        for bx, by in ((0, 0), (3, 1)):                 # buttons "7" and "*"
            m.click(627 + 34 * bx + 17, 319 + 96 + 24 * by + 12)
        shot = m.screenshot(f"{out}/3-calculator.png")
        text = read_text(shot, 633, 323, 14, 5)
        check("calculator computes (12+34)*7", "322" in text[3], repr(text))

        # Editor: bring it forward, type on the first line of block 1, save.
        m.click(480, 278)
        m.key("s", "ctrl")
        shot = m.screenshot(f"{out}/4-editor.png")
        text = read_text(shot, 33, 291, 64, 16)
        check("editor shows block 1", text[0].startswith(": hello 104 emit"), repr(text[0]))
        m.key("down")
        m.type("( edited )")
        m.key("s", "ctrl")
        shot = m.screenshot(f"{out}/5-edited.png")
        time.sleep(0.5)
        block = open(disk, "rb").read()[1024:2048]
        check("editor saves to disk", block[64:74] == b"( edited )", repr(block[64:80]))

        # Image viewer: bring it forward and compare with the reference dither.
        m.click(600, 48)
        m.move(756, 24)                                 # pointer off the image
        shot = m.screenshot(f"{out}/6-viewer.png")
        want = dither(tiff)
        px = shot.load()
        bad = sum(1 for y in range(512) for x in range(512)
                  if bool(px[251 + x, 59 + y]) != bool(want[y][x]))
        check("viewer matches the reference dither", bad == 0, f"{bad} pixels differ")

        # Drag the viewer by its title bar: 40 right, 42 down.
        m.drag(500, 48, 540, 90)
        m.move(756, 24)
        shot = m.screenshot(f"{out}/7-dragged.png")
        check("window moves when dragged", shows(shot, "Mandrill", 298, 84, inverse=True))

        # ^W brings the back window to the front: after the clicks above
        # that is the Words window.  Its content starts at 140+1, 296+19;
        # the definition is 112 further in.  sq, defined in the Forth
        # window above, is the newest word and so the first listed.
        m.key("w", "ctrl")
        m.click(143 + 8, 315 + 8)                       # the name "sq"
        m.move(756, 24)
        shot = m.screenshot(f"{out}/8-words.png")
        names = read_text(shot, 143, 315, 13, 16)
        source = read_text(shot, 253, 315, 46, 16)
        check("Words window lists the dictionary", names[:3] == ["sq", "cold", "desktop"], repr(names[:3]))
        check("Words window decompiles", source[:3] == [": sq", "  dup *", ";"], repr(source[:3]))
        m.click(253 + 2 * 8 + 4, 315 + 16 + 8)         # the link "dup", on the second line
        m.move(756, 24)
        shot = m.screenshot(f"{out}/9-link.png")
        source = read_text(shot, 253, 315, 46, 16)
        check("a word in a definition is a link", source[:3] == [": dup", "  dup", ";"], repr(source[:3]))
        m.key("backspace")
        shot = m.screenshot(f"{out}/10-back.png")
        source = read_text(shot, 253, 315, 46, 16)
        check("Backspace goes back", source[:3] == [": sq", "  dup *", ";"], repr(source[:3]))
        m.key("down")
        shot = m.screenshot(f"{out}/10-down.png")
        source = read_text(shot, 253, 315, 46, 16)
        check("Down moves to the next word", source[0] == ": cold" and source[1].endswith(" catch drop")
              and source[2] == ";", repr(source[:3]))
        for _ in range(16):                             # page down to "atkinson"
            names = read_text(m.screenshot(f"{out}/10-list.png", settle=1.0), 143, 315, 13, 16)
            if "atkinson" in names:
                break
            m.key("pgdn")
        check("PgDn pages through the dictionary", "atkinson" in names, repr(names[:4]))
        m.click(143 + 8, 315 + names.index("atkinson") * 16 + 8 if "atkinson" in names else 345)
        m.move(756, 24)
        shot = m.screenshot(f"{out}/10-fused.png")
        source = read_text(shot, 253, 315, 45, 16)
        check("a fused instruction is shown as its words",
              source[0] == ": atkinson" and source[8].endswith(" @ < while"), repr(source[8]))
        check("a headerless variable is shown by its label", source[1] == "  aoy !", repr(source[1]))

        # ^W again: the Forth window.
        m.key("w", "ctrl")
        m.type("depth . 7 8\r")
        m.type("+ . depth .\r")
        shot = m.screenshot(f"{out}/11-cycled.png")
        text = read_text(shot, 15, 53, 50, 16)
        check("^W cycles windows and the Forth stack persists",
              text[8] == "0  ok" and text[10] == "15 0  ok", repr(text[7:11]))

        # Put the whole screen out of date from the prompt.  Every window is
        # painted again, each only where it can be seen, so nothing outside
        # the Forth window (12, 32, 406 by 280) may change.
        m.type("0 0 768 576 inval-rect\r")
        again = m.screenshot(f"{out}/12-repainted.png")
        a, b = shot.load(), again.load()
        moved = sum(1 for y in range(H) for x in range(W)
                    if not (12 <= x < 418 and 32 <= y < 312) and bool(a[x, y]) != bool(b[x, y]))
        check("a full repaint leaves the other windows as they were", moved == 0, f"{moved} pixels changed")
    finally:
        m.stop()
    print(f"screenshots in {out}/")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
