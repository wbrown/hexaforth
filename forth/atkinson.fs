\ atkinson.fs - Atkinson dithering of the image tiff.fs has read

\ Each pixel is thresholded, and one eighth of its error goes to
\ each of six neighbours:
\             *  1  1
\          1  1  1
\             1
\ Only six eighths are passed on, which is what gives the look.

\ A pixel is a lane of a cell, sixteen bits, as many to a cell
\ as fit, and a row is cells of them, pixel 0 in the lowest lane
\ of the first.  All that a pixel is thresholded on is added
\ into its lane before it is looked at: its gray, what the four
\ neighbours on the rows above gave it, and what the two before
\ it on its own row gave it.  So a pixel is its lane, and
\ nothing is kept beside it.

\ The two before it are the cell's own business.  The cell is on
\ the stack, and the pixel being done is its lowest lane.  When
\ a pixel is done the cell is shifted down a lane, so that the
\ next pixel is the lowest, and what the pixel gives is added to
\ the lowest two: one multiplication.  After the last pixel of a
\ cell, what is left on the stack is what the first two of the
\ next cell are owed by this one, and the next cell is added
\ to it.

\ What a pixel gives is its error over eight, kept as that and
\ 128 so that it is never negative and a lane never borrows from
\ the next; six of them make the sum a pixel is thresholded on
\ 768 too many, which the threshold and the error allow for, and
\ nothing is subtracted anywhere.  With it, above it at bit 12,
\ goes whether the pixel is black.  A lane's own number is its
\ low twelve bits, and never reaches them: so what a pixel gives
\ can be added to the next two as it is, and the lanes of a row,
\ when the row is done, are both the errors for the rows below
\ and the pixels.

\ A row that is done is left in the cells it was owed in.  Its
\ pixels are gathered out of it, a cell of the bitmap at a time:
\ the bits at 12 of a cell's lanes are brought together by one
\ multiplication.  Its errors go to the row two below as they
\ are, and to the row below with themselves a lane to the left
\ and a lane to the right, which are the three neighbours there;
\ the lane that leaves a cell at either end goes to the cell
\ beside it.  A pixel at the end of a row has a neighbour that
\ is not there, and is given what that would have given (128)
\ when its row is made ready.

\ There are three rows of lanes, and a dither's record keeps
\ where they are, the bitmap, the image and how far down it is,
\ between one row and the next.  Nothing else is kept: a row is
\ done on the stacks.

\ A cell of 16 bits is one lane, and what a pixel gives to the
\ two after it cannot ride in the cell.  Then it is two numbers:
\ what the next pixel is owed by the two before it, and what the
\ last pixel gave, which the one after the next is owed.
\ Everything else is the same words.

meta
\ (lanes, the lanes to a cell, is cross.fs's, with LANES and
\ LANESHIFT: a font's rows are lanes too.)
: each-lane ( -- x ) 0 lanes 0 do #16 lshift 1 or loop ;
each-lane constant lane1        \ 1 in each lane

\ The multiplier that brings bit 0 of each lane to the top of
\ the cell, the pixels in the order the display has them in a
\ cell: the leftmost lowest, or the leftmost highest.
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
\ A cell's lowest lane moved to its highest, and its highest to
\ its lowest; and a cell's lanes each added to the lanes either
\ side of it.  A cell of 16 bits is one lane, which is its
\ lowest and its highest and has none beside it, so for it
\ nothing is compiled.
:: LOW>TOP    lanes 1 > if
    cell-bits #16 - literal s" lshift" evaluate
then ;
:: TOP>LOW    lanes 1 > if
    cell-bits #16 - literal s" rshift" evaluate
then ;

:: EACH-WAY   lanes 1 > if
    s" dup h# 10001 * swap d# 16 rshift +" evaluate
then ;

\ Whether the pixel of each lane of a cell is black, a bit each,
\ where acell-bits gathers them: at the top of a cell if the
\ leftmost pixel is the lowest, else at the bottom (anib).  With
\ one lane to a cell the bit, at 12, is moved there by itself:
\ nothing is multiplied.

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

\ Four grays added in a lane, and half of four, quartered: to
\ the nearest.  The half of four is in the lane before the grays
\ are (AROUND, which the lanes the sums are made in are filled
\ with), so it is added once a row and not once a pixel.  With
\ one lane to a cell there is no lane beside it to keep the
\ quarter from.
:: AROUND     lane1 2 * literal ;
:: QUARTERED ( x -- y )
    lanes 1 = if
        s" d# 2 rshift" evaluate
    else
        s" d# 2 rshift gray-mask @ and" evaluate
    then
;

\ A lane without whether its pixel is black: its number.  With
\ one lane to a cell the mask is a number one instruction
\ pushes, and is not fetched.
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

\ A dither is a record, so that there may be more than one in
\ hand at a time, each going down its own picture a row at a
\ turn: `dth` is the one in hand, and these are its cells.  As
\ with an image, it is in hand only for as long as the word that
\ took it up runs.
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
\ The bitmap need not be of the whole image.  Every pixel is
\ dithered, since what a pixel is depends on those before it;
\ but the bitmap is what is kept, and whoever shows less than
\ the picture keeps less: the rows from ay0, akh of them, and of
\ each row the cells from the one aoff lanes along.
: afull ( -- a ) dth @ c# 7 + ;   \ cells in a row of a bitmap
                                  \ of the whole image

: aoff  ( -- a ) dth @ c# 8 + ;   \ where in a row of lanes the
                                  \ first cell kept starts
: ay0   ( -- a ) dth @ c# 9 + ;   \ the first row kept,
: akh   ( -- a ) dth @ c# 10 + ;  \ and how many are

\ Nor need it be at the image's size.  At half of it a pixel of
\ the bitmap is four of the image, two of each of two rows, and
\ its gray is theirs added and then quartered, to the nearest:
\ so a row that is dithered is two rows read.  The four are
\ added in a row of lanes of their own, since the lanes a row is
\ dithered in already hold what the rows above gave, which is
\ not to be quartered.  At twice its size each pixel of the
\ image becomes a 2 by 2 of the bitmap: an image row is read
\ every other bitmap row into its own row of lanes, and each
\ lane of that is added to two lanes of both bitmap rows.
: arow  ( -- a ) dth @ c# 11 + ;  \ ( lanes -- ) adds the next
                                  \ row's grays, reading the
                                  \ image as needed
: aw    ( -- a ) dth @ c# 12 + ;  \ pixels across what is
                                  \ dithered: the image's width,
                                  \ half it, or twice it

: agr   ( -- a ) dth @ c# 13 + ;  \ the row of lanes grays are
                                  \ gathered in, at half size
                                  \ or twice

\ A row of lanes has as many cells as make whole cells of a
\ bitmap of the whole image, so that the pixels can be gathered
\ sixteen cells at a time, and a cell at each end for what
\ leaves the row.
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

\ Where the image can give a row's grays in lanes ('grays), it
\ adds them a cell at a time.  Any other is asked for a pixel at
\ a time.

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

\ The same for half as many lanes, two pixels to each ('pairs,
\ where the image can).  A row of an odd number of pixels has
\ one over, which is taken and belongs to no lane.
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
        2dupxor
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

\ Split a cell of lanes into two cells with each lane doubled:
\ the lower half of the cell makes the first.  With four lanes,
\ each pair is moved a lane apart and multiplied by $10001,
\ which copies each into the lane above it.  With two lanes,
\ each is multiplied on its own.  With one, both cells are
\ the lane.
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
        2dupxor
    while
        dup @ TWICE  r@ 1+ +!  r@ +!
        r> CELL2 + >r  1+
    repeat
    2drop rdrop
;

\ If the row is an odd number of cells, the last one also adds
\ to the cell past the end of the row, which is never dithered.

\ add a row's grays at twice the size, reading an image row
\ every other row
: atwice-row ( lanes -- )
    ay @ d# 1 and 0= if
        d# 0 agr @ an @ 1+ u2/ cells-fill
        aimg @ tiff-row  agr @ agrays
    then
    atwice
;

W32+? [if]

\ every pixel of the row: each lane is left as what its
\ pixel gives
: apixels ( lanes -- )
    h# 800100 >r  \ what the two before the first would have
                  \ left: 256, and 128
    an @ bounds
    begin
        2dupxor
    while
        dup @ r> +  CELL-DONE  >r over !
        1+
    repeat
    2drop rdrop
;

[else]

\ What the next pixel is owed by the two before it is added into
\ its lane as soon as it is known, and not carried to it on the
\ stack: only what the last pixel gave is carried, on the return
\ stack.  It goes in without whether they were black, so a lane
\ holds only its number, and the lane after the last pixel,
\ which is given it too, has no pixel made black.
\
\ What the last pixel gave is not taken off the return stack and
\ put back: what this one gives is written over it where it is
\ (swap rdrop >r is one instruction, its result to R with R not
\ moved), and is stored from there while the address is brought
\ up (swap r@ over !, one).  Each is an instruction where the
\ plain way was two (doc/superoptimizations.md).

: apixels ( lanes -- ) \ the same, a cell being one pixel
    d# 128 >r  ( lanes ) ( R: what the last pixel gave )
    d# 256 over +!  \ what the two before the first would leave
    an @ bounds
    begin 2dupxor while
        dup @  dup d# 896 <  h# 80ff and + 1+  d# 3 rshift
        \ it gives; the next is owed this and the one before,
        h# fff over r@ + and  ( end p gives owed )
        \ the one after this
        swap rdrop >r         ( end p owed ) ( R: gives )
        \ and it is the pixel's lane
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
    begin 2dupxor while
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
        2dupxor
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
        2dupxor
    while
        dup @ CLEAN  over r@ + +!
        1+
    repeat
    2drop rdrop
;

\ Where the three are is p and R added, which is one instruction
\ while p is under the top: so it is made where it is wanted,
\ and not kept in the way of the rest.
: adown1 ( from to -- ) \ and to the three on the row below
    over - 1- >r  ( from ) ( R: from a cell to the one before )
    an @ bounds
    begin 2dupxor while
        dup @ CLEAN  over r@ +         ( end p e t )
        \ its lowest lane to the highest of the cell before, its
        \ highest to the lowest of the cell after, and itself
        \ and a lane each way
        over LOW>TOP over +!  CELL2 +  over TOP>LOW swap +!
        EACH-WAY  over r@ + 1+ +!  1+
    repeat
    2drop rdrop
;

\ A row is dithered at a time, so that whoever wants the picture
\ can do something else between the rows, and can show the rows
\ that are made.

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

\ A picture kept as gray (gray.fs) is dithered the same way,
\ except that a lane is a sample, not a pixel, and is rounded to
\ the nearest of sixteen levels instead of to black or white.  A
\ sample is the average of 1, 2 by 2, 4 by 4 or 8 by 8 of the
\ image's pixels, rounded.  The pixels of a sample's rows are
\ added two to a lane into one row of lanes, as at half size;
\ for wider samples, adjacent lanes are then added in pairs
\ (fold-pairs, tiff.fs), a cell at a time, until a lane is a
\ sample; then the sum is shifted down.

\ As with a pixel, a lane's value when its sample is looked at
\ is its gray, plus 768, plus what it was passed.  One fetch
\ from a table, indexed by that value, gives both the sample's
\ level and the error it passes on.  The table is on top of the
\ return stack while a row is done.  The error is the low twelve
\ bits, passed to the next two lanes as a pixel's is, and the
\ level is the four bits above.  So when a row is done its lanes
\ hold both the errors for the rows below and the levels, and
\ each of the four level planes is gathered out of them as the
\ dither gathers pixels, from bit 12, 13, 14 or 15 of each lane.
\ The error is how far the level is from the sample, an eighth
\ to each of six neighbours, as for a pixel.

\ A lane's value never goes below 762 or above 1029.  A sample
\ is off its level by at most half a level (8), unless it is
\ past black or white, and then by less than it was passed; it
\ is passed an eighth of six such errors, so it is never more
\ than 6 past black or white.  So the table covers the 272
\ values from 760.

meta
\ A number in each lane of a cell; and the bits of a lane that
\ hold a gray once a sum is shifted down, without what came down
\ from the lane above.
:: LANES*     lanes 1 > if s" aone @ *" evaluate then ;
:: GRAY-BITS  lanes 1 > if s" gray-mask @ and" evaluate then ;

\ The samples of a cell, as CELL-DONE does a cell's pixels: one
\ table fetch each, from the table on top of the return stack
\ (offset by 760).
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

\ Plane pb of the levels of a cell's lanes, one bit each, where
\ acell-bits gathers them.  With one lane per cell and the
\ leftmost pixel in the lowest bit, the bit is wanted at the
\ top, and is moved straight there.
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

\ Gather plane pb of the samples whose lanes start at p into one
\ cell; the cell being built is on top of the return stack.
\ There is a word per plane, so the bit's position is a literal
\ in the code.
:: GATHER ( p -- p' bits )
    s" d# 0 >r  dup d# 16 + swap" evaluate
    s" begin 2dupxor while  dup @" evaluate
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
        2dupxor
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
        2dupxor
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

\ A sum is rounded when it is shifted down, by adding half the
\ divisor first.  Each lane of pair sums starts with its share
\ of that, 1 << s, since 1 << (s - 1) of them are added into a
\ sample's lane; so the sum only needs shifting at the end.

\ the pair sums of the rows of a sample, each row read but the
\ first, which the caller has read
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
    begin 2dupxor while
        dup @ ash @ 2* rshift GRAY-BITS  over r@ + +!  1+
    repeat
    2drop rdrop
;

\ The table is on top of the return stack while the row is done,
\ where each sample can use it as an ALU input.  What a cell
\ owes the first two lanes of the next is added straight into
\ the next cell, so nothing is carried between cells; after the
\ last cell it goes into the cell at the end of the row.
W32+? [if]
: lpixels ( lanes table -- ) \ each lane: its error and level
    >r  h# 800100 over +!  an @ bounds
    ( end p ) ( R: the table )
    begin 2dupxor while
        dup @ CELL-LEVELS               ( end p e x' )
        >r over !  r> over 1+ +!  1+
    repeat
    2drop rdrop
;
[then]

\ With one lane per cell, what the next sample is owed by the
\ two before it (this one's error, and the previous one's, which
\ is in the lane before) is added into its lane as soon as it is
\ known, so nothing is carried from sample to sample.  The lane
\ before the first holds what a missing pixel would pass on.

W32+? [if] [else]
: lpixels ( lanes table -- ) \ the same, one sample per cell
    >r  d# 128 over 1- !  d# 256 over +!
    an @ bounds                      ( end p ) ( R: the table )
    begin 2dupxor while
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
        2dupxor
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
