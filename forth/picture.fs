\ picture.fs - a system with only the picture viewer: blocks, the display,
\ regions, the console, windows, the TIFF reader, the dither, and the viewer.
\
\ It is system.fs's list of files without the applications: the Forth
\ window, the editor, the calculator, the decompiler and HexaPaint.  nuc.fs
\ includes it instead of system.fs when cross.fs is run with
\ SYSTEM=picture.fs.
\
\ It is for the 16-bit machine.  There, halloc gives out the memory above
\ the display and the data placed there: the opened-out fonts and the
\ decompiler's table of instructions as words.  With the whole desktop,
\ that leaves too little for the picture's full-size bitmap (32KB) plus
\ the dither's rows.  This system has no decompiler and only one text
\ font, sans, used for titles, the menu and everything else.  (The disk
\ light in the menu bar is a small font of its own.)
\
\ The files are in system.fs's order, and between them this says which
\ page what follows goes on.  Page 1 has the peephole first (nuc.fs), what
\ the rest is built on, and the picture's reader and the dither; the
\ display, regions and rectangles have the next; the window manager, the
\ picture kept as gray and its viewer the one after; and what the disks
\ have, the console and the picture's window the last.

include machine.fs      \ device registers, high memory, byte-order helpers
include blocks.fs       \ block interface, BLOCK UPDATE FLUSH LOAD
include sans.fs         \ the font
include spin.fs         \ the disk light in the menu bar
\ a port's font until it is set (plain-font, display.fs)
meta
:: SYSTEM-FONT s" sans" evaluate ;
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
