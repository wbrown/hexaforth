\ gfx.fs - rectangles in plain white and black
\
\ pat-hline and pat-rect, in display.fs, draw in the pattern and mode that
\ are set.  The words here take a flag: they draw plain white for true and
\ plain black for false, and leave that as the pattern.

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
