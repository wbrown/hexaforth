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

\ ---- the port
\
\ What drawing depends on, other than what is being drawn, is the port:
\
\     ox oy       where on the screen its coordinates start
\     clip        the rectangle it may draw in: left, top, and past the
\                 right and bottom, on the screen
\     clip-rgn    a region it may draw in, as well, or 0 for none
\     pattern     what fills are made of
\     'mode       how what is drawn meets what is there
\     inverse bold italic     how text looks
\     font        and what it is set in
\
\ Everything that draws takes coordinates in the port.  These words are
\ the port in use.  A port that is not in use is a record of the same
\ thirteen cells, and port! changes from one to another.

create gstate 0 , 0 ,  0 , 0 , 0 , 0 ,  0 , 0 ,  0 , 0 , 0 ,  0 ,  0 ,

header ox      : ox      gstate ;
header oy      : oy      gstate CELL + ;
header clip    : clip    gstate CELL2 + ;
header pattern : pattern gstate c# 6 + ;
header 'mode   : 'mode   gstate c# 7 + ;
header inverse : inverse gstate c# 8 + ;
header bold    : bold    gstate c# 9 + ;
header italic  : italic  gstate c# 10 + ;
header font    : font    gstate c# 11 + ;
header clip-rgn : clip-rgn gstate c# 12 + ;

\ Those are for the dictionary.  Code compiled here has the addresses in line.
$0 at: gstate ox        $1 at: gstate oy        $2 at: gstate clip
$6 at: gstate pattern   $7 at: gstate 'mode     $8 at: gstate inverse
$9 at: gstate bold      $a at: gstate italic    $b at: gstate font
$c at: gstate clip-rgn

header origin! : origin! ( x y -- ) oy ! ox ! ;

header row : row ( y -- a ) \ the first cell of row y
BANKED? [if]
    DISP-BANKS u/mod DISP-STRIDE * swap DISP-BANK-STEP * + DISP-BASE +
[else]
    DISP-STRIDE * DISP-BASE +
[then]
;

\ Each is one instruction or two: memory, through the ALU, back to memory.
INK-BLACK? [if]
header ink   : ink   ( mask a -- ) or! ;
header paper : paper ( mask a -- ) swap invert swap and! ;
[else]
header paper : paper ( mask a -- ) or! ;
header ink   : ink   ( mask a -- ) swap invert swap and! ;
[then]

: x!   ( bits a -- ) xor! ;                                 \ invert
: seg! ( bits mask a -- ) dup >r @ swap invert and or r> ! ; \ replace

\ Where a pixel is: its cell, and how far into the cell it is.
: prow ( x y -- n a ) row over PXSHIFT rshift + swap PXMASK and swap ;

: dplot ( on? x y -- )
    prow >r LEFT1 swap PIXR
    swap if r> paper else r> ink then
;

header 'plot create 'plot 0 ,       \ ( on? x y -- )  where plot goes
header plot
: plot ( on? x y -- ) oy @ + swap ox @ + swap 'plot @ execute ;

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

\ ---- clipping to a region
\
\ A region (region.fs) is clipped to a row at a time.  mrow is a row of the
\ screen with a pixel set wherever the region is, for the rows from mk0 up
\ to mk1, which are all the same in the region.  rmask makes it so for a
\ row, and rm then takes out of a mask the pixels that are not in the
\ region.  With no region neither does anything.  mnone is true if the
\ region has nothing on the row, and whatever draws a row at a time looks
\ at it first and leaves the row alone.

create mrow  meta disp-stride cell-bytes * target allot
create mk0 1 ,
create mk1 0 ,
create mbase 0 ,        \ the first cell of the row on the screen
create my 0 ,
create mfound 0 ,
create mnone 0 ,

: mspan ( x0 x1 -- ) \ set those pixels of mrow
    fb-w @ min swap d# 0 max tuck - hn !
    dup PXSHIFT rshift mrow + ba !  PXMASK and bn !
    begin
        hn @ 0>
    while
        PXCELL bn @ - hn @ min
        dup leftn bn @ PIXR  ba @ or!
        negate hn +!
        d# 0 bn !  d# 1 ba +!
    repeat
;

: mfind ( y -- row|0 ) \ the row of the region that y is in; sets mk0 and mk1
    my !
    h# 7fffffff negate mk0 !  h# 7fffffff mk1 !
    d# 0 mfound !
    clip-rgn @ dup 1+ swap @ bounds                 ( end p )
    begin
        2dupxor
    while
        dup @ my @ > if @ mk1 !  drop mfound @ exit then    \ the first row below y
        dup mfound !
        dup CELL + @ + CELL2 +
    repeat
    2drop mfound @
;

: rmask ( y -- ) \ y on the screen
    clip-rgn @ 0= if drop exit then
    dup row mbase !
    dup mk0 @ < 0=  over mk1 @ <  and if drop exit then
    mrow DISP-STRIDE bounds begin 2dupxor while d# 0 over ! 1+ repeat 2drop
    mfind dup 0= mnone !
    ?dup if
        dup @ mk0 !
        dup CELL + @ 0= mnone !
        dup CELL2 + swap CELL + @ bounds            ( end p )
        begin
            2dupxor
        while
            dup @ over 1+ @ mspan
            CELL2 +
        repeat
        2drop
    then
;

: rm ( mask a -- mask' a )
    clip-rgn @ if dup mbase @ - mrow + @ rot and swap then
;

header rgn-clip! : rgn-clip! ( rgn|0 -- ) clip-rgn !  d# 1 mk0 !  d# 0 mk1 !  d# 0 mnone ! ;

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
        ba @ rm  psrc @ -rot  'mode @ execute
        negate hn +!
        d# 0 bn !  d# 1 ba +!
    repeat
;

\ Plain white or black, for the words that take a flag.
header solid : solid ( on? -- ) if p-white else p-black then pattern !  copy-mode ;

\ ---- clipping
\
\ Everything from here on takes coordinates in the port, and draws only
\ inside the clip rectangle, which is never bigger than the screen.

: >screen ( x y -- x' y' ) oy @ + swap ox @ + swap ;

header clip! : clip! ( x y w h -- )
    >r >r >screen
    over d# 0 max clip !
    dup d# 0 max clip CELL + !
    swap r> + fb-w @ min clip CELL2 + !
    r> + fb-h @ min clip c# 3 + !
;

header noclip : noclip ( -- ) \ the whole screen
    d# 0 rgn-clip!
    d# 0 clip !  d# 0 clip CELL + !  fb-w @ clip CELL2 + !  fb-h @ clip c# 3 + !
;

header clip-to : clip-to ( x y w h -- ) \ what is in both this and the clip rectangle
    >r >r >screen
    over clip @ max clip !
    dup clip CELL + @ max clip CELL + !
    swap r> + clip CELL2 + @ min clip CELL2 + !
    r> + clip c# 3 + @ min clip c# 3 + !
;

: clip-span ( x n -- x' n' ) \ n' is not positive if nothing is left
    over clip @ < if + clip @ tuck - then
    over + clip CELL2 + @ min over -
;

header pat-hline : pat-hline ( x y n -- )
    >r >screen r>
    over dup clip CELL + @ < swap clip c# 3 + @ < 0= or if drop 2drop exit then
    over rmask
    mnone @ if drop 2drop exit then
    swap >r clip-span r> swap
    dup 0> if slab else drop 2drop then
;

create py1 0 ,          \ past the last row of a rectangle that is to be drawn

header pat-rect : pat-rect ( x y w h -- ) \ only the rows the clip rectangle has
    >r over r> +  clip c# 3 + @ oy @ - min  py1 !
    swap  clip CELL + @ oy @ - max  swap            ( x y w )
    begin
        over py1 @ <
    while
        >r 2dup r@ pat-hline
        1+ r>
    repeat
    drop 2drop
;

\ ---- copying
\
\ A bitmap is two cells: where its first row is, and how many cells from
\ one row to the next.  `screen` is the display, whose rows are where the
\ machine puts them.  copy-bits copies a rectangle of a bitmap to the
\ screen, in the mode.  (scroll-rect, which moves part of the screen, is
\ in region.fs.)
\
\ A row of the source is first copied aside, so that a row copied onto
\ itself comes out right whichever way it moves; the rows are taken from
\ the bottom up when they move down.  The source and the destination are
\ seldom lined up, so each cell of the destination is made from two of the
\ source, by a shift that is the same for every cell.
\
\ A cell of the destination that is wholly inside what is copied, when the
\ mode is copy and the region has all of it, is just stored: nothing of
\ what was there is kept, so it is not fetched.

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
create bstore 0 ,       \ true in copy mode: a whole cell may be stored
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

: bsrc ( -- raw ) \ the cell of the source that goes to the cell at ba
    bp @ @
    boff @ if
        boff @ PIXL  bp @ 1+ @ PXCELL boff @ - PIXR  or
    then
;

: whole? ( -- f ) \ is the cell at ba to be stored as it comes?
    bn @ if false exit then
    hn @ PXCELL < if false exit then
    bstore @ 0= if false exit then
    clip-rgn @ if ba @ mbase @ - mrow + @ 1+ 0= else true then
;

: row-out ( -- ) \ hn pixels from rowbuf to the row at ba, bn
    bi0 @ rowbuf + bp !
    begin
        hn @ 0>
    while
        whole? if
            bsrc ba @ !
            PXCELL negate hn +!
        else
            PXCELL bn @ - hn @ min
            dup leftn bn @ PIXR                     ( pixels mask )
            bsrc RAW>SRC swap ba @ rm 'mode @ execute
            negate hn +!
        then
        d# 0 bn !  d# 1 ba +!  d# 1 bp +!
    repeat
;

header copy-bits : copy-bits ( bitmap sx sy dx dy w h -- )
    bh ! bw ! bdy ! bdx ! ssy ! ssx ! sbm !
    ox @ bdx +!  oy @ bdy +!
    sbm @ @ 0= if ox @ ssx +!  oy @ ssy +! then      \ the screen, as the port sees it
    clip @ bdx @ - d# 0 max  dup bdx +!  dup ssx +!  negate bw +!
    clip CELL + @ bdy @ - d# 0 max  dup bdy +!  dup ssy +!  negate bh +!
    clip CELL2 + @ bdx @ - bw @ min bw !
    clip c# 3 + @ bdy @ - bh @ min bh !
    bw @ 0>  bh @ 0>  and 0= if exit then
    ssx @ PXMASK and  bdx @ PXMASK and -  PXCELL +
    dup PXMASK and boff !  PXSHIFT rshift bi0 !
    'mode @ ['] m-copy = bstore !
    d# 0 br !
    begin
        br @ bh @ <
    while
        ssy @ bdy @ < if bh @ 1- br @ - else br @ then  ( row )
        dup bdy @ + rmask
        mnone @ if
            drop
        else
            dup ssy @ + row-aside
            bdx @ swap bdy @ + prow ba ! bn !
            bw @ hn !  row-out
        then
        d# 1 br +!
    repeat
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

\ ---- text
\
\ Text is black on white.  With `inverse` set it is white on black, with
\ `bold` set each stroke is a pixel wider, and with `italic` set the top
\ half of each character leans a pixel to the right.
\
\ A font is a strike: all its glyphs side by side in one bitmap as high as
\ the font, and a table of where each starts.  The record is five cells
\ (height, first character, how many, cells in a row of the strike, and
\ one for the system's use), the table, with one more entry for where the
\ last glyph ends, and the strike.  A glyph is as wide as the distance to
\ the next, so a font may be proportional or not.
\
\ A strike is small, but taking a glyph's row out of it is a shift and a
\ mask across two cells.  So the first time a font is used it is opened
\ out, into memory from halloc: two cells for each glyph, where its rows
\ are and how wide it is, and then the rows, each in a cell of its own
\ with the glyph at the left.  Drawing reads only that.

create kf 0 ,           \ the font being opened out
create kc 0 ,           \ what it is opened out into
create ki 0 ,           \ the glyph
create kr 0 ,           \ the row
create kx 0 ,           \ where the glyph is in the strike
create kw 0 ,           \ and how wide

: krow ( -- pixels ) \ row kr of the glyph, at the left of a cell
    kf @ c# 6 + kf @ CELL2 + @ +                    \ the strike, after the table
    kr @ kf @ c# 3 + @ * +  kx @ PXSHIFT rshift +   ( a )
    dup @  kx @ PXMASK and ?dup if PIXL then
    kx @ PXMASK and kw @ + PXCELL > if
        swap 1+ @  PXCELL kx @ PXMASK and - PIXR  or
    else
        nip
    then
    kw @ leftn and
;

: open-font ( font -- a )
    kf !
    kf @ CELL2 + @  dup 2*  swap kf @ @ * +  >c halloc kc !
    d# 0 ki !
    begin
        ki @ kf @ CELL2 + @ <
    while
        kf @ c# 5 + ki @ +  dup @ kx !  1+ @ kx @ - kw !
        kc @  kf @ CELL2 + @ 2* +  ki @ kf @ @ * +  ( rows )
        dup kc @ ki @ 2* + !
        kw @ kc @ ki @ 2* + CELL + !
        d# 0 kr !
        begin
            kr @ kf @ @ <
        while
            krow over kr @ + !
            d# 1 kr +!
        repeat
        drop
        d# 1 ki +!
    repeat
    kc @
;

create fc 0 ,           \ the port's font, opened out,
create ffirst 0 ,       \ its first character
create fcount 0 ,       \ and how many it has

: font-open ( -- )
    font @ c# 4 + @ ?dup 0= if
        font @ open-font dup font @ c# 4 + !
    then
    fc !
    font @ CELL + @ ffirst !
    font @ CELL2 + @ fcount !
;

: glyph ( c -- a ) \ a character's two cells: its rows, and its width
    ffirst @ -
    dup fcount @ u< 0= if drop d# 0 then
    2* fc @ +
;

header text-width : text-width ( addr u -- n ) \ in pixels
    font-open
    d# 0 -rot bounds
    begin
        2dupxor
    while
        dup c@ glyph CELL + @  rot + -rot
        1+
    repeat
    2drop
;

\ A string is drawn a cell of the screen at a time: as many characters as
\ are as wide as a cell, for all the rows of the font at once.  tband is a
\ cell for each row, and a glyph is put in it by setting bits of each: what
\ is worked out for a glyph (where it goes, how wide it is, whether it is
\ styled) is worked out once and not once a row.  When the cells are full
\ they are stored, one below the other, and the part of the glyph that did
\ not fit starts them again.  Only the cells at the two ends of what is
\ drawn hold other pixels that have to be kept.  Which glyphs will be seen
\ is worked out once for the string.

$20 constant MAX-ROWS    \ the highest a font may be

$80 constant MAX-GLYPHS  \ the most that one call will draw

create ta 0 ,           \ the string
create tu 0 ,
create tx 0 ,           \ where the first glyph drawn goes
create ty 0 ,
create pen 0 ,          \ where the next glyph goes
create tr 0 ,           \ the row being drawn
create styled 0 ,       \ true if the rows need more than copying
create da 0 ,           \ the cell being stored
create tband  meta $20 cell-bytes * target allot    \ the cells being gathered
create tsty   meta $20 cell-bytes * target allot    \ a glyph's rows, styled
create tn 0 ,           \ how many rows are drawn
create tend 0 ,         \ the end of those cells of tband
create gd 0 ,           \ from a cell of tband to the glyph's row for it
create tk 0 ,
create tq 0 ,
create tcm 0 ,          \ the pixels drawn, of the cells being stored
create hi 0 ,           \ how far across the cell they reach
create cx 0 ,           \ where on the screen the cell starts
create vx0 0 ,          \ the pixels of the row that are drawn: from here
create vx1 0 ,          \ to here
create r0 0 ,           \ and the rows of the glyphs: from this
create r1 0 ,           \ to this
create m0x 0 ,          \ the cell the drawing starts in,
create m0 0 ,           \ and the pixels of it that are drawn
create m1x 0 ,          \ the cell it ends in,
create m1 0 ,           \ and its
create gn 0 ,           \ how many glyphs will be seen
create gbuf 0 ,         \ and which: a cell for each
create gw 0 ,           \ how wide the one in hand is

: glyphs ( -- ) \ note each glyph that will be seen
    d# 0 gn !
    tx @ pen !
    ta @ tu @ bounds
    begin
        2dupxor
    while
        dup c@ glyph dup CELL + @ gw !              ( end a glyph )
        pen @ gw @ + clip @ > 0=  pen @ 0<  or      \ off the left, or cut by the screen's edge
        pen @ clip CELL2 + @ < 0= or                \ or off the right
        gn @ MAX-GLYPHS = or  if
            drop
        else
            gn @ 0= if pen @ tx ! then
            gbuf @ gn @ + !  d# 1 gn +!
            pen @ gw @ + vx1 !                      \ where the last one drawn ends
        then
        gw @ pen +!
        1+
    repeat
    2drop
;

: style ( pixels n -- pixels' n ) \ a row of a glyph as it goes in the bitmap
    >r
    italic @ if tr @ d# 8 < if d# 1 PIXR then then
    bold @ if dup d# 1 PIXR or then
    r@ leftn and
    inverse @ if r@ leftn xor then
INK-BLACK? [if] [else]
    r@ leftn xor
[then]
    r>
;

: endmask ( x -- mask ) \ the pixels that are drawn, of the cell that starts at x
    vx1 @ over - d# 0 max PXCELL min leftn
    vx0 @ rot - d# 0 max PXCELL min leftn invert and
;

: colmask ( -- mask ) \ the pixels that are drawn, of the cell at cx
    cx @ vx0 @ < 0=  cx @ PXCELL + vx1 @ > 0=  and if d# -1 exit then
    cx @ m0x @ = if m0 @ exit then                  \ at an end: only some of them
    cx @ m1x @ = if m1 @ exit then
    cx @ endmask
;

: tflush ( -- ) \ store the cells gathered, and start on the next
    colmask tcm !
    d# 0
    begin
        dup tn @ <
    while
        dup r0 @ + ty @ +                           ( i y )
        dup rmask
        mnone @ if
            drop
        else
            row cx @ PXSHIFT rshift + da !
            tcm @ da @ rm drop                      ( i mask )
            dup 1+ if
                ?dup if over tband + @ over and swap da @ seg! then
            else
                drop dup tband + @ da @ !
            then
        then
        d# 0 over tband + !
        1+
    repeat
    drop
    PXCELL cx +!  d# 0 hi !
;

: +rows ( -- ) \ the glyph's rows into the cells, hi pixels along
    tend @ tband
    begin
        2dupxor
    while
        dup gd @ + @ hi @ PIXR over or!
        1+
    repeat
    2drop
;

: >rows ( n -- ) \ start the cells with the glyph's rows, less their first n pixels
    tk !
    tend @ tband
    begin
        2dupxor
    while
        dup gd @ + @ tk @ PIXL over !
        1+
    repeat
    2drop
;

: srows ( rows -- rows' ) \ the glyph's rows that are drawn, styled, in tsty
    r0 @ + tq !
    d# 0
    begin
        dup tn @ <
    while
        dup r0 @ + tr !
        tq @ over + @ gw @ style drop  over tsty + !
        1+
    repeat
    drop
    tsty r0 @ -
;

header draw-text : draw-text ( addr u x y -- ) \ in the port's font, clipped to the pixel
    ty ! tx ! tu ! ta !
    ox @ tx +!  oy @ ty +!
    clip CELL + @ ty @ - d# 0 max r0 !
    clip c# 3 + @ ty @ - font @ @ min MAX-ROWS min r1 !
    r0 @ r1 @ < 0= if exit then
    font-open
    glyphs
    gn @ 0= if exit then
    tx @ clip @ max vx0 !
    vx1 @ clip CELL2 + @ min vx1 !
    vx0 @ PXMASK invert and  dup m0x !  endmask m0 !
    vx1 @ 1- PXMASK invert and  dup m1x !  endmask m1 !
    italic @ bold @ or inverse @ or
INK-BLACK? [if] [else]
    drop true
[then]
    styled !
    r1 @ r0 @ - tn !
    tband tn @ + tend !
    tend @ tband begin 2dupxor while d# 0 over ! 1+ repeat 2drop
    tx @ PXMASK invert and cx !
    tx @ PXMASK and hi !
    gbuf @ gn @ bounds
    begin
        2dupxor
    while
        dup @  dup CELL + @ gw !  @                 ( the glyph's rows )
        styled @ if srows then
        r0 @ + tband - gd !
        +rows
        hi @ gw @ +
        dup PXCELL < if
            hi !
        else
            PXCELL -                                ( pixels over )
            tflush
            ?dup if dup hi !  gw @ swap - >rows then
        then
        1+
    repeat
    2drop
    hi @ if tflush then
;

create ch1 0 ,

header draw-char : draw-char ( c x y -- )
    rot ch1 !
    >r >r ch1 >c d# 1 r> r> draw-text
;

\ ---- ports that are not in use

header port create port 0 ,         \ the record of the port in use, or 0
header screen-port create screen-port 0 ,

: cells-move ( from to n -- )
    begin
        dup
    while
        >r over @ over ! 1+ swap 1+ swap r> 1-
    repeat
    drop 2drop
;

header port! : port! ( port -- ) \ put the port in use away, and take up another
    port @ ?dup if gstate swap d# 13 cells-move then
    dup port !
    gstate d# 13 cells-move
    d# 1 mk0 !  d# 0 mk1 !
;

header new-port : new-port ( -- port ) \ the whole screen, black, copying, plain text in mono
    d# 13 cells >c halloc >r
    mono r@ c# 11 + !
    fb-w @ r@ c# 4 + !  fb-h @ r@ c# 5 + !
    p-black r@ c# 6 + !
    ['] m-copy r@ c# 7 + !
    r>
;

header display-init
: display-init ( -- )
    ['] dplot 'plot !
    gbuf @ 0= if MAX-GLYPHS >c halloc gbuf ! then
    d# 0 port !
    new-port dup screen-port ! port!
;
