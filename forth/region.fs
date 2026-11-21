\ region.fs - regions: shapes on the screen that need not
\ be rectangles

\ A region is a count of the cells that follow, and then bands:
\     n  x0 x1 ... x(n-1)  end
\ The pixels from x0 up to x1, x2 up to x3 and so on are inside,
\ in the rows from where the band above ended down to end, which
\ is the row plus AS-END.  The first band has no crossings and
\ ends at the region's top; the last has none and ends at NEVER.
\ A rectangle is 0 top, 2 x0 x1 bottom, 0 never.
\ A region starts on an even cell: a crossing at an odd address
\ goes inside.  Nothing checks that a region fits RGN-CELLS.

$100 constant RGN-CELLS

nearby
header rgn-none : rgn-none ( rgn -- ) \ nothing is in it
    d# 2 over !  d# 0 over 1+ !  NEVER swap CELL2 + !
;
nearby-end

header new-rgn : new-rgn ( -- rgn ) \ with nothing in it
    RGN-CELLS 1+ >c halloc  dup d# 1 and +  dup rgn-none
;

nearby
header rgn-empty? : rgn-empty? ( rgn -- f ) @ d# 2 = ;

header rgn-copy : rgn-copy ( from to -- )
    over @ 1+ cells-move ;

: band-next ( band -- band' )
    dup @ + CELL2 + ;

\ ---- rectangles

\ its sides are in place: the rest, or nothing if it is empty
: rect-seal ( rgn -- )
    >r
    r@ c# 5 + @ r@ c# 4 + @ >  r@ c# 6 + @ r@ CELL2 + @ >
    and 0= if r> rgn-none exit then
    d# 8 r@ !  d# 0 r@ 1+ !  d# 2 r@ c# 3 + !
    d# 0 r@ c# 7 + !  NEVER r> c# 8 + !
;

header rect-rgn : rect-rgn ( x y w h rgn -- )
    >r
    rot AS-END +  dup r@ CELL2 + !  ( x w h top )
    + r@ c# 6 + !                   ( x w )
    over + r@ c# 5 + !
    r@ c# 4 + !
    r> rect-seal
;

: rect? ( rgn -- f ) @ d# 8 = ;

: rect-and ( a b to -- ) \ what two rectangles share
    >r
    over c# 4 + @  over c# 4 + @  max  r@ c# 4 + !
    over c# 5 + @  over c# 5 + @  min  r@ c# 5 + !
    over CELL2 + @  over CELL2 + @  max  r@ CELL2 + !
    c# 6 + @  swap c# 6 + @  min  r@ c# 6 + !
    r> rect-seal
;
nearby-end

\ ---- two regions, combined

\ A merge reads the crossings of a band of each region and
\ writes at R.  A place's low bit is whether it is inside there.
\ A pass writes the nearer crossing and goes past it; one
\ instruction makes the answer of the two places read, and the
\ crossing is kept if that is not the place written's low bit.
meta
:: ADVANCE ( qa qb xa xb -- qa' qb' ) \ past the nearer crossing
    s" 2dup = if 2drop 1+ swap 1+ swap" evaluate
    s" else < if swap 1+ swap else 1+ then then" evaluate
;

:: MERGE[ ( qa qb qo -- qa' qb' qa' qb' ) ( R: -- qo )
    s" >r begin over @ over @  2dup min dup r@ !" evaluate
    s" END-MIN < while ADVANCE 2dup" evaluate
;
:: ]MERGE ( qa qb f -- qa' qb' qo' ) ( R: qo -- )
    s" r@ xor d# 1 and r> + >r repeat ADVANCE r>" evaluate
;

target

: merge-and  ( qa qb qo -- qa' qb' qo' ) MERGE[ and ]MERGE ;
: merge-or   ( qa qb qo -- qa' qb' qo' ) MERGE[ or ]MERGE ;
: merge-diff ( qa qb qo -- qa' qb' qo' )
    MERGE[ invert and ]MERGE ;

nearby
: same? ( last band -- f ) \ the same count and crossings?
    over @ over @ xor if 2drop false exit then
    over - >r  ( last ) ( R: how far on the band is )
    dup 1+ swap @ bounds
    begin
        2dup xor
    while
        dup @  over r@ + @  xor if 2drop rdrop false exit then
        1+
    repeat
    2drop rdrop true
;

: on? ( band q -- band' ) \ q's band, if q went past its end
    dup @ END-MIN < if nip else drop then
;

\ a band of each, merged into the band after the last written
: rgn-band ( a b merge last -- a' b' end band )
    band-next  dup >r  1+       ( a b merge qo ) ( R: band )
    >r >r  over 1+ over 1+  r> r>    ( a b qa qb merge qo )
    swap execute                ( a b qa' qb' qo' )
    dup r@ - 1- r@ !
    @ >r                        ( a b qa' qb' ) ( R: band end )
    rot swap on? >r on? r>      ( a' b' )
    r> r>
;

\ a band the same as the last is the last, ending further down
: rgn-kept ( end band last -- end last' )
    2dup swap same? if
        >r 1- over swap ! r>
    else
        drop
    then
;

: rgn-op ( a b to merge -- )
    swap dup >r  ( a b merge to ) ( R: to )
    1+  d# 0 over !
    swap >r >r  ( a b ) ( R: to merge last )
    1+ swap 1+ swap
    begin
        r> r@ swap dup >r  rgn-band  ( a b end band )
        r> rgn-kept >r  NEVER =
    until
    2drop
    r> band-next rdrop r>                   ( after to )
    tuck - 1- swap !
;

\ `to` must not be either of the regions combined.
header rgn-and : rgn-and ( a b to -- ) \ what is in both
    >r
    over rgn-empty? over rgn-empty? or if
        2drop r> rgn-none exit
    then
    over rect? over rect? and if r> rect-and exit then
    r> ['] merge-and rgn-op
;

header rgn-or : rgn-or ( a b to -- ) \ what is in either
    >r
    over rgn-empty? if nip r> rgn-copy exit then
    dup rgn-empty? if drop r> rgn-copy exit then
    r> ['] merge-or rgn-op
;
nearby-end

header rgn-diff : rgn-diff ( a b to -- ) \ in a and not in b
    >r
    over rgn-empty? if 2drop r> rgn-none exit then
    dup rgn-empty? if drop r> rgn-copy exit then
    over rect? over rect? and if
        2dup r@ rect-and
        r@ rgn-empty? if drop r> rgn-copy exit then
    then
    r> ['] merge-diff rgn-op
;

\ ---- moving, and the rectangle around

nearby
: bands ( rgn -- end band ) \ to go through its bands
    dup 1+ swap @ bounds ;
nearby-end

: rgn-down ( dy rgn -- ) \ every band but the last ends lower
    swap >r bands          ( end band ) ( R: dy )
    begin
        band-next 2dup xor
    while
        r@ over 1- +!
    repeat
    2drop rdrop
;

: band-along ( dx band -- ) \ its crossings further along
    bands swap >r                       ( dx a ) ( R: end )
    dup r@ xor if
        begin  2dup +!  1+  r@ over =  until
    then
    2drop rdrop
;

: rgn-along ( dx rgn -- )
    swap >r bands  ( end band ) ( R: dx )
    begin
        2dup xor
    while
        r@ over band-along  band-next
    repeat
    2drop rdrop
;

header rgn-offset : rgn-offset ( dx dy rgn -- ) \ move it
    tuck rgn-down rgn-along
;

nearby
: rgn-left ( rgn -- x ) \ the least any band starts at
    END-MIN >r bands
    begin
        2dup xor
    while
        dup @ if dup 1+ @ r> min >r then
        band-next
    repeat
    2drop r>
;

: rgn-right ( rgn -- x ) \ and the most any ends at
    END-MIN negate >r bands
    begin
        2dup xor
    while
        dup @ if dup dup @ + @ r> max >r then
        band-next
    repeat
    2drop r>
;

\ the rectangle around it, which must have something in it
header rgn-box : rgn-box ( rgn -- x y w h )
    dup >r rgn-left                           ( x ) ( R: rgn )
    r@ CELL2 + @ AS-END -                     ( x y )
    over r@ rgn-right swap -                  ( x y w )
    over r> dup @ + d# 2 - @ AS-END - swap -  ( x y w h )
;
nearby-end

\ ---- scrolling

\ A pixel goes to a place if the place and where it comes from
\ are both in the rectangle and the clip.  The places nothing
\ comes to are made white, and are scroll-update.

header scroll-update \ a region, on the screen
    create scroll-update 0 ,

$102 constant RGN-STEP  \ from one region to the next of three

: on-screen ( x y w h -- x' y' w h )
    2swap  oy @ + swap ox @ + swap  2swap ;
: in-port   ( x y w h -- x' y' w h )
    2swap  oy @ - swap ox @ - swap  2swap ;

: from ( dx dy x y -- x-dx y-dy x y )
    2swap >r >r  over r> -  over r> -  2swap
;

\ The first region is where pixels may be; the second that,
\ moved; the third the places a pixel comes to.

: scroll-may ( x y w h rgns -- )
    >r  on-screen r@ rect-rgn
    clip @  clip CELL + @  clip CELL2 + @ clip @ -
    clip c# 3 + @ clip CELL + @ -
    r@ RGN-STEP + rect-rgn
    r@  r@ RGN-STEP +  r@ RGN-STEP 2* +  rgn-and
    clip-rgn @ ?dup if
        r@ RGN-STEP 2* +  swap  r> rgn-and
    else
        r@ RGN-STEP 2* +  r> rgn-copy
    then
;

: scroll-to ( dx dy rgns -- dx dy )
    >r  r@  r@ RGN-STEP +  rgn-copy
    2dup r@ RGN-STEP + rgn-offset
    r@  r@ RGN-STEP +  r@ RGN-STEP 2* +  rgn-and
    r@  r@ RGN-STEP 2* +  scroll-update @  rgn-diff
    rdrop
;

: scroll-move ( dx dy third -- )
    copy-mode  dup rgn-clip!
    dup rgn-empty? if drop 2drop exit then
    >r screen -rot r>  ( screen dx dy third )
    rgn-box in-port  ( screen dx dy x y w h )
    >r >r from r> r>
    copy-bits
;

: scroll-white ( -- )
    p-white pattern !
    scroll-update @ dup rgn-clip!
    dup rgn-empty? if drop exit then
    rgn-box in-port pat-rect
;

header scroll-rect : scroll-rect ( x y w h dx dy -- )
    scroll-update @ 0= if new-rgn scroll-update ! then
    2swap >r >r 2swap r> r>  ( dx dy x y w h )
    RGN-STEP d# 3 * >c hreuse >r  ( R: three regions )
    r@ scroll-may  r@ scroll-to
    clip-rgn @ -rot                               ( clip dx dy )
    r@ RGN-STEP 2* + scroll-move
    scroll-white  rgn-clip!
    r> RGN-STEP d# 3 * hgive
;
