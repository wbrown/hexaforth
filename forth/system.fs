\ system.fs - blocks, the display, devices and the desktop.
\
\ It is the same files for a machine with cells of any size that has a
\ display, but for the drivers of QEMU's devices, which are written for
\ the machine the AArch64 kernels run on.
\
\ This is compiled into code page 1 and the pages after the data and the
\ headers.  cross.fs compiles calls between pages as calls by execution
\ token, so the split needs no care beyond each page's code fitting in its
\ 8,192 bytes, as build/nuc.sizes gives them.  A page's files are
\ included one after another, since a page is begun again from its start
\ when it is gone to.  Where memory is more than 64K cells there are eight
\ such pages, from page 8, and this and the nucleus's peephole take four.
\ Where it is 64K cells there are four, from page 4, of which this and the
\ peephole take three, so that definitions made at run time have the
\ last:
\
\                   more than 64K cells             64K cells
\   page 1          machine blocks fonts display    the same
\                     region
\   the next        tiff atkinson keymap gfx wm     tiff atkinson keymap gfx wm
\   the next        text term edit see console      text term edit see console
\   the next        gray view calc paint            gray view desk join
\   the next        (virtio mandrill) desk join
\   the kernel's                                    calc paint, in its upper half
\
\ Storage is reached through a small interface, so a driver can be swapped
\ without touching the code that uses it:
\
\   block device   blk-read blk-write    ( buf blk# -- )   1024-byte blocks
\
\ A driver's words are stored in the interface's vectors: by the ROM, for
\ the disk whose controller it finds (nuc.fs), or by a driver's init word.
\ The display needs no driver: it is a bitmap in memory, and display.fs
\ draws in it.

meta
: big-page ( n -- ) addr-bits #16 > if sys-page else drop then ;
: small-page ( n -- ) addr-bits #16 > if drop else sys-page then ;
\ Where pages are few, code goes to the upper half of the kernel's page
\ too, after what the nucleus has there (cross.fs, upper-half).
: low-page ( -- ) addr-bits #16 > 0= if upper-half then ;
: low-page-end ( -- ) addr-bits #16 > 0= if lower-half then ;
target

\ Page 1: what everything else is built on.

include machine.fs      \ device registers, high memory, byte-order helpers
include blocks.fs       \ block interface, BLOCK UPDATE FLUSH LOAD
include mono.fs         \ the fonts: what the applications write in,
include sans.fs         \ and what titles and the menu are in
include spin.fs         \ the disk light in the menu bar
: plain-font ( -- font ) mono ;                     \ what a port writes in until it is told otherwise
include display.fs      \ the display: a bitmap in memory, and text on it

\ Regions, the picture, and the window manager.

include region.fs       \ shapes that are not rectangles, to clip to

meta 0 sys-page target

include tiff.fs         \ uncompressed TIFF from blocks, a row at a time
include atkinson.fs     \ Atkinson dithering to the display
include keymap.fs       \ key codes to ASCII
include gfx.fs          \ rectangles, and text from the output words
include wm.fs           \ windows, pointer, event loop

\ The applications.

meta 1 big-page  1 small-page target

include text.fs         \ text in cells, a line at a time
include term.fs         \ text area and the Forth window
include edit.fs         \ block editor
\ Every baseword a word with a header, and the names of an instruction's
\ fields for the decompiler, which basewords.fs makes from vm_opcodes.h.
baseword-words
field-names
include see.fs          \ decompiler and the dictionary browser
include files.fs        \ a source of the disk's by its label, and a file's own names
include console.fs      \ the screen as the console, and the boot from a disk

\ The applications: gray, view, calc, paint and the QEMU devices, on
\ pages 2 and 3.  A machine of more than 64K cells,
\ where every word has a header, boots from an image without them and
\ compiles them from its disk (apps.fs); any other machine's image has
\ them.  A page is begun again from its start when it is gone to, so the
\ image without them goes to page 3 itself.

BOOTING? [if] W32+? [if]
meta 3 big-page target
[else]
include apps-wide.fs
[then] [else]
include apps-wide.fs
[then]

\ The nucleus's peephole, which the kernel reaches through a vector, and
\ what it needs to compile the system's source as cross.fs does.

include join.fs         \ instructions joined as they are compiled
include cross-words.fs  \ cross.fs's words, for the nucleus to compile this source

\ What a machine that boots from a disk compiles from it.

BOOTING? [if] [else]
include apps.fs         \ the desktop
[then]

\ The decompiler's table, which names the basewords by their headers.  It
\ is made from an image (util/fused.c), and the image it is made from has
\ the table of no image: so it is laid after all the other data, where
\ how long it is moves nothing, and see.fs is told where it is.

include-fused           \ fused.fs: instructions as words, for the decompiler

\ What the peephole is told of numbers is a table, and is laid after all
\ the code: what reads an image as code reads none that is after a table
\ on its page.

include folds.fs

\ Every word is where it will be: the tables of names for the decompiler,
\ which go to a file for the disk (build/nuc.tables).

meta t' forth0 t' tstamp name-tables target
