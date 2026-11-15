\ graphics.fs - what draws, for a machine smaller than the one the desktop
\ is on
\
\ The display, regions, rectangles and the dither are written for a cell
\ of any width, and this is how that is held to: a machine with cells of
\ 32 bits or of 16, given a display, has these files and no others of
\ system.fs, and the tests of drawing, of regions and of the dither are run
\ on it as they are on the machine with cells of 64.
\
\ Code is where an execution token can reach.  A token is 15 bits in a
\ header where a cell is 16, so with such a cell everything is below
\ instruction $8000: page 1, and then the first of the two pages that
\ definitions made at run time have, which leaves them the second.  With a
\ cell of 32 bits that page is page 8, as on the big machine.

include machine.fs      \ device registers, high memory, byte-order helpers
include blocks.fs       \ block interface, and disk-read
include font.fs         \ the fonts
include display.fs      \ the display: a bitmap in memory, and text on it
include region.fs       \ shapes that are not rectangles, to clip to
include gfx.fs          \ rectangles, and text from the output words

meta code-base $1000 / code-page target

include tiff.fs         \ uncompressed TIFF from blocks into memory
include atkinson.fs     \ Atkinson dithering to the display
include join.fs         \ instructions joined as they are compiled, and the rules
