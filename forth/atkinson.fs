\ atkinson.fs - Atkinson dithering of the image tiff.fs has read

\ Each pixel is thresholded, and one eighth of its error goes to
\ each of six neighbours:
\             *  1  1
\          1  1  1
\             1

\ A pixel is a lane of a cell, sixteen bits: its gray and what
\ its neighbours gave it, and then its error over eight and 128,
\ with whether it is black at bit 12.  Six neighbours each give
\ 128 too much, so a lane is 768 over its gray: black is under
\ 896.

meta
: each-lane ( -- x ) 0 lanes 0 do #16 lshift 1 or loop ;
each-lane constant each1        \ 1 in each lane

\ times this, bit 0 of each lane is at the top of the cell, in
\ the display's order
: to-top ( -- x )
    0 lanes 0 do
        1
        disp-left-low if
            cell-bits lanes - i +
        else
            cell-bits 1- i -
        then  i #16 * -  lshift or
    loop
;
to-top constant gather
:: NIBSHIFT \ the top of a cell, less its pixels
    cell-bits lanes - literal ;

\ a cell's lowest lane to its highest, its highest to its
\ lowest, and each lane added to those either side
:: LOW>TOP    lanes 1 > if
    cell-bits #16 - literal s" lshift" evaluate
then ;
:: TOP>LOW    lanes 1 > if
    cell-bits #16 - literal s" rshift" evaluate
then ;

:: EACH-WAY   lanes 1 > if
    s" dup h# 10001 * swap d# 16 rshift +" evaluate
then ;

\ whether each lane's pixel is black, a bit each
:: BLACK-BITS ( e -- n )
    lanes 1 = if
        disp-left-low if
            $1000 literal s" and" evaluate
            cell-bits 1- #12 - literal s" lshift" evaluate
        else
            #12 literal s" rshift" evaluate
            1 literal s" and" evaluate
        then
    else
        s" anib" evaluate
        disp-left-low if s" NIBSHIFT lshift" evaluate then
    then
;

\ four grays added in a lane that had AROUND, quartered
:: AROUND     each1 2 * literal ;
:: QUARTERED ( x -- y )
    lanes 1 = if
        s" d# 2 rshift" evaluate
    else
        s" d# 2 rshift gray-mask @ and" evaluate
    then
;

:: CLEAN ( x -- y ) \ each lane without whether it is black
    lanes 1 = if
        $fff literal s" and" evaluate
    else
        s" aclean @ and" evaluate
    then
;

\ a cell's pixels: e is what they give, and x' what they leave
\ for the first two of the next cell.  A black lane gets 256
\ more and bit 15: over eight, its gray / 8 + 128 with bit 12
\ set.  A white one: its gray less 255, / 8 + 128.
:: CELL-DONE ( x -- e x' )
    lanes 0 do
        s" dup d# 16 rshift swap h# fff and" evaluate
        s" dup d# 896 <  h# 80ff and + 1+" evaluate \ black?
        s" d# 3 rshift" evaluate
        s" tuck h# 10001 * +" evaluate  \ to the next two
    loop
    s" >r" evaluate
    lanes 1- 0 ?do s" d# 16 lshift +" evaluate loop
    s" r>" evaluate
;
target

W32+? [if]
create aone    meta each1 target ,          \ 1 in each lane
create aclean  meta each1 $fff * target ,   \ each lane's number
create agather meta gather target ,
[then]

create dth 0 ,          \ the dither in hand: its record

: abm   ( -- a ) dth @ ;          \ the bitmap
: an    ( -- a ) dth @ 1+ ;       \ cells of lanes with pixels
: acur  ( -- a ) dth @ CELL2 + ;  \ the row being done,
: anxt  ( -- a ) dth @ c# 3 + ;   \ the next,
: anx2  ( -- a ) dth @ c# 4 + ;   \ and the one after, as lanes
: ay    ( -- a ) dth @ c# 5 + ;   \ how many rows are done
: aimg  ( -- a ) dth @ c# 6 + ;   \ the image it is of
: afull ( -- a ) dth @ c# 7 + ;   \ a whole row's cells
: aoff  ( -- a ) dth @ c# 8 + ;   \ the first lane kept,
: ay0   ( -- a ) dth @ c# 9 + ;   \ the first row kept,
: akh   ( -- a ) dth @ c# 10 + ;  \ and how many are
: arow  ( -- a ) dth @ c# 11 + ;  \ ( lanes -- ) a row's grays
: aw    ( -- a ) dth @ c# 12 + ;  \ pixels across

: agr   ( -- a ) dth @ c# 13 + ;  \ lanes grays are gathered in

: alanes ( x a -- ) \ a row of lanes, and a cell at each end
    1-  afull @ d# 4 lshift d# 2 +  cells-fill
;

: anew ( x -- a )
    afull @ d# 4 lshift d# 2 + >c halloc 1+  tuck alanes ;

\ what the pixel beside each end of the row would give it
: apreset ( a -- )
    d# 128 over +!
    aw @ 1-  dup LANES 1- and d# 4 lshift d# 128 swap lshift
    ( a last x )
    swap LANESHIFT rshift rot + +!
;

\ a gray into the lane at that shift of the cell at a
: lane+ ( shift a g -- shift' a' )
    >r over r> swap lshift  over +!
    swap d# 16 +  dup CELLBITS = if drop 1+ d# 0 then  swap
;

: agrays ( lanes -- ) \ the next row's grays, each to its lane
    'grays @ ?dup if execute exit then
    d# 0 swap  tw @ negate  ( shift a -n )
    begin
        dup
    while
        >r  'pixel @ execute lane+  r> 1+
    repeat
    drop 2drop
;

: aown ( lanes -- ) \ at the image's own size
    aimg @ tiff-row  agrays
;

: apairs ( lanes -- ) \ the next row's grays, two to a lane
    'pairs @ ?dup if execute exit then
    d# 0 swap  tw @ d# 1 rshift negate  ( shift a -n )
    begin
        dup
    while
        >r  'pixel @ execute 'pixel @ execute + lane+  r> 1+
    repeat
    drop 2drop
    tw @ d# 1 and if 'pixel @ execute drop then
;

: aquarters ( lanes -- ) \ a quarter of each sum, to its lane
    agr @ - >r  ( R: from the sums to the lanes )
    agr @ an @ bounds
    begin
        2dup xor
    while
        dup @ QUARTERED  over r@ + +!
        1+
    repeat
    2drop rdrop
;

: ahalf-row ( lanes -- ) \ two rows of the image, as one
    AROUND agr @ an @ cells-fill
    aimg @ tiff-row  agr @ apairs  aimg @ tiff-row
    agr @ apairs
    aquarters
;

\ a cell of lanes as two cells, each lane twice
meta
:: TWICE ( y -- lo hi )
    lanes 4 = if
        s" dup HALFMASK and  dup d# 16 lshift or" evaluate
        s" pair-mask @ and  h# 10001 *" evaluate
        s" swap HALFBITS rshift  dup d# 16 lshift or" evaluate
        s" pair-mask @ and  h# 10001 *" evaluate
    then
    lanes 2 = if
        s" dup HALFMASK and h# 10001 *" evaluate
        s" swap d# 16 rshift h# 10001 *" evaluate
    then
    lanes 1 = if s" dup" evaluate then
;
target

: atwice ( lanes -- ) \ each lane at agr to two of the row
    >r  agr @  an @ 1+ u2/  bounds
    ( end p ) ( R: the two cells it adds to )
    begin
        2dup xor
    while
        dup @ TWICE  r@ 1+ +!  r@ +!
        r> CELL2 + >r  1+
    repeat
    2drop rdrop
;

: atwice-row ( lanes -- ) \ a row of the image, as two
    ay @ d# 1 and 0= if
        d# 0 agr @ an @ 1+ u2/ cells-fill
        aimg @ tiff-row  agr @ agrays
    then
    atwice
;

\ every pixel of the row: its lane is left as what it gives.
\ Pixels before the first give no error, so 128 each: twice to
\ the first lane and once to the second (h# 800100).
W32+? [if]
: apixels ( lanes -- )
    h# 800100 >r  \ what the two before the first would leave
    an @ bounds
    begin
        2dup xor
    while
        dup @ r> +  CELL-DONE  >r over !
        1+
    repeat
    2drop rdrop
;
[then]

W32+? [if] [else]
: apixels ( lanes -- ) \ the same, a cell being one pixel
    d# 128 >r  ( lanes ) ( R: what the last pixel gave )
    d# 256 over +!
    an @ bounds
    begin 2dup xor while
        dup @  dup d# 896 <  h# 80ff and + 1+  d# 3 rshift
        h# fff over r@ + and  ( end p gives owed )
        swap rdrop >r         ( end p owed ) ( R: gives )
        swap r@ over !        ( end owed p )
        1+ tuck +!            ( end p' )
    repeat
    2drop rdrop
;
[then]

: atrim ( lanes -- ) \ lanes past the last pixel are 0
    aw @ LANES 1- and ?dup if
        d# 4 lshift d# 1 swap lshift 1-  ( lanes mask )
        swap an @ + 1- and!
    else
        drop
    then
;

W32+? [if]
: anib ( e -- n ) \ whether each pixel of a cell is black
    d# 12 rshift aone @ and  agather @ *  NIBSHIFT rshift
;
[then]

\ the pixels whose lanes start at p, as a cell of the bitmap
: acell-bits ( p -- p' bits )
    d# 0 >r  ( p ) ( R: the cell being made )
    dup d# 16 + swap
    begin 2dup xor while
        dup @ BLACK-BITS
LEFT-LOW? [if]
        r> LANES rshift  or >r
[else]
        r> LANES lshift  or >r
[then]
        1+
    repeat
    nip r>
INK-BLACK? [if] [else] invert [then]
;

: abits ( lanes dst -- ) \ the row's pixels, to the bitmap
    dup abm @ CELL + @ + swap  ( lanes end dst )
    rot >r                     ( end dst ) ( R: the lanes )
    begin
        2dup xor
    while
        r> acell-bits swap >r
        over ! 1+
    repeat
    2drop rdrop
;

: adown2 ( from to -- ) \ each error to the pixel two rows below
    over - >r  ( from ) ( R: how far on the other row is )
    an @ bounds
    begin
        2dup xor
    while
        dup @ CLEAN  over r@ + +!
        1+
    repeat
    2drop rdrop
;

: adown1 ( from to -- ) \ and to the three on the row below
    over - 1- >r  ( from ) ( R: from a cell to the one before )
    an @ bounds
    begin 2dup xor while
        dup @ CLEAN  over r@ +         ( end p e t )
        over LOW>TOP over +!  CELL2 +  over TOP>LOW swap +!
        EACH-WAY  over r@ + 1+ +!  1+
    repeat
    2drop rdrop
;

\ ---- a dither, a row at a time

: awidths ( -- ) \ the cells of a row of the bitmap, and lanes
    aw @ PXMASK + PXSHIFT rshift afull !
    aw @ LANES 1- + LANESHIFT rshift an !
;

\ rows above the first give 128 a neighbour: the first row has
\ four such neighbours, the second one
: alane-rows ( -- ) \ as the rows that are not there leave them
    d# 512 LANE1 * anew acur !
    d# 128 LANE1 * anew dup apreset anxt !
    d# 0 anew dup apreset anx2 !
;

\ into a bitmap laid as the display is; xt is its arow
: (dither) ( bitmap img x y w h xt -- dither )
    d# 14 >c halloc dth !  arow !
    akh !  >r  ay0 !  LANESHIFT rshift aoff !  r>
    swap dup aimg ! image!  swap abm !             ( w )
    PXMASK + PXSHIFT rshift abm @ CELL + !
    tw @  arow @ ['] ahalf-row = if d# 1 rshift then
    arow @ ['] atwice-row = if 2* then  aw !  awidths
    arow @ ['] aown <> if an @ >c halloc agr ! then
    alane-rows
    \ a bitmap that has no memory is given it, last, to be kept
    abm @ @ 0= if abm @ CELL + @ akh @ * >c halloc abm @ ! then
    pixels  d# 0 ay !  dth @
;

\ The w by h of the picture as it is dithered, from x, y: x is
\ where a cell of the bitmap starts.
header dither-part \ at the image's size
: dither-part ( bitmap img x y w h -- dither )
    ['] aown (dither)
;

header dither-half \ at half of it
: dither-half ( bitmap img x y w h -- dither )
    ['] ahalf-row (dither)
;

header dither-twice \ at twice it
: dither-twice ( bitmap img x y w h -- dither )
    ['] atwice-row (dither)
;

header dither-start \ the whole image
: dither-start ( bitmap img -- dither )
    dup image!  d# 0 d# 0 tw @ th @ dither-part
;

header dither-row : dither-row ( dither -- ) \ the next row
    dth !
    acur @  dup arow @ execute
    dup apixels  dup atrim           ( lanes )
    ay @ ay0 @ -  dup akh @ u< if
        abm @ CELL + @ *  abm @ @ +  ( lanes dst )
        swap aoff @ + swap abits
    else  2drop  then
    acur @ anx2 @ adown2
    acur @ anxt @ adown1
    acur @  anxt @ acur !  anx2 @ anxt !  dup anx2 !
    d# 0 over alanes  apreset
    d# 1 ay +!
;

header atkinson
: atkinson ( bitmap img -- ) \ the whole image
    dither-start
    begin
        ay @ th @ <
    while
        dup dither-row
    repeat
    drop
;

\ ---- to sixteen levels

\ A lane is a sample, the average of a square of pixels.  A
\ table gives its level at bit 12 and its error below.  Levels
\ are 17 grays apart.  The table is by the lane's value from
\ 760, which is gray -8 under its bias of 768, for 272 values.

meta
:: LANES*     lanes 1 > if s" aone @ *" evaluate then ;
:: GRAY-BITS  lanes 1 > if s" gray-mask @ and" evaluate then ;

\ a cell's samples, as CELL-DONE; the table is on R
:: CELL-LEVELS ( x -- e x' )
    lanes 0 do
        s" dup d# 16 rshift swap h# fff and" evaluate
        s" r@ + @" evaluate
        s" tuck h# fff and h# 10001 * +" evaluate
    loop
    s" >r" evaluate
    lanes 1- 0 ?do s" d# 16 lshift +" evaluate loop
    s" r>" evaluate
;

\ plane pb of the levels of a cell's lanes, a bit each
variable pb
:: PLANE-BITS ( e -- n )
    lanes 1 = if
        disp-left-low if
            3 pb @ - ?dup if literal s" lshift" evaluate then
            1 cell-bits 1- lshift literal s" and" evaluate
        else
            #12 pb @ + literal s" rshift d# 1 and" evaluate
        then
    else
        #12 pb @ + literal  s" rshift aone @ and" evaluate
        s" agather @ * NIBSHIFT rshift" evaluate
        disp-left-low if s" NIBSHIFT lshift" evaluate then
    then
;

\ plane pb of the samples whose lanes start at p, as a cell
:: GATHER ( p -- p' bits )
    s" d# 0 >r  dup d# 16 + swap" evaluate
    s" begin 2dup xor while  dup @" evaluate
    s" PLANE-BITS" evaluate
    disp-left-low if
        s" r> LANES rshift or >r"
    else
        s" r> LANES lshift or >r"
    then evaluate
    s" 1+ repeat  nip r>" evaluate
;
:: GATHER0  0 pb !  s" GATHER" evaluate ;
:: GATHER1  1 pb !  s" GATHER" evaluate ;
:: GATHER2  2 pb !  s" GATHER" evaluate ;
:: GATHER3  3 pb !  s" GATHER" evaluate ;

target

: agw   ( -- a ) dth @ c# 14 + ;  \ lanes pairs are summed in
: ash   ( -- a ) dth @ c# 15 + ;  \ a sample's side, as a shift
: alt   ( -- a ) dth @ c# 16 + ;  \ the table, less 760

: v-level ( v -- level ) \ the nearest of the sixteen
    d# 760 -  d# 0 swap  ( level n ) \ n is the gray plus 8
    begin  over d# 15 <  over d# 16 >  and  while
        d# 17 -  swap 1+ swap
    repeat
    drop
;

: levels-table ( a -- )
    d# 760 >r  ( R: the lane value )
    d# 272 bounds
    begin
        2dup xor
    while
        r@ v-level                              ( end a level )
        r@ d# 256 + over d# 17 * - d# 3 rshift
        ( end a level error )
        swap d# 12 lshift or  over !
        r> 1+ >r  1+
    repeat
    2drop rdrop
;

\ the lanes of two cells added in pairs, x's in the lower half
W32+? [if]
: fold2 ( x y -- z )
    fold-pairs HALFBITS lshift  swap fold-pairs or
;
[else]
: fold2 ( x y -- z ) + ;
[then]

\ the lanes of the n cells at a, in pairs, into the first half
: afold ( a n -- )
    over >r  u2/ over + swap  ( end to ) ( R: a )
    begin
        2dup xor
    while
        dup 2* r@ -  dup @ swap 1+ @ fold2  over !
        1+
    repeat
    2drop rdrop
;

: awide ( -- n ) \ cells in the row of pair sums
    tw @ d# 1 rshift LANES 1- + LANESHIFT rshift
    an @ ash @ 1- lshift  max
;

\ the pair sums of a sample's rows; the first has been read
: asum-rows ( -- )
    d# 1 ash @ lshift LANES* agw @ awide cells-fill
    agw @ apairs
    d# 1 ash @ lshift 1- negate
    begin dup while aimg @ tiff-row agw @ apairs 1+ repeat
    drop
;

: asums ( lanes -- ) \ the next row of samples, each to its lane
    ash @ 0= if agrays exit then
    asum-rows
    agw @  an @ ash @ 1- lshift  ( lanes a n )
    begin dup an @ <> while 2dup afold u2/ repeat
    drop                         ( lanes a )
    tuck - >r  ( a ) ( R: offset from the sums to the lanes )
    an @ bounds
    begin 2dup xor while
        dup @ ash @ 2* rshift GRAY-BITS  over r@ + +!  1+
    repeat
    2drop rdrop
;

W32+? [if]
: lpixels ( lanes table -- ) \ each lane: its error and level
    >r  h# 800100 over +!  an @ bounds
    ( end p ) ( R: the table )
    begin 2dup xor while
        dup @ CELL-LEVELS               ( end p e x' )
        >r over !  r> over 1+ +!  1+
    repeat
    2drop rdrop
;
[then]

W32+? [if] [else]
: lpixels ( lanes table -- ) \ the same, one sample per cell
    >r  d# 128 over 1- !  d# 256 over +!
    an @ bounds                      ( end p ) ( R: the table )
    begin 2dup xor while
        dup @ r@ + @                 ( end p error )
        2dup swap 1- @ + h# fff and  ( end p error owed )
        >r over !  1+  r> over +!    ( end p' )
    repeat
    2drop rdrop
;
[then]

: lplane0 ( p -- p' bits ) GATHER0 ;  \ a cell of a plane
: lplane1 ( p -- p' bits ) GATHER1 ;
: lplane2 ( p -- p' bits ) GATHER2 ;
: lplane3 ( p -- p' bits ) GATHER3 ;

\ a plane of the row's levels, to dst; xt gathers a cell of it
: lbits ( lanes dst xt -- )
    >r  dup abm @ CELL + @ + swap  ( lanes end dst ) ( R: xt )
    begin
        2dup xor
    while
        rot r@ execute             ( end dst lanes' bits )
        rot tuck !  1+  rot swap   ( lanes' end dst' )
    repeat
    2drop drop rdrop
;

: lplanes ( lanes -- ) \ the row's four planes, the lowest first
    ay @ d# 2 lshift abm @ CELL + @ *  abm @ @ +  ( lanes dst )
    2dup ['] lplane0 lbits  abm @ CELL + @ +
    2dup ['] lplane1 lbits  abm @ CELL + @ +
    2dup ['] lplane2 lbits  abm @ CELL + @ +
    ['] lplane3 lbits
;

\ The whole image, a sample 1 << shift pixels a side.  planes is
\ their address and the cells of a plane row; a row of samples
\ is four plane rows.
header dither-levels
: dither-levels ( planes img shift -- dither )
    d# 17 >c halloc dth !
    ash !  dup aimg ! image!  abm !
    tw @ ash @ rshift aw !  awidths
    d# 0 aoff !  d# 0 ay0 !  th @ ash @ rshift akh !
    d# 0 arow !
    ash @ if awide >c halloc agw ! then
    d# 272 >c halloc  dup levels-table  d# 760 - alt !
    alane-rows  pixels
    d# 0 ay !  dth @
;

header level-row
: level-row ( dither -- ) \ the next row of samples
    dth !
    aimg @ tiff-row
    acur @  dup asums  dup alt @ lpixels  dup atrim
    dup lplanes
    anx2 @ adown2
    acur @ anxt @ adown1
    acur @  anxt @ acur !  anx2 @ anxt !  dup anx2 !
    d# 0 over alanes  apreset
    d# 1 ay +!
;
