\ gray.fs - a picture kept as gray, and drawn black and white
\ through a mask

\ The dither (atkinson.fs) turns a picture into a bitmap once.
\ This keeps the picture as gray instead, in sixteen levels, at
\ full resolution or less, and turns it black and white only
\ when it is drawn: a pixel is white if its level is above the
\ threshold at that place in a mask tiled over the bitmap.  The
\ mask is blue noise, whose thresholds are ordered to keep each
\ level's pixels spread apart, or Bayer's grid.  With one sample
\ per 4 by 4 pixels, the Mandrill takes as many cells as a
\ bitmap of it at half size, and can be redrawn through another
\ mask, or darker or lighter, without reading the picture again.
\ bench/masks.c compares the ways a picture can be kept.

\ The gray is made by the dither run to sixteen levels instead
\ of two (dither-levels, level-row), a row of samples at a time:
\ each sample is the average of the pixels it covers, rounded to
\ the nearest level, with the error passed on as the dither
\ passes it on.  A row of samples is stored as four bit planes,
\ one per bit of the level, lowest first; each plane is a row of
\ cells, one bit per sample, in the display's pixel order.

\ A mask is also four bit planes per row, and is stored on the
\ disk that way (util/bluenoise.c), so loading one is copying
\ cells.  While copying, the thresholds can be raised or lowered
\ a level at a time, by a bit-sliced add across the four planes
\ that does every pixel of a cell at once.  A mask row is as
\ wide as on the disk; narrower masks repeat across it, so every
\ mask has the same number of cells across.

\ To draw a row, each plane's row of samples is first widened to
\ pixels: the samples are shifted apart in a few steps and the
\ gaps filled, so each covers as many pixels as it stands for.
\ Then each bitmap cell is set by comparing those levels with
\ the mask's thresholds.  The mask is stored inverted (15 minus
\ the threshold), so a level is above the threshold exactly when
\ level + (15 - threshold) carries out of four bits.  That carry
\ is computed plane by plane, lowest first, like a bit-serial
\ add, for every pixel of the cell at once.

\ A gray, a mask and a drawing are each a record passed to the
\ words that use it, so several can be in use at once, each kept
\ or drawn a row at a time by its own task.

\ ---- a gray picture

\ The record starts with the planes' address and cells per plane
\ row, which is the pair the dither takes as the place to
\ write them.
$6 constant GRAY-CELLS

: gpl   ( gray -- a ) ;          \ the planes: four plane rows
                                 \ per row of samples
: grs   ( gray -- a ) 1+ ;       \ cells in a plane row
: gcols ( gray -- a ) CELL2 + ;  \ samples across,
: grows ( gray -- a ) c# 3 + ;   \ and down
: gsh   ( gray -- a ) c# 4 + ;   \ pixels per side of a sample,
                                 \ as a shift: 0 to 3
: gdth  ( gray -- a ) c# 5 + ;   \ the dither making it, while
                                 \ it is being made

header gray-cols   : gray-cols   ( gray -- n ) gcols @ ;
header gray-rows   : gray-rows   ( gray -- n ) grows @ ;
header gray-dither : gray-dither ( gray -- dither ) gdth @ ;

\ Whoever keeps the planes somewhere else than all in memory (a
\ block of them at a time, the rest on a disk: view.fs) makes
\ the record alone, and stores in its first cell where the
\ planes would start if they were all there, before a row is
\ made or drawn: both are told the row, and look there for it.

\ the record of a gray for a w by h picture, one sample per 1 <<
\ shift pixels a side, with no planes yet
header gray-record
: gray-record ( w h shift -- gray )
    GRAY-CELLS >c halloc >r
    dup r@ gsh !  tuck rshift r@ grows !  rshift r@ gcols !
    r@ gcols @ PXMASK + PXSHIFT rshift r@ grs !
    r>
;

header gray-planes
: gray-planes ( gray -- ) \ memory for all of its planes
    dup grows @ d# 2 lshift over grs @ *  >c halloc swap gpl ! ;

\ an empty gray, all of it in memory
header gray-new
: gray-new ( w h shift -- gray ) gray-record dup gray-planes ;

\ The dither's memory is allocated after the gray's.  Whoever
\ opens the gray frees it after the last row.

\ start making the gray from an image, a row of samples at
\ a time
header gray-open
: gray-open ( img gray -- )
    dup >r  swap r@ gsh @ dither-levels  r> gdth !
;

header gray-row : gray-row ( gray -- )
    \ make its next row of samples
    gdth @ level-row ;

header gray-keep
: gray-keep ( img shift -- gray )
    \ make the whole gray of a picture at once
    over image!  tw @ th @ rot gray-new  ( img gray )
    himem @ >r  tuck gray-open
    dup grows @ negate
    begin dup while over gray-row 1+ repeat
    drop
    \ free the dither's memory: nothing was allocated after it
    r> himem !
;

\ ---- masks

\ A mask's record is its planes' address and its number of rows.
\ A row is its four planes, lowest first, each MASK-CELLS cells
\ across: MASK-ROW cells in all, the same as a row on the disk.
: mpl ( mask -- a ) ;
: mn  ( mask -- a ) 1+ ;

meta
\ On the disk a mask cell has its leftmost pixel in the lowest
\ bit.  Where the display has the leftmost pixel highest, each
\ cell's bits are reversed: swap the halves, then the halves of
\ each half, and so on.
variable rk
\ alternate groups of k bits of a cell, starting with the
\ lowest group
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

\ Add or subtract one level at every threshold of the four
\ planes at an address, every pixel of the cell at once.  The
\ carry (or borrow) from plane to plane is kept on the data
\ stack over the address.  A carry left over at the top means
\ the threshold overflowed, so it is set back to the highest
\ level (or, for a borrow, the lowest).
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

\ Copy every cell of a mask row, from the disk's layout at src
\ to the mask's at dst, with the brightness at the top of the
\ return stack.
:: LINE-CELLS ( src dst -- src' dst' )
    disk-mask-width cell-bits / 0 ?do
        s" 2dup r@ mask-cell  d# 4 +  swap 1+ swap" evaluate
    loop
;
target

\ copy one cell of each plane: from src, MASK-CELLS apart, to
\ dst and the next three cells
: mask-copy ( src dst -- )
    over @ REVERSE over !
    over MASK-CELLS + @ REVERSE over 1+ !
    over MASK-CELLS 2* + @ REVERSE over CELL2 + !
    swap MASK-CELLS c# 3 * + @ REVERSE swap c# 3 + !
;

\ raise each threshold at a by a level, stopping at the highest
: level-up ( a -- ) UP-A-LEVEL ;
: level-down ( a -- )
    \ lower each by a level, stopping at the lowest
    DOWN-A-LEVEL ;

\ copy a cell of each plane, adjust it by bright levels, and
\ invert it
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

\ copy a mask row from the disk's layout at src to dst; return
\ the next row of each
: mask-line ( src dst bright -- src' dst' )
    >r  LINE-CELLS  rdrop
    swap MASK-ROW MASK-CELLS - + swap  \ src to the next row's
                                       \ first cell
;

: mask-lines ( src dst n bright -- src' dst' )
    \ copy n mask rows, each MASK-ROW cells after the last
    >r  MASK-ROW * over + swap  ( src end dst ) ( R: bright )
    begin
        2dupxor
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

\ bright is added to every threshold: more is darker, less is
\ lighter.  A threshold stays within the lowest and
\ highest levels.
header mask-from
: mask-from ( a n bright -- mask )
    \ a mask of n rows, from memory in the disk's layout
    >r  dup mask-new dup >r    ( a n mask ) ( R: bright mask )
    mpl @ swap  r> r> swap >r  ( a dst n bright ) ( R: mask )
    mask-lines 2drop  r>
;

\ read n mask rows from the disk, starting at a block, to dst, a
\ block at a time
: mask-blocks ( blk dst n bright -- )
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
    \ a mask of n rows, read from the disk starting at a block
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
\ Widen a slice of a cell of samples into a cell of pixels, f
\ pixels per sample.  The slice is shifted to the left of the
\ cell and the rest cleared (the first mask in its table).  Then
\ the samples are moved apart in steps, widest step first (the
\ masks after it), and each sample's bit is copied into the f -
\ 1 pixels after it.
variable sk  variable sf
: log2 ( n -- k )
    0 begin 2dup 1 swap lshift > while 1+ repeat nip ;
: at-left ( i -- bit ) \ the bit that holds pixel i of a cell
    disp-left-low if else cell-bits 1- swap - then ;

\ groups of 1 << k pixels, one every (1 << k) * f pixels, from
\ the left
: spread-mask ( k f -- m )
    sf !  1 swap lshift sk !
    0 cell-bits 0 ?do
        i sk @ sf @ * mod sk @ < if 1 i at-left lshift or then
    loop
;

\ lay down the masks for widening by f: the slice's, then
\ each step's
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

\ spread x's pixels, from the left, to sp-f pixels each, filling
\ a cell
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
\ A loop that widens a plane's row of samples, from src, into a
\ row of pixel cells, storing every fourth cell from out up to
\ end.  The cell number in the picture as drawn is on top of the
\ return stack.  gex-mid holds the text that makes one cell of
\ pixels from src.  (It is kept in a variable because the host
\ stack is in use while the loop's text is evaluated.)
2variable gex-mid

: gex-loop ( a u -- )
    gex-mid 2!
    s" >r begin 2dupxor while rot" evaluate
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

: widen2 ( x j -- y )
    \ half j of a cell of samples, each sample made two pixels
    SLICE2 gsp2 @ and APART2 ;
: widen4 ( x j -- y ) \ quarter j, each sample made four pixels
    SLICE4 gsp4 @ and APART4 ;

: gex0 ( src end out ca -- ) \ one sample per pixel,
    ROW-AS-IS ;
: gex1 ( src end out ca -- ) ROW-BY-2 ;    \ per 2 by 2,
: gex2 ( src end out ca -- ) ROW-BY-4 ;    \ per 4 by 4

\ ---- drawing

\ A drawing is a record: the bitmap drawn into, the mask, the
\ gray's planes, the drawing scale, and the current row of
\ samples widened to pixels (ex).  ex is placed in memory so
\ that a cell's offset in it, masked to a mask row's size, is
\ the offset of that cell's thresholds in the mask row: one AND
\ finds the thresholds.  The drawing can be at a smaller scale
\ than the gray, and of only part of it: the bitmap holds the
\ part of the picture starting at a given cell across and row
\ down, as many cells across as a bitmap row.
$c constant DRAW-CELLS

: drw-bm   ( d -- a ) ;          \ the bitmap drawn into
: drw-mask ( d -- a ) 1+ ;       \ the mask
: drw-pl   ( d -- a ) CELL2 + ;  \ the gray, with its planes,
: drw-rs   ( d -- a ) c# 3 + ;   \ and the cells in a row of one
: drw-dsh  ( d -- a ) c# 4 + ;   \ pixels per side of a sample
                                 \ as drawn, as a shift: 0, 1
                                 \ or 2
: drw-xc   ( d -- a ) c# 5 + ;   \ the cell across the picture
                                 \ as drawn where the
                                 \ bitmap starts,
: drw-y0   ( d -- a ) c# 6 + ;   \ and the row
: drw-oc   ( d -- a ) c# 7 + ;   \ cells in a row of the bitmap
: drw-ex   ( d -- a ) c# 8 + ;   \ the current row of samples as
                                 \ pixels: four planes per cell
: drw-exy  ( d -- a ) c# 9 + ;   \ which row of samples ex holds
: drw-mem  ( d -- a ) c# 10 + ;  \ the memory ex is in

\ widen a plane's row of samples from src, storing every fourth
\ cell from out
: gexplane ( src out d -- )
    >r  dup r@ drw-oc @ d# 2 lshift + swap  r@ drw-xc @
    ( src end out ca )
    r> drw-dsh @  ?dup 0= if gex0 exit then
    d# 1 = if gex1 exit then
    gex2
;

\ widen row sy of the samples into ex: four planes per cell of a
\ bitmap row
: gexpand ( sy d -- )
    \ the lowest plane
    >r  d# 2 lshift r@ drw-rs @ * r@ drw-pl @ gpl @ +
    ( src ) ( R: d )
    dup r@ drw-ex @ r@ gexplane
    r@ drw-rs @ +  dup r@ drw-ex @ 1+ r@ gexplane
    r@ drw-rs @ +  dup r@ drw-ex @ CELL2 + r@ gexplane
    r@ drw-rs @ +  r@ drw-ex @ c# 3 + r@ gexplane
    rdrop
;

meta
\ Which pixels of a cell are white: the carry out of four bits
\ when the levels at p are added to the inverted thresholds at
\ the top of the return stack.  Only the carry is kept on the
\ stack, over p, from plane to plane.
:: WHITE ( p -- p w )
    s" dup @ r@ @ and" evaluate
    4 1 do
        s" over" evaluate  i literal  s" + @  r@" evaluate
        i literal  s" + @  2dup and >r xor and r> or" evaluate
    loop
;
target

\ draw n bitmap cells at a, from the levels at p and the mask
\ row at qrow
: gcells ( a n p qrow -- )
    >r  -rot over + swap  ( p end a ) ( R: qrow )
    begin
        2dupxor
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

\ A picture is drawn a row at a time (gray-draw-row), so a task
\ can draw a few rows each turn.  gray-draw-done frees the
\ drawing's memory.

\ start drawing the gray, a sample to 1, 2 or 4 pixels a side,
\ from x0 y0 into a bitmap
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

\ draw row y of the bitmap, which is row y0 + y of the picture
\ as drawn
header gray-draw-row
: gray-draw-row ( y d -- )
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
: gray-draw-done ( d -- ) \ free the drawing's memory
    dup drw-mem @  over drw-oc @ d# 2 lshift MASK-ROW +  hgive
    DRAW-CELLS hgive
;

\ allocate a bitmap for the descriptor at bitmap, big enough for
\ the whole gray drawn at 1 << dshift
header gray-bitmap
: gray-bitmap ( bitmap gray dshift -- )
    >r
    dup gcols @ r@ lshift PXMASK + PXSHIFT rshift
    rot tuck 1+ !
    ( gray bitmap )
    swap grows @ r> lshift  over 1+ @ *  >c halloc  swap !
;

\ draw the whole gray into a bitmap through a mask, all at once
header gray-draw
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
