\ system.fs - blocks, the display, devices and the desktop.
\
\ It is the same files for a machine with cells of any size that has a
\ display, but for the drivers of QEMU's devices, which are written for
\ the machine the AArch64 kernels run on.
\
\ This is compiled into code page 1 and the pages after the data and the
\ headers.  cross.fs compiles calls between pages as calls by execution
\ token, so the split needs no care beyond each page's code fitting in its
\ 8,192 bytes, as build/nuc.sizes gives them.  Where memory is more than
\ 64K cells there are eight such pages, from page 8, and this takes three
\ and the nucleus's peephole a fourth.  Where it is 64K cells there are
\ four, from page 4, of which this and the peephole are packed into two,
\ so that definitions made at run time have the other two:
\
\                   more than 64K cells             64K cells
\   page 1          machine blocks font display     and region
\   the next        region (virtio) tiff atkinson   tiff atkinson keymap gfx wm
\                     keymap gfx wm                   term edit view calc
\   the next        term edit view calc see         see desk join
\   the next        paint desk
\   the next        join
\   the kernel's                                    paint, in its upper half
\
\ Storage is reached through a small interface, so a driver can be swapped
\ without touching the code that uses it:
\
\   block device   blk-read blk-write    ( buf blk# -- )   1024-byte blocks
\
\ A driver's init word stores its routines in the interface's vectors.  The
\ display needs no driver: it is a bitmap in memory, and display.fs draws
\ in it.

meta
: sys-page ( n -- ) code-base $1000 / + code-page ; \ the nth page after the data and the headers
: big-page ( n -- ) addr-bits #16 > if sys-page else drop then ;
: small-page ( n -- ) addr-bits #16 > if drop else sys-page then ;
variable low-back       \ where code was going, while it goes to the upper half of the kernel's page
: low-page ( -- ) addr-bits #16 > 0= if tcp @ low-back !  $1000 org then ;
: low-page-end ( -- )
    addr-bits #16 > 0= if
        tcp @ $2000 > abort" the upper half of the kernel's page is full"
        low-back @ org
    then
;
target

\ Page 1: what everything else is built on.

include machine.fs      \ device registers, high memory, byte-order helpers
include blocks.fs       \ block interface, BLOCK UPDATE FLUSH LOAD, disk-read
include vmdisk.fs       \ block driver for the reference VM's disk
include font.fs         \ the fonts
include display.fs      \ the display: a bitmap in memory, and text on it

\ Regions, the drivers, the picture, and the window manager.

meta 0 big-page target

include region.fs       \ shapes that are not rectangles, to clip to
W64? [if]               \ the QEMU virt machine's devices, which the AArch64 kernels run on
include virtio.fs       \ shared by the virtio drivers
include virtio-blk.fs   \ block driver
include virtio-input.fs \ keyboard and pointer driver
[then]

meta 0 small-page target

include tiff.fs         \ uncompressed TIFF from blocks, a row at a time
include atkinson.fs     \ Atkinson dithering to the display
W64? [if]
include mandrill.fs     \ the demo, without the desktop
[then]
include keymap.fs       \ key codes to ASCII
include gfx.fs          \ rectangles, and text from the output words
include wm.fs           \ windows, pointer, event loop

\ The applications.

meta 1 big-page target

include term.fs         \ text area and the Forth window
include edit.fs         \ block editor
include view.fs         \ image viewer
include calc.fs         \ RPN calculator

meta 1 small-page target

include fused.fs        \ fused instructions as words, for the decompiler
include see.fs          \ decompiler and the dictionary browser

\ HexaPaint.  Where there are pages to spare it has one, with the desktop.
\ Where there are not, it is in the upper half of the kernel's page, which
\ the kernel, a ROM of 4KB, leaves empty: 4KB of code that any page calls
\ with one instruction.

meta 2 big-page  low-page target

include paint.fs        \ a window to draw in

meta low-page-end target

include desk.fs         \ the desktop itself

\ The nucleus's peephole, which the kernel reaches through a vector.

meta 3 big-page target

include join.fs         \ instructions joined as they are compiled, and the rules

\ What the image has above its display is all laid down: what is given out
\ at run time starts after it, at an even cell.

meta lofty [if]  thi @ byte-shift rshift 1+ -2 and  t' himem tvar!  [then] target

\ Every word is where it will be: the tables of names for the decompiler,
\ which go to a file for the disk (build/nuc.tables).

meta t' forth0 t' tstamp name-tables target
