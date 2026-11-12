#!/usr/bin/env python3
"""Boot hexaforth in QEMU with a disk and a display, run commands, and
check the screen.

    demo.py ELF DISK.img TIFF OUT.png [command ...]

Sends each command to the prompt (default: `mandrill`), then takes a
screenshot through the QEMU monitor.  The same TIFF is dithered here with
the same integer arithmetic as atkinson.fs, and the screenshot must match it
pixel for pixel.
"""
import os
import socket
import struct
import subprocess
import sys
import tempfile
import time

from PIL import Image

W, H = 800, 600


def expected(tiff_path):
    """Atkinson dithering exactly as atkinson.fs and tiff.fs do it."""
    im = Image.open(tiff_path).convert("RGB")
    w, h = im.size
    px = im.tobytes()
    gray = [(77 * px[i] + 150 * px[i + 1] + 29 * px[i + 2]) >> 8 for i in range(0, len(px), 3)]
    err = [[0] * (w + 4) for _ in range(h + 2)]
    out = Image.new("1", (W, H), 0)
    ox, oy = (W - w) // 2, (H - h) // 2
    for y in range(h):
        for x in range(w):
            old = gray[y * w + x] + err[y][x + 2]
            new = 255 if old > 127 else 0
            out.putpixel((ox + x, oy + y), 1 if new else 0)
            e = ((old - new + 1024) >> 3) - 128
            for dx, dy in ((1, 0), (2, 0), (-1, 1), (0, 1), (1, 1), (0, 2)):
                err[y + dy][x + 2 + dx] += e
    return out


def main():
    elf, disk, tiff, png = sys.argv[1:5]
    commands = sys.argv[5:] or ["mandrill"]
    tmp = tempfile.mkdtemp()
    mon, ppm = f"{tmp}/monitor", f"{tmp}/screen.ppm"
    qemu = subprocess.Popen(
        ["qemu-system-aarch64", "-M", "virt", "-cpu", "cortex-a53", "-m", "256",
         "-display", "none", "-serial", "stdio", "-nic", "none",
         "-monitor", f"unix:{mon},server,nowait",
         "-device", "ramfb",
         "-global", "virtio-mmio.force-legacy=false",
         "-drive", f"file={disk},if=none,format=raw,id=hd0",
         "-device", "virtio-blk-device,drive=hd0",
         "-kernel", elf],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, bufsize=0)
    os.set_blocking(qemu.stdout.fileno(), False)

    def run(command, timeout=300):
        qemu.stdin.write(command.encode() + b"\r")
        seen, start = b"", time.time()
        while b" ok\r\n" not in seen and b"error:" not in seen:
            if time.time() - start > timeout:
                seen += b" [timed out]"
                break
            chunk = qemu.stdout.read()
            if chunk:
                seen += chunk
            else:
                time.sleep(0.05)
        print(f"{seen.decode(errors='replace').strip()}   [{time.time() - start:.1f}s]")
        return b" ok" in seen

    try:
        ok = all([run(c) for c in commands])
        s = socket.socket(socket.AF_UNIX)
        s.connect(mon)
        time.sleep(0.3)
        s.recv(4096)
        s.sendall(f"screendump {ppm}\n".encode())
        time.sleep(1.5)
    finally:
        qemu.kill()
    if not os.path.exists(ppm):
        print("no screenshot was produced")
        return 1
    shot = Image.open(ppm)
    shot.save(png)
    print(f"screenshot {shot.size[0]}x{shot.size[1]} saved to {png}")
    want = expected(tiff)
    got = shot.convert("L").point(lambda v: 255 if v else 0).convert("1")
    if got.size != want.size:
        print(f"screen is {got.size}, expected {want.size}")
        return 1
    diff = sum(1 for a, b in zip(got.getdata(), want.getdata()) if bool(a) != bool(b))
    lit = sum(1 for a in got.getdata() if a)
    print(f"{lit} pixels lit; {diff} differ from the reference dither")
    return 0 if ok and diff == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
