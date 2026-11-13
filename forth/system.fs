\ system.fs - devices, blocks and graphics, compiled into code page 1.
\
\ Storage and display are each reached through a small interface, so a
\ driver can be swapped without touching the code that uses it:
\
\   block device   blk-read blk-write    ( buf blk# -- )   1024-byte blocks
\
\ A driver's init word stores its routines in the interface's vectors.  The
\ display needs no driver: it is a bitmap in memory, and display.fs draws
\ in it.

include machine.fs      \ device registers, high memory, byte-order helpers
include blocks.fs       \ block interface, BLOCK UPDATE FLUSH LOAD, disk-read
include font.fs         \ 8x16 glyphs
include display.fs      \ the display: a bitmap in memory, and text on it
include region.fs       \ shapes that are not rectangles, to clip to
include virtio.fs       \ shared by the virtio drivers
include virtio-blk.fs   \ block driver
include virtio-input.fs \ keyboard and pointer driver
include tiff.fs         \ uncompressed TIFF from blocks into memory
include atkinson.fs     \ Atkinson dithering to the display
include mandrill.fs     \ the demo, without the desktop

\ The desktop goes in code pages 8 and 9.  cross.fs compiles calls between
\ pages as calls by execution token, so the split needs no care here beyond
\ each page's code fitting in 8KB.

meta
    $10000 org
target

include keymap.fs       \ key codes to ASCII
include gfx.fs          \ rectangles, and text from the output words
include wm.fs           \ windows, pointer, event loop

meta
    $12000 org
target

include term.fs         \ text area and the Forth window
include edit.fs         \ block editor
include view.fs         \ image viewer
include calc.fs         \ RPN calculator
include fused.fs        \ fused instructions as words, for the decompiler
include see.fs          \ decompiler and the dictionary browser
include desk.fs         \ the desktop itself
