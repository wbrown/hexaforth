\ gfx.fs - rectangles and text on the 1-bit display, clipped to the screen
\
\ Ink is a flag: true is white, false is black.

: clip-span ( x n -- x' n' ) \ n' is not positive if nothing is left
    over 0< if + d# 0 swap then
    over + fb-w @ min over -
;

header hline : hline ( on? x y n -- )
    over fb-h @ u< 0= if 2drop 2drop exit then
    swap >r clip-span r> swap
    dup 0> if hspan else 2drop 2drop then
;

header fill-rect : fill-rect ( on? x y w h -- )
    begin
        dup 0>
    while
        >r >r
        3dup r@ hline
        1+ r> r> 1-
    repeat
    2drop 2drop drop
;

create gx 0 ,
create gy 0 ,
create gw 0 ,
create gh 0 ,

header frame-rect : frame-rect ( on? x y w h -- ) \ a one-pixel outline
    gh ! gw ! gy ! gx !
    dup gx @ gy @ gw @ hline
    dup gx @ gy @ gh @ + 1- gw @ hline
    dup gx @ gy @ d# 1 gh @ fill-rect
    gx @ gw @ + 1- gy @ d# 1 gh @ fill-rect
;

header cblit8 : cblit8 ( bits x y -- ) \ blit8, dropped if any of it is off the screen
    over fb-w @ d# 8 - u> over fb-h @ u< 0= or if drop 2drop exit then
    blit8
;

\ Text built with the ordinary output words: sb-begin sends emit to a
\ buffer, sb-end puts it back, sb-draw puts the buffer on the screen.

create sbuf #64 allot
header sb : sb ( -- a ) sbuf >c ;       \ the address of its first byte
header sbn create sbn 0 ,
create sb-old 0 ,

: sb-emit ( c -- )
    sbn @ d# 63 < if sb sbn @ + c! d# 1 sbn +! else drop then
;

header sb-begin : sb-begin ( -- ) d# 0 sbn !  'emit @ sb-old !  ['] sb-emit 'emit ! ;
header sb-end : sb-end   ( -- ) sb-old @ 'emit ! ;
header sb-draw : sb-draw  ( x y -- ) >r >r sb sbn @ r> r> draw-text ;
