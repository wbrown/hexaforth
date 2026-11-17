\ gray.fs - a picture kept as gray, and drawn black and white through a mask
\
\ The dither (atkinson.fs) makes a picture black and white once, and what is
\ kept is the bitmap.  This keeps the picture as gray instead, in sixteen
\ levels and at the picture's resolution or less, and makes it black and
\ white only when it is drawn: a pixel is white if its level is more than
\ the threshold that a mask, laid over the bitmap in tiles, has there.  The
\ mask's thresholds are in an order that keeps the pixels of each level
\ apart (blue noise), or in Bayer's grid.  Kept with a sample to 4 by 4 of
\ the picture's pixels, the Mandrill is as many cells as a bitmap of it at
\ half its size, and is drawn again through another mask, darker or
\ lighter, without the picture being read.  bench/masks.c measures how each
\ way of keeping a picture looks.
\
\ The gray is kept by the dither, to sixteen levels and not two
\ (dither-levels, level-row): a row of samples at a time, each the average
\ of the pixels it stands for, made the nearest level, and what it is out
\ by passed on as the dither passes it on.  A row of samples is kept as
\ four rows of planes, a plane for each bit of a level, the lowest first,
\ each a row of cells with a sample a bit, in the order the display has
\ its pixels.
\
\ A mask is planes too, four to a row of it, and is on the disk as planes
\ (util/bluenoise.c says how): to have one is to move its cells into
\ memory.  As each cell of it is moved, its thresholds are made darker or
\ lighter a level at a time by adding to the four planes together, as a
\ number is added a bit at a time, every pixel of the cell at once.
\
\ To draw, a row of samples is first made a row of pixels: each plane's
\ cells of samples are widened, each sample as many pixels across as it
\ stands for, by moving the samples apart in a few shifts and then filling
\ the gaps.  Then each cell of the bitmap is whether the levels of its
\ pixels are more than the mask's thresholds there, worked out from the
\ planes of both a bit at a time from the top, for all the pixels of the
\ cell at once.  The mask is kept the other way about, a pixel's bit set
\ where the threshold's is not, which is one instruction fewer a bit.
\
\ What is in hand while a mask is had or a picture drawn is in variables.

\ ---- a gray picture

\ Its record starts with where its planes are and how many cells a row of
\ one is, which is what the dither is given to keep them in.
create gry 0 ,          \ the gray picture in hand: its record

$6 constant GRAY-CELLS

: gpl   ( -- a ) gry @ ;                \ the planes: a row of samples is four rows of them
: grs   ( -- a ) gry @ 1+ ;             \ cells in a row of a plane
: gcols ( -- a ) gry @ CELL2 + ;        \ samples across,
: grows ( -- a ) gry @ c# 3 + ;         \ and down
: gsh   ( -- a ) gry @ c# 4 + ;         \ pixels a side to a sample, as a shift: 0 to 3
: gdth  ( -- a ) gry @ c# 5 + ;         \ the dither that keeps it, while it is kept

header gray-new
: gray-new ( w h shift -- gray ) \ the gray of a picture w by h, a sample to 1 << shift pixels a side; nothing kept yet
    GRAY-CELLS >c halloc gry !
    dup gsh !  tuck rshift grows !  rshift gcols !
    gcols @ PXMASK + PXSHIFT rshift grs !
    grows @ d# 2 lshift grs @ *  >c halloc gpl !
    gry @
;

\ What keeping it has, its dither, is had after the gray's own memory, and
\ is given back by whoever opened it when the last row is kept.
header gray-open
: gray-open ( img gray -- ) \ to keep the picture an image is, as that gray, a row of samples at a time
    gry !  gry @ swap gsh @ dither-levels gdth !
;

header gray-row : gray-row ( gray -- ) gry ! gdth @ level-row ;  \ its next row of samples, kept

header gray-keep
: gray-keep ( img shift -- gray ) \ a picture kept as gray, all at once
    over image!  tw @ th @ rot gray-new             ( img gray )
    himem @ >r  tuck gray-open
    grows @ negate
    begin dup while over gray-row 1+ repeat
    drop
    r> himem !                                      \ what keeping it had, given back: nothing has had any since
;

\ ---- masks

create gmask 0 ,        \ the mask in hand: its record
\ Levels added to every threshold of a mask as it is had: more is darker,
\ less is lighter.  A threshold stays between the first level and the last.
header gbright create gbright 0 ,

$3 constant MASK-CELLS-OF

: mn    ( -- a ) gmask @ ;              \ rows of it, and places across if it is as wide as a cell or wider
: mcs   ( -- a ) gmask @ 1+ ;           \ cells in a row of a plane: n's, or one with it as many times as fit
: mpl   ( -- a ) gmask @ CELL2 + ;      \ the planes: for each cell of each row, its four planes, the lowest first

meta
\ A cell of a mask on the disk has its leftmost pixel lowest.  Where the
\ display has it highest, each cell's bits are put the other way about:
\ each half of it swapped with the other, and each half of each half, and
\ so on.
variable rk
: rev-mask ( k -- m ) \ every other group of k bits of a cell, the lowest first
    rk !  0 cell-bits 0 ?do  i rk @ / 1 and 0= if 1 i lshift or then  loop
;
:: REVERSE ( x -- y )
    disp-left-low 0= if
        cell-bits 2/
        begin dup while
            s" dup" evaluate  dup literal s" rshift" evaluate  dup rev-mask literal s" and swap" evaluate
            dup rev-mask literal s" and" evaluate  dup literal s" lshift or" evaluate
            2/
        repeat
        drop
    then
;
target

create mcell 0 , 0 , 0 , 0 ,    \ a cell of each of the four planes of a mask, as it is had

: mcell-up ( -- ) \ each threshold of the cells in mcell a level more, but where it is the last
    true  mcell d# 4 bounds                         ( carry end a )
    begin
        2dupxor
    while
        rot over @  2dup and >r  xor over !  r> -rot
        1+
    repeat
    2drop                                           ( carry ) \ set where it was the last, and is now the first
    mcell d# 4 bounds
    begin 2dupxor while  rot dup >r over or!  r> -rot  1+  repeat
    2drop drop
;

: mcell-down ( -- ) \ a level less, but where it is the first
    true  mcell d# 4 bounds                         ( borrow end a )
    begin
        2dupxor
    while
        rot over @  2dup invert and >r  xor over !  r> -rot
        1+
    repeat
    2drop invert                                    ( kept ) \ clear where it was the first, and is now the last
    mcell d# 4 bounds
    begin 2dupxor while  rot dup >r over and!  r> -rot  1+  repeat
    2drop drop
;

: mask-cell ( src dst -- ) \ a cell of each of a row's four planes from src, MASK-CELLS cells apart, as gbright says, to dst and the three after
    swap
    dup @ REVERSE mcell !  MASK-CELLS +  dup @ REVERSE mcell 1+ !  MASK-CELLS +
    dup @ REVERSE mcell CELL2 + !  MASK-CELLS +  @ REVERSE mcell c# 3 + !
    gbright @  dup 0< if
        begin dup while mcell-down 1+ repeat
    else
        negate begin dup while mcell-up 1+ repeat
    then
    drop
    mcell @ invert over !  mcell 1+ @ invert over 1+ !
    mcell CELL2 + @ invert over CELL2 + !  mcell c# 3 + @ invert swap c# 3 + !
;

: mask-new ( n -- mask ) \ a mask of n rows, with nothing in it
    MASK-CELLS-OF >c halloc gmask !
    dup mn !  PXSHIFT rshift d# 1 max mcs !
    mn @ mcs @ * d# 2 lshift >c halloc mpl !
    gmask @
;

: mask-line ( src y -- ) \ row y of the mask in hand, from its row of planes at src
    mcs @ * d# 2 lshift mpl @ +                     ( src dst )
    swap mcs @ bounds                               ( dst end src )
    begin
        2dupxor
    while
        rot 2dup mask-cell  d# 4 +  -rot
        1+
    repeat
    2drop drop
;

header mask-from
: mask-from ( a n -- mask ) \ a mask of n rows, from memory laid out as the disk has it
    mask-new drop
    d# 0
    begin
        dup mn @ <
    while
        2dup MASK-ROW * +  over mask-line  1+
    repeat
    2drop gmask @
;

header mask-load
: mask-load ( blk n -- mask ) \ a mask of n rows, from the disk from a block
    mask-new drop
    d# 0
    begin
        dup mn @ <
    while
        2dup MASK-ROWS-SHIFT rshift + cblock                            ( blk y a )
        over d# 1 MASK-ROWS-SHIFT lshift 1- and MASK-ROW * +  over mask-line
        1+
    repeat
    2drop gmask @
;

header mask-bayer  : mask-bayer  ( -- mask ) BAYER-BLK d# 8 mask-load ;
header mask-blue64 : mask-blue64 ( -- mask ) MASK64-BLK d# 64 mask-load ;
header mask-blue32 : mask-blue32 ( -- mask ) MASK32-BLK d# 32 mask-load ;

\ ---- drawing

\ What is drawn may be smaller than the gray is kept, and only a part of
\ it: the bitmap has the part from a cell across and a row down of the
\ picture as drawn, as many cells across as a row of the bitmap is.
create gex 0 ,          \ the row of samples as pixels: for each of goc cells, its four planes, the lowest first
create gexy 0 ,         \ the row of samples gex has
create goc 0 ,          \ cells in a row of the bitmap
create gdsh 0 ,         \ pixels a side to a sample as drawn, as a shift: 0, 1 or 2
create gxc 0 ,          \ the cell of the picture as drawn that the bitmap's first is,
create gy0 0 ,          \ and the row
create goff 0 ,         \ how far into gex the bitmap's first cell would be, if it had the picture's
create gmr 0 ,          \ the mask's row for the row being drawn
create gqm 0 ,          \ what of how far a cell is into gex is how far its mask's is into the mask's row
create gbm 0 ,          \ the bitmap drawn into
create gp  0 ,          \ the cell being drawn: its levels,
create gq  0 ,          \ and its mask's thresholds

meta
\ A slice of a cell of samples, made a cell of pixels: the slice moved to
\ the left of the cell, and what else is there cleared (the first mask of
\ its table); its samples moved apart a step at a time, the widest first,
\ to f pixels each (the masks after it); and each then made f.
variable sk  variable sf
: log2 ( n -- k ) 0 begin 2dup 1 swap lshift > while 1+ repeat nip ;
: at-left ( i -- bit ) disp-left-low if else cell-bits 1- swap - then ;     \ pixel i of a cell, as a bit
: spread-mask ( k f -- m ) \ a group of 1 << k pixels, every (1 << k) * f, from the left
    sf !  1 swap lshift sk !
    0 cell-bits 0 ?do  i sk @ sf @ * mod sk @ < if 1 i at-left lshift or then  loop
;
: spread-table ( f -- ) \ its masks, laid down: the slice's, and then each step's
    >r
    0  cell-bits r@ / 0 ?do  1 i at-left lshift or  loop  tcalign tcell,
    cell-bits r@ / log2
    begin dup while  1-  dup r@ spread-mask tcalign tcell,  repeat
    drop r> drop
;
variable sp-f  variable sp-k  variable sp-i
:: APART ( x -- y ) \ x's pixels from its left, sp-f to each, as many as fill a cell
    cell-bits sp-f @ / log2 sp-k !  1 sp-i !
    begin sp-k @ while
        -1 sp-k +!
        s" dup" evaluate  1 sp-k @ lshift sp-f @ 1- * literal  s" PIXR or" evaluate
        sp-f @ 2 = if s" gsp2" else s" gsp4" then evaluate  sp-i @ literal  s" + @ and" evaluate
        1 sp-i +!
    repeat
    1 begin dup sp-f @ < while  s" dup" evaluate  dup literal s" PIXR or" evaluate  2*  repeat drop
;
:: APART2  2 sp-f !  s" APART" evaluate ;
:: APART4  4 sp-f !  s" APART" evaluate ;
:: SLICE2   cell-bits 2/ log2 literal s" lshift PIXL" evaluate ;     \ ( x j -- x' ) slice j of two, at the left
:: SLICE4   cell-bits 4 / log2 literal s" lshift PIXL" evaluate ;    \ ( x j -- x' ) slice j of four
target

create gsp2 meta 2 spread-table target
create gsp4 meta 4 spread-table target

: widen2 ( x j -- y ) SLICE2 gsp2 @ and APART2 ;   \ slice j of a cell of samples, each made two pixels
: widen4 ( x j -- y ) SLICE4 gsp4 @ and APART4 ;   \ and of four, each made four

: gpix ( x ca -- y ) \ cell ca of a row of the picture as drawn, out of the cell x of a plane that has its samples
    gdsh @ ?dup 0= if drop exit then
    d# 1 = if d# 1 and widen2 exit then
    d# 3 and widen4
;

: gexplane ( src out -- ) \ a plane's row of samples from src, as the pixels of each cell of a row drawn: every fourth cell from out
    gxc @ >r                                        ( src out ) ( R: the cell of the picture as drawn )
    goc @ d# 2 lshift over + swap                   ( src end out )
    begin
        2dupxor
    while
        rot  dup r@ gdsh @ rshift + @  r@ gpix      ( end out src y )
        rot tuck !                                  ( end src out )
        rot swap  d# 4 +                            ( src end out' )
        r> 1+ >r
    repeat
    2drop drop rdrop
;

: gexpand ( sy -- ) \ row sy of the samples, as a row of pixels: for each cell of a row drawn, its four planes
    d# 2 lshift grs @ * gpl @ +                     ( row ) \ of the lowest plane
    dup gex @ gexplane
    grs @ +  dup gex @ 1+ gexplane
    grs @ +  dup gex @ CELL2 + gexplane
    grs @ +  gex @ c# 3 + gexplane
;

meta
\ Whether each pixel of a cell is white: its level at gp more than its
\ threshold at gq, the top bit first.  The first bit makes which are more
\ and which are the same; each after it adds to the first those of the
\ second that it has more, and keeps in the second those it has the same.
\ A pixel's bit of a threshold's plane is set where the threshold's is not:
\ so it is more where the level's is set too, and the same where the two
\ planes' bits are the same.
:: WHITE ( -- )
    3
    dup s" gp @" evaluate literal s" + @" evaluate
    dup s" gq @" evaluate literal s" + @" evaluate
    s" 2dup and -rot xor" evaluate                  \ ( more same )
    begin dup while
        1-
        dup s" gp @" evaluate literal s" + @" evaluate
        dup s" gq @" evaluate literal s" + @" evaluate
        s" 2dup xor >r and  over and  rot or  swap r> and" evaluate
    repeat drop
    s" drop" evaluate
;
\ The cells of a row of the bitmap, from a, of the levels in gex through
\ the mask's row at gmr: each cell's levels and thresholds are four cells
\ on from the last, and its thresholds are where it is in gex, cut to the
\ mask's row.
:: CELLS ( -- )
    s" gex @ gp !  goc @ over + swap" evaluate      \ ( end a )
    s" begin 2dupxor while" evaluate
    s" gp @ gex @ - goff @ + gqm @ and gmr @ + gq !" evaluate
    s" WHITE" evaluate
    disp-ink-black if s" invert" evaluate then
    s" over !  1+" evaluate
    s" d# 4 gp +!  repeat 2drop" evaluate
;
target

: gcells ( a -- ) CELLS ;

\ A picture is drawn a row at a time (gray-draw-row), so that a task can
\ draw some rows at each of its turns; what is had while it is, and given
\ back after (gray-draw-done), is one at a time.
header gray-draw-start
: gray-draw-start ( bitmap mask gray dshift x0 y0 -- ) \ to draw the gray, a sample to 1, 2 or 4 pixels a side, into a bitmap with the part of it from x0 y0
    gy0 !  PXSHIFT rshift gxc !  gdsh !  gry !  gmask !  gbm !
    gbm @ 1+ @ goc !
    mcs @ d# 2 lshift 1- gqm !  gxc @ d# 2 lshift goff !
    goc @ d# 2 lshift >c halloc gex !
    d# -1 gexy !
;

header gray-draw-row
: gray-draw-row ( y -- ) \ row y of the bitmap, which is row y0 + y of the picture as drawn
    dup gy0 @ +                                     ( y row )
    dup gdsh @ rshift  dup gexy @ <> if dup gexpand dup gexy ! then drop
    mn @ 1- and mcs @ * d# 2 lshift mpl @ + gmr !   ( y )
    gbm @ 1+ @ * gbm @ @ +
    gcells
;

header gray-draw-done : gray-draw-done ( -- ) gex @ goc @ d# 2 lshift hgive ;

header gray-bitmap
: gray-bitmap ( bitmap gray dshift -- ) \ a new bitmap, for the descriptor at bitmap, of the whole gray as drawn a sample to 1 << dshift pixels a side
    >r gry !
    gcols @ r@ lshift PXMASK + PXSHIFT rshift  over 1+ !
    dup 1+ @ grows @ r> lshift * >c halloc  swap !
;

header gray-draw
: gray-draw ( bitmap mask gray dshift -- ) \ the whole gray drawn into a bitmap of it as drawn, through a mask, all at once
    over gry !  d# 0 d# 0 gray-draw-start
    d# 0
    begin
        dup grows @ gdsh @ lshift <
    while
        dup gray-draw-row 1+
    repeat
    drop
    gray-draw-done
;
