\ gfx.fs - rectangles on the display, clipped to the screen
\
\ pat-hline and pat-rect draw in the pattern and mode that are set.  The
\ words that take a flag draw plain white for true and plain black for
\ false, and leave that as the pattern.

: clip-span ( x n -- x' n' ) \ n' is not positive if nothing is left
    over 0< if + d# 0 swap then
    over + fb-w @ min over -
;

header pat-hline : pat-hline ( x y n -- )
    over fb-h @ u< 0= if drop 2drop exit then
    swap >r clip-span r> swap
    dup 0> if slab else drop 2drop then
;

header pat-rect : pat-rect ( x y w h -- )
    begin
        dup 0>
    while
        >r >r
        2dup r@ pat-hline
        1+ r> r> 1-
    repeat
    2drop 2drop
;

header hline : hline ( on? x y n -- ) >r >r >r solid r> r> r> pat-hline ;

header fill-rect : fill-rect ( on? x y w h -- )
    >r >r >r >r solid r> r> r> r> pat-rect
;

create gx 0 ,
create gy 0 ,
create gw 0 ,
create gh 0 ,

header frame-rect : frame-rect ( on? x y w h -- ) \ a one-pixel outline
    gh ! gw ! gy ! gx !
    solid
    gx @ gy @ gw @ pat-hline
    gx @ gy @ gh @ + 1- gw @ pat-hline
    gx @ gy @ d# 1 gh @ pat-rect
    gx @ gw @ + 1- gy @ d# 1 gh @ pat-rect
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
