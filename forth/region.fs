\ region.fs - regions: shapes on the screen that need not be rectangles
\
\ A region says, for each band of rows across which its shape is the same,
\ where along a row it goes from outside to inside and back.  It is a count
\ of the cells that follow, and then bands:
\
\     n  x0 x1 ... x(n-1)  end
\
\ The pixels from x0 up to x1, from x2 up to x3 and so on are inside, in
\ every row from where the band above ended down to this one's end.  The
\ end is the row, plus AS-END: further along than any crossing, so it is
\ also what ends them, and nothing has to count them to know where they
\ stop.  The first band has no crossings and ends at the region's top; the
\ last has none and ends at NEVER.  So every row is in a band, and nothing
\ is special about the top of a region or the bottom.  A region with
\ nothing in it is that one last band.  A rectangle is eight cells: 0 top,
\ 2 x0 x1 bottom, 0 never; and nothing else is eight.
\
\ A band is an even number of cells and a region starts on an even cell,
\ so a crossing in is at an even address and a crossing out at an odd one:
\ a place among the crossings is inside the region if its address is odd.
\ Nothing has to keep whether it is inside.
\
\ Two regions are combined a band at a time.  The crossings of a band of
\ each are merged: the nearer of the two is written where the answer's next
\ would go, and stays there if the answer is not what it was, which the
\ three places say between them.  The nearer of the two ends is the end of
\ the band they make.  A band the same as the one above it is not written:
\ the one above is made to end where it would have.
\
\ Nothing here is kept in a variable, so any number of these can be in
\ hand at once.  Coordinates are those of the screen, and less than
\ END-MIN from nothing either way, which display.fs makes from the width
\ of a cell.  A region is RGN-CELLS cells from halloc, and nothing
\ checks that it fits: the regions of a few windows are a few dozen cells.

$100 constant RGN-CELLS

header rgn-none : rgn-none ( rgn -- ) \ nothing is in it
    d# 2 over !  d# 0 over 1+ !  NEVER swap CELL2 + !
;

header new-rgn : new-rgn ( -- rgn ) \ with nothing in it, and on an even cell
    RGN-CELLS 1+ >c halloc  dup d# 1 and +  dup rgn-none
;

header rgn-empty? : rgn-empty? ( rgn -- f ) @ d# 2 = ;

header rgn-copy : rgn-copy ( from to -- ) over @ 1+ cells-move ;

: band-next ( band -- band' ) dup @ + CELL2 + ;     \ the band after it

\ ---- rectangles

\ The four cells that say where it is are in place: its left and right,
\ and its top and bottom as the ends of bands.  The rest; or nothing, if
\ there is nothing between them.
: rect-seal ( rgn -- )
    >r
    r@ c# 5 + @ r@ c# 4 + @ >  r@ c# 6 + @ r@ CELL2 + @ >  and 0= if r> rgn-none exit then
    d# 8 r@ !  d# 0 r@ 1+ !  d# 2 r@ c# 3 + !
    d# 0 r@ c# 7 + !  NEVER r> c# 8 + !
;

header rect-rgn : rect-rgn ( x y w h rgn -- ) \ make it this rectangle
    >r
    rot AS-END +  dup r@ CELL2 + !                  ( x w h top )
    + r@ c# 6 + !                                   ( x w )
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

\ ---- two regions, combined

\ A merge has two places to read, among the crossings of a band of each
\ region, and one to write, which is on the return stack.  A turn: the
\ nearer of the two crossings is written where the answer's next would go;
\ each place that was at it moves on; and it stays written, the place to
\ write moving on too, if the answer is not what it was.  Whether the
\ answer is inside is what one instruction makes of the two places to
\ read, and whether it was is the place to write.
\
\ The end of a band is a crossing like any other, but that it is the last:
\ the place that was at the nearer end moves on past it, to its region's
\ next band, and the other is left at its own end, with more of its band
\ to come.
\
\ The three merges differ only in that one instruction, so the rest is
\ written once, as what comes before it and what comes after.
meta
:: ADVANCE ( qa qb xa xb -- qa' qb' ) \ each place that was at the nearer crossing moves on
    s" 2dup = if 2drop 1+ swap 1+ swap else < if swap 1+ swap else 1+ then then" evaluate
;
:: MERGE[ ( qa qb qo -- qa' qb' qa' qb' ) ( R: -- qo )
    s" >r begin over @ over @  2dup min dup r@ !  END-MIN < while ADVANCE 2dup" evaluate
;
:: ]MERGE ( qa qb f -- qa' qb' qo' ) ( R: qo -- )
    s" r@ xor d# 1 and r> + >r repeat ADVANCE r>" evaluate
;
target

\ Each leaves the two places where they got to, and where it wrote the end.
: merge-and  ( qa qb qo -- qa' qb' qo' ) MERGE[ and ]MERGE ;
: merge-or   ( qa qb qo -- qa' qb' qo' ) MERGE[ or ]MERGE ;
: merge-diff ( qa qb qo -- qa' qb' qo' ) MERGE[ invert and ]MERGE ;

: same? ( last band -- f ) \ are the count and the crossings of the band those of the last?
    over @ over @ xor if 2drop false exit then      \ not as many
    over - >r                                       ( last ) ( R: how far on the band is )
    dup 1+ swap @ bounds
    begin
        2dupxor
    while
        dup @  over r@ + @  xor if 2drop rdrop false exit then
        1+
    repeat
    2drop rdrop true
;

\ A place that went past its band's end is at the next band, whose count
\ is no end; one that did not is still at the end.
: on? ( band q -- band' ) \ the band to look at next: the one q got to, or the same again
    dup @ END-MIN < if nip else drop then
;

\ The band of each region that is being looked at is on the data stack.  On
\ the return stack are the region being made, the merge, and the last band
\ written.  It starts with a band of nothing, for the first to be the same
\ as: the first band of every region has no crossings.
: rgn-op ( a b to merge -- )
    swap dup >r                                     ( a b merge to ) ( R: to )
    1+  d# 0 over !
    swap >r >r                                      ( a b ) ( R: to merge last )
    1+ swap 1+ swap
    begin
        over 1+ over 1+                             ( a b qa qb )
        r> r@ swap                                  ( a b qa qb merge last ) ( R: to merge )
        dup >r  dup @ + CELL2 +  dup >r 1+          ( a b qa qb merge qo ) ( R: to merge last band )
        swap execute                                ( a b qa' qb' qo' )
        dup r@ - 1- r@ !                            \ how many crossings the band has
        @ >r                                        ( a b qa' qb' ) ( R: to merge last band end )
        rot swap on? >r on? r>                      ( a' b' )
        r> r>                                       ( a b end band ) ( R: to merge last )
        r@ over same? if
            over swap 1- !                          \ the same: the last band goes on down to where this would
        else
            rdrop >r                                \ or this is the last band now
        then
        NEVER =
    until
    2drop
    r> band-next rdrop r>                           ( after to )
    tuck - 1- swap !
;

\ Most regions are rectangles or nothing, and most of what is asked of two
\ regions needs no band of either looked at: nothing and anything, and two
\ rectangles.  `to` must not be either of the regions combined.
header rgn-and : rgn-and ( a b to -- ) \ what is in both
    >r
    over rgn-empty? over rgn-empty? or if 2drop r> rgn-none exit then
    over rect? over rect? and if r> rect-and exit then
    r> ['] merge-and rgn-op
;

header rgn-or : rgn-or ( a b to -- ) \ what is in either
    >r
    over rgn-empty? if nip r> rgn-copy exit then
    dup rgn-empty? if drop r> rgn-copy exit then
    r> ['] merge-or rgn-op
;

header rgn-diff : rgn-diff ( a b to -- ) \ what is in a and not in b
    >r
    over rgn-empty? if 2drop r> rgn-none exit then
    dup rgn-empty? if drop r> rgn-copy exit then
    over rect? over rect? and if                    \ two rectangles that share nothing
        2dup r@ rect-and
        r@ rgn-empty? if drop r> rgn-copy exit then
    then
    r> ['] merge-diff rgn-op
;

\ ---- moving, and the rectangle around

: bands ( rgn -- end band ) dup 1+ swap @ bounds ;  \ to go through its bands

: rgn-down ( dy rgn -- ) \ every band ends that much further down, but the last
    swap >r bands                                   ( end band ) ( R: dy )
    begin
        band-next 2dupxor                           \ is there a band after it?
    while
        r@ over 1- +!
    repeat
    2drop rdrop
;

: rgn-along ( dx rgn -- ) \ every crossing is that much further along
    swap >r bands                                   ( end band ) ( R: dx )
    begin
        2dupxor
    while
        dup 1+ over @ bounds
        begin
            2dupxor
        while
            r@ over +!  1+
        repeat
        2drop
        band-next
    repeat
    2drop rdrop
;

header rgn-offset : rgn-offset ( dx dy rgn -- ) \ move it
    tuck rgn-down rgn-along
;

: rgn-left ( rgn -- x ) \ the least that the crossings of any band start at
    END-MIN >r bands                                \ further along than any crossing
    begin
        2dupxor
    while
        dup @ if dup 1+ @ r> min >r then
        band-next
    repeat
    2drop r>
;

: rgn-right ( rgn -- x ) \ and the most that they end at
    END-MIN negate >r bands
    begin
        2dupxor
    while
        dup @ if dup dup @ + @ r> max >r then
        band-next
    repeat
    2drop r>
;

\ Its top is where its first band ends, and its bottom where the band
\ before the last does.  It must have something in it.
header rgn-box : rgn-box ( rgn -- x y w h ) \ the rectangle around it
    dup >r rgn-left                                 ( x ) ( R: rgn )
    r@ CELL2 + @ AS-END -                           ( x y )
    over r@ rgn-right swap -                        ( x y w )
    over r> dup @ + d# 2 - @ AS-END - swap -        ( x y w h )
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

\
\ It works in three regions of its own, which are next to each other, so
\ that one place on the return stack is all three: taken for as long as it
\ runs and given back, from memory that has been given back if there is
\ enough of that (hreuse, hgive), and if not from what has not been given
\ out.  (Not at here: three regions are more cells than the data space
\ has left.)  How far to move is on the data
\ stack throughout.  Once the regions are made the rectangle itself is not
\ wanted again: what is moved is clipped to the region of what may move,
\ and what is made white to scroll-update, so the rectangle round each will
\ do for it.

$102 constant RGN-STEP      \ from one region to the next, of several taken together

: on-screen ( x y w h -- x' y' w h ) 2swap  oy @ + swap ox @ + swap  2swap ;    \ from the port's coordinates
: in-port   ( x y w h -- x' y' w h ) 2swap  oy @ - swap ox @ - swap  2swap ;    \ and back to them

: from ( dx dy x y -- x-dx y-dy x y ) \ where what goes to x y comes from
    2swap >r >r  over r> -  over r> -  2swap
;

header scroll-rect : scroll-rect ( x y w h dx dy -- )
    scroll-update @ 0= if new-rgn scroll-update ! then
    2swap >r >r 2swap r> r>                         ( dx dy x y w h )
    RGN-STEP d# 3 * >c hreuse >r                    ( R: three regions )
    on-screen r@ rect-rgn                           \ the first: the rectangle
    clip @  clip CELL + @  clip CELL2 + @ clip @ -  clip c# 3 + @ clip CELL + @ -
    r@ RGN-STEP + rect-rgn                          \ the second: the clip rectangle
    r@  r@ RGN-STEP +  r@ RGN-STEP 2* +  rgn-and    \ the third: what is in both
    clip-rgn @ ?dup if                              \ the first: and in the clip region too, where pixels may be
        r@ RGN-STEP 2* +  swap  r@ rgn-and
    else
        r@ RGN-STEP 2* +  r@ rgn-copy
    then
    r@  r@ RGN-STEP +  rgn-copy
    2dup r@ RGN-STEP + rgn-offset                   \ the second: that, moved
    r@  r@ RGN-STEP +  r@ RGN-STEP 2* +  rgn-and    \ the third: the places that have a pixel to come to them
    r@  r@ RGN-STEP 2* +  scroll-update @  rgn-diff \ and those that have none
    clip-rgn @ -rot                                 ( clip dx dy )
    copy-mode
    r@ RGN-STEP 2* + dup rgn-clip!                  ( clip dx dy third )
    dup rgn-empty? if
        drop 2drop
    else
        >r screen -rot r>                           ( clip screen dx dy third )
        rgn-box in-port                             ( clip screen dx dy x y w h )
        >r >r from r> r>
        copy-bits
    then
    p-white pattern !
    scroll-update @ dup rgn-clip!
    dup rgn-empty? if
        drop
    else
        rgn-box in-port pat-rect
    then
    rgn-clip!
    r> RGN-STEP d# 3 * hgive
;
