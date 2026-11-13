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

create qa 0 ,           \ the crossings of each still to come, and how many
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
    ca @ ?dup if dup CELL2 + qa !  CELL + @ qan ! else d# 0 qan ! then
    cb @ ?dup if dup CELL2 + qb !  CELL + @ qbn ! else d# 0 qbn ! then
    d# 0 ina !  d# 0 inb !  d# 0 ino !
    po @ CELL2 + qo !
    begin
        qan @ qbn @ or
    while
        qan @ if qa @ @ else FAR then
        qbn @ if qb @ @ else FAR then  min          ( x )
        qan @ if dup qa @ @ = if ina @ invert ina !  d# 1 qa +!  d# -1 qan +! then then
        qbn @ if dup qb @ @ = if inb @ invert inb !  d# 1 qb +!  d# -1 qbn +! then then
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
    begin
        dup
    while
        1-
        dup pl @ CELL2 + + @  over po @ CELL2 + + @  <> if drop false exit then
    repeat
    drop true
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

\ `to` must not be either of the regions combined.
header rgn-and  : rgn-and  ( a b to -- ) d# 0 rgn-op ;  \ what is in both
header rgn-or   : rgn-or   ( a b to -- ) d# 1 rgn-op ;  \ what is in either
header rgn-diff : rgn-diff ( a b to -- ) d# 2 rgn-op ;  \ what is in a and not in b
