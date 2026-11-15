#!/usr/bin/env python3
"""Run the desktop on the reference VM and check what is on the screen.

    desktop_test.py VM IMAGE.hex OUTDIR DISK.img [CELL_BITS [W H BASE [TABLES]]]

The checks are those of test/deskcheck.py, which baremetal/guitest.py runs
on the AArch64 kernel under QEMU.  Here the machine is the reference VM,
which has cells of 64 bits, 32 or 16.

    the disk        the VM's own: a file, given to it as its second
                    argument, which vmdisk.fs is the driver for.  It is a
                    copy of DISK.img, which baremetal/mkdisk.py made as it
                    makes QEMU's: block 1 for the editor, the Mandrill as a
                    TIFF from block 16 (DISK.tiff, beside it, is the same
                    file, and is dithered here to compare the screen with),
                    and from block 1024 the tables of names, if TABLES says
                    the build made any.
    keys, pointer   the VM has none, so the test is them: a word in 'poll,
                    typed at the prompt, which the event loop runs each
                    time round.  It takes one thing from the console: a key
                    going down or up, by its Linux code, as a virtio
                    keyboard gives them; the pointer's place; its button;
                    nothing; or to print the screen.  It hands events to
                    on-event, as any driver does.

The VM waits for the console whenever it is asked for a key, so the
desktop takes exactly one turn of its loop for each thing sent: the test
is the same every time it is run.  The screen is the display in the VM's
memory, printed a cell at a time.
"""
import os
import re
import shutil
import subprocess
import sys
import threading

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import deskcheck                                    # noqa: E402

BLOCK = 1024
NAMED = {"ret": 28, "backspace": 14, "up": 103, "down": 108, "left": 105, "right": 106,
         "pgup": 104, "pgdn": 109, "ctrl": 29, "shift": 42}


def keycodes():
    """{character: (Linux key code, shifted?)}, from the map the desktop itself uses."""
    text = open(f"{HERE}/../forth/keymap.fs").read()
    table = [int(v, 16) for v in re.findall(r"\$([0-9a-f]{4}) ,", text)]    # a cell a key: plain, and shifted above it
    assert len(table) == 128, "keymap.fs is not 128 cells"
    codes = {}
    for shifted in (1, 0):
        for code in range(127, 0, -1):              # the lowest code that gives it, unshifted if it can be
            ch = table[code] >> 8 * shifted & 0xff
            if ch:
                codes[chr(ch)] = (code, bool(shifted))
    return codes


class Machine:
    def __init__(self, vm, image, disk, out, bits, W, H, base):
        self.bits, self.W, self.H, self.base = bits, W, H, base
        self.cell_pixels = bits                     # a cell of the display is as many pixels as it has bits
        self.codes = keycodes()
        self.tiff = os.path.splitext(disk)[0] + ".tiff"
        self.disk = f"{out}/disk.img"               # a copy, for the editor to save to
        shutil.copyfile(disk, self.disk)
        self.rows = Image.open(self.tiff).size[1]
        stride = W // bits
        lines = [": k2 key 8 lshift key or ;",
                 f": scr 91 emit hex {base} {stride * H} over + swap do i @ . loop decimal 93 emit ;",
                 ": ek 1 key key 48 - on-event ;",
                 # A device says each thing and then that it has said all there is (type 0).
                 ": em 3 0 k2 on-event 3 1 k2 on-event 0 0 0 on-event ;",
                 ": eb 1 272 key 48 - on-event 0 0 0 on-event ;",
                 ": t1 dup 107 = if ek then dup 109 = if em then ;",
                 ": t2 dup 98 = if eb then dup 115 = if scr then ;",
                 ": tp key t1 t2 drop ;",
                 "display-init vmdisk-init ' tp 'poll ! windows"]
        self.vm = subprocess.Popen([vm, image, self.disk], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                   bufsize=0)
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
        self.idle(self.rows + 80)                   # the picture comes down a row a turn
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

    def button(self, down):
        self.send(b"b1" if down else b"b0")

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

    def screenshot(self, path, settle=1.5):
        self.idle(12)
        start = len(self.out)
        self.send(b"s")
        end = self.until(b"]", start)
        text = bytes(self.out[self.out.index(b"[", start) + 1:end - 1]).decode()
        try:
            cells = [int(v, 16) & (1 << self.bits) - 1 for v in text.split()]
        except ValueError:                          # not the screen: the VM has stopped, and said why
            raise RuntimeError("the VM stopped: " + bytes(self.out[-400:]).decode("latin1")) from None
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

    def block(self, n, count=1):
        self.idle(4)
        start = len(self.out)                       # by now the VM has done all that was sent before:
        self.send(b"s")                             # it prints the screen only after it
        self.until(b"]", start)
        return open(self.disk, "rb").read()[BLOCK * n:BLOCK * (n + count)]

    def picture(self):
        im = Image.open(self.tiff).convert("RGB")
        w, h = im.size
        px = im.tobytes()
        return [(77 * px[i] + 150 * px[i + 1] + 29 * px[i + 2]) >> 8 for i in range(0, len(px), 3)], w, h

    def stop(self):
        try:
            self.key("q", "ctrl")
            self.send(b"\n8000 execute\n")
            self.vm.wait(timeout=20)
        except Exception:
            self.vm.kill()


def main():
    vm, image, out, disk = sys.argv[1:5]
    bits = int(sys.argv[5]) if len(sys.argv) > 5 else 64
    W, H, base = (int(v, 0) for v in sys.argv[6:9]) if len(sys.argv) > 8 else (768, 576, 0x4000)
    tables = len(sys.argv) > 9
    os.makedirs(out, exist_ok=True)
    m = Machine(vm, image, disk, out, bits, W, H, base)
    try:
        # The newest words in the dictionary are the test's own: tp is the one in 'poll.
        desk = deskcheck.standard(W, H, newest=("tp", "t2", " key t1 t2 drop"), names_loaded=tables)
        failures = deskcheck.run(m, desk, out)
    finally:
        m.stop()
    print(f"the desktop on the reference VM, cells of {bits} bits, {W} by {H}: "
          f"{len(failures)} checks failed; screens in {out}/")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
