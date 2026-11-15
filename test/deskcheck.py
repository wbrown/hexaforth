"""What the desktop is checked for, whatever machine it is running on.

The checks are one script: type, click, drag, and look at the screen.  The
machine they are run on is given as an object that can do those things:
baremetal/guitest.py has one that drives QEMU, and test/desktop_test.py
one that drives the reference VM, with cells of any size.  Text is read
back from a screen by matching cells of 8 by 16 pixels against the font,
and the picture is compared with a dither done on the host.

A machine has:
    W, H                    the screen
    cell_pixels             how many pixels of it a cell of the display holds
    boot()                  what the console said as the desktop came up
    type(text)  key(name, *modifiers)  move(x, y)  click(x, y)  drag(x0, y0, x1, y1)
    screenshot(path)        the screen, as an image: a pixel is true if white
    block(n)                the bytes of a block of the disk, as it now is
    picture()               the picture on the disk: its grays, a row after a row,
                            and its width and height
"""
import re

import fontfile

FONTS = fontfile.fonts()
# The fixed-width font, as the 16 bytes of each glyph: the windows that hold
# text on a grid are read back a character cell at a time.
FONT = {bytes(sum(b << (7 - i) for i, b in enumerate(row)) for row in rows): chr(32 + n)
        for n, (width, rows) in enumerate(FONTS["mono"])}
UNDERLINED = {glyph[:15]: c for glyph, c in FONT.items()}   # the bottom row is the underline
BOLD = {bytes(r | r >> 1 for r in glyph[:15]): c for glyph, c in FONT.items()}
ITALIC = {bytes(r >> 1 for r in glyph[:8]) + glyph[8:15]: c for glyph, c in FONT.items()}


def atkinson(gray, w, h):
    """Atkinson dithering exactly as atkinson.fs does it: rows of 0/1, 1 for white."""
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


def cell_glyph(px, x, y, size):
    """The 16 bytes of the character cell at x, y; nothing where it is off the screen."""
    w, h = size
    return bytes(
        sum((0 if 0 <= x + b < w and 0 <= y + r < h and px[x + b, y + r] else 1) << (7 - b) for b in range(8))
        for r in range(16))


def read_text(shot, x0, y0, cols, rows):
    """The characters in a grid of 8x16 cells; '?' where nothing matches."""
    px = shot.load()
    lines = []
    for r in range(rows):
        line = ""
        for c in range(cols):
            glyph = cell_glyph(px, x0 + c * 8, y0 + r * 16, shot.size)
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
            glyph = cell_glyph(px, x0 + c * 8, y0 + r * 16, shot.size)
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


TITLES = {"viewer": "Mandrill", "editor": "Editor", "words": "Words", "calc": "Calculator", "forth": "Forth",
          "paint": "HexaPaint"}


class Canvas:
    """HexaPaint's canvas, drawn in as paint.fs draws in it: pixels of 1 for black."""

    PATTERNS = [[0x00] * 8, [0xFF] * 8, [0x77, 0xDD] * 4, [0xAA, 0x55] * 4, [0x88, 0x22] * 4,
                [0xFF, 0x80, 0x80, 0x80, 0xFF, 0x08, 0x08, 0x08], [0x88, 0x44, 0x22, 0x11] * 2,
                [0x00, 0x22, 0x00, 0x88] * 2]
    WHITE, BLACK = PATTERNS[0], PATTERNS[1]

    def __init__(self, w, h):
        self.w, self.h = w, h
        self.px = [[0] * w for _ in range(h)]
        self.fill, self.pen = self.BLACK, 1

    def span(self, x0, x1, y):
        """The pixels of row y from x0 up to x1, in the pattern."""
        if 0 <= y < self.h:
            for x in range(max(x0, 0), min(x1, self.w)):
                self.px[y][x] = self.fill[y & 7] >> (7 - (x & 7)) & 1

    def dot(self, x, y):
        for row in range(y, y + self.pen):
            self.span(x, x + self.pen, row)

    def line(self, x0, y0, x1, y1):
        dx, sx = abs(x1 - x0), (-1 if x1 < x0 else 1)
        dy, sy = -abs(y1 - y0), (-1 if y1 < y0 else 1)
        err, x, y = dx + dy, x0, y0
        while True:
            self.dot(x, y)
            if (x, y) == (x1, y1):
                return
            e2 = 2 * err
            if e2 >= dy:
                err += dy
                x += sx
            if e2 <= dx:
                err += dx
                y += sy

    def box(self, a, b):
        return min(a[0], b[0]), min(a[1], b[1]), max(a[0], b[0]), max(a[1], b[1])

    def rect_frame(self, a, b):
        x0, y0, x1, y1 = self.box(a, b)
        self.span(x0, x1 + 1, y0)
        self.span(x0, x1 + 1, y1)
        for y in range(y0 + 1, y1):
            self.span(x0, x0 + 1, y)
            self.span(x1, x1 + 1, y)

    def rect_fill(self, a, b):
        x0, y0, x1, y1 = self.box(a, b)
        for y in range(y0, y1 + 1):
            self.span(x0, x1 + 1, y)

    def oval(self, a, b):
        """Half its width and height, its middle, and how far it reaches at each row from the middle."""
        x0, y0, x1, y1 = self.box(a, b)
        ha, hb = (x1 - x0) >> 1, (y1 - y0) >> 1
        reach, s = [], hb
        for dy in range(hb + 1):
            if hb == 0:
                reach.append(ha)
                continue
            while s * s > hb * hb - dy * dy:
                s -= 1
            reach.append(ha * s // hb)
        return x0 + ha, y0 + hb, hb, reach

    def oval_fill(self, a, b):
        cx, cy, hb, reach = self.oval(a, b)
        for dy in range(hb + 1):
            for y in (cy - dy, cy + dy):
                self.span(cx - reach[dy], cx + reach[dy] + 1, y)

    def oval_frame(self, a, b):
        cx, cy, hb, reach = self.oval(a, b)
        for dy in range(hb + 1):
            out = reach[dy]
            inn = min(out, reach[dy + 1] + 1) if dy < hb else 0
            for y in (cy - dy, cy + dy):
                self.span(cx - out, cx - inn + 1, y)
                self.span(cx + inn, cx + out + 1, y)

    def outline(self, tool, a, b):
        self.fill, self.pen = self.BLACK, 1
        if tool == 3:
            self.line(*a, *b)
        elif tool < 6:
            self.rect_frame(a, b)
        else:
            self.oval_frame(a, b)

    def draw(self, tool, pattern, a, b):
        """What a tool does, pulled from a to b with a pattern in hand."""
        if tool < 3:                                # pencil, brush, eraser: a line of dots
            self.fill, self.pen = [(self.BLACK, 1), (self.PATTERNS[pattern], 4), (self.WHITE, 12)][tool]
            self.dot(*a)
            self.line(*a, *b)
            return
        self.fill = self.PATTERNS[pattern]
        if tool == 5:
            self.rect_fill(a, b)
        if tool == 7:
            self.oval_fill(a, b)
        self.outline(tool, a, b)


CANVAS_BLOCK = 1152     # where HexaPaint keeps its canvas on the disk (CANVAS-BLK in paint.fs)


def canvas_on_disk(m, model):
    """HexaPaint's canvas as the disk has it, rows of 0 and 1, or None if the disk does not say it has one.
    A block says it is there: a mark, its size, and what one pixel is in a cell.  Then its rows, as
    many whole rows to a block as a block holds; a cell's lowest bit is its leftmost pixel, and set is black."""
    bits = m.cell_pixels
    size = bits // 8
    stride = (model.w + bits - 1) // bits
    per = 1024 // size // stride                    # rows to a block
    blocks = (model.h + per - 1) // per
    data = m.block(CANVAS_BLOCK, 1 + blocks)

    def cells(n):
        return [int.from_bytes(data[at:at + size], "little") for at in range(1024 * n, 1024 * (n + 1), size)]

    if cells(0)[:4] != [0x4850, model.w, model.h, 1]:
        return None
    rows = []
    for b in range(blocks):
        block = cells(1 + b)
        for r in range(min(per, model.h - b * per)):
            row = block[r * stride:(r + 1) * stride]
            rows.append([row[x // bits] >> x % bits & 1 for x in range(model.w)])
    return rows


class Desk:
    """Where the windows are, from back to front, as the checks move them."""

    def __init__(self, W, H, windows, newest, names_loaded):
        self.W, self.H = W, H
        self.rect = dict(windows)                   # name: (x, y, w, h)
        self.order = [name for name, _ in windows]  # back to front
        self.newest = newest        # the two newest words of the dictionary, and how the first ends
        self.names_loaded = names_loaded            # whether the disk has the tables of names

    def content(self, name):
        x, y, w, h = self.rect[name]
        return x + 1, y + 19

    def front(self, name):
        self.order.remove(name)
        self.order.append(name)

    def cycle(self):                                # ^W: the one at the back comes to the front
        self.front(self.order[0])

    def title(self, name, dx=0, dy=0):
        """A point of the window's title bar that no window in front covers,
        from which the pointer can go dx and dy further and still be on the screen."""
        x, y, w, h = self.rect[name]
        ahead = self.order[self.order.index(name) + 1:]
        for px in range(x + w - 12, x + 4, -4):
            py = y + 8
            if not (0 <= px < self.W - 8 - dx and 20 <= py < self.H - 12 - dy):
                continue
            if not any(ax <= px < ax + aw and ay <= py < ay + ah
                       for ax, ay, aw, ah in (self.rect[a] for a in ahead)):
                return px, py
        raise AssertionError(f"no part of the title of {name} can be seen")

    def moved(self, name, dx, dy):
        x, y, w, h = self.rect[name]
        self.rect[name] = (x + dx, y + dy, w, h)


def run(m, d, out, scale_note=None):
    """Run every check on machine m, whose desktop is laid out as d.  The names of those that failed."""
    failures = []
    W, H = d.W, d.H
    away = (W - 12, 24)                             # where the pointer is put to be off everything

    def check(name, ok, detail=""):
        print(f"{'ok  ' if ok else 'FAIL'} {name}{': ' + detail if detail else ''}")
        if not ok:
            failures.append(name)

    def raise_(name):                               # click its title: it comes to the front
        m.click(*d.title(name))
        d.front(name)

    def cycle_to(name):                             # ^W, which brings the one at the back, until it is this one
        for _ in d.order:
            if d.order[-1] == name:
                return
            m.key("w", "ctrl")
            d.cycle()
        raise AssertionError(f"^W did not bring {name} to the front: {d.order}")

    def forth_text(shot, cols=50):
        fx, fy = d.content("forth")
        return read_text(shot, fx + 2, fy + 2, cols, 16)

    boot = m.boot()
    check("desktop comes up", "desktop" in boot)
    shot = m.screenshot(f"{out}/1-desktop.png")

    # Forth window: text is inset by 2 from its content.
    m.type("2 3 + .\r")
    m.type(": sq dup * ; 12 sq .\r")
    m.type("nosuch\r")
    shot = m.screenshot(f"{out}/2-forth.png")
    text = forth_text(shot)
    check("Forth window opens with a prompt", text[0] == "ok", repr(text[0]))
    check("Forth window evaluates", text[1] == "2 3 + ." and text[2] == "5  ok", repr(text[1:3]))
    check("Forth window compiles", text[4] == "144  ok", repr(text[3:5]))
    check("Forth window reports errors", text[6].startswith("? -13"), repr(text[5:7]))

    # Calculator: click its title to bring it forward, then use keys and buttons.
    raise_("calc")
    cx, cy = d.content("calc")
    m.type("12\r34+")
    for bx, by in ((0, 0), (3, 1)):                 # buttons "7" and "*"
        m.click(cx + 34 * bx + 17, cy + 96 + 24 * by + 12)
    shot = m.screenshot(f"{out}/3-calculator.png")
    text = read_text(shot, cx + 6, cy + 4, 14, 5)
    check("calculator computes (12+34)*7", "322" in text[3], repr(text))

    # Editor: bring it forward, type on the first line of block 1, save.
    raise_("editor")
    ex, ey = d.content("editor")
    m.key("s", "ctrl")
    shot = m.screenshot(f"{out}/4-editor.png")
    text = read_text(shot, ex + 2, ey + 2, 64, 16)
    check("editor shows block 1", text[0].startswith(": hello 104 emit"), repr(text[0]))
    m.key("down")
    m.type("( edited )")
    m.key("s", "ctrl")
    shot = m.screenshot(f"{out}/5-edited.png")
    block = m.block(1)
    check("editor saves to disk", block[64:74] == b"( edited )", repr(block[64:80]))

    # Image viewer: bring it forward and compare with the reference dither.
    raise_("viewer")
    m.move(*away)
    shot = m.screenshot(f"{out}/6-viewer.png")
    # A window smaller than its picture shows it at half its size, each
    # pixel the gray of four, to the nearest.  If that is still more than
    # the window shows, it shows the middle: the rows in the middle, and
    # across from the middle or the start of the cell of the display that
    # is in.
    gray, pw, ph = m.picture()
    _, _, vw, vh = d.rect["viewer"]
    if pw > vw - 2 or ph > vh - 20:
        gray = [(gray[2 * y * pw + 2 * x] + gray[2 * y * pw + 2 * x + 1]
                 + gray[(2 * y + 1) * pw + 2 * x] + gray[(2 * y + 1) * pw + 2 * x + 1] + 2) >> 2
                for y in range(ph // 2) for x in range(pw // 2)]
        pw, ph = pw // 2, ph // 2
    want = atkinson(gray, pw, ph)
    kw, kh = min(vw - 2, pw), min(vh - 20, ph)
    ox, oy = (pw - kw) // 2 // m.cell_pixels * m.cell_pixels, (ph - kh) // 2

    def picture_wrong(shot, x0, y0, x1, y1):
        """Pixels of the picture, between two corners of the screen, that are not as dithered."""
        vx, vy = d.content("viewer")
        px = shot.load()
        return sum(1 for y in range(max(y0, vy), min(y1, vy + kh, H))
                   for x in range(max(x0, vx), min(x1, vx + kw, W))
                   if bool(px[x, y]) != bool(want[y - vy + oy][x - vx + ox]))

    bad = picture_wrong(shot, 0, 0, W, H)
    check("viewer matches the reference dither", bad == 0,
          f"{bad} pixels differ, of the {kw} by {kh} from {ox}, {oy} of the picture as {pw} by {ph}")

    # Drag the viewer by its title bar: 40 right, 42 down.
    tx, ty = d.title("viewer", 40, 42)
    m.drag(tx, ty, tx + 40, ty + 42)
    d.moved("viewer", 40, 42)
    m.move(*away)
    shot = m.screenshot(f"{out}/7-dragged.png")
    vx, vy, _, _ = d.rect["viewer"]
    check("window moves when dragged", shows(shot, "Mandrill", vx + 8, vy + 2, inverse=True))

    # ^W brings the back window to the front: after the clicks above
    # that is the Words window.  The definition is 112 further in than
    # the list.  sq, defined in the Forth window above, is the newest
    # word and so the first listed.
    cycle_to("words")
    wx, wy = d.content("words")
    lx, sx = wx + 2, wx + 112                       # the list, and the definition

    def source_of(shot, cols=46):
        return read_text(shot, sx, wy, cols, 16)

    m.click(lx + 8, wy + 8)                         # the name "sq"
    m.move(*away)
    shot = m.screenshot(f"{out}/8-words.png")
    names = read_text(shot, lx, wy, 13, 16)
    source = source_of(shot)
    second, third, ending = d.newest
    check("Words window lists the dictionary", names[:3] == ["sq", second, third], repr(names[:3]))
    check("Words window decompiles", source[:3] == [": sq", "  dup *", ";"], repr(source[:3]))
    m.click(sx + 2 * 8 + 4, wy + 16 + 8)            # the link "dup", on the second line
    m.move(*away)
    shot = m.screenshot(f"{out}/9-link.png")
    source = source_of(shot)
    check("a word in a definition is a link", source[:3] == [": dup", "  dup", ";"], repr(source[:3]))
    m.key("backspace")
    shot = m.screenshot(f"{out}/10-back.png")
    source = source_of(shot)
    check("Backspace goes back", source[:3] == [": sq", "  dup *", ";"], repr(source[:3]))
    m.key("down")
    shot = m.screenshot(f"{out}/10-down.png")
    source = source_of(shot)
    check("Down moves to the next word", source[0] == ": " + second and source[1].endswith(ending)
          and source[2] == ";", repr(source[:3]))
    # Two words of the dither, which are next to each other in the list:
    # dither-row has fused instructions in it, and dither-start a variable
    # that has no header and a plain word that is one instruction.
    wanted = ("dither-row", "dither-start")
    for _ in range(16):                             # page down to them
        names = read_text(m.screenshot(f"{out}/10-list.png", settle=1.0), lx, wy, 13, 16)
        if all(name in names for name in wanted):
            break
        m.key("pgdn")
    check("PgDn pages through the dictionary", all(name in names for name in wanted), repr(names[:4]))

    def listed(name):                               # click a name in the list
        m.click(lx + 8, wy + (names.index(name) * 16 + 8 if name in names else 30))
        m.move(*away)

    # Words in bold are one instruction between them.  With the pointer on
    # them, a box by them says which: its number, and its fields.  The box
    # is a line down, or on the last two lines over them, and starts under
    # the words unless that would take it past the edge of the window, so
    # its text is looked for from there leftwards.
    listed("dither-row")
    shot = m.screenshot(f"{out}/10-fused.png")
    source = source_of(shot, 45)
    where = first_bold(shot, sx, wy, 45, 16)
    check("a fused instruction is shown as its words, in bold",
          source[0] == ": dither-row" and where is not None, repr(source[:4]))
    told = ""
    if where:
        r, c = where
        m.move(sx + c * 8 + 4, wy + r * 16 + 1)
        hover = m.screenshot(f"{out}/10-hover.png")
        y = wy + ((r + 1) * 16 + 1 if r < 14 else r * 16 - 17)
        for x in range(sx + 1 + c * 8, sx - 3, -1):
            told = read_text(hover, x, y, min(40, (W - x) // 8), 1)[0]
            if re.match(r"[0-9A-F]{4} \S*->IN \S+ ->\S+", told):
                break
    check("the pointer on a fused instruction shows the machine's form of it",
          re.match(r"[0-9A-F]{4} \S*->IN \S+ ->\S+", told) is not None, repr((where, told)))
    # dither-start's first line stores into img, which has no header.
    listed("dither-start")
    shot = m.screenshot(f"{out}/10-label.png")
    source = source_of(shot, 45)
    if d.names_loaded:
        check("a headerless variable is shown by its label",
              source[0] == ": dither-start" and source[1].endswith(" img !"), repr(source[:2]))
    else:
        check("a headerless variable is shown as its number, with no names on the disk",
              source[0] == ": dither-start" and re.search(r" \d+ !$", source[1]) is not None, repr(source[:2]))
    # A word that is an instruction by itself says so too: the dup that line starts with.
    told = ""
    plain = r"[0-9A-F]{4} T->IN IN-> ->T d\+1"
    if "dup" in source[1]:
        r, c = 1, source[1].index("dup")
        m.move(sx + c * 8 + 4, wy + r * 16 + 1)
        hover = m.screenshot(f"{out}/10-hover-word.png")
        for x in range(sx + 1 + c * 8, sx - 3, -1):
            told = read_text(hover, x, wy + (r + 1) * 16 + 1, min(40, (W - x) // 8), 1)[0]
            if re.match(plain, told):
                break
    check("and on a word that is one instruction", re.match(plain, told) is not None, repr(told))
    m.move(*away)
    again = m.screenshot(f"{out}/10-hover-off.png")
    check("and the box goes when the pointer does",
          source_of(again, 45) == source, repr(source_of(again, 45)[:3]))

    # ^W again: the Forth window.
    cycle_to("forth")
    m.type("depth . 7 8\r")
    m.type("+ . depth .\r")
    shot = m.screenshot(f"{out}/11-cycled.png")
    text = forth_text(shot)
    check("^W cycles windows and the Forth stack persists",
          text[8] == "0  ok" and text[10] == "15 0  ok", repr(text[7:11]))

    # Fill the Forth window so that it scrolls.  The text is moved on the
    # screen, not drawn again, so check what is there: the last lines,
    # the first, and that the only solid cell is the cursor.
    for i in range(8):
        m.type(f"{i} .\r")
    shot = m.screenshot(f"{out}/11-scrolled.png")
    text = forth_text(shot)
    px = shot.load()
    fx, fy = d.content("forth")
    solid = sum(1 for r in range(16) for c in range(50)
                if not any(px[fx + 2 + c * 8 + i, fy + 2 + r * 16 + j] for i in range(8) for j in range(16)))
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
    raise_("viewer")
    m.move(*away)
    shot = m.screenshot(f"{out}/11-behind.png")
    fx0, fy0, fw, fh = d.rect["forth"]
    vx, vy, _, _ = d.rect["viewer"]
    bad = picture_wrong(shot, fx0, fy0, fx0 + fw, fy0 + fh)
    cols = min(50, (vx - (fx + 2)) // 8)            # of the Forth window, left of the viewer
    text = forth_text(shot, cols)
    check("a window behind can print and scroll", bad == 0 and cols > 2 and text[12:15] == ["18", "19", " ok"],
          f"{bad} pixels of the picture differ; {text[11:16]!r}")
    raise_("forth")
    shot = m.screenshot(f"{out}/11-task.png")
    text = forth_text(shot)
    check("the Forth window runs as a task", text[12:15] == ["18", "19", " ok"], repr(text[11:16]))

    # Put the whole screen out of date from the prompt.  Every window is
    # painted again, each only where it can be seen, so nothing outside
    # the Forth window may change.
    m.type(f"0 0 {W} {H} inval-rect\r")
    again = m.screenshot(f"{out}/12-repainted.png")
    a, b = shot.load(), again.load()
    moved = sum(1 for y in range(H) for x in range(W)
                if not (fx0 <= x < fx0 + fw and fy0 <= y < fy0 + fh) and bool(a[x, y]) != bool(b[x, y]))
    check("a full repaint leaves the other windows as they were", moved == 0, f"{moved} pixels changed")

    # The tables of names for the decompiler are on the disk.  They are
    # read into memory, which says they were found and are this image's,
    # and the memory is given back.  A disk without them says so.
    m.type("names-load names? . names-unload names? .\r")
    shot = m.screenshot(f"{out}/13-names.png")
    text = forth_text(shot)
    if d.names_loaded:
        check("the tables of names load from the disk and unload", text[14] == "-1 0  ok", repr(text[12:16]))
    else:
        check("with no tables of names on the disk, none are loaded", text[14] == "0 0  ok", repr(text[12:16]))

    # see has them in memory for as long as it runs, and what it prints
    # scrolls the window, which wants memory of its own for as long as a
    # scroll takes: there has to be room for both.  The word decompiled is
    # long enough to scroll, and near its end calls a word with no header,
    # which is shown by its name only if the names are in.
    m.type("see scroll-rect\r")
    shot = m.screenshot(f"{out}/13-see.png")
    text = forth_text(shot)
    check("see prints a word longer than the window, with the names in memory",
          text[14].endswith(" ok") and not any("? -" in row for row in text)    # an error is ? and its number
          and (not d.names_loaded or any("in-port" in row for row in text)), repr(text[9:16]))
    # Where memory has got to: the next cell that would be given out, with the names out and in.
    m.type("0 halloc . names-load 0 halloc . names-unload\r")
    shot = m.screenshot(f"{out}/13-memory.png")
    text = forth_text(shot)
    try:
        out_, in_ = (int(v) % (1 << m.cell_pixels) for v in text[14].split()[:2])
        print(f"note the next cell to be given out is {out_}, and with the tables of names in memory {in_}: "
              f"they are {in_ - out_} cells")
    except ValueError:
        out_ = in_ = None
        print(f"note where memory has got to was not read: {text[13:16]!r}")
    # see scrolled the window while it had the names in, and gave them back
    # after: if it had not, they would be in still, and loading them here
    # would have taken no memory.
    if d.names_loaded:
        check("see gives the names' memory back, though the window scrolled while it had them",
              out_ is not None and in_ > out_, f"{out_} and {in_}")

    # The picture was read and dithered by a task of its own, a row at a
    # turn, which took itself out of the ring when the last row was made.
    # Count the tasks round the ring: the desktop's and the Forth window's.
    m.type(":noname up @ 0 begin 1+ swap @ swap over up @ = until nip ;\r")
    m.type("execute .\r")
    shot = m.screenshot(f"{out}/14-tasks.png")
    text = forth_text(shot)
    check("the picture's task has left the ring", text[14] == "2  ok", repr(text[12:16]))

    # The title in the bar is a menu of the windows, the one in front first,
    # a line of 16 pixels each from just under the bar.  A click on the title
    # brings it down and it stays; a click on a line brings that window to
    # the front and takes the menu away.
    def lines(shot):
        return [name for i, name in enumerate(reversed(d.order))
                if shows(shot, TITLES[name], 12, 20 + 16 * i)]

    def in_front(shot, name):
        x, y, _, _ = d.rect[name]
        return shows(shot, TITLES[name], x + 8, y + 2, inverse=True)

    m.click(20, 8)
    m.move(*away)                                   # the pointer off the title, to read it
    shot = m.screenshot(f"{out}/15-menu.png")
    check("the title is a menu of the windows, the one in front first",
          lines(shot) == list(reversed(d.order)) and shows(shot, "hexaforth", 8, 2, inverse=True),
          repr(lines(shot)))
    at = list(reversed(d.order)).index("words")     # the pointer on a line, to the right of its name
    m.move(80, 20 + 16 * at + 8)
    shot = m.screenshot(f"{out}/15-menu-line.png")
    check("the line the pointer is on is shown so", shows(shot, "Words", 12, 20 + 16 * at, inverse=True)
          and len(lines(shot)) == len(d.order) - 1, repr(lines(shot)))
    m.click(32, 20 + 16 * list(reversed(d.order)).index("calc") + 8)
    d.front("calc")
    m.move(*away)
    shot = m.screenshot(f"{out}/15-picked.png")
    check("a click on a line brings its window to the front", in_front(shot, "calc") and lines(shot) == []
          and shows(shot, "hexaforth", 8, 2), repr(lines(shot)))
    # Held down and dragged: the button goes down on the title and up on a line.
    m.drag(20, 8, 32, 20 + 16 * list(reversed(d.order)).index("words") + 8)
    d.front("words")
    m.move(*away)
    shot = m.screenshot(f"{out}/15-dragged.png")
    check("and so does letting the button go on one", in_front(shot, "words") and lines(shot) == [],
          repr(lines(shot)))
    # A window under the menu does not draw where it is.  The Forth window
    # is brought to the front and given a word that waits for the menu to
    # be down and then prints twenty lines, so that it scrolls under it:
    # the menu must be whole, and the text beside it moved.  When the menu
    # is taken away, by a press that is not on it, what was under it is
    # painted as it now is.
    raise_("forth")
    m.type(":noname begin pause menu-open @ until 20 0 do i . cr loop ;\r")
    m.type("execute\r")
    m.click(20, 8)
    m.move(*away)
    shot = m.screenshot(f"{out}/15-over.png")
    text = forth_text(shot)
    check("a window under the menu prints and scrolls without drawing on it",
          lines(shot) == list(reversed(d.order)) and text[12:15] == ["18", "19", " ok"],
          repr((lines(shot), text[11:15])))
    fx0, fy0, fw, fh = d.rect["forth"]
    m.click(fx0 + fw - 40, fy0 + fh - 40)           # in the Forth window, which does nothing with a click
    m.move(*away)
    shot = m.screenshot(f"{out}/15-away.png")
    text = forth_text(shot)
    check("a press off the menu takes it away, and what was under it is painted",
          lines(shot) == [] and text[0:3] == ["6", "7", "8"] and in_front(shot, "forth"),
          repr((lines(shot), text[0:3])))
    m.click(20, 8)
    m.key("backspace")
    m.move(*away)
    shot = m.screenshot(f"{out}/15-key.png")
    check("and so does a key", lines(shot) == [] and shows(shot, "hexaforth", 8, 2), repr(lines(shot)))

    # HexaPaint.  It is at the back, and is brought to the front from the
    # menu.  What is drawn in it with the pointer is drawn here too, in a
    # model of its canvas (Canvas, above), and every pixel of the canvas on
    # the screen is compared with that.
    canvas = Canvas(384, 256) if H >= 576 else Canvas(240, 152)
    m.click(20, 8)
    m.click(32, 20 + 16 * list(reversed(d.order)).index("paint") + 8)
    d.front("paint")
    m.move(*away)
    px0, py0 = d.content("paint")
    hand = {"pattern": 1}                           # black, until one is picked

    def at(p):                                      # a place of the canvas, on the screen
        return px0 + 45 + p[0], py0 + p[1]

    def tool(n):                                    # click its picture: two to a row, 22 pixels each
        m.click(px0 + (n & 1) * 22 + 11, py0 + (n >> 1) * 22 + 11)
        hand["tool"] = n

    def pattern(n):                                 # click it, in the row under the canvas
        m.click(px0 + 83 + 18 * n + 8, py0 + canvas.h + 3 + 8)
        hand["pattern"] = n

    def pull(a, b):                                 # the button down at a, the pointer to b, the button up
        m.drag(*at(a), *at(b))
        canvas.draw(hand["tool"], hand["pattern"], a, b)

    def wrong(shot, model, but=None, under=()):
        """Pixels of the canvas on the screen that are not the model's, but for a box round a place,
        and but for what is under the windows named."""
        px = shot.load()
        return sum(1 for y in range(model.h) for x in range(model.w)
                   if bool(px[at((x, y))]) == bool(model.px[y][x])
                   and not (but and but[0] - 1 <= x < but[0] + 9 and but[1] - 1 <= y < but[1] + 13)
                   and not covered(at((x, y)), under))

    def covered(p, names):                          # is a place of the screen under one of those windows?
        return any(x - 2 <= p[0] < x + w + 4 and y - 2 <= p[1] < y + h + 4
                   for x, y, w, h in (d.rect[n] for n in names))

    shot = m.screenshot(f"{out}/16-paint.png")
    check("HexaPaint comes to the front from the menu, with nothing drawn",
          in_front(shot, "paint") and wrong(shot, canvas) == 0, f"{wrong(shot, canvas)} pixels of the canvas")
    hand["tool"] = 0                                # the pencil is in hand to start with
    pull((10, 10), (60, 40))
    m.move(*away)
    shot = m.screenshot(f"{out}/16-pencil.png")
    check("the pencil draws where the pointer is pulled", wrong(shot, canvas) == 0,
          f"{wrong(shot, canvas)} pixels differ")
    tool(1), pattern(3), pull((20, 60), (100, 70))          # the brush, in gray
    tool(5), pattern(5), pull((120, 20), (200, 90))         # a rectangle filled with bricks
    tool(6), pull((30, 80), (110, 140))                     # an oval
    tool(7), pattern(4), pull((130, 100), (220, 145))       # an oval filled with light gray
    tool(3), pull((5, 145), (230, 5))                       # a line
    tool(2), pull((60, 30), (150, 120))                     # the eraser
    tool(4), pull((200, 60), (150, 10))                     # a rectangle, pulled up and to the left
    m.move(*away)
    shot = m.screenshot(f"{out}/16-drawn.png")
    check("the brush, the shapes, the line and the eraser draw as the model of them does",
          wrong(shot, canvas) == 0, f"{wrong(shot, canvas)} pixels differ")
    # A shape is shown while it is pulled out, and is not in the canvas until
    # the button goes up.  The pointer is on its corner, so that is left out.
    tool(6)
    a, b = (40, 20), (100, 50)
    m.move(*at(a))
    m.button(True)
    m.move(*at(b))
    shot = m.screenshot(f"{out}/16-pulling.png")
    shown = Canvas(canvas.w, canvas.h)
    shown.px = [row[:] for row in canvas.px]
    shown.outline(6, a, b)
    check("a shape is shown as it is pulled out", wrong(shot, shown, but=b) == 0,
          f"{wrong(shot, shown, but=b)} pixels differ")
    m.button(False)
    canvas.draw(6, hand["pattern"], a, b)
    m.move(*away)
    shot = m.screenshot(f"{out}/16-let-go.png")
    check("and is drawn when the button goes up", wrong(shot, canvas) == 0, f"{wrong(shot, canvas)} pixels differ")
    # The canvas's place is the disk, and it is in memory only while HexaPaint
    # is in front.  Another window comes to the front: the disk has what was
    # drawn, every pixel.  That window is then pulled over the canvas and
    # back, so that some of the canvas is painted again, behind it: which is
    # from the disk, since it is no longer in memory.
    raise_("calc")
    saved = canvas_on_disk(m, canvas)
    differ = None if saved is None else sum(a != b for ra, rb in zip(saved, canvas.px) for a, b in zip(ra, rb))
    check("the canvas is written to the disk when another window comes to the front", differ == 0,
          "the disk does not say it has one" if saved is None else f"{differ} pixels differ")
    cx, cy = d.rect["calc"][:2]
    dx, dy = at((100, 0))[0] - cx, py0 - 5 - cy     # to where it is over the middle of what was drawn
    x, y = d.title("calc")
    m.drag(x, y, x + dx, y + dy)
    d.moved("calc", dx, dy)
    over = [(x, y) for y in range(canvas.h) for x in range(canvas.w) if covered(at((x, y)), ["calc"])]
    black = sum(canvas.px[y][x] for x, y in over)   # of the drawing, what the window is over
    x, y = d.title("calc", -dx, -dy)
    m.drag(x, y, x - dx, y - dy)
    d.moved("calc", -dx, -dy)
    m.move(*away)
    shot = m.screenshot(f"{out}/16-behind.png")
    seen = sum(1 for y in range(canvas.h) for x in range(canvas.w) if not covered(at((x, y)), ["calc"]))
    check("behind another window it is painted from the disk",
          black > 500 and seen > 5000 and wrong(shot, canvas, under=["calc"]) == 0,
          f"{wrong(shot, canvas, under=['calc'])} of {seen} pixels differ; "
          f"{black} black pixels were under the window pulled over it")
    # It is read back when HexaPaint comes to the front again.
    m.click(20, 8)
    m.click(32, 20 + 16 * list(reversed(d.order)).index("paint") + 8)
    d.front("paint")
    m.move(*away)
    shot = m.screenshot(f"{out}/16-again.png")
    check("what was drawn is there when the window has been under another",
          in_front(shot, "paint") and wrong(shot, canvas) == 0, f"{wrong(shot, canvas)} pixels differ")
    # The tables of names are memory while the Words window is drawn, and
    # the canvas is while HexaPaint is in front.  The Words window, brought
    # to the front, has its names.
    m.click(20, 8)
    m.click(32, 20 + 16 * list(reversed(d.order)).index("words") + 8)
    d.front("words")
    m.move(*away)
    shot = m.screenshot(f"{out}/17-names.png")
    source = read_text(shot, d.content("words")[0] + 112, d.content("words")[1], 45, 16)
    if d.names_loaded:
        check("there is room for the canvas and the tables of names", source[1].endswith(" img !"),
              repr(source[:2]))
    return failures


def standard(W=768, H=576, newest=("cold", "desktop", " catch drop"), names_loaded=True):
    """The desktop as desk.fs lays it out: on a screen of 768 by 576, or on a smaller one."""
    if H < 576:
        windows = [("paint", (110, 30, 287, 193)),
                   ("viewer", (246, 22, 258, 276)), ("editor", (-3, 40, 518, 300)),
                   ("words", (4, 60, 482, 276)), ("calc", (366, 96, 138, 238)),
                   ("forth", (6, 26, 406, 280))]
    else:
        windows = [("paint", (180, 140, 431, 297)),
                   ("viewer", (250, 40, 514, 532)), ("editor", (30, 270, 518, 300)),
                   ("words", (140, 296, 482, 276)), ("calc", (626, 300, 138, 238)),
                   ("forth", (12, 32, 406, 280))]
    return Desk(W, H, windows, newest, names_loaded)
