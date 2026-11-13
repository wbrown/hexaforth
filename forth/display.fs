\ display.fs - the display: a bitmap in memory, one bit to a pixel
\
\ The machine definition says where the bitmap is, how big, which end of a
\ cell is the left, whether a set bit is black, and where each row starts
\ (see MODEL.md).  cross.fs turns those into the words in capitals used
\ here, and nothing outside this file knows them: all drawing is built on
\ row, the address of a row; on a mask for the pixels wanted; and on ink
\ and paper, which make the pixels of a mask black and white.  No drawing
\ steps from one row to the next by adding to an address.
\
\ Eight pixels at a time are a byte with the pixels in the machine's order,
\ as the font has them.  PIXR moves pixels to the right by a count and PIXL
\ to the left; EDGE takes such a byte to the left end of a cell and LEFT8
\ takes the leftmost eight pixels of a cell as such a byte.
\
\ A cell is as many pixels across as it has bits, which is the machine's
\ too: PXCELL, with PXSHIFT and PXMASK to divide by it.
\
\ In the flags these words take, true is white.

header fb-w create fb-w  meta disp-w target ,   \ width and height in pixels
header fb-h create fb-h  meta disp-h target ,

header row : row ( y -- a ) \ the first cell of row y
BANKED? [if]
    DISP-BANKS u/mod DISP-STRIDE * swap DISP-BANK-STEP * + DISP-BASE +
[else]
    DISP-STRIDE * DISP-BASE +
[then]
;

INK-BLACK? [if]
header ink   : ink   ( mask a -- ) dup >r @ or r> ! ;
header paper : paper ( mask a -- ) dup >r @ swap invert and r> ! ;
[else]
header paper : paper ( mask a -- ) dup >r @ or r> ! ;
header ink   : ink   ( mask a -- ) dup >r @ swap invert and r> ! ;
[then]

: x!   ( bits a -- ) dup >r @ xor r> ! ;                    \ invert
: seg! ( bits mask a -- ) dup >r @ swap invert and or r> ! ; \ replace

\ Where a pixel is: its cell, and how far into the cell it is.
: prow ( x y -- n a ) row over PXSHIFT rshift + swap PXMASK and swap ;

: dplot ( on? x y -- )
    prow >r LEFT1 swap PIXR
    swap if r> paper else r> ink then
;

header 'plot create 'plot 0 ,       \ ( on? x y -- )  where plot goes
header plot
: plot ( on? x y -- ) 'plot @ execute ;

\ A run of n pixels to the right, a cell at a time.  No shift here is by a
\ whole cell, which the machine does not promise is zero.
create hon 0 ,
create hn  0 ,
create ba  0 ,          \ the cell being drawn in
create bn  0 ,          \ and how far into it

: leftn ( n -- mask ) \ the n leftmost pixels of a cell
    ?dup 0= if d# 0 exit then
LEFT-LOW? [if]
    PXCELL swap - d# -1 swap rshift
[else]
    PXCELL swap - d# -1 swap lshift
[then]
;

header hspan
: hspan ( on? x y n -- )
    hn ! prow ba ! bn ! hon !
    begin
        hn @ 0>
    while
        PXCELL bn @ - hn @ min                      ( pixels in this cell )
        dup leftn bn @ PIXR
        hon @ if ba @ paper else ba @ ink then
        negate hn +!
        d# 0 bn !  d# 1 ba +!
    repeat
;

\ Eight pixels at x: the part of them in the cell x is in, and the part
\ that runs over into the next, which is nothing unless x is in the last
\ seven pixels of the cell.
: over8 ( bits -- here next )
    dup bn @ PIXR swap
    bn @ PXCELL d# 8 - > if PXCELL bn @ - PIXL else drop d# 0 then
;

header blit8
: blit8 ( bits x y -- ) \ draw eight pixels; a set bit is white
    prow ba ! bn !
INK-BLACK? [if]
    h# ff xor
[then]
    EDGE over8  M8 over8                            ( here next mhere mnext )
    ?dup if rot swap ba @ 1+ seg! else nip then
    ba @ seg!
;

header xor8
: xor8 ( bits x y -- ) \ invert the pixels where bits are set
    prow ba ! bn !
    EDGE over8
    ?dup if ba @ 1+ x! then
    ba @ x!
;

header display-init
: display-init ( -- ) ['] dplot 'plot ! ;

\ ---- text
\
\ Text is black on white.  With `inverse` set it is white on black, with
\ `bold` set each stroke is a pixel wider, and with `italic` set the top
\ half of each character leans a pixel to the right.

header inverse create inverse 0 ,
header bold create bold 0 ,
header italic create italic 0 ,

\ A string is drawn a row of pixels at a time, not a character at a time.
\ The rows of its characters are gathered into a cell, and the cell is
\ stored when it is full: one store for as many characters as a cell is
\ wide.  Only the cells at the two ends of the string hold other pixels
\ that have to be kept.
\
\ Everything that is the same for every row is done once for the string:
\ where each character's glyph is, and whether any styling is wanted.  A
\ glyph is 16 bytes, a row of eight pixels in each, so a row is a byte of
\ one of the glyph's cells: which cell, and how far up it, is worked out
\ once for each row from the size of a cell.

create ta 0 ,           \ the string
create tu 0 ,
create tx 0 ,           \ where it goes
create ty 0 ,
create tr 0 ,           \ the row being drawn
create roff 0 ,         \ the cell of a glyph that row is in
create rsh 0 ,          \ and the shift that brings it down
create styled 0 ,       \ true if the rows need more than copying
create da 0 ,           \ the cell being gathered
create acc 0 ,          \ its pixels so far
create hi 0 ,           \ how far across the cell they reach
create m 0 ,            \ the pixels of the cell that are the string's
create lo 0 ,           \ the first cell: where the string starts in it,
create lmask 0 ,        \ and its pixels from there on
create emask 0 ,        \ the last cell: its pixels up to the string's end
create gn 0 ,           \ how many glyphs
create gbuf  meta disp-w 8 / cell-bytes * target allot  \ and where each is

: glyphs ( -- ) \ find the glyph of each character
    d# 0 gn !
    ta @ tu @ bounds
    begin
        2dupxor
    while
        dup c@ d# 32 - dup d# 94 u> if drop d# 0 then
        GLYPH-CELLS * font +
        gn @ gbuf + !  d# 1 gn +!
        1+
    repeat
    2drop
;

: style ( bits -- bits' ) \ a row as it goes in the bitmap
    italic @ if tr @ d# 8 < if d# 1 PIXR h# ff and then then
    bold @ if dup d# 1 PIXR or h# ff and then
    inverse @ if h# ff xor then
INK-BLACK? [if] [else]
    h# ff xor
[then]
;

: put ( -- ) \ store a cell that has been filled, and start on the next
    m @ 1+ if
        acc @ m @ da @ seg!                         \ the first, if it is not all text
        d# -1 m !
    else
        acc @ da @ !
    then
    d# 1 da +!  d# 0 hi !  d# 0 acc !
;

: +row ( bits -- ) \ eight more pixels
    EDGE
    dup hi @ PIXR acc @ or acc !
    hi @ d# 8 + dup PXCELL < if hi ! drop exit then
    PXCELL - >r                                     ( bits ) ( r: pixels over )
    put
    r@ if
        d# 8 r@ - PIXL acc !
        r@ hi !
    else
        drop
    then
    rdrop
;

header draw-text : draw-text ( addr u x y -- ) \ whole characters, where they fit
    ty ! tx ! tu ! ta !
    ty @ fb-h @ d# 16 - u> if exit then
    begin                                           \ off the left
        tu @ 0>  tx @ 0<  and
    while
        d# 1 ta +!  d# -1 tu +!  d# 8 tx +!
    repeat
    fb-w @ tx @ - d# 0 max d# 3 rshift tu @ min tu ! \ off the right
    tu @ 0> 0= if exit then
    glyphs
    italic @ bold @ or inverse @ or
INK-BLACK? [if] [else]
    drop true
[then]
    styled !
    tx @ PXMASK and lo !
    lo @ leftn invert lmask !
    tu @ d# 3 lshift lo @ + PXMASK and leftn emask !
    d# 0 tr !
    begin
        tr @ d# 16 <
    while
        ty @ tr @ + row  tx @ PXSHIFT rshift +  da !
        lo @ hi !  lmask @ m !
        d# 0 acc !
        tr @ BYTESHIFT rshift roff !
        tr @ OFFSET-MASK and d# 3 lshift rsh !
        gbuf gn @ bounds
        begin
            2dupxor
        while
            dup @ roff @ + @  rsh @ rshift h# ff and
            styled @ if style then
            +row
            1+
        repeat
        2drop
        hi @ if acc @  m @ emask @ and  da @ seg! then  \ the last, if it is not full
        d# 1 tr +!
    repeat
;

create ch1 0 ,

header draw-char : draw-char ( c x y -- )
    rot ch1 !
    >r >r ch1 >c d# 1 r> r> draw-text
;
