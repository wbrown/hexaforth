\ system.fs - blocks, the display, devices and the desktop.
\
\ It is the same files for a machine with cells of any size that has a
\ display, but for the drivers of QEMU's devices, which are written for
\ the machine the AArch64 kernels run on.
\
\ It is two parts.  The first is the least that compiles source from a
\ disk and draws: memory and blocks, cross.fs's words, the fonts, the
\ display, regions, rectangles, the keys, the window manager, what the
\ disks have by label and the console.  It is the kernel's page and two
\ more, on a machine of any size, and is the image a machine boots from a
\ disk with (BOOT=on).  The second is the applications (apps.fs), which
\ that machine compiles from the source on its disk, and which cross.fs
\ lays in the whole image.
\
\ A call to a word on another page is the word's place pushed and a call
\ of execute, where a call within a page or into the kernel's is one
\ instruction: so the order of this list, and where it says each page
\ starts, is what every such call costs.  A page that is gone back to
\ goes on from where its code had got to (cross.fs, code-page).  The
\ targets listing-16 and the like say what each page has and has left,
\ and which calls go from one page to another.  The pages after the data
\ and the headers are eight where memory is more than 64K cells, and four
\ where it is 64K cells; the last that has nothing is where definitions
\ made at run time go on, after what page 1 has left.
\
\   the kernel's    in its upper half: regions and rectangles whole, and
\                   the words of machine, blocks, display and wm that
\                   other pages call most (nearby, cross.fs)
\   page 1          (the peephole) machine cross-words files console:
\                   what compiles
\   the next        the fonts, display keymap wm, and the peephole's
\                   table: what draws
\   after them      the applications (apps.fs says which page each has)
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
: small-first ( -- ) small if 1 code-page then ;    \ where pages are few: back to page 1
target

\ Page 1, after the peephole: what compiles.  What reads and writes a
\ block is in the upper half of the kernel's page: everything that reads
\ a disk calls it (blocks.fs says which words).

include machine.fs      \ device registers, high memory, byte-order helpers
include blocks.fs       \ block interface, BLOCK UPDATE FLUSH LOAD

\ What the nucleus needs to compile the system's source as cross.fs does,
\ and what the image says of itself to the decompiler.

include cross-words.fs  \ cross.fs's words, for the nucleus to compile this source

\ What draws, on a page of its own.  Regions and rectangles are in the
\ upper half of the kernel's page whole, with the words of the display
\ and the window manager that are marked there.

meta 0 sys-page target

include mono.fs         \ the fonts: what the applications write in,
include sans.fs         \ and what titles and the menu are in
include spin.fs         \ the disk light in the menu bar
\ what a port writes in until it is told otherwise (plain-font, display.fs)
meta
:: SYSTEM-FONT s" mono" evaluate ;
target

include display.fs      \ the display: a bitmap in memory, and text on it
nearby
include region.fs       \ shapes that are not rectangles, to clip to
include gfx.fs          \ rectangles, and text from the output words
nearby-end
include keymap.fs       \ key codes to ASCII
include wm.fs           \ windows, pointer, event loop

\ What the disks have by label, and the console, which is what a machine
\ that boots from a disk starts in: back on page 1, with what else
\ compiles.  What the console draws with is in the kernel's page.

meta 1 code-page target

include files.fs        \ a source of the disk's by its label, and a file's own names
include console.fs      \ the screen as the console, and the boot from a disk

\ The applications.  A machine that boots from a disk compiles them from
\ their source there: its image is the system without them.

BOOTING? [if] [else]
include apps.fs
[then]

\ The decompiler's table, which names the basewords by their headers.  It
\ is made from an image (util/fused.c), and the image it is made from has
\ the table of no image: so it is laid after all the other data, where
\ how long it is moves nothing, and the image's cell for it is told where
\ it is (fused-at, cross-words.fs).

include-fused           \ fused.fs: instructions as words, for the decompiler

\ What the peephole is told of numbers is a table, and is the last thing
\ laid on its page: what reads an image as code reads none that is after
\ a table on its page.  It is on the page of what draws, so that what
\ page 1 has left is free for what the machine compiles.

meta 0 sys-page target
include folds.fs

\ Every word is where it will be: the tables of names for the decompiler,
\ which go to a file for the disk (build/nuc.tables).

meta t' forth0 t' tstamp name-tables target
