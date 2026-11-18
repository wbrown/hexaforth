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
\     clip0       the rectangle that was asked for (see reclip)
\     text-or     true if text only adds its black to what is there
\     pattern     what fills are made of
\     'mode       how what is drawn meets what is there
\     inverse bold italic     how text looks
\     font        and what it is set in
\
\ Everything that draws takes coordinates in the port.  These words are
\ the port in use.  A port that is not in use is a record of the same
\ eighteen cells, and port! changes from one to another.

create gstate 0 , 0 ,  0 , 0 , 0 , 0 ,  0 , 0 ,  0 , 0 , 0 ,  0 ,  0 ,  0 , 0 , 0 , 0 ,  0 ,
$12 constant PORT-CELLS

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
header text-or : text-or gstate c# 17 + ;

\ Those are for the dictionary.  Code compiled here has the addresses in line.
$0 at: gstate ox        $1 at: gstate oy        $2 at: gstate clip
$6 at: gstate pattern   $7 at: gstate 'mode     $8 at: gstate inverse
$9 at: gstate bold      $a at: gstate italic    $b at: gstate font
$c at: gstate clip-rgn  $d at: gstate clip0     $11 at: gstate text-or

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

\ What a word is working on is on the two stacks, not in variables, so that
\ a word may be in use more than once: by two tasks, or by something that
\ draws while something else is drawing.  A word has the top of the data
\ stack, the cell under it and the top of the return stack to hand.  Where
\ it needs one more, it is made of smaller words that each need no more
\ (slab).
\
\ Where it needs a great deal more (copy-bits, draw-text), what it has in
\ hand is variables, which are an instruction each to reach, all in one
\ run of memory.  The word says while it is running that it is busy, and
\ if it is asked to run again while it is, it first sets that run of
\ memory aside and afterwards puts it back.  So the one who was running
\ finds everything as it was, and it costs nothing unless it happens.
\
\ The memory to set it aside in is allotted at here, and given back by
\ putting dp where it was: a word's locals.
: borrow ( cells -- a ) \ that many cells, for as long as the word that asks is running
    align here swap >c dp +!
    dp @ DATA-LIMIT u> d# 8 -throw
;

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

\ The n leftmost pixels of a cell are looked up, not made: making them is a
\ shift by a whole cell less n, and for none that is a shift by the width
\ of a cell.  A shift is taken modulo 64 (MODEL.md), so with cells of 16
\ and 32 bits that gives nothing, but with 64 it shifts by nothing, and
\ would be every pixel.
create lefts  lefts,

: leftn ( n -- mask ) lefts + @ ;

\ ---- clipping to a region
\
\ A region (region.fs) is clipped to a row at a time.  mrow is a row of the
\ screen with a pixel set wherever the region is, for the rows from mk0 up
\ to mk1, which are all the same in the region.  rmask makes it so for a
\ row, and rm then takes out of a mask the pixels that are not in the
\ region.  With no region neither does anything.  mnone is true if the
\ region has nothing on the row, and whatever draws a row at a time looks
\ at it first and leaves the row alone.
\
\ Where the region is one run of pixels across the row, as a window partly
\ covered at a corner is, there is nothing to mask: mone is true, the run
\ is from mx0 up to mx1, and what draws the row trims what it draws to
\ that, as it does to the clip rectangle.  mmask is true only where a mask
\ is to be used: where there is more than one run, or where mforce says
\ that whoever is drawing does not trim.

create mrow  meta disp-stride cell-bytes * target allot
create mk0 1 ,
create mk1 0 ,
create mbase 0 ,        \ the first cell of the row on the screen
create mnone 0 ,
create mrgn 0 ,         \ the region the mask is of, or 0 if none is needed
create mone 0 ,
create mx0 0 ,
create mx1 0 ,
create mmask 0 ,
create mforce 0 ,

: mspan ( x0 x1 -- ) \ set those pixels of mrow
    fb-w @ min swap d# 0 max tuck -                 ( x n )
    swap dup PXSHIFT rshift mrow + >r  PXMASK and   ( n b ) ( R: the cell )
    begin
        over 0>
    while
        2dup PXCELL swap - min                      ( n b k ) \ pixels in this cell
        tuck leftn swap PIXR  r@ or!                ( n k )
        -  d# 0                                     \ and the next starts at its left
        r> 1+ >r
    repeat
    2drop rdrop
;

\ A region (region.fs) is a count of the cells that follow, and then bands:
\ a count of crossings, the crossings, and where the band ends, which is
\ the row it goes down to plus AS-END.  So the end is further along than
\ any crossing, and ends them: anything from END-MIN up is an end.  The
\ first band has no crossings and ends at the region's top, and the last
\ has none and ends at NEVER, so every row is in a band.
\
\ The three are made from the width of a cell, so that a region is the
\ same thing whatever the cell: END-MIN is half AS-END, and NEVER twice
\ it.  A crossing or a row is then less than END-MIN from nothing, either
\ way: 2^31 with a cell of 64 bits, where AS-END is 2^32 and each of the
\ three is a number one instruction pushes; 2^28 with 32; 2^12 with 16.
meta
1 cell-bits 3 - #32 min lshift
dup target constant AS-END
meta dup 2/ target constant END-MIN
meta 2* target constant NEVER

: mfind ( y -- band ) \ the band of the region that y is in; sets mk0 and mk1
    AS-END + >r                                     ( R: y, as the end of a band is said )
    END-MIN negate mk0 !
    mrgn @ 1+
    begin
        dup dup @ + 1+ @                            ( band end )
        dup r@ > 0=                                 \ it ends at y, or above
    while
        AS-END - mk0 !
        dup @ + CELL2 +
    repeat
    AS-END - mk1 !  rdrop
;

: rmask ( y -- ) \ y on the screen
    mrgn @ 0= if drop exit then
    dup row mbase !
    dup mk0 @ < 0=  over mk1 @ <  and if drop exit then
    d# 0 mrow DISP-STRIDE cells-fill
    d# 0 mone !
    mfind                                           ( band )
    dup @ 0= mnone !
    dup @ d# 2 = if
        true mone !  dup CELL + @ mx0 !  dup CELL2 + @ mx1 !
    then
    dup 1+ swap @ bounds                            ( end p )
    begin
        2dupxor
    while
        dup @ over 1+ @ mspan
        CELL2 +
    repeat
    2drop
    mforce @  mone @ 0=  or mmask !
;

: rm ( mask a -- mask' a )
    mmask @ if dup mbase @ - mrow + @ rot and swap then
;

: span-clip ( x n -- x' n' ) \ what of n pixels from x is in the row's one run
    over mx0 @ < if + mx0 @ tuck - then
    over + mx1 @ min over -
;

: band-stale ( -- ) d# 1 mk0 !  d# 0 mk1 ! ;       \ the band is to be found again

\ Drawing is clipped to a rectangle and to a region.  A region that is a
\ rectangle, as most are, needs no mask: it and the rectangle asked for are
\ one smaller rectangle.  So `clip` is what drawing is clipped to, worked
\ out here from clip0, the rectangle asked for, and the region; and mrgn is
\ the region if a mask is needed for it, or 0.  A region with nothing in it
\ leaves a rectangle with nothing in it.
: reclip ( -- )
    clip0 @ clip !  clip0 CELL + @ clip CELL + !
    clip0 CELL2 + @ clip CELL2 + !  clip0 c# 3 + @ clip c# 3 + !
    d# 0 mrgn !  d# 1 mk0 !  d# 0 mk1 !  d# 0 mnone !
    d# 0 mone !  d# 0 mmask !  d# 0 mforce !
    clip-rgn @ ?dup if
        dup @ d# 8 = if                             \ a rectangle: 0 top, 2 x0 x1 bottom, 0 never
            >r
            r@ c# 4 + @ clip @ max clip !
            r@ CELL2 + @ AS-END - clip CELL + @ max clip CELL + !
            r@ c# 5 + @ clip CELL2 + @ min clip CELL2 + !
            r> c# 6 + @ AS-END - clip c# 3 + @ min clip c# 3 + !
        else
            dup @ d# 2 = if drop  clip @ clip CELL2 + ! else mrgn ! then
        then
    then
    clip CELL2 + @ clip @ >  clip c# 3 + @ clip CELL + @ >  and 0= if
        clip @ clip CELL2 + !  clip CELL + @ clip c# 3 + !      \ nothing at all
    then
;

header rgn-clip! : rgn-clip! ( rgn|0 -- ) clip-rgn !  reclip ;

\ Where the pattern takes the place of what is there and nothing is masked,
\ which is most fills, only the cells at the two ends of the slab hold other
\ pixels that have to be kept.  Every cell between is the row of the
\ pattern, stored as it is: one store, and nothing fetched.
\
\ A slab has four things in hand: the row of the pattern, the cell, how far
\ into it the pixels start, and how many are left.  The row and the cell
\ are on the return stack, the row under the cell, and the other two on
\ the data stack; the row is on top while the cells between the two ends
\ are stored, which is the store's own input.
: under! ( a mask row -- a ) \ the row's pixels into the cell at a, where the mask says
    over and >r  invert over @ and  r> or  over !
;

: fast-slab ( b a row n -- ) \ n pixels from bit b of the cell at a, of a row of the pattern
    swap RAW>SRC >r  swap >r  swap                  ( n b ) ( R: row a )
    2dup + PXCELL > 0= if                           \ all in one cell
        swap leftn swap PIXR  r> swap r> under! drop
        exit
    then
    ?dup if                                         \ the cell it starts part of the way into
        tuck PXCELL in-place - +                    ( b n ) \ what is left after that cell
        swap d# -1 swap PIXR
        r> swap r@ under! 1+ >r
    then
    dup PXSHIFT rshift  r> swap bounds              ( n end a ) ( R: row ) \ the whole cells between
    begin
        2dupxor
    while
        r@ over ! 1+
    repeat
    drop                                            ( n a ) \ the cell after the last whole one
    swap PXMASK and ?dup if                         \ and the cell it ends part of the way into
        leftn r@ under!
    then
    drop rdrop
;

\ The same slab where a region masks it or the mode is not copy: each cell
\ goes through the mode, with what the region leaves of its mask.
: cell-mode ( a mask row -- a )
    >r over rm  r> -rot  'mode @ execute
;

: slow-slab ( b a row n -- )
    swap >r  swap >r  swap                          ( n b ) ( R: row a )
    2dup + PXCELL > 0= if
        swap leftn swap PIXR  r> swap r> cell-mode drop
        exit
    then
    ?dup if
        tuck PXCELL - +
        swap d# -1 swap PIXR
        r> swap r@ cell-mode 1+ >r
    then
    dup PXSHIFT rshift  r> swap bounds              ( n end a ) ( R: row )
    begin
        2dupxor
    while
        d# -1 r@ cell-mode 1+
    repeat
    drop
    swap PXMASK and ?dup if
        leftn r@ cell-mode
    then
    drop rdrop
;

header slab
: slab ( x y n -- )
    >r
    dup d# 7 and pattern @ + @ >r                   ( x y ) ( R: n row )
    prow r> r>                                      ( b a row n )
    mmask @ 0=  'mode @ ['] m-copy =  and if fast-slab exit then
    slow-slab
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
    over d# 0 max clip0 !
    dup d# 0 max clip0 CELL + !
    swap r> + fb-w @ min clip0 CELL2 + !
    r> + fb-h @ min clip0 c# 3 + !
    reclip
;

header clip-screen! : clip-screen! ( x0 y0 x1 y1 -- ) \ on the screen: left, top, past the right and bottom
    clip0 c# 3 + !  clip0 CELL2 + !  clip0 CELL + !  clip0 !
    reclip
;

header noclip : noclip ( -- ) \ the whole screen
    d# 0 clip-rgn !
    d# 0 d# 0 fb-w @ fb-h @ clip-screen!
;

header clip-to : clip-to ( x y w h -- ) \ what is in both this and the clip rectangle
    >r >r >screen
    over clip0 @ max clip0 !
    dup clip0 CELL + @ max clip0 CELL + !
    swap r> + clip0 CELL2 + @ min clip0 CELL2 + !
    r> + clip0 c# 3 + @ min clip0 c# 3 + !
    reclip
;

header clip-rows : clip-rows ( -- top bottom )
    \ the clip's rows, in the port: from top up to bottom
    clip CELL + @ oy @ -  clip c# 3 + @ oy @ - ;

: clip-span ( x n -- x' n' ) \ n' is not positive if nothing is left
    over clip @ < if + clip @ tuck - then
    over + clip CELL2 + @ min over -
;

: rgn-slab ( x y n -- ) \ on the screen, in a row the clip rectangle has: what the region leaves of it
    over rmask
    mnone @ if drop 2drop exit then
    swap >r clip-span  mone @ if span-clip then  r> swap
    dup 0> if slab else drop 2drop then
;

header pat-hline : pat-hline ( x y n -- )
    >r >screen r>
    over dup clip CELL + @ < swap clip c# 3 + @ < 0= or if drop 2drop exit then
    mrgn @ if rgn-slab exit then                    \ a region that is not a rectangle
    swap >r clip-span r> swap
    dup 0> if slab else drop 2drop then
;

\ A rectangle of plain white or plain black, which most are, is the same in
\ every row: the same pixels of the same cells, made the same.  So it is
\ filled a column at a time, a column being one cell of every row: the
\ pixels that are to change are taken out of each cell of the column
\ (and!), and those that are to be set put in (or!); a cell that is all in
\ the rectangle is stored.  That is an instruction for a cell, memory
\ through the ALU to memory, and a few to go round; and what a row would
\ have to work out, where it starts in a cell and which pixels of its two
\ ends are its own, is worked out once.
\
\ A column is gone down by adding the stride, so this is for a display
\ whose rows are one after another in memory, and not for one whose rows
\ are dealt into banks.
BANKED? [if]

: plain-rect ( x y n rows -- ) drop drop 2drop ;
: plain? ( -- f ) false ;

[else]

: col-and! ( x a n -- ) \ in one cell of each of n rows, from the one at a
    DISP-STRIDE * over + swap rot >r                ( end a ) ( R: x )
    begin 2dupxor while  r@ over and!  DISP-STRIDE +  repeat
    2drop rdrop
;

: col-or! ( x a n -- )
    DISP-STRIDE * over + swap rot >r
    begin 2dupxor while  r@ over or!  DISP-STRIDE +  repeat
    2drop rdrop
;

: col! ( x a n -- )
    DISP-STRIDE * over + swap rot >r
    begin 2dupxor while  r@ over !  DISP-STRIDE +  repeat
    2drop rdrop
;

: column ( v mask a n -- ) \ the pixels of v that the mask says, in one cell of each of n rows
    >r >r                                           ( v mask ) ( R: n a )
    dup invert 0= if drop r> r> col! exit then      \ the whole cell
    dup invert r> r> 2dup >r >r col-and!
    and ?dup if r> r> col-or! else rdrop rdrop then
;

\ How n pixels from bit b lie in cells: the mask of the first cell, if they
\ start part of the way into it or end in it; how many whole cells; and the
\ mask of the cell they end part of the way into.  A mask of 0 is no cell.
: spans ( b n -- m1 w m0 )
    2dup + PXCELL > 0= if                           \ all in one cell
        leftn swap PIXR  >r d# 0 d# 0 r> exit
    then
    over if
        over PXCELL - +  swap d# -1 swap PIXR       ( n' m0 )
    else
        nip d# 0
    then
    >r  dup PXMASK and leftn  swap PXSHIFT rshift  r>
;

: strip ( a n mask -- a n ) \ a column of the pattern
    >r 2dup  pattern @ @ RAW>SRC  r> 2swap column
;

: plain-rect ( x y n rows -- ) \ on the screen and clipped; n and rows are more than none
    >r >r prow r> swap >r                           ( b n ) ( R: rows a )
    spans r> r> rot                                 ( m1 w a rows m0 )
    ?dup if strip swap 1+ swap then
    rot negate                                      ( m1 a rows -w )
    begin
        dup
    while
        >r  d# -1 strip swap 1+ swap  r> 1+
    repeat
    drop
    rot ?dup if strip then
    2drop
;

: plain? ( -- f ) \ can what is to be filled go a column at a time?
    pattern @ dup p-white = swap p-black = or
    'mode @ ['] m-copy = and
    mmask @ 0= and
;

[then]

: rows-fill ( x y n rows -- ) \ on the screen and clipped, with nothing masked; n and rows are more than none
    plain? if plain-rect exit then
    >r swap r> over + >r swap                       ( x y n ) ( R: past the last row )
    begin
        over r@ <
    while
        >r 2dup r@ slab
        1+ r>
    repeat
    drop 2drop rdrop
;

\ Under a region that is not a rectangle, a rectangle is a rectangle for
\ each run of each band of the region that it meets: the band's rows that
\ it has, and what the run and it both have across.  Each is filled as any
\ rectangle is, so no mask is made and no row is looked at by itself.  The
\ clip rectangle is trimmed to the rectangle's left and right for as long
\ as that takes, so that a run is clipped to it as anything is.
: runs ( y rows band -- ) \ what the band's runs have of those rows
    -rot >r >r                                      ( band ) ( R: rows y )
    dup 1+ swap @ bounds
    begin
        2dupxor
    while
        dup @  over 1+ @ over -  clip-span          ( end p x n )
        dup 0> if
            r@ swap  r> r@ swap >r  rows-fill       \ x y n rows
        else
            2drop
        then
        CELL2 +
    repeat
    2drop rdrop rdrop
;

: bands-fill ( y yend -- ) \ the rows from y up to yend, a band of the region at a time
    swap  mrgn @ 1+ >r                              ( yend y ) ( R: band )
    begin
        2dup >
    while
        over  r@ dup @ + 1+ @ AS-END -  min  over max      ( yend y e ) \ where the band ends, in these rows
        tuck over -                                 ( yend e y rows )
        dup 0> if r@ runs else 2drop then
        r> dup @ + CELL2 + >r
    repeat
    2drop rdrop
;

\ A rectangle is clipped once, not a row at a time: the rows the clip
\ rectangle has, and the pixels of a row that it has, which are the same
\ in every row.
header pat-rect : pat-rect ( x y w h -- )
    >r >r >screen r> r>                             \ on the screen from here
    >r over r> +  clip c# 3 + @ min  >r             ( x y w ) ( R: past the last row drawn )
    swap  clip CELL + @ max  swap
    >r swap r> clip-span rot swap                   ( x' y n' )
    over r@ <  over 0> and  0= if drop 2drop rdrop exit then    \ nothing of it is left
    mrgn @ 0= if
        over r> swap - rows-fill exit
    then
    d# 0 mmask !  band-stale                        \ no mask is used, and the one there is of no row of this
    r>                                              ( x y n yend )
    clip @ >r  clip CELL2 + @ >r                    ( R: the clip rectangle's left and right )
    >r rot  dup clip !  + clip CELL2 + !  r>        ( y yend )
    bands-fill
    r> clip CELL2 + !  r> clip !
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
\ what was there is kept, so it is not fetched.  (And see move-row, for a
\ copy that needs none of this.)

header screen create screen 0 , 0 ,

\ A copy has more in hand than the stacks hold: what it was asked for, what
\ it works out from that, and a row set aside.  It is all in one run of
\ memory, from copy-state up to copy-end, which copy-bits sets aside if it
\ is asked to copy while it is copying, and puts back after.
header copy-busy create copy-busy 0 ,

header copy-state create copy-state
create sbm 0 ,          \ the source, and where in it
create ssx 0 ,
create ssy 0 ,
create bdx 0 ,          \ where on the screen
create bdy 0 ,
create bw 0 ,
create bh 0 ,
create boff 0 ,         \ pixels to shift the source left by
create bi0 0 ,          \ and the cell of rowbuf the first cell comes from
create bstore 0 ,       \ true in copy mode: a whole cell may be stored
create bp0 0 ,          \ the cell of rowbuf a row's first cell comes from
create bdirect 0 ,      \ true if rows go straight across (move-row)
create rowbuf  meta disp-stride 4 + cell-bytes * target allot
header copy-end create copy-end

: src-row ( y -- a )
    sbm @ @ ?dup if swap sbm @ CELL + @ * + else row then
;

: row-aside ( y -- ) \ the cells of a source row that are wanted, into rowbuf from its second
    src-row ssx @ PXSHIFT rshift +
    rowbuf 1+
    ssx @ PXMASK and bw @ + PXMASK + PXSHIFT rshift 1+
    cells-move
;

\ A row is copied as a slab is filled: the cell it starts part of the way
\ into, the whole cells, and the cell it ends part of the way into.  What
\ is in hand is the cell on the screen, how far into it, how many pixels,
\ and where the pixels come from, which is the cell on the screen plus how
\ far apart the two are: that distance is on the return stack under the
\ cell, and on top of it while the whole cells are done.
: bsrc ( p -- raw ) \ a cell of what is copied, from the cell of rowbuf at p and the next
    boff @ ?dup if                                  \ fetched once: it is in the record
        >r  dup @ r@ PIXL  swap 1+ @ PXCELL r> - PIXR  or
    else
        @
    then
;

: copy-cell ( a mask dist -- a ) \ the pixels of the mask, to the cell at a from rowbuf
    >r over r> + bsrc RAW>SRC cell-mode
;

: whole? ( a -- f ) \ is the whole cell at a to be stored as it comes?
    bstore @ 0= if drop false exit then
    mmask @ if mbase @ - mrow + @ 1+ 0= else drop true then
;

: row-out ( b n dist a -- ) \ n pixels from bit b of the cell at a, from rowbuf dist cells on
    swap >r >r swap                                 ( n b ) ( R: dist a )
    2dup + PXCELL > 0= if                           \ all in one cell
        swap leftn swap PIXR  r> swap r> copy-cell drop
        exit
    then
    ?dup if
        tuck PXCELL - +
        swap d# -1 swap PIXR
        r> swap r@ copy-cell 1+ >r
    then
    dup PXSHIFT rshift  r> swap bounds              ( n end a ) ( R: dist )
    begin
        2dupxor
    while
        dup whole? if
            dup r@ + bsrc over !
        else
            d# -1 r@ copy-cell
        then
        1+
    repeat
    drop
    swap PXMASK and ?dup if
        leftn r@ copy-cell
    then
    drop rdrop
;

: whole-row ( row -- ) \ all of the row that is copied
    bdx @ swap bdy @ + prow                         ( b a )
    bw @ swap  bp0 @ over -  swap                   ( b n dist a )
    row-out
;

\ A copy that only replaces what is there, from cells lined up with the
\ cells they go to, and not from a row onto itself, is a move: every
\ scroll up or down is one.  The cells between the two ends of the row go
\ straight from where they are to where they go, with nothing set aside
\ and nothing asked of each; the cell at either end is merged under its
\ mask.  bdirect is true if the copy is of that kind.

: move! ( a mask dist -- a ) \ the pixels of the mask, to the cell at a from the cell dist on
    >r over r> + @ under!
;

: move-row ( b n dist a -- ) \ n pixels from bit b of the cell at a, from the cell dist cells on
    swap >r >r swap                                 ( n b ) ( R: dist a )
    2dup + PXCELL > 0= if                           \ all in one cell
        swap leftn swap PIXR  r> swap r> move! drop
        exit
    then
    ?dup if
        tuck PXCELL in-place - +
        swap d# -1 swap PIXR
        r> swap r@ move! 1+ >r
    then
    dup PXSHIFT rshift  r> swap bounds              ( n end a ) ( R: dist )
    begin
        2dupxor
    while
        \ Where the cell comes from goes on the return stack, where it is
        \ what the store reads from: two instructions for a cell moved, and
        \ the second takes it off again, the rdrop being joined to it.
        over  meta R->IN IN+N ->R d-1 r+1 alu target    ( R: dist from )
        meta R->IN [IN] ->[T] alu target
        1+ rdrop
    repeat
    drop
    swap PXMASK and ?dup if
        leftn r@ move!
    then
    drop rdrop
;

: row-direct ( row x n -- ) \ n pixels of a row of what is copied, from x on the screen
    >r
    2dup swap bdy @ + prow                          ( row x b a ) ( R: n )
    2swap
    ssx @ bdx @ - +  PXSHIFT rshift
    swap ssy @ + src-row +                          ( b a from )
    over in-place -  swap  r> -rot                  ( b n dist a )
    move-row
;

: (copy-bits) ( bitmap sx sy dx dy w h -- ) \ with a record to work in
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
    bi0 @ rowbuf + bp0 !
    bstore @  boff @ 0=  and
    sbm @ @ 0<>  ssy @ bdy @ <>  or  and  bdirect !
    bh @ negate                                     \ the rows are counted up to nothing
    begin
        dup
    while
        dup bh @ +
        ssy @ bdy @ < if bh @ 1- swap - then        ( count row ) \ from the bottom up if they move down
        mrgn @ if                                   \ a region that is not a rectangle
            dup bdy @ + rmask
            mnone @ if
                drop
            else
                mone @ if                           \ only the part in the row's one run
                    bdx @ bw @ span-clip            ( row x n )
                    dup 0> if
                        bdirect @ if
                            row-direct
                        else
                            >r                      ( row x ) ( R: n )
                            over ssy @ + row-aside
                            dup PXSHIFT rshift  bdx @ PXSHIFT rshift -  bp0 @ +
                            >r  swap bdy @ + prow   ( b a ) ( R: n, where in rowbuf )
                            r> over -  swap  r> -rot
                            row-out
                        then
                    else
                        2drop drop
                    then
                else
                    dup ssy @ + row-aside
                    whole-row
                then
            then
        else
            bdirect @ if
                bdx @ bw @ row-direct
            else
                dup ssy @ + row-aside
                whole-row
            then
        then
        1+
    repeat
    drop
;

header copy-bits : copy-bits ( bitmap sx sy dx dy w h -- )
    copy-busy @ if                                  \ a copy is being made: set it aside
        dp @ >r
        copy-end copy-state - borrow >r             ( R: where memory was, the copy )
        copy-state r@ copy-end copy-state - cells-move
        (copy-bits)
        r> copy-state copy-end copy-state - cells-move
        r> dp !
        exit
    then
    true copy-busy !  (copy-bits)  d# 0 copy-busy !
;

\ What is under a rectangle of the screen, kept aside and put back: the cells it
\ is in, whole, a row of the bitmap for a row of the screen, the first of them
\ the bitmap's first cell.  As much of the rectangle as is on the screen is
\ kept, and it is put back with copy-bits, in the port's clip and region: the
\ two line up, so whole cells go straight across (move-row).
: screen-rect ( x y w h -- x0 y0 x1 y1 ) \ the rectangle on the screen, cut to it
    >r >r >screen  over r> +  over r> +             ( X0 Y0 X1 Y1 )
    fb-h @ min >r  fb-w @ min >r
    d# 0 max swap d# 0 max swap                     ( x0 y0 ) ( R: Y1 X1 )
    over r> max  over r> max
;

: kept-cells ( x0 x1 -- c0 n ) \ the cells from the one x0 is in to the one x1 - 1 is in
    1- PXSHIFT rshift  swap PXSHIFT rshift  tuck - 1+
;

: keep-row ( c0 n a y -- ) \ n cells of row y from its cell c0, to a
    row  >r rot r> +  -rot swap  cells-move
;

header keep-bits : keep-bits ( bitmap x y w h -- ) \ the screen's cells under the rectangle, into the bitmap
    screen-rect                                     ( bm x0 y0 x1 y1 )
    >r  rot swap kept-cells                         ( bm y0 c0 n ) ( R: y1 )
    dup 0> 0= if 2drop 2drop rdrop exit then
    2swap swap  dup 1+ @  r> swap >r >r  @ swap     ( c0 n a y0 ) ( R: stride y1 )
    begin
        dup r@ <
    while
        2over 2over keep-row
        swap  r> r@ swap >r  +  swap  1+
    repeat
    2drop 2drop  rdrop rdrop
;

header put-bits : put-bits ( bitmap x y w h -- ) \ what keep-bits kept of the rectangle, back where it was
    screen-rect
    rot tuck - >r                                   ( bm x0 x1 y0 ) ( R: h )
    -rot over - >r                                  ( bm y0 x0 ) ( R: h w )
    swap oy @ -  swap dup PXMASK and  swap ox @ -   ( bm dy sx dx )
    rot  >r >r d# 0 r> r>                           ( bm sx 0 dx dy )
    r> r>  copy-bits
;

\ Eight pixels at x: the part of them in the cell x is in, and the part
\ that runs over into the next, which is nothing unless x is in the last
\ seven pixels of the cell.
: over8 ( bits b -- here next ) \ b is how far into the cell x is
    >r dup r@ PIXR swap r>                          ( here bits b )
    dup PXCELL d# 8 - > if PXCELL swap - PIXL else 2drop d# 0 then
;

header xor8
: xor8 ( bits x y -- ) \ invert the pixels where bits are set
    prow >r                                         ( bits b ) ( R: the cell )
    swap EDGE swap over8
    ?dup if r@ 1+ x! then
    r> x!
;

\ A column one pixel wide, inverted where the port's pattern has ink: the
\ sides of a window's outline while it is dragged (wm.fs).  pat-rect would
\ do the same, but goes through its whole setup for every row.  It is in
\ screen coordinates, as xor8 is, and leaves out what is off the screen.
\
\ The pattern repeats every 8 rows, so the column is done in 8 passes, one
\ per pattern row.  Each pass works out its bits and its first cell once,
\ then inverts that cell every 8 rows by adding 8 rows' stride; a pattern
\ row with no ink in the column is skipped.  On a banked display the rows
\ are not a fixed stride apart, and each row is found with `row`.

: (column-bits) ( x y -- bits a ) \ the column's pixel in row y, masked by the pattern's row for y; and its cell
    tuck prow >r                                    ( y n ) ( R: the cell )
    d# 1 leftn swap PIXR                            ( y pixel )
    swap d# 7 and pattern @ + @ and  r>
;

BANKED? [if]

header xor-column
: xor-column ( x y n -- ) \ invert column x for n rows from y, where the pattern has ink
    over + fb-h @ min >r  d# 0 max                  ( x y ) ( R: the row after the last )
    over fb-w @ u< 0= if 2drop rdrop exit then      \ the column is off the screen
    begin
        dup r@ <
    while
        2dup (column-bits) x!
        1+
    repeat
    2drop rdrop
;

[else]

: column-run ( bits end a -- ) \ invert bits in the cell at a, and in the same cell every 8 rows down, before end
    rot ?dup 0= if 2drop exit then  >r              ( end a ) ( R: bits )
    begin
        2dup u>
    while
        r@ over x!
        DISP-STRIDE d# 3 lshift +
    repeat
    2drop rdrop
;

: column-pass ( x k end -- ) \ the pass for pattern row k & 7: rows k, k + 8, ... before row end
    >r  over >r  (column-bits)                      ( bits a ) ( R: end x )
    r> PXSHIFT rshift  r> row +                     ( bits a end-cell )
    swap column-run
;

header xor-column
: xor-column ( x y n -- ) \ invert column x for n rows from y, where the pattern has ink
    over + fb-h @ min  swap d# 0 max swap           ( x y end ) \ clipped to the screen's rows
    rot dup fb-w @ u< 0= if drop 2drop exit then  -rot      \ the column is off the screen
    d# -8 >r                                        ( R: passes left, as less than nothing )
    begin
        r@
    while
        3dup column-pass
        swap 1+ swap
        r> 1+ >r
    repeat
    rdrop drop 2drop
;

[then]

\ ---- text
\
\ Text is black on white.  With `inverse` set it is white on black, with
\ `bold` set each stroke is a pixel wider, and with `italic` set the top
\ half of each character leans a pixel to the right.
\
\ With `text-or` set, text that is not inverse only adds its black to what
\ is there, and what is there is taken to be the white it would have had:
\ whoever sets it has made that so.  Then plain text with no region to
\ mask goes straight to the screen, a row of a glyph at a time with or!,
\ and nothing is gathered or stored.
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
\ out, into memory from halloc: four cells for each glyph, and then the
\ rows, each in a cell of its own with the glyph at the left.  The four
\ are where its rows are, how wide it is, the first row that has any black
\ in it, and the row after the last that has.  Drawing reads only that.
\ Text that only adds its black skips the rows above and below: most of a
\ glyph's rows have nothing in them, and a space has none.

create kf 0 ,           \ the font being opened out
create kc 0 ,           \ what it is opened out into
create ki 0 ,           \ the glyph
create kr 0 ,           \ the row
create kx 0 ,           \ where the glyph is in the strike
create kw 0 ,           \ and how wide
create kg 0 ,           \ its cells in what is opened out

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
    kf @ CELL2 + @  dup d# 2 lshift  swap kf @ @ * +  >c halloc kc !
    d# 0 ki !
    begin
        ki @ kf @ CELL2 + @ <
    while
        kf @ c# 5 + ki @ +  dup @ kx !  1+ @ kx @ - kw !
        kc @ ki @ d# 2 lshift + kg !                ( the glyph's four cells )
        kc @  kf @ CELL2 + @ d# 2 lshift +  ki @ kf @ @ * +  ( rows )
        dup kg @ !
        kw @ kg @ CELL + !
        d# 0 kg @ CELL2 + !  d# 0 kg @ c# 3 + !
        d# 0 kr !
        begin
            kr @ kf @ @ <
        while
            krow dup if                             \ a row with black in it:
                kg @ c# 3 + @ 0= if kr @ kg @ CELL2 + ! then    \ the first,
                kr @ 1+ kg @ c# 3 + !                           \ and the last so far
            then
            over kr @ + !
            d# 1 kr +!
        repeat
        drop
        d# 1 ki +!
    repeat
    kc @
;

: font-open ( -- ) \ see that the port's font is opened out
    font @ c# 4 + @ 0= if
        font @ open-font  font @ c# 4 + !
    then
;

\ What is asked of the font is asked of the port's font each time, and
\ nothing is kept of it between.
: glyph ( c -- a ) \ a character's four cells: its rows, its width, and the rows with black in
    font @ CELL + @ in-place -
    dup font @ CELL2 + @ u< 0= if drop d# 0 then
    d# 2 lshift font @ c# 4 + @ +
;

\ Text is characters packed into cells, a cell's lowest eight bits first.
\ It is said by a cell and which character of the cells from there is its
\ first: so it may be anywhere in memory, which the address of a byte
\ cannot say of the upper half where a cell is 16 bits, and a line of a
\ grid of characters is the grid's cell and the line's number times its
\ length.  A character is eight bits on from the one before, so text is
\ gone along by the bit: the cell is that over the bits in a cell, and the
\ character is shifted down by the rest.
header char@ : char@ ( a bit -- c ) \ the character that starts that many bits into the cells from a
    dup PXSHIFT rshift rot + @
    swap PXMASK and rshift
    h# ff and
;

header ctext : ctext ( cstr -- a i u ) \ the text of a counted string that starts where a cell does
    d# 1 over @ h# ff and
;

: >chars ( addr -- a i ) \ the address of a byte as a cell and a character of it
    dup c> swap OFFSET-MASK and
;

header chars-width : chars-width ( a i u -- n ) \ in pixels
    font-open
    rot >r                                          ( i u ) ( R: a )
    d# 3 lshift swap d# 3 lshift swap
    d# 0 -rot bounds                                ( n end bit )
    begin
        2dupxor
    while
        r@ over char@ glyph CELL + @                ( n end bit w )
        >r rot r> + -rot
        d# 8 +
    repeat
    2drop
    r> drop
;

header text-width : text-width ( addr u -- n ) \ of text at the address of a byte
    >r >chars r> chars-width
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

\ Drawing a string has a great deal in hand, and the cells it gathers: far
\ more than the stacks hold.  As with a copy (copy-state), it is all in one
\ run of memory, from text-state up to text-end, which draw-text sets
\ aside if it is asked to draw while it is drawing, and puts back after.
header text-busy create text-busy 0 ,

header text-state create text-state
create ta 0 ,           \ the text: a cell,
create tf 0 ,           \ which character of the cells from there is its first,
create tu 0 ,           \ and how many there are
create tx 0 ,           \ where the first glyph drawn goes
create ty 0 ,
create pen 0 ,          \ where the next glyph goes
create tr 0 ,           \ the row being drawn
create styled 0 ,       \ true if the rows need more than copying
create da 0 ,           \ the cell being stored
create tn 0 ,           \ how many rows are drawn
create tend 0 ,         \ the end of those cells of tband
create gd 0 ,           \ from a cell of tband to the glyph's row for it
create tk 0 ,
create tq 0 ,
create tcm 0 ,          \ the pixels drawn, of the cells being stored
create tor 0 ,          \ true if the text only adds its black
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
create gw 0 ,           \ how wide the one in hand is
create rend 0 ,         \ where the rows of the glyph in hand that are drawn end
create ttrim 0 ,        \ true if the clip rectangle was trimmed to the region's one run
create tcx0 0 ,         \ and what its sides were
create tcx1 0 ,
create tband  meta $20 cell-bytes * target allot    \ the cells being gathered
create tsty   meta $20 cell-bytes * target allot    \ a glyph's rows, styled
create gbuf   meta $80 cell-bytes * target allot    \ the glyphs that will be seen: a cell for each
header text-end create text-end

: glyphs ( -- ) \ note each glyph that will be seen
    d# 0 gn !
    tx @ pen !
    ta @ >r                                         \ the cell is where the fetch has it to hand
    tf @ d# 3 lshift  tu @ d# 3 lshift  bounds      ( end bit )
    begin
        2dupxor
    while
        dup PXSHIFT rshift r@ + @                   \ the cell the character is in,
        over PXMASK and rshift  h# ff and           \ and the character out of it
        glyph dup CELL + @ gw !                     ( end bit glyph )
        pen @ gw @ + clip @ > 0=  pen @ 0<  or      \ off the left, or cut by the screen's edge
        pen @ clip CELL2 + @ < 0= or                \ or off the right
        gn @ MAX-GLYPHS = or  if
            drop
        else
            gn @ 0= if pen @ tx ! then
            gbuf gn @ + !  d# 1 gn +!
            pen @ gw @ + vx1 !                      \ where the last one drawn ends
        then
        gw @ pen +!
        d# 8 +
    repeat
    2drop
    r> drop
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

: put-rows ( -- ) \ the cells gathered take the place of what is there
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
;

: or-rows ( -- ) \ their black is added to what is there
    d# 0
    begin
        dup tn @ <
    while
        dup r0 @ + ty @ +
        dup rmask
        mnone @ if
            drop
        else
            row cx @ PXSHIFT rshift + da !
            tcm @ da @ rm drop
            over tband + @ and  da @ or!
        then
        d# 0 over tband + !
        1+
    repeat
    drop
;

: tflush ( -- ) \ store the cells gathered, and start on the next
    colmask tcm !
    tor @ if or-rows else put-rows then
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

\ ---- text that only adds its black, with nothing to mask: straight to the screen
\
\ This is for a display whose black is a set bit and whose rows follow one
\ another in memory; on any other, such text is gathered and stored as the
\ rest is.

INK-BLACK? [if] BANKED? [if] [else]

: or-plain ( rows dst -- ) \ the rows of a glyph up to rend, hi pixels along, all of them drawn
    >r rend @ swap                                  ( end rows ) ( R: dst )
    begin
        2dupxor
    while
        dup @ hi @ PIXR  r@ or!
        r> DISP-STRIDE + >r
        1+
    repeat
    2drop rdrop
;

: or-right ( rows dst -- ) \ the same, of which only the pixels of tcm are drawn
    >r rend @ swap
    begin
        2dupxor
    while
        dup @ hi @ PIXR tcm @ and  r@ or!
        r> DISP-STRIDE + >r
        1+
    repeat
    2drop rdrop
;

: or-left ( rows dst -- ) \ the part of a glyph that runs over into the next cell
    >r rend @ swap
    begin
        2dupxor
    while
        dup @ tk @ PIXL tcm @ and  r@ or!
        r> DISP-STRIDE + >r
        1+
    repeat
    2drop rdrop
;

: glyph-or ( glyph -- ) \ the rows of a glyph that have black in them and are drawn, at pen
    \ The rows are from a first to a last, and the loops that draw them go
    \ from one place to another: so how many there are is never worked
    \ out, which would be a subtraction here and an addition in each loop.
    dup CELL2 + @ r0 @ max                          ( glyph first )
    over c# 3 + @ r1 @ min                          ( glyph first last )
    2dup< 0= if drop 2drop exit then                \ none of them is drawn
    rot @ tuck + rend !                             ( first where-its-rows-are )
    over +  swap                                    ( rows first )
    pen @ PXMASK invert and cx !  pen @ PXMASK and hi !
    ty @ + row  pen @ PXSHIFT rshift +              ( rows dst )
    colmask
    dup 1+ if
        ?dup if tcm ! 2dup or-right then
    else
        drop 2dup or-plain
    then
    hi @ gw @ + PXCELL > if
        PXCELL hi @ - tk !
        PXCELL cx +!
        colmask ?dup if tcm ! 1+ or-left exit then
    then
    2drop
;

: text-fast ( -- ) \ every glyph that will be seen
    tx @ pen !
    gbuf gn @ bounds
    begin
        2dupxor
    while
        dup @  dup CELL + @ gw !  glyph-or
        gw @ pen +!
        1+
    repeat
    2drop
;

[then] [then]

: text-body ( -- ) \ rows r0 up to r1 of the string, clipped to the clip rectangle
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
INK-BLACK? [if]
    text-or @ inverse @ 0= and tor !
BANKED? [if] [else]
    tor @  styled @ 0= and  mrgn @ 0= ttrim @ or  and if text-fast exit then
[then]
[else]
    d# 0 tor !
[then]
    tband tn @ + tend !
    d# 0 tband tn @ cells-fill
    tx @ PXMASK invert and cx !
    tx @ PXMASK and hi !
    gbuf gn @ bounds
    begin
        2dupxor
    while
        dup @  dup CELL + @ gw !  @                 ( the glyph's rows )
        styled @ if srows then
        r0 @ + tband in-place - gd !
        +rows
        hi @ gw @ +
        dup PXCELL < if
            hi !
        else
            PXCELL in-place -                       ( pixels over )
            tflush
            ?dup if dup hi !  gw @ swap - >rows then
        then
        1+
    repeat
    2drop
    hi @ if tflush then
;

\ A line of text is a few rows, and under a region they are nearly always
\ all in one band of it.  If that band is one run of pixels, the clip
\ rectangle is trimmed to the run for as long as the text is drawn, and
\ nothing is masked.  Otherwise the text is masked a row at a time.
: (draw-chars) ( a i u x y -- ) \ with a record to work in
    ty ! tx ! tu ! tf ! ta !
    ox @ tx +!  oy @ ty +!
    clip CELL + @ ty @ - d# 0 max r0 !
    clip c# 3 + @ ty @ - font @ @ min MAX-ROWS min r1 !
    r0 @ r1 @ < 0= if exit then
    d# 0 ttrim !
    mrgn @ if
        ty @ r0 @ + rmask
        ty @ r1 @ + mk1 @ > 0= if                   \ every row is in this band
            mnone @ if exit then
            mone @ if
                true ttrim !
                clip @ tcx0 !  clip CELL2 + @ tcx1 !
                clip @ mx0 @ max clip !  clip CELL2 + @ mx1 @ min clip CELL2 + !
            then
        then
        ttrim @ 0= if true mforce !  band-stale then
    then
    text-body
    ttrim @ if tcx0 @ clip !  tcx1 @ clip CELL2 + ! then
    mforce @ if d# 0 mforce !  band-stale then
;

header draw-chars : draw-chars ( a i u x y -- ) \ u characters from character i of the cells from a,
    text-busy @ if                                  \ in the port's font, clipped to the pixel
        dp @ >r                                     \ a string is being drawn: set it aside
        text-end text-state - borrow >r             ( R: where memory was, the copy )
        text-state r@ text-end text-state - cells-move
        (draw-chars)
        r> text-state text-end text-state - cells-move
        r> dp !
        exit
    then
    true text-busy !  (draw-chars)  d# 0 text-busy !
;

header draw-text : draw-text ( addr u x y -- ) \ text at the address of a byte
    >r >r >r >chars r> r> r> draw-chars
;

header draw-char : draw-char ( c x y -- ) \ the character is text of one, in a cell taken for it
    dp @ >r
    rot d# 1 borrow tuck !                          ( x y a )
    -rot >r >r  d# 0 d# 1  r> r> draw-chars
    r> dp !
;

\ ---- ports that are not in use

header port create port 0 ,         \ the record of the port in use, or 0
header screen-port create screen-port 0 ,

header port! : port! ( port -- ) \ put the port in use away, and take up another
    port @ ?dup if gstate swap PORT-CELLS cells-move then
    dup port !
    gstate PORT-CELLS cells-move
    reclip
;

header new-port : new-port ( -- port ) \ the whole screen, black, copying, plain text in the system's plain font
    PORT-CELLS cells >c halloc >r
    plain-font r@ c# 11 + !
    fb-w @ r@ c# 4 + !  fb-h @ r@ c# 5 + !
    fb-w @ r@ c# 15 + !  fb-h @ r@ c# 16 + !
    p-black r@ c# 6 + !
    ['] m-copy r@ c# 7 + !
    r>
;

header display-init
: display-init ( -- )
    ['] dplot 'plot !
    d# 0 text-busy !  d# 0 copy-busy !
    d# 0 port !
    new-port dup screen-port ! port!
;
