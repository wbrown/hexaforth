\ system.fs - devices, blocks and graphics, compiled into code page 1.
\
\ Storage and display are each reached through a small interface, so a
\ driver can be swapped without touching the code that uses it:
\
\   block device   blk-read blk-write    ( buf blk# -- )   1024-byte blocks
\   display        plot  fb-w  fb-h      ( on? x y -- )    1 bit per pixel
\
\ A driver's init word stores its routines in the interface's vectors.

include machine.fs      \ device registers, high memory, byte-order helpers
include blocks.fs       \ block interface, BLOCK UPDATE FLUSH LOAD, disk-read
include display.fs      \ display interface
include virtio.fs       \ shared by the virtio drivers
include virtio-blk.fs   \ block driver
include virtio-input.fs \ keyboard and pointer driver
include ramfb.fs        \ display driver: QEMU ramfb
include tiff.fs         \ uncompressed TIFF from blocks into memory
include atkinson.fs     \ Atkinson dithering to the display
include mandrill.fs     \ the demo, without the desktop

\ The desktop goes in code pages 8 and 9.  cross.fs compiles calls between
\ pages as calls by execution token, so the split needs no care here beyond
\ each page's code fitting in 8KB.

meta
    $10000 org
target

include font.fs         \ 8x16 glyphs
include keymap.fs       \ key codes to ASCII
include gfx.fs          \ rectangles and text
include wm.fs           \ windows, pointer, event loop

meta
    $12000 org
target

include term.fs         \ text area and the Forth window
include edit.fs         \ block editor
include view.fs         \ image viewer
include calc.fs         \ RPN calculator
include desk.fs         \ the desktop itself
