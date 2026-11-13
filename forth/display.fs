\ display.fs - the display: a bitmap in memory, one bit to a pixel
\
\ The machine definition says where the bitmap is, how big, which end of a
\ cell is the left, whether a set bit is black, and where each row starts
\ (see MODEL.md).  cross.fs turns those into the words in capitals used
\ here, and nothing outside this file knows them: all drawing is built on
\ row, the address of a row; on a mask for the pixels wanted; and on ink
\ and paper, which make the pixels of a mask black and white.  No drawing
\ steps from one row to the next by adding to an address.
\
\ Eight pixels at a time are a byte with the pixels in the machine's order,
\ as the font has them.  PIXR moves pixels to the right by a count and PIXL
\ to the left; EDGE takes such a byte to the left end of a cell and LEFT8
\ takes the leftmost eight pixels of a cell as such a byte.
\
\ In the flags these words take, true is white.

header fb-w create fb-w  meta disp-w target ,   \ width and height in pixels
header fb-h create fb-h  meta disp-h target ,

header row : row ( y -- a ) \ the first cell of row y
BANKED? [if]
    DISP-BANKS u/mod DISP-STRIDE * swap DISP-BANK-STEP * + DISP-BASE +
[else]
    DISP-STRIDE * DISP-BASE +
[then]
;

INK-BLACK? [if]
header ink   : ink   ( mask a -- ) dup >r @ or r> ! ;
header paper : paper ( mask a -- ) dup >r @ swap invert and r> ! ;
[else]
header paper : paper ( mask a -- ) dup >r @ or r> ! ;
header ink   : ink   ( mask a -- ) dup >r @ swap invert and r> ! ;
[then]

: x!   ( bits a -- ) dup >r @ xor r> ! ;                    \ invert
: seg! ( bits mask a -- ) dup >r @ swap invert and or r> ! ; \ replace

\ Where a pixel is: its cell, and how far into the cell it is.
: prow ( x y -- n a ) row over d# 6 rshift + swap d# 63 and swap ;

: dplot ( on? x y -- )
    prow >r LEFT1 swap PIXR
    swap if r> paper else r> ink then
;

header 'plot create 'plot 0 ,       \ ( on? x y -- )  where plot goes
header plot
: plot ( on? x y -- ) 'plot @ execute ;

\ A run of n pixels to the right, a cell at a time.  No shift here is by a
\ whole cell, which the machine does not promise is zero.
create hon 0 ,
create hn  0 ,
create ba  0 ,          \ the cell being drawn in
create bn  0 ,          \ and how far into it

: leftn ( n -- mask ) \ the n leftmost pixels of a cell
LEFT-LOW? [if]
    d# 64 swap - d# -1 swap rshift
[else]
    d# 64 swap - d# -1 swap lshift
[then]
;

header hspan
: hspan ( on? x y n -- )
    hn ! prow ba ! bn ! hon !
    begin
        hn @ 0>
    while
        d# 64 bn @ - hn @ min                       ( pixels in this cell )
        dup leftn bn @ PIXR
        hon @ if ba @ paper else ba @ ink then
        negate hn +!
        d# 0 bn !  d# 1 ba +!
    repeat
;

\ Eight pixels at x: the part of them in the cell x is in, and the part
\ that runs over into the next, which is nothing unless x is in the last
\ seven pixels of the cell.
: over8 ( bits -- here next )
    dup bn @ PIXR swap
    bn @ d# 56 > if d# 64 bn @ - PIXL else drop d# 0 then
;

header blit8
: blit8 ( bits x y -- ) \ draw eight pixels; a set bit is white
    prow ba ! bn !
INK-BLACK? [if]
    h# ff xor
[then]
    EDGE over8  M8 over8                            ( here next mhere mnext )
    ?dup if rot swap ba @ 1+ seg! else nip then
    ba @ seg!
;

header xor8
: xor8 ( bits x y -- ) \ invert the pixels where bits are set
    prow ba ! bn !
    EDGE over8
    ?dup if ba @ 1+ x! then
    ba @ x!
;

header display-init
: display-init ( -- ) ['] dplot 'plot ! ;
