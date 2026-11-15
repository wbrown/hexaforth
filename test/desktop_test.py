#!/usr/bin/env python3
"""Run the desktop on the reference VM and check what is on the screen.

    desktop_test.py VM IMAGE.hex OUTDIR [CELL_BITS [W H BASE [TABLES]]]

The checks are those of test/deskcheck.py, which baremetal/guitest.py runs
on the AArch64 kernel under QEMU.  Here the machine is the reference VM,
which has cells of 64 bits, 32 or 16, and no devices: so the test is its
devices, given at the prompt before the desktop is started.

    the disk        a word in 'blk-read and one in 'blk-write.  Block 1 is
                    kept in memory, so that the editor can save it.  The
                    picture, from block 16, is a TIFF whose header and
                    directory are laid over blocks made from their numbers
                    (as test/tiff_test.py does): it takes no memory, and is
                    bigger than a machine with cells of 16 bits has.  The
                    tables of names, where the build made them, are from
                    block 1024.
    keys, pointer   a word in 'poll, which the event loop runs each time
                    round.  It takes one thing from the console: a key going
                    down or up, by its Linux code, as a virtio keyboard gives
                    them; the pointer's place; its button; nothing; or to
                    print the screen, or block 1.  It hands events to
                    on-event, as any driver does.

The VM waits for the console whenever it is asked for a key, so the
desktop takes exactly one turn of its loop for each thing sent: the test
is the same every time it is run.  The screen is the display in the VM's
memory, printed a cell at a time.
"""
import os
import re
import subprocess
import sys
import threading

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import deskcheck                                    # noqa: E402
from tiff_test import BLOCK, File                   # noqa: E402

NAMED = {"ret": 28, "backspace": 14, "up": 103, "down": 108, "left": 105, "right": 106,
         "pgup": 104, "pgdn": 109, "ctrl": 29, "shift": 42}


def keycodes():
    """{character: (Linux key code, shifted?)}, from the map the desktop itself uses."""
    text = open(f"{HERE}/../forth/keymap.fs").read()
    table = b"".join(int(v, 16).to_bytes(8, "little") for v in re.findall(r"^\$([0-9a-f]+) b8,", text, re.M))
    codes = {}
    for shifted in (1, 0):
        for code in range(127, 0, -1):              # the lowest code that gives it, unshifted if it can be
            ch = table[shifted * 128 + code]
            if ch:
                codes[chr(ch)] = (code, bool(shifted))
    return codes


class Machine:
    def __init__(self, vm, image, bits, W, H, base, tables=None, picture=(512, 512)):
        self.bits, self.W, self.H, self.base = bits, W, H, base
        self.codes = keycodes()
        cellb = bits // 8
        cells = BLOCK // cellb

        def as_cells(data):
            data += bytes(-len(data) % cellb)
            return [int.from_bytes(data[i:i + cellb], "little") for i in range(0, len(data), cellb)]

        def laid(word, values):                     # lines that lay cells down with a word
            return [" ".join(f"{v:x} {word}" for v in values[i:i + 6]) for i in range(0, len(values), 6)]

        pw, ph = picture
        self.tiff = File(0, pw, ph, 3, 64, [BLOCK + y * pw * 3 for y in range(0, ph, 64)], base=16)
        source = open(f"{HERE}/../baremetal/block1.fs").read()
        block1 = " ".join(source.split()).encode().ljust(BLOCK)
        lines = ["variable sd  variable bf  variable bk",
                 ": rnd sd @ 25173 * 13849 + 65535 and dup sd ! 8 rshift ;",
                 f": cl 0 {cellb} 0 do rnd i 8 * lshift or loop ;",
                 f": gen 40503 * 12345 + 65535 and sd !  {cells} over + swap do cl i ! loop ;"]
        data, table = [], []
        for blk, cell, values in self.tiff.patches(cellb):
            table.append((blk, cell, len(values), len(data)))
            data += values
        lines += ["create pd hex"] + laid(",", data) + ["decimal create pt"]
        lines += [f"{blk} , {cell} , {n} , pd {src} + ," for blk, cell, n, src in table]
        lines += ["0 , 0 , 0 , 0 ,", "create b1 hex"] + laid(",", as_cells(bytearray(block1))) + ["decimal"]
        names = ""
        if tables:
            content = bytearray(open(tables, "rb").read())
            content += bytes(-len(content) % BLOCK)
            lines += [f"{len(content)} halloc constant nm  variable np  nm np !", ": l, np @ ! 1 np +! ;", "hex"]
            lines += laid("l,", as_cells(content)) + ["decimal"]
            names = (f" bk @ 1024 - {len(content) // BLOCK} u< if"
                     f" bk @ 1024 - {cells} * nm + bf @ {cells} cells-move then")
        stride = W // bits
        lines += [": lay dup >r 3 + @  r@ 1 + @ bf @ +  r@ 2 + @ cells-move r> ;",
                  ": rd1 pt begin dup 2 + @ while dup @ bk @ = if lay then 4 + repeat drop ;",
                  f": rd2 bk @ 1 = if b1 bf @ {cells} cells-move then{names} ;",
                  ": rd 2dup gen  bk ! bf !  rd1 rd2 ;",
                  f": wr 1 = if b1 {cells} cells-move else drop then ;",
                  "' rd 'blk-read !  ' wr 'blk-write !",
                  ": k2 key 8 lshift key or ;",
                  f": scr 91 emit hex {base} {stride * H} over + swap do i @ . loop decimal 93 emit ;",
                  f": blk1 123 emit hex b1 {cells} over + swap do i @ . loop decimal 125 emit ;",
                  ": ek 1 key key 48 - on-event ;",
                  ": em 3 0 k2 on-event 3 1 k2 on-event ;",
                  ": eb 1 272 key 48 - on-event ;",
                  ": t1 dup 107 = if ek then dup 109 = if em then dup 98 = if eb then ;",
                  ": t2 dup 115 = if scr then dup 101 = if blk1 then ;",
                  ": tp key t1 t2 drop ;",
                  "display-init ' tp 'poll ! windows"]
        self.vm = subprocess.Popen([vm, image], stdin=subprocess.PIPE, stdout=subprocess.PIPE, bufsize=0)
        self.out = bytearray()
        self.more = threading.Condition()
        threading.Thread(target=self.reader, daemon=True).start()
        self.send("".join(line + "\n" for line in lines).encode())

    def reader(self):
        while True:
            chunk = self.vm.stdout.read(65536)
            with self.more:
                if chunk:
                    self.out += chunk
                else:
                    self.out += b"\n(the VM has stopped)"
                self.more.notify_all()
            if not chunk:
                return

    def send(self, data):
        self.vm.stdin.write(data)
        self.vm.stdin.flush()

    def until(self, token, start=0, timeout=300):
        """Wait for something to be printed after a place; where it ends."""
        with self.more:
            ok = self.more.wait_for(
                lambda: token in self.out[start:] or b"(the VM has stopped)" in self.out[start:], timeout)
            at = self.out.find(token, start)
            if not ok or at < 0:
                raise RuntimeError(f"the VM did not print {token!r}: {bytes(self.out[-400:])!r}")
            return at + len(token)

    def idle(self, turns):
        self.send(b"." * turns)

    def boot(self):
        end = self.until(b"desktop")
        self.idle(self.tiff.h + 80)                 # the picture comes down a row a turn
        return self.out[:end].decode(errors="replace")

    def key(self, name, *modifiers):
        keys = [NAMED[k] if k in NAMED else self.codes[k][0] for k in list(modifiers) + [name]]
        for k in keys:
            self.send(b"k" + bytes([k]) + b"1")
        for k in reversed(keys):
            self.send(b"k" + bytes([k]) + b"0")

    def type(self, text):
        for ch in text:
            code, shifted = self.codes[ch]
            for step in ([(42, 1)] if shifted else []) + [(code, 1), (code, 0)] + ([(42, 0)] if shifted else []):
                self.send(b"k" + bytes([step[0]]) + (b"1" if step[1] else b"0"))
        self.idle(4)

    def move(self, x, y):
        vx, vy = x * 0x8000 // self.W + 1, y * 0x8000 // self.H + 1
        self.send(b"m" + bytes([vx >> 8, vx & 255, vy >> 8, vy & 255]))

    def click(self, x, y):
        self.move(x, y)
        self.send(b"b1b0")
        self.idle(6)

    def drag(self, x0, y0, x1, y1):
        self.move(x0, y0)
        self.send(b"b1")
        self.move(x1, y1)
        self.send(b"b0")
        self.idle(6)

    def printed(self, command, opening, closing):
        """Send a command that prints cells between two characters; the cells."""
        start = len(self.out)
        self.send(command)
        end = self.until(closing, start)
        text = bytes(self.out[self.out.index(opening, start) + 1:end - 1]).decode()
        return [int(v, 16) & (1 << self.bits) - 1 for v in text.split()]

    def screenshot(self, path, settle=1.5):
        self.idle(12)
        cells = self.printed(b"s", b"[", b"]")
        bits, stride = self.bits, self.W // self.bits
        assert len(cells) == stride * self.H, f"{len(cells)} cells of the screen were printed"
        raw = bytearray(self.W * self.H)
        for i, cell in enumerate(cells):            # a set bit is black; a cell's lowest bit is its leftmost pixel
            at = i * bits
            if cell == 0:
                raw[at:at + bits] = b"\xff" * bits
            else:
                for b in range(bits):
                    if not cell >> b & 1:
                        raw[at + b] = 255
        shot = Image.frombytes("L", (self.W, self.H), bytes(raw))
        shot.save(path)
        return shot

    def block(self, n):
        assert n == 1, "only block 1 is kept"
        self.idle(4)
        cells = self.printed(b"e", b"{", b"}")
        return b"".join(c.to_bytes(self.bits // 8, "little") for c in cells)

    def picture(self):
        return self.tiff.rows()

    def stop(self):
        try:
            self.key("q", "ctrl")
            self.send(b"\n8000 execute\n")
            self.vm.wait(timeout=20)
        except Exception:
            self.vm.kill()


def main():
    vm, image, out = sys.argv[1:4]
    bits = int(sys.argv[4]) if len(sys.argv) > 4 else 64
    W, H, base = (int(v, 0) for v in sys.argv[5:8]) if len(sys.argv) > 7 else (768, 576, 0x4000)
    tables = sys.argv[8] if len(sys.argv) > 8 else None
    os.makedirs(out, exist_ok=True)
    m = Machine(vm, image, bits, W, H, base, tables)
    try:
        # The newest words in the dictionary are the test's own: tp is the one in 'poll.
        desk = deskcheck.standard(W, H, newest=("tp", "t2", " key t1 t2 drop"), names_loaded=tables is not None)
        failures = deskcheck.run(m, desk, out)
    finally:
        m.stop()
    print(f"the desktop on the reference VM, cells of {bits} bits, {W} by {H}: "
          f"{len(failures)} checks failed; screens in {out}/")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
