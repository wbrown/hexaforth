\ region.fs - regions: shapes on the screen that need not be rectangles
\
\ A region says, for each row of pixels where its shape changes, where
\ along that row it goes from outside to inside and back.  It is a count
\ of the cells that follow, and then rows:
\
\     y  n  x0 x1 ... x(n-1)
\
\ From row y down to the y of the next row, the pixels from x0 up to x1,
\ from x2 up to x3 and so on are inside.  The last row has n of zero and
\ marks the bottom.  A region with nothing in it has a count of zero.  A
\ rectangle is six cells: y 2 x0 x1, then the y past its bottom and 0.
\
\ Two regions are combined a band at a time: wherever either changes, the
\ two lists of crossings are merged, counting in and out of each, and a
\ crossing is kept where the answer changes.  A band the same as the one
\ above it is not written.
\
\ Coordinates are those of the screen.  A region is RGN-CELLS cells from
\ halloc, and nothing checks that it fits: the regions of a few windows
\ are a few dozen cells.

$100 constant RGN-CELLS
$7fffffff constant FAR      \ further down or along than anything

header new-rgn : new-rgn ( -- rgn ) RGN-CELLS >c halloc ;   \ with nothing in it

header rgn-empty? : rgn-empty? ( rgn -- f ) @ 0= ;

header rgn-copy : rgn-copy ( from to -- ) over @ 1+ cells-move ;

create rx 0 ,
create ry 0 ,
create rw 0 ,
create rh 0 ,

header rect-rgn : rect-rgn ( x y w h rgn -- ) \ make it this rectangle
    >r rh ! rw ! ry ! rx !
    rw @ 0>  rh @ 0>  and 0= if d# 0 r> ! exit then
    d# 6 r@ !
    ry @ r@ 1+ !  d# 2 r@ CELL2 + !
    rx @ r@ c# 3 + !  rx @ rw @ + r@ c# 4 + !
    ry @ rh @ + r@ c# 5 + !  d# 0 r> c# 6 + !
;

create opc 0 ,          \ 0: in both  1: in either  2: in the first and not the second
create pa 0 ,           \ the next row of the first region, and where its rows end
create ea 0 ,
create pb 0 ,           \ and of the second
create eb 0 ,
create ca 0 ,           \ the row of each that the band is in, or 0 above its top
create cb 0 ,
create ro 0 ,           \ the region being made
create po 0 ,           \ the row being written
create pl 0 ,           \ the last row written, or 0

create qa 0 ,           \ the crossings of each still to come, and how many, below nothing:
                        \ the count goes up to nothing, which is an add of one
create qan 0 ,
create qb 0 ,
create qbn 0 ,
create ina 0 ,          \ true while inside each
create inb 0 ,
create ino 0 ,          \ and inside the answer
create qo 0 ,           \ where the next crossing of the answer goes

: next-y ( p e -- y ) over = if drop FAR else @ then ;

: inside ( -- f )
    opc @ 0= if ina @ inb @ and exit then
    opc @ d# 1 = if ina @ inb @ or exit then
    ina @ inb @ invert and
;

: merge ( -- n ) \ the crossings of the band, written after po's two cells
    ca @ ?dup if dup CELL2 + qa !  CELL + @ negate qan ! else d# 0 qan ! then
    cb @ ?dup if dup CELL2 + qb !  CELL + @ negate qbn ! else d# 0 qbn ! then
    d# 0 ina !  d# 0 inb !  d# 0 ino !
    po @ CELL2 + qo !
    begin
        qan @ qbn @ or
    while
        qan @ if qa @ @ else FAR then
        qbn @ if qb @ @ else FAR then  min          ( x )
        qan @ if dup qa @ @ = if ina @ invert ina !  d# 1 qa +!  d# 1 qan +! then then
        qbn @ if dup qb @ @ = if inb @ invert inb !  d# 1 qb +!  d# 1 qbn +! then then
        inside dup ino @ <> if
            ino !  qo @ !  d# 1 qo +!
        else
            2drop
        then
    repeat
    qo @ po @ CELL2 + -
;

: same? ( -- f ) \ are the crossings at po those of the last row written?
    pl @ CELL + @ po @ CELL + @ <> if false exit then
    po @ CELL + @
    pl @ CELL2 +  po @ CELL2 + over - >r            ( n last ) ( R: from the last row to this )
    swap bounds
    begin
        2dupxor
    while
        dup @  over r@ + @  <> if 2drop rdrop false exit then
        1+
    repeat
    2drop rdrop true
;

: band ( y -- ) \ write the band that starts at y, unless it changes nothing
    po @ !
    merge po @ CELL + !
    pl @ if same? else po @ CELL + @ 0= then if exit then
    po @ pl !
    po @ dup CELL + @ + CELL2 + po !
;

: rgn-op ( a b to op -- )
    opc ! ro !
    dup 1+ pb !  dup @ + 1+ eb !
    dup 1+ pa !  dup @ + 1+ ea !
    d# 0 ca !  d# 0 cb !  d# 0 pl !
    ro @ 1+ po !
    begin
        pa @ ea @ next-y  pb @ eb @ next-y  min     ( y )
        dup FAR <>
    while
        dup pa @ ea @ next-y = if pa @ ca !  pa @ dup CELL + @ + CELL2 + pa ! then
        dup pb @ eb @ next-y = if pb @ cb !  pb @ dup CELL + @ + CELL2 + pb ! then
        band
    repeat
    drop
    po @ ro @ - 1- ro @ !
;

\ Most regions are rectangles or nothing, and most of what is asked of two
\ regions needs no band of either looked at: nothing and anything, and two
\ rectangles.  A rectangle is six cells, y 2 x0 x1 and y 0, and nothing
\ else is.
create ga 0 ,
create gb 0 ,
create gt 0 ,
: 3rgn ( a b to -- ) gt ! gb ! ga ! ;
: rect? ( rgn -- f ) @ d# 6 = ;
: none ( -- ) d# 0 gt @ ! ;
: both-rect? ( -- f ) ga @ rect?  gb @ rect?  and ;

: rect-and ( -- ) \ what two rectangles share
    ga @ c# 3 + @  gb @ c# 3 + @  max               ( x0 )
    ga @ 1+ @      gb @ 1+ @      max               ( x0 y0 )
    over ga @ c# 4 + @  gb @ c# 4 + @  min  swap -  ( x0 y0 w )
    over ga @ c# 5 + @  gb @ c# 5 + @  min  swap -  ( x0 y0 w h )
    gt @ rect-rgn
;

: apart? ( -- f ) \ do two rectangles share nothing?
    ga @ c# 4 + @  gb @ c# 3 + @  > 0=
    gb @ c# 4 + @  ga @ c# 3 + @  > 0=  or
    ga @ c# 5 + @  gb @ 1+ @      > 0=  or
    gb @ c# 5 + @  ga @ 1+ @      > 0=  or
;

\ `to` must not be either of the regions combined.
header rgn-and : rgn-and ( a b to -- ) \ what is in both
    3rgn
    ga @ rgn-empty?  gb @ rgn-empty?  or if none exit then
    both-rect? if rect-and exit then
    ga @ gb @ gt @ d# 0 rgn-op
;

header rgn-or : rgn-or ( a b to -- ) \ what is in either
    3rgn
    ga @ rgn-empty? if gb @ gt @ rgn-copy exit then
    gb @ rgn-empty? if ga @ gt @ rgn-copy exit then
    ga @ gb @ gt @ d# 1 rgn-op
;

header rgn-diff : rgn-diff ( a b to -- ) \ what is in a and not in b
    3rgn
    ga @ rgn-empty? if none exit then
    gb @ rgn-empty? if ga @ gt @ rgn-copy exit then
    both-rect? if apart? if ga @ gt @ rgn-copy exit then then
    ga @ gb @ gt @ d# 2 rgn-op
;

create odx 0 ,
create ody 0 ,

header rgn-offset : rgn-offset ( dx dy rgn -- ) \ move it
    >r ody ! odx !
    r@ 1+ r> @ bounds
    begin
        2dupxor
    while
        ody @ over +!
        dup CELL2 + over CELL + @ bounds
        begin
            2dupxor
        while
            odx @ over +!  1+
        repeat
        2drop
        dup CELL + @ + CELL2 +
    repeat
    2drop
;

create bx0 0 ,
create bx1 0 ,
create by0 0 ,
create by1 0 ,

header rgn-box : rgn-box ( rgn -- x y w h ) \ the rectangle around it
    FAR bx0 !  FAR negate bx1 !
    dup 1+ swap @ bounds
    dup @ by0 !
    begin
        2dupxor
    while
        dup @ by1 !
        dup CELL + @ if
            dup CELL2 + @ bx0 @ min bx0 !
            dup dup CELL + @ + 1+ @ bx1 @ max bx1 !
        then
        dup CELL + @ + CELL2 +
    repeat
    2drop
    bx0 @ by0 @  bx1 @ bx0 @ -  by1 @ by0 @ -
;

\ ---- scrolling
\
\ scroll-rect moves what is in a rectangle of the port.  Only what can be
\ seen can be moved: a pixel is moved to a place if both the place and
\ where it comes from are in the rectangle, in the clip rectangle and in
\ the clip region.  The places that are in all three but have nothing to
\ come from are made white and left as a region in scroll-update, for
\ whoever called to draw again: the strip that was uncovered, and whatever
\ would have come from under something else.

header scroll-update create scroll-update 0 ,   \ a region, on the screen

create sdx 0 ,
create sdy 0 ,
create srx 0 ,
create sry 0 ,
create srw 0 ,
create srh 0 ,
create s-rect 0 ,       \ regions: the rectangle,
create s-clip 0 ,       \ the clip rectangle,
create s-both 0 ,       \ what is in both,
create s-all 0 ,        \ and in the clip region too: where pixels may be
create s-from 0 ,       \ that, moved
create s-move 0 ,       \ the places that have a pixel to come to them
create s-old 0 ,        \ the port's clip region

: scroll-regions ( -- )
    s-rect @ if exit then
    new-rgn s-rect !  new-rgn s-clip !  new-rgn s-both !  new-rgn s-all !
    new-rgn s-from !  new-rgn s-move !  new-rgn scroll-update !
;

header scroll-rect : scroll-rect ( x y w h dx dy -- )
    sdy ! sdx ! srh ! srw ! sry ! srx !
    scroll-regions
    srx @ ox @ +  sry @ oy @ +  srw @ srh @  s-rect @ rect-rgn
    clip @  clip CELL + @  clip CELL2 + @ clip @ -  clip c# 3 + @ clip CELL + @ -
    s-clip @ rect-rgn
    s-rect @ s-clip @ s-both @ rgn-and
    clip-rgn @ if
        s-both @ clip-rgn @ s-all @ rgn-and
    else
        s-both @ s-all @ rgn-copy
    then
    s-all @ s-from @ rgn-copy
    sdx @ sdy @ s-from @ rgn-offset
    s-all @ s-from @ s-move @ rgn-and
    s-all @ s-move @ scroll-update @ rgn-diff
    clip-rgn @ s-old !
    s-move @ rgn-clip!
    copy-mode
    screen srx @ sry @  srx @ sdx @ +  sry @ sdy @ +  srw @ srh @ copy-bits
    scroll-update @ rgn-clip!
    p-white pattern !
    srx @ sry @ srw @ srh @ pat-rect
    s-old @ rgn-clip!
;
