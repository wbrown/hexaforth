#!/usr/bin/env python3
"""Say when the desktop is ready, and when its picture is made.

    desktime.py KERNEL-display.elf DISK TIFF IMAGE.hex [SECONDS]

Boots the desktop as guitest.py does and watches it, without touching it:
when the console says the desktop is up, when the Forth window has its
prompt on the screen, and when the first row of the picture, half its rows
and all of them have been made.  The times are in seconds from when the
machine was started.  On a kernel held to a speed (make demo-time MIPS=1)
they are what a machine of that speed is like to wait for, and with the
disk held to one as well (DISK=40, which guitest.py takes from the
environment) one with a floppy.  They are what to measure before and
after a change to how the picture is read, dithered or shown.  It gives
up after SECONDS, 600 if none is given.

The screen is looked at for the prompt.  How many rows of the picture are
made is read out of the machine's memory: `vw` in view.fs is the viewer in
hand, and the third cell of a viewer's record is the rows of its bitmap
that are made (`bmh`).  If either changes, so must this; it stops and says
so if the image has no `vw`.
"""
import os
import struct
import sys
import time

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, f"{HERE}/../util")
import guitest                                      # noqa: E402
import deskprof                                     # noqa: E402
import insn_map                                     # noqa: E402
from guitest import deskcheck                       # noqa: E402

ROWS_MADE = 2               # bmh: where it is in a viewer's record (view.fs)


def main():
    elf, disk, tiff, image = sys.argv[1:5]
    limit = float(sys.argv[5]) if len(sys.argv) > 5 else 600
    height = Image.open(tiff).size[1]
    vw = [value for value, name in insn_map.labels_of(image) if name == "vw" and value >> 63]
    if not vw:
        raise SystemExit(f"{image} has no label vw: view.fs has changed, and this must change with it")
    vw = vw[0] & ~(1 << 63)
    memory = deskprof.symbols(elf)["image"]         # where the machine's cell 0 is
    fx, fy = deskcheck.standard().content("forth")  # the Forth window's text is 2 in from there

    m = guitest.Machine(elf, disk, tiff)
    start = time.perf_counter()

    def cell(a):
        path = f"{m.tmp}/cell"
        m.command("pmemsave", val=memory + 8 * a, size=8, filename=path)
        return struct.unpack("<Q", open(path, "rb").read())[0]

    def rows():
        viewer = cell(vw)
        return cell(viewer + ROWS_MADE) if viewer else 0

    def prompt():
        path = f"{m.tmp}/screen.ppm"
        m.command("screendump", filename=path)
        shot = Image.open(path).convert("L").point(lambda v: 255 if v else 0)
        return deskcheck.read_text(shot, fx + 2, fy + 2, 2, 1)[0] == "ok"

    when = {}

    def note(what, now):
        when.setdefault(what, now)

    try:
        said = b""
        while "all" not in when and time.perf_counter() - start < limit:
            said += m.qemu.stdout.read() or b""
            n = rows()
            now = time.perf_counter() - start
            if b"desktop" in said:
                note("up", now)
            if n:
                note("first", now)
            if 2 * n >= height:
                note("half", now)
            if n >= height:
                note("all", now)
            if "prompt" not in when and prompt():
                note("prompt", time.perf_counter() - start)
            time.sleep(0.05)
    finally:
        m.stop()

    for what, said in (("up", "the console says the desktop is up"),
                       ("prompt", "the Forth window has its prompt"),
                       ("first", "the first row of the picture is made"),
                       ("half", f"half its {height} rows are made"),
                       ("all", "the picture is finished")):
        at = f"{when[what]:6.1f} s" if what in when else f"not in {limit:.0f} s"
        print(f"{said:38} {at}")
    return 0 if "all" in when and "prompt" in when else 1


if __name__ == "__main__":
    sys.exit(main())
