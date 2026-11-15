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
SCALE = int(os.environ.get("SCALE", "1"))   # the bridge draws each pixel this many across and down
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
        self.shots = self.inexact = 0       # screens taken, and those not in whole blocks
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
        self.shots += 1
        if SCALE > 1:
            # One pixel of each block is the display's; the screen is exact
            # if making blocks of those again gives the same screen.
            big, shot = shot, shot.resize((W, H), Image.NEAREST)
            if big.size != (W * SCALE, H * SCALE) or \
                    shot.resize(big.size, Image.NEAREST).tobytes() != big.tobytes():
                self.inexact += 1
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


def first_bold(shot, x0, y0, cols, rows):
    """(row, column) of the first bold character in a grid of cells, or None."""
    px = shot.load()
    for r in range(rows):
        for c in range(cols):
            glyph = bytes(
                sum((0 if px[x0 + c * 8 + b, y0 + r * 16 + y] else 1) << (7 - b) for b in range(8))
                for y in range(16))
            if any(glyph) and glyph not in FONT and glyph[:15] in BOLD:
                return r, c
    return None


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
        # Two words of the dither, which are next to each other in the list:
        # dither-row has a fused instruction in it, and dither-start a plain
        # word that is one instruction, and a variable that has no header.
        for _ in range(16):                             # page down to them
            names = read_text(m.screenshot(f"{out}/10-list.png", settle=1.0), 143, 315, 13, 16)
            if "dither-row" in names and "dither-start" in names:
                break
            m.key("pgdn")
        check("PgDn pages through the dictionary", "dither-row" in names and "dither-start" in names,
              repr(names[:4]))

        def listed(name):                               # click a name in the list
            m.click(143 + 8, 315 + names.index(name) * 16 + 8 if name in names else 345)
            m.move(756, 24)

        listed("dither-row")
        shot = m.screenshot(f"{out}/10-fused.png")
        source = read_text(shot, 253, 315, 45, 16)
        check("a fused instruction is shown as its words",
              source[0] == ": dither-row" and " abm @ 1 + @ * + " in source[1], repr(source[1]))

        # Words in bold are one instruction.  With the pointer on them, a box
        # by them says which: its number, and its fields.  The box is a line
        # down, or on the last two lines over them, and starts under the
        # words unless that would take it past the edge of the window, so
        # its text is looked for from there leftwards.
        where = first_bold(shot, 253, 315, 45, 16)
        told = ""
        if where:
            r, c = where
            m.move(253 + c * 8 + 4, 315 + r * 16 + 1)
            hover = m.screenshot(f"{out}/10-hover.png")
            y = 315 + ((r + 1) * 16 + 1 if r < 14 else r * 16 - 17)
            for x in range(254 + c * 8, 250, -1):
                told = read_text(hover, x, y, min(40, (W - x) // 8), 1)[0]
                if re.match(r"[0-9A-F]{4} \S*->IN \S+ ->\S+", told):
                    break
        check("the pointer on a fused instruction shows the machine's form of it",
              re.match(r"[0-9A-F]{4} \S*->IN \S+ ->\S+", told) is not None, repr((where, told)))
        # A word that is an instruction by itself says so too: rshift, on the
        # third line of dither-start.
        listed("dither-start")
        shot = m.screenshot(f"{out}/10-plain.png")
        source = read_text(shot, 253, 315, 45, 16)
        check("a headerless variable is shown by its label",
              source[0] == ": dither-start" and source[1] == "  abm !", repr(source[:2]))
        told = ""
        if "rshift" in source[2]:
            c = source[2].index("rshift")
            m.move(253 + c * 8 + 4, 315 + 2 * 16 + 1)
            hover = m.screenshot(f"{out}/10-hover-word.png")
            for x in range(254 + c * 8, 250, -1):
                told = read_text(hover, x, 315 + 3 * 16 + 1, min(40, (W - x) // 8), 1)[0]
                if told.startswith("60CC "):
                    break
        check("and on a word that is one instruction", told.startswith("60CC N->IN IN>>T ->T d-1"), repr(told))
        m.move(756, 24)
        again = m.screenshot(f"{out}/10-hover-off.png")
        check("and the box goes when the pointer does",
              read_text(again, 253, 315, 45, 16) == source, repr(read_text(again, 253, 315, 45, 16)[:3]))

        # ^W again: the Forth window.
        m.key("w", "ctrl")
        m.type("depth . 7 8\r")
        m.type("+ . depth .\r")
        shot = m.screenshot(f"{out}/11-cycled.png")
        text = read_text(shot, 15, 53, 50, 16)
        check("^W cycles windows and the Forth stack persists",
              text[8] == "0  ok" and text[10] == "15 0  ok", repr(text[7:11]))

        # Fill the Forth window so that it scrolls.  The text is moved on the
        # screen, not drawn again, so check what is there: the last lines,
        # the first, and that the only solid cell is the cursor.
        for i in range(8):
            m.type(f"{i} .\r")
        shot = m.screenshot(f"{out}/11-scrolled.png")
        text = read_text(shot, 15, 53, 50, 16)
        px = shot.load()
        solid = sum(1 for r in range(16) for c in range(50)
                    if not any(px[15 + c * 8 + i, 53 + r * 16 + j] for i in range(8) for j in range(16)))
        check("the Forth window scrolls", text[13:16] == ["7 .", "7  ok", ""] and text[0] == "0  ok"
              and solid == 1, repr((text[0], text[13:16], solid)))

        # The Forth window is a task of its own.  Start a word in it that
        # waits, taking turns, until another window comes to the front, and
        # then prints twenty lines.  Bring the viewer to the front: since it
        # was dragged it covers the right of the Forth window, which now
        # scrolls behind it.  The picture where it lies over the Forth
        # window must be untouched; then the Forth window is brought back.
        # (The word has no name, so that the Words window stays as it is.)
        m.type(":noname top begin pause dup top <> until drop 20 0 do i . cr loop ;\r")
        m.type("execute\r")
        m.click(600, 90)
        m.move(756, 24)
        shot = m.screenshot(f"{out}/11-behind.png")
        px = shot.load()
        bad = sum(1 for y in range(211) for x in range(127)
                  if bool(px[291 + x, 101 + y]) != bool(want[y][x]))
        text = read_text(shot, 15, 53, 34, 16)
        check("a window behind can print and scroll", bad == 0 and text[12:15] == ["18", "19", " ok"],
              f"{bad} pixels of the picture differ; {text[11:16]!r}")
        m.click(100, 40)
        shot = m.screenshot(f"{out}/11-task.png")
        text = read_text(shot, 15, 53, 50, 16)
        check("the Forth window runs as a task", text[12:15] == ["18", "19", " ok"], repr(text[11:16]))

        # Put the whole screen out of date from the prompt.  Every window is
        # painted again, each only where it can be seen, so nothing outside
        # the Forth window (12, 32, 406 by 280) may change.
        m.type("0 0 768 576 inval-rect\r")
        again = m.screenshot(f"{out}/12-repainted.png")
        a, b = shot.load(), again.load()
        moved = sum(1 for y in range(H) for x in range(W)
                    if not (12 <= x < 418 and 32 <= y < 312) and bool(a[x, y]) != bool(b[x, y]))
        check("a full repaint leaves the other windows as they were", moved == 0, f"{moved} pixels changed")

        # The tables of names for the decompiler are on the disk.  They are
        # read into memory, which says they were found and are this image's,
        # and the memory is given back.
        m.type("names-load names? . names-unload names? .\r")
        shot = m.screenshot(f"{out}/13-names.png")
        text = read_text(shot, 15, 53, 50, 16)
        check("the tables of names load from the disk and unload", text[14] == "-1 0  ok", repr(text[12:16]))

        # The picture was read and dithered by a task of its own, a row at a
        # turn, which took itself out of the ring when the last row was made.
        # Count the tasks round the ring: the desktop's and the Forth window's.
        m.type(":noname up @ 0 begin 1+ swap @ swap over up @ = until nip ;\r")
        m.type("execute .\r")
        shot = m.screenshot(f"{out}/14-tasks.png")
        text = read_text(shot, 15, 53, 50, 16)
        check("the picture's task has left the ring", text[14] == "2  ok", repr(text[12:16]))
        if SCALE > 1:
            check(f"every pixel is a block of {SCALE} by {SCALE}", m.inexact == 0,
                  f"{m.inexact} of {m.shots} screens were not exact")
    finally:
        m.stop()
    print(f"screenshots in {out}/")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
