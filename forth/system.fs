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
include virtio-blk.fs   \ block driver: virtio-blk over virtio-mmio
include ramfb.fs        \ display driver: QEMU ramfb
include tiff.fs         \ uncompressed TIFF from blocks into memory
include atkinson.fs     \ Atkinson dithering to the display
include mandrill.fs     \ the demo
