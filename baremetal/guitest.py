#!/usr/bin/env python3
"""Drive the hexaforth desktop in QEMU and check what is on the screen.

    guitest.py ELF DISK.img TIFF OUTDIR

Boots with a display, disk, keyboard and tablet, sends key and pointer
events through QMP, and takes screenshots.  What is checked is in
test/deskcheck.py, which test/desktop_test.py runs as well, on the
reference VM with cells of each size: here it is run on the AArch64
kernel, with QEMU's devices.
"""
import json
import os
import socket
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
import deskcheck                                    # noqa: E402


class Machine:
    W, H = W, H
    cell_pixels = 64

    def __init__(self, elf, disk, tiff=None):
        self.disk, self.tiff = disk, tiff
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

    def boot(self):
        return self.serial(b"desktop")

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

    def block(self, n):
        time.sleep(0.5)
        return open(self.disk, "rb").read()[1024 * n:1024 * (n + 1)]

    def picture(self):
        im = Image.open(self.tiff).convert("RGB")
        w, h = im.size
        px = im.tobytes()
        gray = [(77 * px[i] + 150 * px[i + 1] + 29 * px[i + 2]) >> 8 for i in range(0, len(px), 3)]
        return deskcheck.atkinson(gray, w, h)

    def stop(self):
        self.qemu.kill()


def main():
    elf, disk, tiff, out = sys.argv[1:5]
    os.makedirs(out, exist_ok=True)
    m = Machine(elf, disk, tiff)
    try:
        failures = deskcheck.run(m, deskcheck.standard(), out)
        if SCALE > 1:
            ok = m.inexact == 0
            print(f"{'ok  ' if ok else 'FAIL'} every pixel is a block of {SCALE} by {SCALE}"
                  f"{'' if ok else f': {m.inexact} of {m.shots} screens were not exact'}")
            if not ok:
                failures.append("scale")
    finally:
        m.stop()
    print(f"screenshots in {out}/")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
