\ system.fs - blocks, the display, devices and the desktop.
\
\ It is the same files for a machine with cells of any size that has a
\ display, but for the drivers of QEMU's devices, which are written for
\ the machine the AArch64 kernels run on.
\
\ This is compiled into code page 1 and code pages 8 and 9.  cross.fs
\ compiles calls between pages as calls by execution token, so the split
\ needs no care beyond each page's code fitting in its 8,192 bytes, as
\ build/nuc.sizes gives them.
\
\ Storage is reached through a small interface, so a driver can be swapped
\ without touching the code that uses it:
\
\   block device   blk-read blk-write    ( buf blk# -- )   1024-byte blocks
\
\ A driver's init word stores its routines in the interface's vectors.  The
\ display needs no driver: it is a bitmap in memory, and display.fs draws
\ in it.

\ Page 1: what everything else is built on.

include machine.fs      \ device registers, high memory, byte-order helpers
include blocks.fs       \ block interface, BLOCK UPDATE FLUSH LOAD, disk-read
include font.fs         \ the fonts
include display.fs      \ the display: a bitmap in memory, and text on it

\ Page 8: regions, the drivers, the picture, and the window manager.

meta 8 code-page target

include region.fs       \ shapes that are not rectangles, to clip to
W64? [if]               \ the QEMU virt machine's devices, which the AArch64 kernels run on
include virtio.fs       \ shared by the virtio drivers
include virtio-blk.fs   \ block driver
include virtio-input.fs \ keyboard and pointer driver
[then]
include tiff.fs         \ uncompressed TIFF from blocks, a row at a time
include atkinson.fs     \ Atkinson dithering to the display
W64? [if]
include mandrill.fs     \ the demo, without the desktop
[then]
include keymap.fs       \ key codes to ASCII
include gfx.fs          \ rectangles, and text from the output words
include wm.fs           \ windows, pointer, event loop

\ Page 9: the applications.

meta 9 code-page target

include term.fs         \ text area and the Forth window
include edit.fs         \ block editor
include view.fs         \ image viewer
include calc.fs         \ RPN calculator
include fused.fs        \ fused instructions as words, for the decompiler
include see.fs          \ decompiler and the dictionary browser
include desk.fs         \ the desktop itself

\ Page 10: the nucleus's peephole, which the kernel reaches through a vector.

meta $a code-page target

include join.fs         \ instructions joined as they are compiled, and the rules

\ Every word is where it will be: the tables of names for the decompiler,
\ which go to a file for the disk (build/nuc.tables).  An entry of them is
\ two halves of a cell, which with cells of 16 bits is too little.

W32+? [if]
meta t' forth0 t' tstamp name-tables target
[then]
