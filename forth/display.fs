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

\ ---- modes and patterns
\
\ What is drawn is a source, in which a set bit is black whatever the
\ display's own polarity, and a mask of the pixels to draw.  The mode says
\ how the source meets what is there already:
\
\     copy-mode   the source replaces it
\     or-mode     black is added where the source is black
\     xor-mode    it is inverted where the source is black
\     bic-mode    it is made white where the source is black
\
\ The mode is a vector, so that the loops which draw do not test it.

INK-BLACK? [if]
: m-copy ( src mask a -- ) >r tuck and swap r> seg! ;
[else]
: m-copy ( src mask a -- ) >r swap invert over and swap r> seg! ;
[then]
: m-or   ( src mask a -- ) >r and r> ink ;
: m-xor  ( src mask a -- ) >r and r> x! ;
: m-bic  ( src mask a -- ) >r and r> paper ;

header 'mode create 'mode 0 ,
header copy-mode : copy-mode ( -- ) ['] m-copy 'mode ! ;
header or-mode   : or-mode   ( -- ) ['] m-or   'mode ! ;
header xor-mode  : xor-mode  ( -- ) ['] m-xor  'mode ! ;
header bic-mode  : bic-mode  ( -- ) ['] m-bic  'mode ! ;

\ A pattern is eight rows of eight pixels, which repeat across and down the
\ screen from its top left corner, so that two shapes filled with the same
\ pattern meet without a seam.  It is kept as eight cells, each a row
\ repeated across the cell: a row of a filled shape is then a row of the
\ pattern stored as it is.

header p-white  create p-white   $00 prow, $00 prow, $00 prow, $00 prow, $00 prow, $00 prow, $00 prow, $00 prow,
header p-black  create p-black   $ff prow, $ff prow, $ff prow, $ff prow, $ff prow, $ff prow, $ff prow, $ff prow,
header p-gray   create p-gray    $aa prow, $55 prow, $aa prow, $55 prow, $aa prow, $55 prow, $aa prow, $55 prow,
header p-ltgray create p-ltgray  $88 prow, $22 prow, $88 prow, $22 prow, $88 prow, $22 prow, $88 prow, $22 prow,
header p-dkgray create p-dkgray  $77 prow, $dd prow, $77 prow, $dd prow, $77 prow, $dd prow, $77 prow, $dd prow,

header pattern create pattern 0 ,   \ the one that slab draws with

\ A slab: n pixels of a row, in the pattern and the mode, a cell at a time.
\ Every filled shape is made of these.  No shift here is by a whole cell,
\ which the machine does not promise is zero.
create hn  0 ,
create psrc 0 ,         \ this row of the pattern
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

header slab
: slab ( x y n -- )
    hn !
    dup d# 7 and pattern @ + @ psrc !
    prow ba ! bn !
    begin
        hn @ 0>
    while
        PXCELL bn @ - hn @ min                      ( pixels in this cell )
        dup leftn bn @ PIXR
        psrc @ swap ba @ 'mode @ execute
        negate hn +!
        d# 0 bn !  d# 1 ba +!
    repeat
;

\ Plain white or black, for the words that take a flag.
header solid : solid ( on? -- ) if p-white else p-black then pattern !  copy-mode ;

\ ---- clipping
\
\ Everything from here on draws only inside the clip rectangle, which is
\ never bigger than the screen.

header clip create clip 0 , 0 , 0 , 0 ,     \ left, top, and past the right and bottom

header clip! : clip! ( x y w h -- )
    >r >r
    over d# 0 max clip !
    dup d# 0 max clip CELL + !
    swap r> + fb-w @ min clip CELL2 + !
    r> + fb-h @ min clip c# 3 + !
;

header noclip : noclip ( -- ) d# 0 d# 0 fb-w @ fb-h @ clip! ;

header clip-to : clip-to ( x y w h -- ) \ what is in both this and the clip rectangle
    >r >r
    over clip @ max clip !
    dup clip CELL + @ max clip CELL + !
    swap r> + clip CELL2 + @ min clip CELL2 + !
    r> + clip c# 3 + @ min clip c# 3 + !
;

create oclip 0 , 0 , 0 , 0 ,
: clip-save ( -- )
    clip @ oclip !  clip CELL + @ oclip CELL + !
    clip CELL2 + @ oclip CELL2 + !  clip c# 3 + @ oclip c# 3 + !
;
: clip-restore ( -- )
    oclip @ clip !  oclip CELL + @ clip CELL + !
    oclip CELL2 + @ clip CELL2 + !  oclip c# 3 + @ clip c# 3 + !
;

: clip-span ( x n -- x' n' ) \ n' is not positive if nothing is left
    over clip @ < if + clip @ tuck - then
    over + clip CELL2 + @ min over -
;

header pat-hline : pat-hline ( x y n -- )
    over dup clip CELL + @ < swap clip c# 3 + @ < 0= or if drop 2drop exit then
    swap >r clip-span r> swap
    dup 0> if slab else drop 2drop then
;

header pat-rect : pat-rect ( x y w h -- )
    begin
        dup 0>
    while
        >r >r
        2dup r@ pat-hline
        1+ r> r> 1-
    repeat
    2drop 2drop
;

\ ---- copying
\
\ A bitmap is two cells: where its first row is, and how many cells from
\ one row to the next.  `screen` is the display, whose rows are where the
\ machine puts them.  copy-bits copies a rectangle of a bitmap to the
\ screen, in the mode.
\
\ A row of the source is first copied aside, so that a row copied onto
\ itself comes out right whichever way it moves; the rows are taken from
\ the bottom up when they move down.  The source and the destination are
\ seldom lined up, so each cell of the destination is made from two of the
\ source, by a shift that is the same for every cell.

header screen create screen 0 , 0 ,

create sbm 0 ,          \ the source, and where in it
create ssx 0 ,
create ssy 0 ,
create bdx 0 ,          \ where on the screen
create bdy 0 ,
create bw 0 ,
create bh 0 ,
create br 0 ,           \ the row being copied
create boff 0 ,         \ pixels to shift the source left by
create bi0 0 ,          \ and the cell of rowbuf the first cell comes from
create bp 0 ,
create rowbuf  meta disp-stride 4 + cell-bytes * target allot

: src-row ( y -- a )
    sbm @ @ ?dup if swap sbm @ CELL + @ * + else row then
;

: row-aside ( y -- ) \ the cells of a source row that are wanted, into rowbuf from its second
    src-row ssx @ PXSHIFT rshift +
    rowbuf 1+
    ssx @ PXMASK and bw @ + PXMASK + PXSHIFT rshift 1+
    begin
        dup
    while
        >r over @ over ! 1+ swap 1+ swap r> 1-
    repeat
    drop 2drop
;

: row-out ( -- ) \ hn pixels from rowbuf to the row at ba, bn
    bi0 @ rowbuf + bp !
    begin
        hn @ 0>
    while
        PXCELL bn @ - hn @ min
        dup leftn bn @ PIXR                         ( pixels mask )
        bp @ @
        boff @ if
            boff @ PIXL  bp @ 1+ @ PXCELL boff @ - PIXR  or
        then
        RAW>SRC swap ba @ 'mode @ execute
        negate hn +!
        d# 0 bn !  d# 1 ba +!  d# 1 bp +!
    repeat
;

header copy-bits : copy-bits ( bitmap sx sy dx dy w h -- )
    bh ! bw ! bdy ! bdx ! ssy ! ssx ! sbm !
    clip @ bdx @ - d# 0 max  dup bdx +!  dup ssx +!  negate bw +!
    clip CELL + @ bdy @ - d# 0 max  dup bdy +!  dup ssy +!  negate bh +!
    clip CELL2 + @ bdx @ - bw @ min bw !
    clip c# 3 + @ bdy @ - bh @ min bh !
    bw @ 0>  bh @ 0>  and 0= if exit then
    ssx @ PXMASK and  bdx @ PXMASK and -  PXCELL +
    dup PXMASK and boff !  PXSHIFT rshift bi0 !
    d# 0 br !
    begin
        br @ bh @ <
    while
        ssy @ bdy @ < if bh @ 1- br @ - else br @ then  ( row )
        dup ssy @ + row-aside
        bdx @ swap bdy @ + prow ba ! bn !
        bw @ hn !  row-out
        d# 1 br +!
    repeat
;

create sdx 0 ,
create sdy 0 ,
create srx 0 ,
create sry 0 ,
create srw 0 ,
create srh 0 ,

header scroll-rect : scroll-rect ( x y w h dx dy -- ) \ move what is in a rectangle; what is uncovered is white
    sdy ! sdx ! srh ! srw ! sry ! srx !
    clip-save
    srx @ sry @ srw @ srh @ clip-to
    copy-mode
    screen srx @ sry @  srx @ sdx @ +  sry @ sdy @ +  srw @ srh @ copy-bits
    p-white pattern !
    sdy @ 0> if srx @ sry @ srw @ sdy @ pat-rect then
    sdy @ 0< if srx @ sry @ srh @ + sdy @ + srw @ sdy @ negate pat-rect then
    sdx @ 0> if srx @ sry @ sdx @ srh @ pat-rect then
    sdx @ 0< if srx @ srw @ + sdx @ + sry @ sdx @ negate srh @ pat-rect then
    clip-restore
;

\ Eight pixels at x: the part of them in the cell x is in, and the part
\ that runs over into the next, which is nothing unless x is in the last
\ seven pixels of the cell.
: over8 ( bits -- here next )
    dup bn @ PIXR swap
    bn @ PXCELL d# 8 - > if PXCELL bn @ - PIXL else drop d# 0 then
;

header xor8
: xor8 ( bits x y -- ) \ invert the pixels where bits are set
    prow ba ! bn !
    EDGE over8
    ?dup if ba @ 1+ x! then
    ba @ x!
;

header display-init
: display-init ( -- ) ['] dplot 'plot !  p-black pattern !  copy-mode  noclip ;

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
create cx 0 ,           \ where on the screen the cell starts
create vx0 0 ,          \ the pixels of the row that are drawn: from here
create vx1 0 ,          \ to here
create r0 0 ,           \ and the rows of the glyphs: from this
create r1 0 ,           \ to this
create gn 0 ,           \ how many glyphs
create gbuf  meta disp-w 8 / 2 + cell-bytes * target allot  \ and where each is

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

: put ( -- ) \ store the cell gathered, and start on the next
    cx @ vx0 @ < 0=  cx @ PXCELL + vx1 @ > 0=  and if
        acc @ da @ !
    else                                            \ at an end: only the pixels to be drawn
        vx1 @ cx @ - d# 0 max PXCELL min leftn
        vx0 @ cx @ - d# 0 max PXCELL min leftn invert and
        ?dup if acc @ over and swap da @ seg! then
    then
    d# 1 da +!  PXCELL cx +!  d# 0 hi !  d# 0 acc !
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

header draw-text : draw-text ( addr u x y -- ) \ clipped to the pixel
    ty ! tx ! tu ! ta !
    clip CELL + @ ty @ - d# 0 max r0 !
    clip c# 3 + @ ty @ - d# 16 min r1 !
    r0 @ r1 @ < 0= if exit then
    tx @ clip @ max vx0 !
    tu @ d# 3 lshift tx @ +  clip CELL2 + @ min vx1 !
    vx0 @ vx1 @ < 0= if exit then
    vx0 @ tx @ - d# 3 rshift                        \ characters wholly off the left
    dup ta +!  dup negate tu +!  d# 3 lshift tx +!
    tx @ 0< if                                      \ and one cut by the screen's edge
        d# 1 ta +!  d# -1 tu +!  d# 8 tx +!
        tx @ vx0 !
    then
    vx1 @ tx @ - d# 7 + d# 3 rshift tu !            \ and off the right
    tu @ 0> 0= if exit then
    glyphs
    italic @ bold @ or inverse @ or
INK-BLACK? [if] [else]
    drop true
[then]
    styled !
    r0 @ tr !
    begin
        tr @ r1 @ <
    while
        ty @ tr @ + row  tx @ PXSHIFT rshift +  da !
        tx @ PXMASK invert and cx !
        tx @ PXMASK and hi !
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
        hi @ if put then
        d# 1 tr +!
    repeat
;

create ch1 0 ,

header draw-char : draw-char ( c x y -- )
    rot ch1 !
    >r >r ch1 >c d# 1 r> r> draw-text
;
