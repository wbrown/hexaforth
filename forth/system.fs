\ system.fs - blocks, the display, devices and the desktop
\
\ The order of this list is the order code is laid in.
\   the kernel's upper half   what other pages call most (nearby)
\   page 1                    what compiles
\   the next                  what draws
\   after them                the applications (apps.fs)

meta
: big-page ( n -- ) addr-bits #16 > if sys-page else drop then ;
: small-page ( n -- ) addr-bits #16 > if drop else sys-page then ;
: small-first ( -- ) small if 1 code-page then ;
target

\ ---- page 1, after the peephole: what compiles

include machine.fs      \ device registers, high memory, byte-order helpers
include blocks.fs       \ block interface, BLOCK UPDATE FLUSH LOAD
include literals.fs     \ numbers, characters and strings in source
include conditional.fs  \ [if] [else] [then]
include divide.fs       \ / mod
include placement.fs    \ where data and code are laid
include macros.fs       \ :: on the machine
include pixels.fs       \ pictures and rows of pixels in source
include names.fs        \ names source uses that the kernel has not
include image.fs        \ what the image says of itself

\ ---- what draws

meta 0 sys-page target

include mono.fs         \ the fonts: what the applications write in,
include sans.fs         \ and what titles and the menu are in
include spin.fs         \ the disk light in the menu bar
meta
:: SYSTEM-FONT s" mono" evaluate ;  \ plain-font, display.fs
target

include display.fs      \ the display: a bitmap in memory, and text on it
nearby
include region.fs       \ shapes that are not rectangles, to clip to
include gfx.fs          \ rectangles, and text from the output words
nearby-end
include keymap.fs       \ key codes to ASCII
include wm.fs           \ windows, pointer, event loop

\ ---- page 1 again

meta 1 code-page target

include files.fs        \ a source of the disk's by its label, and a file's own names
include console.fs      \ the screen as the console, and the boot from a disk

BOOTING? [if] [else]
include apps.fs
[then]

\ after all the other data: how long it is moves nothing
include-fused           \ fused.fs: instructions as words, for the decompiler

\ a table of instructions: the last thing laid on its page
meta 0 sys-page target
include folds.fs

meta t' forth0 t' tstamp name-tables target
