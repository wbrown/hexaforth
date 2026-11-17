\ picture.fs - the system with the picture alone: blocks, the display,
\ regions, windows, the TIFF reader, the dither, and the picture's viewer.
\
\ It is a list of the files of system.fs, the whole desktop, less the
\ applications: the Forth window, the editor, the calculator, the
\ decompiler and HexaPaint.  nuc.fs includes it in place of system.fs when
\ cross.fs is told SYSTEM=picture.fs.
\
\ What it is for is the machine with cells of 16 bits.  What that machine
\ gives out at run time (halloc) is what is above its display and above
\ the data laid down there: the fonts opened out, and the decompiler's
\ table of instructions as words.  With the whole desktop that leaves too
\ little for the bitmap of the picture at its own size, 32KB, and the rows
\ the dither works in.  This has no decompiler, and one font: windows'
\ titles and the menu are in sans, and with nothing else to write, sans is
\ what a port writes in.
\
\ The pages of code are as system.fs has them, without what is not here.

include machine.fs      \ device registers, high memory, byte-order helpers
include blocks.fs       \ block interface, BLOCK UPDATE FLUSH LOAD
include vmdisk.fs       \ block driver for the reference VM's disk
include sans.fs         \ the font
: plain-font ( -- font ) sans ;                     \ what a port writes in until it is told otherwise
include display.fs      \ the display: a bitmap in memory, and text on it
include region.fs       \ shapes that are not rectangles, to clip to

meta 0 sys-page target

include tiff.fs        \ uncompressed TIFF from blocks, a row at a time
include atkinson.fs     \ Atkinson dithering to the display
include keymap.fs       \ key codes to ASCII
include gfx.fs          \ rectangles, and text from the output words
include wm.fs           \ windows, pointer, event loop

meta 1 sys-page target

include view.fs         \ image viewer
include gray.fs        \ a picture kept as gray, drawn through a mask
include viewmenu.fs     \ the viewer's menu: how the picture is drawn
include viewdesk.fs     \ the picture's window, by itself
include join.fs        \ instructions joined as they are compiled, and the rules
