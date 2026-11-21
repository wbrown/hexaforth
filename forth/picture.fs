\ picture.fs - a system with only the picture viewer, for the
\ 16-bit machine (SYSTEM=picture.fs)

include machine.fs      \ device registers, high memory, byte-order helpers
include blocks.fs       \ block interface, BLOCK UPDATE FLUSH LOAD
include sans.fs         \ the font
include spin.fs         \ the disk light in the menu bar
meta
:: SYSTEM-FONT s" sans" evaluate ;  \ plain-font, display.fs
target

meta 0 sys-page target

include display.fs      \ the display: a bitmap in memory, and text on it
include region.fs       \ shapes that are not rectangles, to clip to

meta 1 code-page target

include tiff.fs        \ uncompressed TIFF from blocks, a row at a time
include atkinson.fs     \ Atkinson dithering to the display

meta 1 sys-page target

include keymap.fs       \ key codes to ASCII

meta 0 sys-page target

include gfx.fs          \ rectangles, and text from the output words

meta 1 sys-page target

include wm.fs           \ windows, pointer, event loop

meta 2 sys-page target

include files.fs        \ what the disks have by its label, and a file's own names
include console.fs      \ the screen as the console, and the boot from a disk

meta 1 sys-page target

include gray.fs       \ a picture kept as gray, drawn through a mask
include view.fs         \ image viewer, and its menu: how it is drawn

meta 2 sys-page target

include viewdesk.fs     \ the picture's window, by itself
include folds.fs     \ what the peephole is told of numbers: a table, last
