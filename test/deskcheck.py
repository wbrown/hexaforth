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
    m.key("w", "ctrl")
    d.cycle()
    assert d.order[-1] == "words", d.order
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
    m.key("w", "ctrl")
    d.cycle()
    assert d.order[-1] == "forth", d.order
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

    # The picture was read and dithered by a task of its own, a row at a
    # turn, which took itself out of the ring when the last row was made.
    # Count the tasks round the ring: the desktop's and the Forth window's.
    m.type(":noname up @ 0 begin 1+ swap @ swap over up @ = until nip ;\r")
    m.type("execute .\r")
    shot = m.screenshot(f"{out}/14-tasks.png")
    text = forth_text(shot)
    check("the picture's task has left the ring", text[14] == "2  ok", repr(text[12:16]))
    return failures


def standard(W=768, H=576, newest=("cold", "desktop", " catch drop"), names_loaded=True):
    """The desktop as desk.fs lays it out: on a screen of 768 by 576, or on a smaller one."""
    if H < 576:
        windows = [("viewer", (246, 22, 258, 276)), ("editor", (-3, 40, 518, 300)),
                   ("words", (4, 60, 482, 276)), ("calc", (366, 96, 138, 238)),
                   ("forth", (6, 26, 406, 280))]
    else:
        windows = [("viewer", (250, 40, 514, 532)), ("editor", (30, 270, 518, 300)),
                   ("words", (140, 296, 482, 276)), ("calc", (626, 300, 138, 238)),
                   ("forth", (12, 32, 406, 280))]
    return Desk(W, H, windows, newest, names_loaded)
