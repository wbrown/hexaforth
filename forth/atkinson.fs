\ atkinson.fs - Atkinson dithering of the image tiff.fs has read
\
\ Each pixel is thresholded, and one eighth of its error goes to each of six
\ neighbours:
\             *  1  1
\          1  1  1
\             1
\ Only six eighths are passed on, which is what gives the look.
\
\ A pixel is a lane of a cell, sixteen bits, as many to a cell as fit, and
\ a row is cells of them, pixel 0 in the lowest lane of the first.  All
\ that a pixel is thresholded on is added into its lane before it is looked
\ at: its gray, what the four neighbours on the rows above gave it, and
\ what the two before it on its own row gave it.  So a pixel is its lane,
\ and nothing is kept beside it.
\
\ The two before it are the cell's own business.  The cell is on the
\ stack, and the pixel being done is its lowest lane.  When a pixel is
\ done the cell is shifted down a lane, so that the next pixel is the
\ lowest, and what the pixel gives is added to the lowest two: one
\ multiplication.  After the last pixel of a cell, what is left on the
\ stack is what the first two of the next cell are owed by this one, and
\ the next cell is added to it.
\
\ What a pixel gives is its error over eight, kept as that and 128 so that
\ it is never negative and a lane never borrows from the next; six of them
\ make the sum a pixel is thresholded on 768 too many, which the threshold
\ and the error allow for, and nothing is subtracted anywhere.  With it,
\ above it at bit 12, goes whether the pixel is black.  A lane's own number
\ is its low twelve bits, and never reaches them: so what a pixel gives can
\ be added to the next two as it is, and the lanes of a row, when the row
\ is done, are both the errors for the rows below and the pixels.
\
\ A row that is done is left in the cells it was owed in.  Its pixels are
\ gathered out of it, a cell of the bitmap at a time: the bits at 12 of a
\ cell's lanes are brought together by one multiplication.  Its errors go
\ to the row two below as they are, and to the row below with themselves a
\ lane to the left and a lane to the right, which are the three neighbours
\ there; the lane that leaves a cell at either end goes to the cell beside
\ it.  A pixel at the end of a row has a neighbour that is not there, and
\ is given what that would have given (128) when its row is made ready.
\
\ There are three rows of lanes, and a dither's record keeps where they
\ are, the bitmap, the image and how far down it is, between one row and
\ the next.  Nothing else is kept: a row is done on the stacks.
\
\ A cell of 16 bits is one lane, and what a pixel gives to the two after
\ it cannot ride in the cell.  Then it is two numbers: what the next pixel
\ is owed by the two before it, and what the last pixel gave, which the
\ one after the next is owed.  Everything else is the same words.

meta
cell-bits #16 / constant lanes                      \ lanes to a cell
: each-lane ( -- x ) 0 lanes 0 do #16 lshift 1 or loop ;
each-lane constant lane1                            \ 1 in each lane
\ The multiplier that brings bit 0 of each lane to the top of the cell, the
\ pixels in the order the display has them in a cell: the leftmost lowest,
\ or the leftmost highest.
: to-top ( -- x )
    0 lanes 0 do
        1  disp-left-low if cell-bits lanes - i + else cell-bits 1- i - then  i #16 * -  lshift or
    loop
;
to-top constant gather
:: LANES      lanes literal ;
:: LANESHIFT  lanes 4 = if 2 else lanes 2 = if 1 else 0 then then literal ;  \ a pixel to its cell
:: NIBSHIFT   cell-bits lanes - literal ;           \ the top of a cell, less its pixels
\ A cell's lowest lane moved to its highest, and its highest to its lowest;
\ and a cell's lanes each added to the lanes either side of it.  A cell of
\ 16 bits is one lane, which is its lowest and its highest and has none
\ beside it, so for it nothing is compiled.
:: LOW>TOP    lanes 1 > if cell-bits #16 - literal s" lshift" evaluate then ;
:: TOP>LOW    lanes 1 > if cell-bits #16 - literal s" rshift" evaluate then ;
:: SPREAD     lanes 1 > if s" dup h# 10001 * swap d# 16 rshift +" evaluate then ;
\ Whether the pixel of each lane of a cell is black, a bit each, where
\ acell-bits gathers them: at the top of a cell if the leftmost pixel is
\ the lowest, else at the bottom (anib).  With one lane to a cell the bit,
\ at 12, is moved there by itself: nothing is multiplied.
:: BLACK-BITS ( e -- n )
    lanes 1 = if
        disp-left-low if
            $1000 literal s" and" evaluate  cell-bits 1- #12 - literal s" lshift" evaluate
        else
            #12 literal s" rshift" evaluate  1 literal s" and" evaluate
        then
    else
        s" anib" evaluate  disp-left-low if s" NIBSHIFT lshift" evaluate then
    then
;
\ Four grays added in a lane, and half of four, quartered: to the nearest.
\ The half of four is in the lane before the grays are (AROUND, which the
\ lanes the sums are made in are filled with), so it is added once a row
\ and not once a pixel.  With one lane to a cell there is no lane beside it
\ to keep the quarter from.
:: AROUND     lane1 2 * literal ;
:: QUARTERED ( x -- y )
    lanes 1 = if s" d# 2 rshift" evaluate else s" d# 2 rshift gray-mask @ and" evaluate then
;
\ A lane without whether its pixel is black: its number.  With one lane to
\ a cell the mask is a number one instruction pushes, and is not fetched.
:: CLEAN ( x -- y )
    lanes 1 = if $fff literal s" and" evaluate else s" aclean @ and" evaluate then
;

\ The pixels of a cell: x is the cell, with what the cell before left for
\ its first two added; e is what its pixels give, and x' what they leave
\ for the first two of the next.
:: CELL-DONE ( x -- e x' )
    lanes 0 do
        s" dup d# 16 rshift swap h# fff and" evaluate   \ the rest of the cell, and the pixel
        s" dup d# 896 <  h# 80ff and + 1+" evaluate     \ black, if it is under the threshold:
        s" d# 3 rshift" evaluate                        \ what it gives, with whether it is black above it
        s" tuck h# 10001 * +" evaluate                  \ to the next two
    loop
    s" >r" evaluate
    lanes 1- 0 ?do s" d# 16 lshift +" evaluate loop
    s" r>" evaluate
;
target

\ These are for more lanes than one (BLACK-BITS, CLEAN).
W32+? [if]
create aone    meta lane1 target ,                  \ 1 in each lane
create aclean  meta lane1 $fff * target ,           \ the number in each lane, without whether it is black
create agather meta gather target ,
[then]

\ A dither is a record, so that there may be more than one in hand at a
\ time, each going down its own picture a row at a turn: `dth` is the one
\ in hand, and these are its cells.  As with an image, it is in hand only
\ for as long as the word that took it up runs.
create dth 0 ,          \ the dither in hand: its record

: abm   ( -- a ) dth @ ;                \ the bitmap: its descriptor
: an    ( -- a ) dth @ 1+ ;             \ cells of lanes that have pixels
: acur  ( -- a ) dth @ CELL2 + ;        \ the row being done,
: anxt  ( -- a ) dth @ c# 3 + ;         \ the next,
: anx2  ( -- a ) dth @ c# 4 + ;         \ and the one after: each a row of lanes
: ay    ( -- a ) dth @ c# 5 + ;         \ how many rows are done
: aimg  ( -- a ) dth @ c# 6 + ;         \ the image it is of
\ The bitmap need not be of the whole image.  Every pixel is dithered, since
\ what a pixel is depends on those before it; but the bitmap is what is
\ kept, and whoever shows less than the picture keeps less: the rows from
\ ay0, akh of them, and of each row the cells from the one aoff lanes along.
: afull ( -- a ) dth @ c# 7 + ;         \ cells in a row of a bitmap of the whole image
: aoff  ( -- a ) dth @ c# 8 + ;         \ where in a row of lanes the first cell kept starts
: ay0   ( -- a ) dth @ c# 9 + ;         \ the first row kept,
: akh   ( -- a ) dth @ c# 10 + ;        \ and how many are
\ Nor need it be at the image's size.  At half of it a pixel of the bitmap
\ is four of the image, two of each of two rows, and its gray is theirs
\ added and then quartered, to the nearest: so a row that is dithered is
\ two rows read.  The four are added in a row of lanes of their own, since
\ the lanes a row is dithered in already hold what the rows above gave,
\ which is not to be quartered.
: ahalf ( -- a ) dth @ c# 11 + ;        \ true if it is at half the image's size
: aw    ( -- a ) dth @ c# 12 + ;        \ pixels across what is dithered: the image's, or half
: agr   ( -- a ) dth @ c# 13 + ;        \ the row of lanes that grays are added up in, at half size

\ A row of lanes has as many cells as make whole cells of a bitmap of the
\ whole image, so that the pixels can be gathered sixteen cells at a time,
\ and a cell at each end for what leaves the row.
: alanes ( x a -- ) \ set a row of lanes, and the cell at each end of it
    1-  afull @ d# 4 lshift d# 2 +  cells-fill
;

: anew ( x -- a ) afull @ d# 4 lshift d# 2 + >c halloc 1+  tuck alanes ;

: apreset ( a -- ) \ what the pixel that is not there, beside each end of the row, would give the row
    d# 128 over +!
    aw @ 1-  dup LANES 1- and d# 4 lshift d# 128 swap lshift    ( a last x )
    swap LANESHIFT rshift rot + +!
;

\ Where the image can give a row's grays in lanes ('grays), it adds them a
\ cell at a time.  Any other is asked for a pixel at a time.
: lane+ ( shift a g -- shift' a' ) \ a gray into the lane at that shift of the cell at a, and on to the next lane
    >r over r> swap lshift  over +!
    swap d# 16 +  dup CELLBITS = if drop 1+ d# 0 then  swap
;

: agrays ( lanes -- ) \ the next row's grays, each added to its pixel's lane
    'grays @ ?dup if execute exit then
    d# 0 swap  tw @ negate                          ( shift a -n )
    begin
        dup
    while
        >r  'pixel @ execute lane+  r> 1+
    repeat
    drop 2drop
;

\ The same for half as many lanes, two pixels to each ('pairs, where the
\ image can).  A row of an odd number of pixels has one over, which is
\ taken and belongs to no lane.
: apairs ( lanes -- ) \ the next row's grays, each two pixels' added into a lane
    'pairs @ ?dup if execute exit then
    d# 0 swap  tw @ d# 1 rshift negate              ( shift a -n )
    begin
        dup
    while
        >r  'pixel @ execute 'pixel @ execute + lane+  r> 1+
    repeat
    drop 2drop
    tw @ d# 1 and if 'pixel @ execute drop then
;

: aquarters ( lanes -- ) \ a quarter of what each lane of sums holds, to the nearest, added to its lane
    agr @ - >r                                      ( R: from the sums to the lanes )
    agr @ an @ bounds
    begin
        2dupxor
    while
        dup @ QUARTERED  over r@ + +!
        1+
    repeat
    2drop rdrop
;

: ahalf-row ( lanes -- ) \ the next two rows of the image, as the grays of one row at half the size
    AROUND agr @ an @ cells-fill
    agr @ apairs  aimg @ tiff-row  agr @ apairs
    aquarters
;

W32+? [if]

: apixels ( lanes -- ) \ every pixel of the row: each lane is left as what its pixel gives
    h# 800100 >r                                    \ what the two before the first would have left: 256, and 128
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

\ What the next pixel is owed by the two before it is added into its lane
\ as soon as it is known, and not carried to it on the stack: only what the
\ last pixel gave is carried, on the return stack.  It goes in without
\ whether they were black, so a lane holds only its number, and the lane
\ after the last pixel, which is given it too, has no pixel made black.
\
\ What the last pixel gave is not taken off the return stack and put back:
\ what this one gives is written over it where it is (swap rdrop >r is
\ one instruction, its result to R with R not moved), and is stored from
\ there while the address is brought up (swap r@ over !, one).  Each is
\ an instruction where the plain way was two (doc/superoptimizations.md).
: apixels ( lanes -- ) \ the same, a cell being one pixel
    d# 128 >r                                       ( lanes ) ( R: what the last pixel gave )
    d# 256 over +!                                  \ what the two before the first would have left it
    an @ bounds
    begin
        2dupxor
    while
        dup @                                       ( end p pixel )
        dup d# 896 <  h# 80ff and + 1+  d# 3 rshift ( end p gives )
        h# fff over r@ + and                        ( end p gives owed ) \ the next is owed this and the one before,
        swap rdrop >r                               ( end p owed ) ( R: gives ) \ the one after this
        swap r@ over !                              ( end owed p ) \ and it is the pixel's lane
        1+ tuck +!                                  ( end p' )
    repeat
    2drop rdrop
;

[then]

: atrim ( lanes -- ) \ the lanes of the last cell that are past the last pixel give nothing
    aw @ LANES 1- and ?dup if
        d# 4 lshift d# 1 swap lshift 1-             ( lanes mask )
        swap an @ + 1- and!
    else
        drop
    then
;

W32+? [if]
: anib ( e -- n ) \ whether each pixel of a cell is black: a bit each, as the display has them
    d# 12 rshift aone @ and  agather @ *  NIBSHIFT rshift
;
[then]

: acell-bits ( p -- p' bits ) \ the pixels whose lanes start at p, as a cell of the bitmap
    d# 0 >r                                         ( p ) ( R: the cell being made )
    dup d# 16 + swap
    begin
        2dupxor
    while
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

: abits ( lanes dst -- ) \ the row's pixels, into the row of the bitmap at dst
    dup abm @ CELL + @ + swap                       ( lanes end dst )
    rot >r                                          ( end dst ) ( R: the lanes )
    begin
        2dupxor
    while
        r> acell-bits swap >r
        over ! 1+
    repeat
    2drop rdrop
;

: adown2 ( from to -- ) \ the row's errors, each to the pixel two rows below
    over - >r                                       ( from ) ( R: how far on the other row is )
    an @ bounds
    begin
        2dupxor
    while
        dup @ CLEAN  over r@ + +!
        1+
    repeat
    2drop rdrop
;

: adown1 ( from to -- ) \ and to the three on the row below
    over - 1- >r                                    ( from ) ( R: from a cell to the one before it in the other row )
    an @ bounds
    begin
        2dupxor
    while
        \ Where the three are is p and R added, which is one instruction
        \ while p is under the top: so it is made where it is wanted, and
        \ not kept in the way of the rest.
        dup @ CLEAN  over r@ +                      ( end p e t )
        over LOW>TOP over +!                        \ its lowest lane, to the highest of the cell before
        CELL2 +  over TOP>LOW swap +!               \ its highest, to the lowest of the cell after ( end p e )
        SPREAD  over r@ + 1+ +!                     \ itself, and a lane each way
        1+
    repeat
    2drop rdrop
;

\ A row is dithered at a time, so that whoever wants the picture can do
\ something else between the rows, and can show the rows that are made.

: (dither) ( bitmap img x y w h half? -- dither ) \ a new bitmap, as the display is, with no rows made
    d# 14 >c halloc dth !
    ahalf !
    akh !  >r  ay0 !  LANESHIFT rshift aoff !  r>   ( bitmap img w )
    swap dup aimg ! image!                          ( bitmap w )
    swap abm !
    PXMASK + PXSHIFT rshift abm @ CELL + !
    tw @  ahalf @ if d# 1 rshift then  aw !
    aw @ PXMASK + PXSHIFT rshift afull !
    aw @ LANES 1- + LANESHIFT rshift an !
    ahalf @ if an @ >c halloc agr ! then
    d# 512 LANE1 * anew acur !                      \ the rows that are not there gave the first row four
    d# 128 LANE1 * anew dup apreset anxt !          \ times nothing, and the second once
    d# 0 anew dup apreset anx2 !
    abm @ @ 0= if                                   \ a bitmap that has none is given one,
        abm @ CELL + @ akh @ * >c halloc abm @ !    \ last: it is what is kept when the rest is done with
    then                                            \ and one that has is dithered into again
    pixels
    d# 0 ay !
    dth @
;

\ The bitmap is of the w by h of the picture from x, y, and x is where a
\ cell of the bitmap starts.
header dither-part
: dither-part ( bitmap img x y w h -- dither ) \ of the image at its own size
    false (dither)
;

header dither-half
: dither-half ( bitmap img x y w h -- dither ) \ of the image at half its size: x, y, w and h are of that
    true (dither)
;

header dither-start
: dither-start ( bitmap img -- dither ) \ for the whole of the image
    dup image!  d# 0 d# 0 tw @ th @ dither-part
;

header dither-row
: dither-row ( dither -- ) \ the next row of its image, which is read if it has to be
    dth !  aimg @ tiff-row
    acur @  dup ahalf @ if ahalf-row else agrays then
    dup apixels  dup atrim                          ( lanes )
    ay @ ay0 @ -  dup akh @ u< if                   \ a row that is kept: its pixels, from the first cell kept
        abm @ CELL + @ *  abm @ @ +                 ( lanes dst )
        swap aoff @ + swap abits
    else
        2drop
    then
    acur @ anx2 @ adown2
    acur @ anxt @ adown1
    acur @  anxt @ acur !  anx2 @ anxt !  dup anx2 !
    d# 0 over alanes  apreset
    d# 1 ay +!
;

header atkinson
: atkinson ( bitmap img -- ) \ dither the whole image into a new bitmap
    dither-start
    begin
        ay @ th @ <
    while
        dup dither-row
    repeat
    drop
;

\ ---- to sixteen levels
\
\ A picture kept as gray (gray.fs) is dithered the same way, but a lane is
\ a sample and not a pixel, and is made the nearest of sixteen levels and
\ not black or white.  A sample is the gray of 1, 2 by 2, 4 by 4 or 8 by 8
\ of the image's pixels, their average to the nearest: the pixels of the
\ rows a sample is are added two to a lane, as at half the size, into one
\ row of lanes; for more than two across, each two lanes of that are then
\ added into one (fold-pairs, tiff.fs), a cell of lanes at a time, until a
\ lane is a sample; and the sum is shifted down.
\
\ What a lane holds when its sample is looked at is its number, as with a
\ pixel: its gray and 768, and what it is owed.  What the sample gives, and
\ its level, are both had by one fetch from a table of the numbers a lane
\ can hold (glt): what it gives is the low twelve bits, which go to the
\ next two as what a pixel gives does, and its level is the four above.  So
\ the lanes of a row that is done are both what the rows below are owed
\ and the levels, and each of the four planes of the levels is gathered
\ out of them as the dither gathers its pixels: a bit of each lane, at
\ 12, 13, 14 or 15.  A sample gives what its level is short of it or over
\ it, an eighth to each of six, as a pixel gives what black or white is.
\
\ A lane's number is never under 762 or over 1029.  A sample is out by
\ half a level, 8, or less, unless it is past black or white, and then by
\ less than it is owed; it is owed an eighth of what each of six was out
\ by, and so never more than 6 below black or above white.  The table is of
\ the 512 numbers from 640.

meta
\ A number in each lane of a cell, and the bits of a lane a gray has: what
\ a sum of grays is once it is shifted down, without what came down from
\ the lane above.
:: LANES*     lanes 1 > if s" aone @ *" evaluate then ;
:: GRAY-BITS  lanes 1 > if s" gray-mask @ and" evaluate then ;
\ The samples of a cell, as CELL-DONE does a cell's pixels: each one fetch.
:: CELL-LEVELS ( x -- e x' )
    lanes 0 do
        s" dup d# 16 rshift swap h# fff and" evaluate   \ the rest of the cell, and the sample's number
        s" glt @ + @" evaluate                          \ what it gives, with its level above it
        s" tuck h# fff and h# 10001 * +" evaluate        \ what it gives, to the next two
    loop
    s" >r" evaluate
    lanes 1- 0 ?do s" d# 16 lshift +" evaluate loop
    s" r>" evaluate
;
\ The bit at lsh of each lane of a cell, a bit each, where acell-bits
\ gathers them.
:: PLANE-BITS ( e -- n )
    lanes 1 = if
        s" lsh @ rshift d# 1 and" evaluate
        disp-left-low if cell-bits 1- literal s" lshift" evaluate then
    else
        s" lsh @ rshift aone @ and agather @ * NIBSHIFT rshift" evaluate
        disp-left-low if s" NIBSHIFT lshift" evaluate then
    then
;
target

: alt   ( -- a ) dth @ c# 14 + ;        \ its table of what a lane's number gives, less 640
: agw   ( -- a ) dth @ c# 15 + ;        \ the row of lanes pairs of pixels are added in, for 2 by 2 or more
: ash   ( -- a ) dth @ c# 16 + ;        \ pixels a side to a sample, as a shift

create glt 0 ,          \ the table of the dither in hand, while a row is done
create lsh 0 ,          \ the bit of each lane that the plane being gathered is of

: v-level ( v -- level ) \ the nearest of the sixteen levels to a lane's number: the first, if it is below it
    d# 760 -  d# 0 swap                             ( level n ) \ n is the gray and 8
    begin  over d# 15 <  over d# 16 >  and  while  d# 17 -  swap 1+ swap  repeat
    drop
;

: levels-table ( a -- ) \ from a, for each number from 640: what its sample gives, and its level at 12
    d# 640 >r                                       ( R: the number )
    d# 512 bounds
    begin
        2dupxor
    while
        r@ v-level                                  ( end a level )
        r@ d# 256 + over d# 17 * - d# 3 rshift      ( end a level gives )
        swap d# 12 lshift or  over !
        r> 1+ >r  1+
    repeat
    2drop rdrop
;

W32+? [if]
: fold2 ( x y -- z ) \ two cells of lanes, each two lanes added: one cell, x's in its lower half
    fold-pairs HALFBITS lshift  swap fold-pairs or
;
[else]
: fold2 ( x y -- z ) + ;
[then]

: afold ( a n -- ) \ the n cells of lanes from a, each two lanes added, into the first half of them
    over >r  u2/ over + swap                        ( end to ) ( R: a )
    begin
        2dupxor
    while
        dup 2* r@ -  dup @ swap 1+ @ fold2  over !  \ from twice as far from a
        1+
    repeat
    2drop rdrop
;

: awide ( -- n ) \ cells of the row of lanes of pairs: as many as the row has, and as many as are folded
    tw @ d# 1 rshift LANES 1- + LANESHIFT rshift  an @ ash @ 1- lshift  max
;

create ahalf-of 0 ,     \ half of what the sums are shifted down by, in each lane,
create ashift-of 0 ,    \ and the shift, while a row of samples is made

: asums ( lanes -- ) \ the grays of the next row of samples, each added into its lane
    ash @ 0= if agrays exit then
    d# 0 agw @ awide cells-fill
    agw @ apairs                                    \ the first row is in hand
    d# 1 ash @ lshift 1- negate
    begin dup while aimg @ tiff-row agw @ apairs 1+ repeat
    drop
    agw @  an @ ash @ 1- lshift                     ( lanes a n ) \ each two lanes added, until a lane is a sample
    begin dup an @ <> while 2dup afold u2/ repeat
    drop                                            ( lanes a )
    ash @ 2* dup ashift-of !  d# 1 swap 1- lshift LANES* ahalf-of !
    tuck - >r                                       ( a ) ( R: from the sums to the lanes )
    an @ bounds
    begin
        2dupxor
    while
        dup @ ahalf-of @ + ashift-of @ rshift GRAY-BITS  over r@ + +!
        1+
    repeat
    2drop rdrop
;

W32+? [if]
: lpixels ( lanes -- ) \ every sample of the row: each lane left as what it gives, and its level
    h# 800100 >r
    an @ bounds
    begin
        2dupxor
    while
        dup @ r> +  CELL-LEVELS  >r over !
        1+
    repeat
    2drop rdrop
;
[else]
: lpixels ( lanes -- ) \ the same, a cell being one sample (apixels says how)
    d# 128 >r
    d# 256 over +!
    an @ bounds
    begin
        2dupxor
    while
        dup @ glt @ + @                             ( end p gives )
        h# fff over r@ + and
        swap rdrop >r
        swap r@ over !
        1+ tuck +!
    repeat
    2drop rdrop
;
[then]

: lcell-bits ( p -- p' bits ) \ the plane at lsh of the samples whose lanes start at p, as a cell of it
    d# 0 >r
    dup d# 16 + swap
    begin
        2dupxor
    while
        dup @ PLANE-BITS
LEFT-LOW? [if]
        r> LANES rshift  or >r
[else]
        r> LANES lshift  or >r
[then]
        1+
    repeat
    nip r>
;

: lbits ( lanes dst -- ) \ the plane at lsh of the row's levels, into the row of it at dst
    dup abm @ CELL + @ + swap                       ( lanes end dst )
    rot >r
    begin
        2dupxor
    while
        r> lcell-bits swap >r
        over ! 1+
    repeat
    2drop rdrop
;

: lplanes ( lanes -- ) \ the four planes of the row's levels, into the four rows it has, the lowest first
    ay @ d# 2 lshift abm @ CELL + @ *  abm @ @ +    ( lanes dst )
    d# 12 lsh !
    begin
        lsh @ d# 16 <
    while
        2dup lbits  abm @ CELL + @ +
        d# 1 lsh +!
    repeat
    2drop
;

header dither-levels
: dither-levels ( planes img shift -- dither ) \ a dither of the whole image to sixteen levels, a sample to 1 << shift pixels a side
    \ planes is where the planes go and how many cells a row of one is: a
    \ row of samples is four rows of planes, its lowest plane first.
    d# 18 >c halloc dth !
    ash !  dup aimg ! image!  abm !
    tw @ ash @ rshift aw !
    aw @ PXMASK + PXSHIFT rshift afull !
    aw @ LANES 1- + LANESHIFT rshift an !
    d# 0 aoff !  d# 0 ay0 !  th @ ash @ rshift akh !  false ahalf !
    ash @ if awide >c halloc agw ! then
    d# 512 >c halloc  dup levels-table  d# 640 - alt !
    d# 512 LANE1 * anew acur !                      \ as (dither) has them
    d# 128 LANE1 * anew dup apreset anxt !
    d# 0 anew dup apreset anx2 !
    pixels
    d# 0 ay !
    dth @
;

header level-row
: level-row ( dither -- ) \ the next row of samples: read, each made the nearest level, its planes kept
    dth !  alt @ glt !
    aimg @ tiff-row
    acur @  dup asums  dup lpixels  dup atrim  dup lplanes
    anx2 @ adown2
    acur @ anxt @ adown1
    acur @  anxt @ acur !  anx2 @ anxt !  dup anx2 !
    d# 0 over alanes  apreset
    d# 1 ay +!
;
