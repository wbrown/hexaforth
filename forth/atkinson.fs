\ atkinson.fs - Atkinson dithering of the image tiff.fs has read

\ Each pixel is thresholded, and one eighth of its error goes to
\ each of six neighbours:
\             *  1  1
\          1  1  1
\             1

\ A pixel is a lane of a cell, sixteen bits.  Its gray and what
\ its neighbours gave it are added into its lane before it is
\ looked at.  What a pixel gives is its error over eight and
\ 128, so it is never negative, with whether it is black above
\ it at bit 12: when a row is done its lanes are both the errors
\ for the rows below and the pixels.

meta
\ (lanes, the lanes to a cell, is cross.fs's.)
: each-lane ( -- x ) 0 lanes 0 do #16 lshift 1 or loop ;
each-lane constant lane1        \ 1 in each lane

\ the multiplier that brings bit 0 of each lane to the top of
\ the cell, in the order the display has pixels in a cell
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

\ a cell's lowest lane to its highest, and its highest to its
\ lowest; and each lane added to those either side.  With one
\ lane to a cell nothing is compiled.
:: LOW>TOP    lanes 1 > if
    cell-bits #16 - literal s" lshift" evaluate
then ;
:: TOP>LOW    lanes 1 > if
    cell-bits #16 - literal s" rshift" evaluate
then ;

:: EACH-WAY   lanes 1 > if
    s" dup h# 10001 * swap d# 16 rshift +" evaluate
then ;

\ whether the pixel of each lane of a cell is black, a bit each,
\ where acell-bits gathers them
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

\ four grays added in a lane, quartered to the nearest: the
\ half of four is in the lane before the grays are (AROUND)
:: AROUND     lane1 2 * literal ;
:: QUARTERED ( x -- y )
    lanes 1 = if
        s" d# 2 rshift" evaluate
    else
        s" d# 2 rshift gray-mask @ and" evaluate
    then
;

\ a lane without whether its pixel is black: its number
:: CLEAN ( x -- y )
    lanes 1 = if
        $fff literal s" and" evaluate
    else
        s" aclean @ and" evaluate
    then
;

\ The pixels of a cell: x is the cell, with what the cell before
\ left for its first two added; e is what its pixels give, and
\ x' what they leave for the first two of the next.

:: CELL-DONE ( x -- e x' )
    lanes 0 do
        \ the rest of the cell, and the pixel
        s" dup d# 16 rshift swap h# fff and" evaluate
        \ black, if it is under the threshold:
        s" dup d# 896 <  h# 80ff and + 1+" evaluate
        \ what it gives, with whether it is black above it
        s" d# 3 rshift" evaluate
        \ to the next two
        s" tuck h# 10001 * +" evaluate
    loop
    s" >r" evaluate
    lanes 1- 0 ?do s" d# 16 lshift +" evaluate loop
    s" r>" evaluate
;
target

\ These are for more lanes than one (BLACK-BITS, CLEAN).
W32+? [if]
\ 1 in each lane
create aone    meta lane1 target ,
\ the number in each lane, without whether it is black
create aclean  meta lane1 $fff * target ,
create agather meta gather target ,
[then]

\ A dither is a record: `dth` is the one in hand, and these are
\ its cells.
create dth 0 ,          \ the dither in hand: its record

: abm   ( -- a ) dth @ ;          \ the bitmap: its descriptor
: an    ( -- a ) dth @ 1+ ;       \ cells of lanes that
                                  \ have pixels
: acur  ( -- a ) dth @ CELL2 + ;  \ the row being done,
: anxt  ( -- a ) dth @ c# 3 + ;   \ the next,
: anx2  ( -- a ) dth @ c# 4 + ;   \ and the one after: each a
                                  \ row of lanes
: ay    ( -- a ) dth @ c# 5 + ;   \ how many rows are done
: aimg  ( -- a ) dth @ c# 6 + ;   \ the image it is of
\ The bitmap need not be of the whole image: the rows from ay0,
\ akh of them, and of each the cells from aoff lanes along.
: afull ( -- a ) dth @ c# 7 + ;   \ cells in a row of a bitmap
                                  \ of the whole image

: aoff  ( -- a ) dth @ c# 8 + ;   \ where in a row of lanes the
                                  \ first cell kept starts
: ay0   ( -- a ) dth @ c# 9 + ;   \ the first row kept,
: akh   ( -- a ) dth @ c# 10 + ;  \ and how many are

\ Nor need it be at the image's size: at half of it a pixel is
\ four of the image's, and at twice it a 2 by 2.
: arow  ( -- a ) dth @ c# 11 + ;  \ ( lanes -- ) adds the next
                                  \ row's grays, reading the
                                  \ image as needed
: aw    ( -- a ) dth @ c# 12 + ;  \ pixels across what is
                                  \ dithered: the image's width,
                                  \ half it, or twice it

: agr   ( -- a ) dth @ c# 13 + ;  \ the row of lanes grays are
                                  \ gathered in, at half size
                                  \ or twice

: alanes ( x a -- )
    \ set a row of lanes, and the cell at each end of it
    1-  afull @ d# 4 lshift d# 2 +  cells-fill
;

: anew ( x -- a )
    afull @ d# 4 lshift d# 2 + >c halloc 1+  tuck alanes ;

\ what the pixel that is not there, beside each end of the row,
\ would give the row
: apreset ( a -- )
    d# 128 over +!
    aw @ 1-  dup LANES 1- and d# 4 lshift d# 128 swap lshift
    ( a last x )
    swap LANESHIFT rshift rot + +!
;

\ a gray into the lane at that shift of the cell at a, and on to
\ the next lane
: lane+ ( shift a g -- shift' a' )
    >r over r> swap lshift  over +!
    swap d# 16 +  dup CELLBITS = if drop 1+ d# 0 then  swap
;

: agrays ( lanes -- )
    \ the next row's grays, each added to its pixel's lane
    'grays @ ?dup if execute exit then
    d# 0 swap  tw @ negate  ( shift a -n )
    begin
        dup
    while
        >r  'pixel @ execute lane+  r> 1+
    repeat
    drop 2drop
;

\ read the next image row and add its grays, at the image's
\ own size
: aown ( lanes -- )
    aimg @ tiff-row  agrays
;

: apairs ( lanes -- )
    \ the next row's grays, each two pixels' added into a lane
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

\ a quarter of what each lane of sums holds, to the nearest,
\ added to its lane
: aquarters ( lanes -- )
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

\ the next two rows of the image, as the grays of one row at
\ half the size
: ahalf-row ( lanes -- )
    AROUND agr @ an @ cells-fill
    aimg @ tiff-row  agr @ apairs  aimg @ tiff-row
    agr @ apairs
    aquarters
;

\ a cell of lanes as two cells with each lane doubled: the lower
\ half of the cell makes the first
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

: atwice ( lanes -- )
    \ add each lane of the grays at agr to two lanes of the row
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

\ add a row's grays at twice the size, reading an image row
\ every other row
: atwice-row ( lanes -- )
    ay @ d# 1 and 0= if
        d# 0 agr @ an @ 1+ u2/ cells-fill
        aimg @ tiff-row  agr @ agrays
    then
    atwice
;

\ every pixel of the row: each lane is left as what its
\ pixel gives
W32+? [if]
: apixels ( lanes -- )
    h# 800100 >r  \ what the two before the first would have
                  \ left: 256, and 128
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

\ With one lane to a cell, what the next pixel is owed is added
\ into its lane as soon as it is known.  Only what the last
\ pixel gave is carried, on the return stack, and what this one
\ gives is written over it there (doc/superoptimizations.md).

W32+? [if] [else]
: apixels ( lanes -- ) \ the same, a cell being one pixel
    d# 128 >r  ( lanes ) ( R: what the last pixel gave )
    d# 256 over +!  \ what the two before the first would leave
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

\ the lanes of the last cell that are past the last pixel
\ give nothing
: atrim ( lanes -- )
    aw @ LANES 1- and ?dup if
        d# 4 lshift d# 1 swap lshift 1-  ( lanes mask )
        swap an @ + 1- and!
    else
        drop
    then
;

W32+? [if]
\ whether each pixel of a cell is black: a bit each, as the
\ display has them
: anib ( e -- n )
    d# 12 rshift aone @ and  agather @ *  NIBSHIFT rshift
;
[then]

: acell-bits ( p -- p' bits )
    \ the pixels whose lanes start at p, as a cell of the bitmap
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

: abits ( lanes dst -- )
    \ the row's pixels, into the row of the bitmap at dst
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

: adown2 ( from to -- )
    \ the row's errors, each to the pixel two rows below
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
        \ its lowest lane to the highest of the cell before, its
        \ highest to the lowest of the cell after, and itself
        \ and a lane each way
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

\ the three rows of lanes: the rows that are not there gave the
\ first row four times nothing, and the second once
: alane-rows ( -- )
    d# 512 LANE1 * anew acur !
    d# 128 LANE1 * anew dup apreset anxt !
    d# 0 anew dup apreset anx2 !
;

\ start a dither into a bitmap laid out as the display is; xt
\ adds each row's grays (arow)
: (dither) ( bitmap img x y w h xt -- dither )
    d# 14 >c halloc dth !  arow !
    akh !  >r  ay0 !  LANESHIFT rshift aoff !  r>
    swap dup aimg ! image!  swap abm !             ( w )
    PXMASK + PXSHIFT rshift abm @ CELL + !
    tw @  arow @ ['] ahalf-row = if d# 1 rshift then
    arow @ ['] atwice-row = if 2* then  aw !  awidths
    arow @ ['] aown <> if an @ >c halloc agr ! then
    alane-rows
    \ a bitmap that has none is given one, last, to be kept;
    \ one that has is dithered into again
    abm @ @ 0= if abm @ CELL + @ akh @ * >c halloc abm @ ! then
    pixels  d# 0 ay !  dth @
;

\ The bitmap is of the w by h of the picture from x, y, and x is
\ where a cell of the bitmap starts.
header dither-part
: dither-part ( bitmap img x y w h -- dither )
    \ of the image at its own size
    ['] aown (dither)
;

header dither-half
: dither-half ( bitmap img x y w h -- dither )
    \ of the image at half its size: x, y, w and h are of that
    ['] ahalf-row (dither)
;

\ of the image at twice its size; x, y, w and h are at that size
header dither-twice
: dither-twice ( bitmap img x y w h -- dither )
    ['] atwice-row (dither)
;

header dither-start
: dither-start ( bitmap img -- dither )
    \ for the whole of the image
    dup image!  d# 0 d# 0 tw @ th @ dither-part
;

header dither-row : dither-row ( dither -- )
    \ the next row of its image, which is read if it has to be
    dth !
    acur @  dup arow @ execute
    dup apixels  dup atrim           ( lanes )
    \ a row that is kept: its pixels, from the first cell kept
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
: atkinson ( bitmap img -- )
    \ dither the whole image into a new bitmap
    dither-start
    begin
        ay @ th @ <
    while
        dup dither-row
    repeat
    drop
;

\ ---- to sixteen levels

\ A picture kept as gray (gray.fs) is dithered the same way, but
\ a lane is a sample, the average of a square of the image's
\ pixels, and is rounded to the nearest of sixteen levels.  One
\ fetch from a table, by the lane's value, gives the sample's
\ level, at bit 12, and the error it passes on, below.  The
\ table covers the 272 values from 760, which is all a lane can
\ come to.

meta
\ a number in each lane of a cell; and the bits of a lane that
\ hold a gray once a sum is shifted down
:: LANES*     lanes 1 > if s" aone @ *" evaluate then ;
:: GRAY-BITS  lanes 1 > if s" gray-mask @ and" evaluate then ;

\ the samples of a cell, as CELL-DONE does a cell's pixels: the
\ table is on top of the return stack
:: CELL-LEVELS ( x -- e x' )
    lanes 0 do
        \ the rest of the cell, and this sample's value
        s" dup d# 16 rshift swap h# fff and" evaluate
        \ its error, with its level above it
        s" r@ + @" evaluate
        \ the error, added to the next two
        s" tuck h# fff and h# 10001 * +" evaluate
    loop
    s" >r" evaluate
    lanes 1- 0 ?do s" d# 16 lshift +" evaluate loop
    s" r>" evaluate
;

\ plane pb of the levels of a cell's lanes, one bit each, where
\ acell-bits gathers them
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

\ plane pb of the samples whose lanes start at p, in one cell: a
\ word a plane, so that the bit's place is a number in the code
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

: agw   ( -- a ) dth @ c# 14 + ;  \ the row of lanes pixel pairs
                                  \ are summed in, for 2 by 2
                                  \ samples or more
: ash   ( -- a ) dth @ c# 15 + ;  \ pixels per side of a sample,
                                  \ as a shift
: alt   ( -- a ) dth @ c# 16 + ;  \ the table of level and error
                                  \ for each lane value, offset
                                  \ by 760

\ the nearest of the sixteen levels to a lane value's gray; 0 if
\ below black
: v-level ( v -- level )
    d# 760 -  d# 0 swap  ( level n ) \ n is the gray plus 8
    begin  over d# 15 <  over d# 16 >  and  while
        d# 17 -  swap 1+ swap
    repeat
    drop
;

\ fill the table at a: for each lane value from 760, its level
\ at bit 12 and its error below
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

W32+? [if]
\ add adjacent lane pairs of two cells into one cell, x's in its
\ lower half
: fold2 ( x y -- z )
    fold-pairs HALFBITS lshift  swap fold-pairs or
;
[else]
: fold2 ( x y -- z ) + ;
[then]

\ add adjacent lane pairs of the n cells at a, into the first
\ half of them
: afold ( a n -- )
    over >r  u2/ over + swap  ( end to ) ( R: a )
    begin
        2dup xor
    while
        \ from the two cells twice as far from a
        dup 2* r@ -  dup @ swap 1+ @ fold2  over !
        1+
    repeat
    2drop rdrop
;

\ cells in the row of pair sums: enough for the row's pairs and
\ for the folding
: awide ( -- n )
    tw @ d# 1 rshift LANES 1- + LANESHIFT rshift
    an @ ash @ 1- lshift  max
;

\ the pair sums of the rows of a sample, each row read but the
\ first, which the caller has read.  Each lane starts with its
\ share of half the divisor, so the sum is rounded when it is
\ shifted down.
: asum-rows ( -- )
    d# 1 ash @ lshift LANES* agw @ awide cells-fill
    agw @ apairs
    d# 1 ash @ lshift 1- negate
    begin dup while aimg @ tiff-row agw @ apairs 1+ repeat
    drop
;

\ add the grays of the next row of samples, each into its lane
: asums ( lanes -- )
    ash @ 0= if agrays exit then
    asum-rows
    \ fold lane pairs until a lane is a sample
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

\ What a cell owes the first two lanes of the next is added
\ straight into the next cell.
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

\ The lane before the first holds what a missing pixel would
\ pass on.
W32+? [if] [else]
: lpixels ( lanes table -- ) \ the same, one sample per cell
    >r  d# 128 over 1- !  d# 256 over +!
    an @ bounds                      ( end p ) ( R: the table )
    begin 2dup xor while
        dup @ r@ + @                 ( end p error )
        \ what the next sample is owed
        2dup swap 1- @ + h# fff and  ( end p error owed )
        >r over !  1+  r> over +!    ( end p' )
    repeat
    2drop rdrop
;
[then]

: lplane0 ( p -- p' bits ) GATHER0 ;  \ gather a cell of the
                                      \ lowest plane from the
                                      \ lanes at p,
: lplane1 ( p -- p' bits ) GATHER1 ;  \ and of the others
: lplane2 ( p -- p' bits ) GATHER2 ;
: lplane3 ( p -- p' bits ) GATHER3 ;

\ store one plane of the row's levels at dst; xt gathers a cell
\ of it
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

\ store the row's four level planes in its four plane rows,
\ lowest first
: lplanes ( lanes -- )
    ay @ d# 2 lshift abm @ CELL + @ *  abm @ @ +  ( lanes dst )
    2dup ['] lplane0 lbits  abm @ CELL + @ +
    2dup ['] lplane1 lbits  abm @ CELL + @ +
    2dup ['] lplane2 lbits  abm @ CELL + @ +
    ['] lplane3 lbits
;

\ start dithering the whole image to sixteen levels, a sample
\ per 1 << shift pixels a side.  planes is the planes' address
\ and cells per plane row; a row of samples is four plane rows,
\ lowest plane first.
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

\ read and dither the next row of samples, and store its
\ level planes
header level-row
: level-row ( dither -- )
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
