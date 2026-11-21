\ gfx.fs - rectangles in plain white and black

\ White for true, black for false: it is left as the pattern.

header hline : hline ( on? x y n -- )
    >r >r >r solid r> r> r> pat-hline ;

header fill-rect : fill-rect ( on? x y w h -- )
    >r >r >r >r solid r> r> r> r> pat-rect
;

: frame-h ( x y w h -- ) \ the top and the bottom
    >r >r 2dup r@ pat-hline  ( x y ) ( R: h w )
    r> swap r> + 1- swap pat-hline
;

: frame-v ( x y w h -- ) \ the left and the right
    swap >r >r 2dup d# 1 r@ pat-rect  ( x y ) ( R: w h )
    r> r> >r rot r> + 1- -rot         ( x' y h )
    d# 1 swap pat-rect
;

header frame-rect : frame-rect ( on? x y w h -- )
    >r >r >r >r solid r> r> r> r>
    2over 2over frame-h frame-v
;

\ sb-begin sends emit to a buffer, sb-end puts it back, and
\ sb-draw draws the buffer

create sbuf #64 allot
header sb : sb ( -- a ) sbuf >c ;
header sbn create sbn 0 ,
create sb-old 0 ,

: sb-emit ( c -- )
    sbn @ d# 63 < if sb sbn @ + c! d# 1 sbn +! else drop then
;

header sb-begin : sb-begin ( -- )
    d# 0 sbn !  'emit @ sb-old !  ['] sb-emit 'emit ! ;
header sb-end : sb-end   ( -- ) sb-old @ 'emit ! ;
header sb-draw : sb-draw  ( x y -- )
    >r >r sbuf d# 0 sbn @ r> r> draw-chars ;

\ characters of 8 by 16 pixels, from 2 in from the corner
header char-xy : char-xy ( col row -- x y )
    d# 4 lshift d# 2 +  swap d# 3 lshift d# 2 +  swap ;
