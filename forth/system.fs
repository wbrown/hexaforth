\ system.fs - blocks, the display, devices and the desktop.
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
include virtio.fs       \ shared by the virtio drivers
include virtio-blk.fs   \ block driver
include virtio-input.fs \ keyboard and pointer driver
include tiff.fs         \ uncompressed TIFF from blocks into memory
include atkinson.fs     \ Atkinson dithering to the display
include mandrill.fs     \ the demo, without the desktop
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

\ Every word is where it will be: the dictionary's tables, for the decompiler.

meta t' xtab t' dtab t' forth0 name-tables target
