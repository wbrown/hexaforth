\ gfx.fs - rectangles in plain white and black
\
\ pat-hline and pat-rect, in display.fs, draw in the pattern and mode that
\ are set.  The words here take a flag: they draw plain white for true and
\ plain black for false, and leave that as the pattern.

header hline : hline ( on? x y n -- ) >r >r >r solid r> r> r> pat-hline ;

header fill-rect : fill-rect ( on? x y w h -- )
    >r >r >r >r solid r> r> r> r> pat-rect
;

\ An outline is two pairs of edges, and each pair is a word that has the
\ corner on the data stack and the size on the return stack.
: frame-h ( x y w h -- ) \ the top and the bottom
    >r >r 2dup r@ pat-hline                         ( x y ) ( R: h w )
    r> swap r> + 1- swap pat-hline
;

: frame-v ( x y w h -- ) \ the left and the right
    swap >r >r 2dup d# 1 r@ pat-rect                ( x y ) ( R: w h )
    r> r> >r rot r> + 1- -rot                       ( x' y h )
    d# 1 swap pat-rect
;

header frame-rect : frame-rect ( on? x y w h -- ) \ a one-pixel outline
    >r >r >r >r solid r> r> r> r>
    2over 2over frame-h frame-v
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
header sb-draw : sb-draw  ( x y -- ) >r >r sbuf d# 0 sbn @ r> r> draw-chars ;

\ Where the character at a column and a row of a text area is: in
\ cells of 8 by 16 pixels, from 2 in from the corner.  The Forth
\ window's text and the editor's block are such areas.
header char-xy : char-xy ( col row -- x y )
    d# 4 lshift d# 2 +  swap d# 3 lshift d# 2 +  swap ;
