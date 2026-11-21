\ gray.fs - a picture kept as gray, and drawn black and white
\ through a mask

\ A row of samples is four bit planes of the level, the lowest
\ first, a bit a sample in the display's order.  A row of a
\ mask is four planes too (util/bluenoise.h), kept as 15 less
\ the threshold: a pixel is white where its level and that carry
\ out of four bits.

\ ---- a gray picture

$6 constant GRAY-CELLS

: gpl   ( gray -- a ) ;          \ the planes
: grs   ( gray -- a ) 1+ ;       \ cells in a plane row
: gcols ( gray -- a ) CELL2 + ;  \ samples across,
: grows ( gray -- a ) c# 3 + ;   \ and down
: gsh   ( gray -- a ) c# 4 + ;   \ a sample's side, as a shift
: gdth  ( gray -- a ) c# 5 + ;   \ the dither making it

header gray-cols   : gray-cols   ( gray -- n ) gcols @ ;
header gray-rows   : gray-rows   ( gray -- n ) grows @ ;
header gray-dither : gray-dither ( gray -- dither ) gdth @ ;

\ of a w by h picture, a sample 1 << shift pixels a side; with
\ no planes yet
header gray-record
: gray-record ( w h shift -- gray )
    GRAY-CELLS >c halloc >r
    dup r@ gsh !  tuck rshift r@ grows !  rshift r@ gcols !
    r@ gcols @ PXMASK + PXSHIFT rshift r@ grs !
    r>
;

header gray-planes
: gray-planes ( gray -- ) \ memory for all of its planes
    dup grows @ d# 2 lshift over grs @ *
    >c halloc swap gpl ! ;

header gray-new
: gray-new ( w h shift -- gray ) gray-record dup gray-planes ;

\ the dither's memory, which whoever opens the gray frees
header gray-open
: gray-open ( img gray -- )
    dup >r  swap r@ gsh @ dither-levels  r> gdth !
;

header gray-row : gray-row ( gray -- ) \ its next row of samples
    gdth @ level-row ;

header gray-keep
: gray-keep ( img shift -- gray ) \ the whole gray, at once
    over image!  tw @ th @ rot gray-new  ( img gray )
    himem @ >r  tuck gray-open
    dup grows @ negate
    begin dup while over gray-row 1+ repeat
    drop
    r> himem !
;

\ ---- masks: a record is the planes' address and how many rows

: mpl ( mask -- a ) ;
: mn  ( mask -- a ) 1+ ;

meta
\ On the disk a mask's leftmost pixel is a cell's lowest bit.
variable rk
\ alternate groups of k bits of a cell, from the lowest
: rev-mask ( k -- m )
    rk !
    0 cell-bits 0 ?do
        i rk @ / 1 and 0= if 1 i lshift or then
    loop
;

:: REVERSE ( x -- y )
    disp-left-low 0= if
        cell-bits 2/
        begin dup while
            s" dup" evaluate  dup literal s" rshift" evaluate
            dup rev-mask literal s" and swap" evaluate
            dup rev-mask literal s" and" evaluate
            dup literal s" lshift or" evaluate
            2/
        repeat
        drop
    then
;

\ a level added to, or taken from, every threshold of the four
\ planes at a, which stops at the highest level, or the lowest:
\ a carry rippled up the planes, and where it comes out of the
\ fourth every plane is set, or cleared
:: UP-A-LEVEL ( a -- )
    s" true swap" evaluate  \ ( carry a )
    4 0 do
        s" dup" evaluate  i literal
        s" + @ rot 2dup and >r xor over" evaluate
        i literal  s" + ! r> swap" evaluate
    loop
    4 0 do s" 2dup" evaluate i literal s" + or!" evaluate loop
    s" 2drop" evaluate
;

:: DOWN-A-LEVEL ( a -- )
    s" true swap" evaluate  \ ( borrow a )
    4 0 do
        s" dup" evaluate  i literal
        s" + @ rot 2dup swap invert and >r xor over" evaluate
        i literal  s" + ! r> swap" evaluate
    loop
    s" swap invert swap" evaluate
    4 0 do s" 2dup" evaluate i literal s" + and!" evaluate loop
    s" 2drop" evaluate
;

\ a mask row from the disk's layout at src; bright is on R
:: LINE-CELLS ( src dst -- src' dst' )
    disk-mask-width cell-bits / 0 ?do
        s" 2dup r@ mask-cell  d# 4 +  swap 1+ swap" evaluate
    loop
;
target

\ a cell of each plane, MASK-CELLS apart at src, to dst on
: mask-copy ( src dst -- )
    over @ REVERSE over !
    over MASK-CELLS + @ REVERSE over 1+ !
    over MASK-CELLS 2* + @ REVERSE over CELL2 + !
    swap MASK-CELLS c# 3 * + @ REVERSE swap c# 3 + !
;

: level-up ( a -- ) UP-A-LEVEL ;
: level-down ( a -- ) DOWN-A-LEVEL ;

\ a cell of each plane, bright levels darker, and inverted
: mask-cell ( src dst bright -- )
    >r tuck mask-copy  r>  ( dst bright )
    dup 0< if
        begin dup while over level-down 1+ repeat
    else
        negate begin dup while over level-up 1+ repeat
    then
    drop
    dup not!  dup 1+ not!  dup CELL2 + not!  c# 3 + not!
;

: mask-line ( src dst bright -- src' dst' )
    >r  LINE-CELLS  rdrop
    swap MASK-ROW MASK-CELLS - + swap
;

: mask-lines ( src dst n bright -- src' dst' )
    >r  MASK-ROW * over + swap  ( src end dst ) ( R: bright )
    begin
        2dup xor
    while
        >r swap r>  r@ mask-line  >r swap r>  ( src' end dst' )
    repeat
    nip rdrop
;

: mask-new ( n -- mask ) \ an empty mask of n rows
    d# 2 >c halloc >r  dup r@ mn !
    MASK-ROW * >c halloc r@ mpl !
    r>
;

\ bright is added to every threshold: more is darker
header mask-from \ n rows, from memory in the disk's layout
: mask-from ( a n bright -- mask )
    >r  dup mask-new dup >r    ( a n mask ) ( R: bright mask )
    mpl @ swap  r> r> swap >r  ( a dst n bright ) ( R: mask )
    mask-lines 2drop  r>
;

: mask-blocks ( blk dst n bright -- ) \ n rows from the disk
    >r  ( blk dst n ) ( R: bright )
    begin
        dup d# 0 >
    while
        >r  over cblock over
        r@ d# 1 MASK-ROWS-SHIFT lshift min
        ( blk dst src dst rows ) ( R: bright n )
        r> r@ swap >r            ( blk dst src dst rows bright )
        mask-lines  >r 2drop r>  ( blk dst' )
        swap 1+ swap  r> d# 1 MASK-ROWS-SHIFT lshift -
    repeat
    drop 2drop rdrop
;

header mask-load
: mask-load ( blk n bright -- mask )
    >r  dup mask-new dup >r    ( blk n mask ) ( R: bright mask )
    mpl @ swap  r> r> swap >r  ( blk dst n bright ) ( R: mask )
    mask-blocks  r>
;

header mask-bayer  : mask-bayer  ( bright -- mask )
    >r BAYER-BLK d# 8 r> mask-load ;
header mask-blue64 : mask-blue64 ( bright -- mask )
    >r MASK64-BLK d# 64 r> mask-load ;
header mask-blue32 : mask-blue32 ( bright -- mask )
    >r MASK32-BLK d# 32 r> mask-load ;

\ ---- widening

meta
\ A slice of a cell of samples is made a cell of pixels, f
\ pixels a sample, by the masks of a table.
variable sk  variable sf
: log2 ( n -- k )
    0 begin 2dup 1 swap lshift > while 1+ repeat nip ;
: at-left ( i -- bit ) \ the bit that holds pixel i of a cell
    disp-left-low if else cell-bits 1- swap - then ;

\ groups of 1 << k pixels, one every (1 << k) * f, from the left
: spread-mask ( k f -- m )
    sf !  1 swap lshift sk !
    0 cell-bits 0 ?do
        i sk @ sf @ * mod sk @ < if 1 i at-left lshift or then
    loop
;

\ the masks for widening by f: the slice's, then each step's
: spread-table ( f -- )
    >r
    0  cell-bits r@ / 0 ?do  1 i at-left lshift or  loop
    tcalign tcell,
    cell-bits r@ / log2
    begin dup while
        1-  dup r@ spread-mask tcalign tcell,
    repeat
    drop r> drop
;
variable sp-f  variable sp-k  variable sp-i

\ x's pixels, from the left, sp-f pixels each: moved apart by
\ the table's masks, a step a mask, then each copied along
:: APART ( x -- y )
    cell-bits sp-f @ / log2 sp-k !  1 sp-i !
    begin sp-k @ while
        -1 sp-k +!
        s" dup" evaluate  1 sp-k @ lshift sp-f @ 1- * literal
        s" PIXR or" evaluate
        sp-f @ 2 = if s" gsp2" else s" gsp4" then evaluate
        sp-i @ literal  s" + @ and" evaluate
        1 sp-i +!
    repeat
    1 begin dup sp-f @ < while
        s" dup" evaluate  dup literal s" PIXR or" evaluate  2*
    repeat drop
;

:: APART2  2 sp-f !  s" APART" evaluate ;
:: APART4  4 sp-f !  s" APART" evaluate ;
:: SLICE2 ( x j -- x' ) \ half j of the cell, moved to the left
    cell-bits 2/ log2 literal s" lshift PIXL" evaluate ;
:: SLICE4 ( x j -- x' ) \ quarter j of the cell
    cell-bits 4 / log2 literal s" lshift PIXL" evaluate ;
\ a plane's row of samples from src widened, to every fourth
\ cell from out up to end; gex-mid ( src -- src x ) makes one
\ cell of pixels, with the cell's number on R
2variable gex-mid

: gex-loop ( a u -- )
    gex-mid 2!
    s" >r begin 2dup xor while rot" evaluate
    gex-mid 2@ evaluate
    s" rot tuck !  d# 4 +  rot swap  r> 1+ >r" evaluate
    s" repeat 2drop drop rdrop" evaluate
;
:: ROW-AS-IS ( src end out ca -- )  s" dup r@ + @" gex-loop ;
:: ROW-BY-2 ( src end out ca -- )
    s" dup r@ d# 1 rshift + @  r@ d# 1 and widen2" gex-loop ;
:: ROW-BY-4 ( src end out ca -- )
    s" dup r@ d# 2 rshift + @  r@ d# 3 and widen4" gex-loop ;
target

create gsp2 meta 2 spread-table target
create gsp4 meta 4 spread-table target

: widen2 ( x j -- y ) \ half j, each sample two pixels
    SLICE2 gsp2 @ and APART2 ;
: widen4 ( x j -- y ) \ quarter j, each sample four pixels
    SLICE4 gsp4 @ and APART4 ;

: gex0 ( src end out ca -- ) ROW-AS-IS ;
: gex1 ( src end out ca -- ) ROW-BY-2 ;
: gex2 ( src end out ca -- ) ROW-BY-4 ;

\ ---- drawing

$c constant DRAW-CELLS

: drw-bm   ( d -- a ) ;          \ the bitmap drawn into
: drw-mask ( d -- a ) 1+ ;       \ the mask
: drw-pl   ( d -- a ) CELL2 + ;  \ the gray,
: drw-rs   ( d -- a ) c# 3 + ;   \ and the cells in a plane row
: drw-dsh  ( d -- a ) c# 4 + ;   \ a sample's side as drawn
: drw-xc   ( d -- a ) c# 5 + ;   \ the cell the bitmap
                                 \ starts at,
: drw-y0   ( d -- a ) c# 6 + ;   \ and the row
: drw-oc   ( d -- a ) c# 7 + ;   \ cells in a row of the bitmap
\ a row of samples as pixels, four planes a cell: a cell's
\ offset in it, masked, is its thresholds' in a mask row
: drw-ex   ( d -- a ) c# 8 + ;
: drw-exy  ( d -- a ) c# 9 + ;   \ which row of samples ex holds
: drw-mem  ( d -- a ) c# 10 + ;  \ the memory ex is in

: gexplane ( src out d -- ) \ a plane's row of samples, widened
    >r  dup r@ drw-oc @ d# 2 lshift + swap  r@ drw-xc @
    ( src end out ca )
    r> drw-dsh @  ?dup 0= if gex0 exit then
    d# 1 = if gex1 exit then
    gex2
;

: gexpand ( sy d -- ) \ row sy of the samples, into ex
    >r  d# 2 lshift r@ drw-rs @ * r@ drw-pl @ gpl @ +
    ( src ) ( R: d )
    dup r@ drw-ex @ r@ gexplane
    r@ drw-rs @ +  dup r@ drw-ex @ 1+ r@ gexplane
    r@ drw-rs @ +  dup r@ drw-ex @ CELL2 + r@ gexplane
    r@ drw-rs @ +  r@ drw-ex @ c# 3 + r@ gexplane
    rdrop
;

meta
\ the carry out of the levels at p added to the thresholds at
\ R, a plane at a time from the lowest
:: WHITE ( p -- p w )
    s" dup @ r@ @ and" evaluate
    4 1 do
        s" over" evaluate  i literal  s" + @  r@" evaluate
        i literal  s" + @  2dup and >r xor and r> or" evaluate
    loop
;
target

\ n cells of bitmap at a, of the levels at p and a mask row
: gcells ( a n p qrow -- )
    >r  -rot over + swap  ( p end a ) ( R: qrow )
    begin
        2dup xor
    while
        rot  dup MASK-ROW 1- and r@ + >r  WHITE  rdrop
        ( end a p w )
INK-BLACK? [if]
        invert
[then]
        rot tuck !  1+  swap d# 4 + -rot  ( p' end a' )
    repeat
    2drop drop rdrop
;

\ a sample 1 << dshift pixels a side, from x0 y0 of the picture
header gray-draw-start
: gray-draw-start ( bitmap mask gray dshift x0 y0 -- d )
    DRAW-CELLS >c halloc >r
    r@ drw-y0 !  PXSHIFT rshift r@ drw-xc !  r@ drw-dsh !
    dup r@ drw-pl !  grs @ r@ drw-rs !
    r@ drw-mask !  dup r@ drw-bm !  1+ @ r@ drw-oc !
    d# -1 r@ drw-exy !
    r@ drw-oc @ d# 2 lshift MASK-ROW +  >c halloc
    dup r@ drw-mem !
    ( m )
    r@ drw-xc @ MASK-CELLS 1- and d# 2 lshift  over -
    MASK-ROW 1- and  +  r@ drw-ex !
    r>
;

header gray-draw-row
: gray-draw-row ( y d -- ) \ row y of the bitmap
    >r
    dup r@ drw-y0 @ +  ( y row ) ( R: d )
    dup r@ drw-dsh @ rshift
    dup r@ drw-exy @ <> if dup r@ gexpand dup r@ drw-exy ! then
    drop
    r@ drw-mask @ dup mn @ 1- rot and MASK-ROW * swap mpl @ +
    ( y qrow )
    swap r@ drw-bm @ dup 1+ @ rot * swap @ +  ( qrow dst )
    r@ drw-oc @  r@ drw-ex @                  ( qrow dst n p )
    >r rot r> swap                            ( dst n p qrow )
    gcells  rdrop
;

header gray-draw-done
: gray-draw-done ( d -- )
    dup drw-mem @  over drw-oc @ d# 2 lshift MASK-ROW +  hgive
    DRAW-CELLS hgive
;

\ memory for a bitmap of the whole gray drawn at 1 << dshift
header gray-bitmap
: gray-bitmap ( bitmap gray dshift -- )
    >r
    dup gcols @ r@ lshift PXMASK + PXSHIFT rshift
    rot tuck 1+ !
    ( gray bitmap )
    swap grows @ r> lshift  over 1+ @ *  >c halloc  swap !
;

header gray-draw \ the whole gray, at once
: gray-draw ( bitmap mask gray dshift -- )
    over grows @ over lshift >r  d# 0 d# 0 gray-draw-start
    ( d ) ( R: rows )
    r> d# 0                                         ( d rows y )
    begin
        2dup >
    while
        >r over r@ swap gray-draw-row r> 1+
    repeat
    2drop gray-draw-done
;
