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
\ There are three rows of lanes, and the dither keeps where they are, the
\ bitmap and how far down it is, between one row and the next.  Nothing
\ else is kept: a row is done on the stacks.
\
\ A cell must hold two lanes at least, for a pixel to give to the two
\ after it: cells of 32 bits or 64.

W32+? [if]

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
:: LANESHIFT  lanes 4 = if 2 else 1 then literal ;  \ a pixel to its cell
:: NIBSHIFT   cell-bits lanes - literal ;           \ the top of a cell, less its pixels
:: TOPLANE    cell-bits #16 - literal ;             \ the highest lane, as a shift

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

create aone    meta lane1 target ,                  \ 1 in each lane
create aclean  meta lane1 $fff * target ,           \ the number in each lane, without whether it is black
create agather meta gather target ,

create abm   0 ,        \ the bitmap: its descriptor
create an    0 ,        \ cells of lanes that have pixels
create acur  0 ,        \ the row being done,
create anxt  0 ,        \ the next,
create anx2  0 ,        \ and the one after: each a row of lanes
create ay    0 ,        \ how many rows are done

\ A row of lanes has as many cells as make whole cells of the bitmap, so
\ that the pixels can be gathered sixteen cells at a time, and a cell at
\ each end for what leaves the row.
: alanes ( x a -- ) \ set a row of lanes, and the cell at each end of it
    1-  abm @ CELL + @ d# 4 lshift d# 2 +  cells-fill
;

: anew ( x -- a ) abm @ CELL + @ d# 4 lshift d# 2 + >c halloc 1+  tuck alanes ;

: apreset ( a -- ) \ what the pixel that is not there, beside each end of the row, would give the row
    d# 128 over +!
    tw @ 1-  dup LANES 1- and d# 4 lshift d# 128 swap lshift    ( a last x )
    swap LANESHIFT rshift rot + +!
;

\ Where the image can give a row's grays in lanes ('grays), it adds them a
\ cell at a time.  Any other is asked for a pixel at a time.
: agrays ( lanes -- ) \ the next row's grays, each added to its pixel's lane
    'grays @ ?dup if execute exit then
    d# 0 swap  tw @ negate                          ( shift a -n )
    begin
        dup
    while
        >r
        'pixel @ execute  >r over r> swap lshift  over +!
        swap d# 16 +  dup CELLBITS = if drop 1+ d# 0 then  swap
        r> 1+
    repeat
    drop 2drop
;

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

: atrim ( lanes -- ) \ the lanes of the last cell that are past the last pixel give nothing
    tw @ LANES 1- and ?dup if
        d# 4 lshift d# 1 swap lshift 1-             ( lanes mask )
        swap an @ + 1- and!
    else
        drop
    then
;

: anib ( e -- n ) \ whether each pixel of a cell is black: a bit each, as the display has them
    d# 12 rshift aone @ and  agather @ *  NIBSHIFT rshift
;

: acell-bits ( p -- p' bits ) \ the pixels whose lanes start at p, as a cell of the bitmap
    d# 0 >r                                         ( p ) ( R: the cell being made )
    dup d# 16 + swap
    begin
        2dupxor
    while
        dup @ anib
LEFT-LOW? [if]
        NIBSHIFT lshift  r> LANES rshift  or >r
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
        dup @ aclean @ and  over r@ + +!
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
        dup r@ +  over @ aclean @ and               ( end p t e )
        2dup TOPLANE lshift swap +!                 \ its lowest lane, to the highest of the cell before
        2dup TOPLANE rshift swap CELL2 + +!         \ its highest, to the lowest of the cell after
        dup h# 10001 * swap d# 16 rshift +          \ itself, and a lane each way
        swap 1+ +!
        1+
    repeat
    2drop rdrop
;

\ A row is dithered at a time, so that whoever wants the picture can do
\ something else between the rows, and can show the rows that are made.

header dither-start
: dither-start ( bitmap -- ) \ a new bitmap for the image, as the display is, with no rows made
    abm !
    tw @ PXMASK + PXSHIFT rshift abm @ CELL + !
    abm @ CELL + @ th @ * >c halloc abm @ !
    tw @ LANES 1- + LANESHIFT rshift an !
    d# 512 LANE1 * anew acur !                      \ the rows that are not there gave the first row four
    d# 128 LANE1 * anew dup apreset anxt !          \ times nothing, and the second once
    d# 0 anew dup apreset anx2 !
    pixels
    d# 0 ay !
;

header dither-row
: dither-row ( -- ) \ the next row of the image, which must have been read
    acur @  abm @ @  ay @ abm @ CELL + @ * +        ( lanes dst )
    over agrays  over apixels  over atrim
    abits
    acur @ anx2 @ adown2
    acur @ anxt @ adown1
    acur @  anxt @ acur !  anx2 @ anxt !  dup anx2 !
    d# 0 over alanes  apreset
    d# 1 ay +!
;

header atkinson
: atkinson ( bitmap -- ) \ dither the whole image into a new bitmap
    dither-start
    begin
        ay @ th @ <
    while
        dither-row
    repeat
;

[then]
