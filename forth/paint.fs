\ paint.fs - HexaPaint: a window to draw in with the pointer
\
\ It is laid out as paint programs of its kind are.  The tools are down
\ the left, two to a row: pencil and brush, eraser and line, rectangle and
\ filled rectangle, oval and filled oval.  The patterns are along the
\ bottom, with the one in hand shown bigger at their left.  The rest is
\ the canvas.
\
\ The canvas is a bitmap of its own, as the display is, and what is drawn
\ is drawn in that: the window shows it by copying it to the screen
\ (copy-bits), so that the drawing is there whatever was over the window.
\ Everything that draws comes down to one thing, a run of pixels of a row
\ in the pattern (span), a cell at a time: a dot of the pen is a run for
\ each of its rows, a line is a dot at each place along it, and a
\ rectangle or an oval is runs.
\
\ The pencil, the brush and the eraser draw as the pointer goes: a line
\ of dots from where it was to where it is, each time it has moved.  A
\ line, a rectangle or an oval is pulled out from where the button went
\ down, and is drawn in the canvas when the button goes up; until then it
\ is shown, in outline, as part of what the window draws, by the same
\ words with the run going to the screen and not the canvas ('run).
\
\ The canvas is asked for when it is first drawn in, and is taken from
\ memory that has been given back if there is enough of that (hreuse):
\ on the machine with cells of 16 bits it goes where the picture was read
\ and dithered.  It is 384 by 256, or 240 by 152 on a screen smaller than
\ 768 by 576.

include icons.fs        \ the tools' pictures

bulk
create p-brick  $ff prow, $80 prow, $80 prow, $80 prow, $ff prow, $08 prow, $08 prow, $08 prow,
create p-diag   $88 prow, $44 prow, $22 prow, $11 prow, $88 prow, $44 prow, $22 prow, $11 prow,
create p-dots   $00 prow, $22 prow, $00 prow, $88 prow, $00 prow, $22 prow, $00 prow, $88 prow,
unbulk

create pcv 0 , 0 ,      \ the canvas: where it is, and the cells in a row of it
create cv-w 0 ,         \ its size in pixels
create cv-h 0 ,
create picons 0 , 0 ,   \ the tools' pictures, as a bitmap
create ppats 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,    \ the patterns along the bottom
create pwin 0 ,         \ the window
create ptool 0 ,        \ the tool in hand: 0 to 7, in the order above
create ppat 0 ,         \ the pattern in hand
create pfill 0 ,        \ the pattern a run is in
create pen 1 ,          \ how many pixels across and down a dot is
create 'run 0 ,         \ ( x0 x1 y -- )  where a run goes: the canvas, or the screen
create px0 0 ,          \ where the button went down, or where a stroke has got to,
create py0 0 ,
create px1 0 ,          \ and where the pointer is now, of a shape being pulled out
create py1 0 ,
create pshape 0 ,       \ true while one is

: paint-init ( -- ) \ once
    cv-w @ if exit then
    fb-h @ d# 576 < if d# 240 d# 152 else d# 384 d# 256 then cv-h ! cv-w !
    icon-bits picons !  d# 128 PXSHIFT rshift picons CELL + !
    p-white ppats !  p-black ppats 1+ !  p-dkgray ppats CELL2 + !  p-gray ppats c# 3 + !
    p-ltgray ppats c# 4 + !  p-brick ppats c# 5 + !  p-diag ppats c# 6 + !  p-dots ppats c# 7 + !
    p-black ppat !
;

: canvas ( -- ) \ see that there is one: white
    pcv @ if exit then
    cv-w @ PXMASK + PXSHIFT rshift dup pcv CELL + !
    cv-h @ *  dup >c hreuse  dup pcv !              ( cells a )
    swap  d# 0 RAW>SRC -rot  cells-fill
;

\ ---- a run of pixels, in the canvas

create srow 0 ,         \ the row of the canvas a run is in,
create spat 0 ,         \ and the row of the pattern that goes in it

: cmask ( x0 x1 -- m n ) \ the pixels of the cell x0 is in, from x0 to x1 or the cell's end; and how many
    over PXMASK and >r                              ( x0 x1 ) ( R: where in the cell x0 is )
    swap - CELLBITS r@ - min                        ( n )
    d# -1 r@ PIXR                                   ( n m ) \ from there on,
    over r> + CELLBITS swap - d# -1 swap PIXL and   \ and no further than n
    swap
;

: span-cell ( m a p -- ) \ the pixels m of the cell at a become those of p
    over @ xor rot and  over @ xor  swap !
;

: span ( x0 x1 y -- ) \ the pixels of row y from x0 up to x1, in the pattern: what of them the canvas has
    dup cv-h @ u< 0= if drop 2drop exit then
    dup d# 7 and pfill @ + @ RAW>SRC spat !
    pcv CELL + @ * pcv @ + srow !
    cv-w @ min  swap d# 0 max swap
    begin
        2dup <
    while
        2dup cmask >r                               ( x0 x1 m ) ( R: n )
        rot dup PXSHIFT rshift srow @ +             ( x1 m x0 a )
        rot swap spat @ span-cell                   ( x1 x0 )
        r> + swap
    repeat
    2drop
;

: prun ( x0 x1 y -- ) \ the same run, on the screen, where the window shows the canvas
    >r over - swap d# 45 + r> rot pat-hline
;

: run ( x0 x1 y -- ) 'run @ execute ;

\ ---- a dot of the pen, and a line of them

create dotx 0 ,

: dot ( x y -- ) \ the pen, with its top left there
    swap dotx !  pen @ bounds
    begin
        2dupxor
    while
        dup  dotx @ dup pen @ +  rot run
        1+
    repeat
    2drop
;

\ A line is a dot at every place from one end to the other.  The places
\ are stepped across or down, whichever the line is longer in, with the
\ other stepped when what is owed to it comes to a pixel.
create lx 0 ,   create ly 0 ,   create lx1 0 ,  create ly1 0 ,
create ldx 0 ,  create ldy 0 ,  create lsx 0 ,  create lsy 0 ,  create lerr 0 ,

: pline ( x0 y0 x1 y1 -- )
    ly1 ! lx1 ! ly ! lx !
    lx1 @ lx @ -  dup abs ldx !  0< if d# -1 else d# 1 then lsx !
    ly1 @ ly @ -  dup abs negate ldy !  0< if d# -1 else d# 1 then lsy !
    ldx @ ldy @ + lerr !
    begin
        lx @ ly @ dot
        lx @ lx1 @ =  ly @ ly1 @ =  and 0=
    while
        lerr @ 2*
        dup ldy @ < 0= if ldy @ lerr +!  lsx @ lx +! then
        ldx @ > 0= if ldx @ lerr +!  lsy @ ly +! then
    repeat
;

\ ---- a rectangle and an oval, between where the button went down and where the pointer is

create bx0 0 ,  create by0 0 ,  create bx1 0 ,  create by1 0 ,  \ left, top, right and bottom

: box! ( -- )
    px0 @ px1 @ 2dup min bx0 ! max bx1 !
    py0 @ py1 @ 2dup min by0 ! max by1 !
;

: rect-frame ( -- )
    bx0 @ bx1 @ 1+ by0 @ run
    bx0 @ bx1 @ 1+ by1 @ run
    by1 @ by0 @ 1+
    begin
        2dup >
    while
        dup  bx0 @ dup 1+  rot run
        dup  bx1 @ dup 1+  rot run
        1+
    repeat
    2drop
;

: rect-fill ( -- )
    by1 @ 1+ by0 @
    begin
        2dupxor
    while
        dup  bx0 @ bx1 @ 1+  rot run
        1+
    repeat
    2drop
;

\ An oval is as wide at a row as its half width times the root of what
\ the row is short of the half height, squared, over the half height.  The
\ root is not worked out: it is no more than it was for the row before,
\ and comes down a step at a time as the rows go out from the middle.
create oa 0 ,   create ob 0 ,   create os 0 ,   \ half its width and height, and the root
create ocx 0 ,  create ocy 0 ,                  \ its middle
create oxo 0 ,  create oin 0 ,                  \ of its edge at a row: how far out it starts and ends

: oval! ( -- ) \ the oval the box holds
    box!
    bx1 @ bx0 @ - d# 1 rshift oa !
    by1 @ by0 @ - d# 1 rshift dup ob ! os !
    bx0 @ oa @ + ocx !  by0 @ ob @ + ocy !
;

: xe ( dy -- x ) \ how far from its middle it reaches, dy rows from the middle; asked in order
    ob @ 0= if drop oa @ exit then
    ob @ dup *  swap dup * -
    begin os @ dup * over > while d# -1 os +! repeat
    drop  oa @ os @ * ob @ u/mod nip
;

: orun ( x dy -- ) \ the two rows dy from the middle, as far as x each way
    >r  ocx @ over -  swap ocx @ + 1+
    2dup ocy @ r@ - run
    ocy @ r> + run
;

: oval-fill ( -- )
    oval!  d# 0
    begin
        dup ob @ > 0=
    while
        dup xe over orun
        1+
    repeat
    drop
;

: oedge-row ( y -- )
    >r
    ocx @ oxo @ -  ocx @ oin @ - 1+  r@ run
    ocx @ oin @ +  ocx @ oxo @ + 1+  r> run
;

: oedge ( dy -- ) dup ocy @ swap - oedge-row  ocy @ + oedge-row ;

: oval-frame ( -- ) \ its edge: at each row from where it reaches in to where the next row out reaches
    oval!  d# 0 xe oxo !
    d# 0
    begin
        dup ob @ > 0=
    while
        dup ob @ < if dup 1+ xe else d# -1 then     ( dy next )
        dup 1+ oxo @ min oin !
        over oedge
        dup 0< if drop else oxo ! then
        1+
    repeat
    drop
;

\ ---- the tools

: shape? ( -- f ) ptool @ d# 2 > ;                  \ one that is pulled out

: stroke-pen ( -- ) \ the pen and the pattern of a tool that draws as it goes
    ['] span 'run !
    ptool @ 0= if d# 1 pen !  p-black pfill !  exit then
    ptool @ d# 1 = if d# 4 pen !  ppat @ pfill !  exit then
    d# 12 pen !  p-white pfill !
;

: shape ( -- ) \ the outline of the one pulled out, a pixel wide
    d# 1 pen !
    ptool @ d# 3 = if px0 @ py0 @ px1 @ py1 @ pline exit then
    box!  ptool @ d# 6 < if rect-frame else oval-frame then
;

: commit ( -- ) \ it is drawn in the canvas: filled, if it is a filled one, and then its outline
    ['] span 'run !
    ppat @ pfill !
    ptool @ d# 5 = if box! rect-fill then
    ptool @ d# 7 = if oval-fill then
    p-black pfill !  shape
;

: cv-inval ( x0 y0 x1 y1 -- ) \ the canvas between two places, and a pen more, is out of date
    rot 2dup min >r max >r                          ( x0 x1 ) ( R: top bottom )
    2dup max >r min r>                              ( left right )
    over - pen @ + 1+                               ( left w )
    swap d# 45 + swap
    r> r> tuck - pen @ + 1+                         ( x w top h )
    rot swap
    pwin @ win-inval
;

: paint-track ( x y win -- ) \ the pointer has moved with the button down, or the button has gone up
    drop  swap d# 45 - swap
    shape? if
        d# 1 pen !
        px0 @ py0 @ px1 @ py1 @ cv-inval            \ where it was shown,
        py1 ! px1 !
        held @ 0= if d# 0 pshape !  commit  d# 1 pen ! then
        px0 @ py0 @ px1 @ py1 @ cv-inval            \ and where it is
        exit
    then
    held @ 0= if 2drop exit then
    stroke-pen
    2dup px0 @ py0 @ 2swap pline
    2dup px0 @ py0 @ cv-inval
    py0 ! px0 !
;

header paint-click
: paint-click ( x y win -- )
    pwin !  paint-init
    over d# 44 < if                                 \ a tool
        d# 22 u/mod nip 2*  swap d# 22 u/mod nip +
        dup d# 8 u< if ptool ! else drop then
        d# 0 d# 0 d# 44 d# 88 pwin @ win-inval
        exit
    then
    dup cv-h @ > if                                 \ a pattern
        drop  d# 83 - d# 18 u/mod nip
        dup d# 8 u< if ppats + @ ppat ! else drop then
        d# 45 cv-h @ cv-w @ d# 21 pwin @ win-inval
        exit
    then
    swap d# 45 - swap                               ( x y ) \ on the canvas
    canvas
    2dup py0 ! px0 !  2dup py1 ! px1 !
    ['] paint-track 'track !
    shape? if
        true pshape !  d# 1 pen !
        2dup cv-inval
        exit
    then
    stroke-pen
    2dup dot  2dup cv-inval
;

header paint-draw
: paint-draw ( win -- )
    pwin !  paint-init
    copy-mode
    d# 0                                            \ the tools' pictures,
    begin
        dup d# 8 <
    while
        >r
        picons  r@ d# 4 lshift  d# 0
        r@ d# 1 and d# 22 * d# 3 +  r@ d# 1 rshift d# 22 * d# 3 +
        d# 16 d# 16 copy-bits
        r> 1+
    repeat
    drop
    xor-mode  p-black pattern !                     \ with the one in hand white on black
    ptool @ d# 1 and d# 22 * 1+  ptool @ d# 1 rshift d# 22 * 1+  d# 20 d# 20 pat-rect
    false d# 44 d# 0 d# 1 cv-h @ d# 21 + fill-rect
    false d# 45 cv-h @ cv-w @ hline
    ppat @ pattern !  copy-mode                     \ the pattern in hand, and the patterns
    d# 47 cv-h @ d# 3 + d# 30 d# 16 pat-rect
    false d# 46 cv-h @ d# 2 + d# 32 d# 18 frame-rect
    d# 0
    begin
        dup d# 8 <
    while
        dup ppats + @ pattern !  copy-mode
        dup d# 18 * d# 83 +  cv-h @ d# 3 +  d# 16 d# 16 pat-rect
        false over d# 18 * d# 82 +  cv-h @ d# 2 +  d# 18 d# 18 frame-rect
        1+
    repeat
    drop
    pcv @ if                                        \ the canvas, and over it what is being pulled out
        copy-mode
        pcv  d# 0 d# 0  d# 45 d# 0  cv-w @ cv-h @  copy-bits
        pshape @ if
            d# 45 d# 0 cv-w @ cv-h @ clip-to
            ['] prun 'run !  p-black pattern !  copy-mode
            shape
        then
    then
;
