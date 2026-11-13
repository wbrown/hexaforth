\ gfx.fs - rectangles and text on the 1-bit display, clipped to the screen
\
\ Ink is a flag: true is white, false is black.

: clip-span ( x n -- x' n' ) \ n' is not positive if nothing is left
    over 0< if + d# 0 swap then
    over + fb-w @ min over -
;

: hline ( on? x y n -- )
    over fb-h @ u< 0= if 2drop 2drop exit then
    swap >r clip-span r> swap
    dup 0> if hspan else 2drop 2drop then
;

: fill-rect ( on? x y w h -- )
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

: frame-rect ( on? x y w h -- ) \ a one-pixel outline
    gh ! gw ! gy ! gx !
    dup gx @ gy @ gw @ hline
    dup gx @ gy @ gh @ + 1- gw @ hline
    dup gx @ gy @ d# 1 gh @ fill-rect
    gx @ gw @ + 1- gy @ d# 1 gh @ fill-rect
;

: cblit8 ( bits x y -- ) \ blit8, dropped if any of it is off the screen
    over fb-w @ d# 8 - u> over fb-h @ u< 0= or if drop 2drop exit then
    blit8
;

\ Text is black on white.  With `inverse` set it is white on black, with
\ `bold` set each stroke is a pixel wider, and with `italic` set the top
\ half of each character leans a pixel to the right.

create inverse 0 ,
create bold 0 ,
create italic 0 ,
create slant 0 ,        \ true while the half being drawn leans

\ A glyph is 16 rows of 8 pixels: two cells, eight rows in each, the top
\ row in the low byte.  The rows are shifted out of the cell; the font is
\ never read a byte at a time.

: rows8 ( x y cell -- x y' ) \ draw the eight rows held in a cell
    d# 8
    begin
        dup
    while
        >r >r
        2dup r@ h# ff and
        slant @ if d# 1 rshift then
        bold @ if dup d# 1 rshift or then
        inverse @ 0= if h# ff xor then -rot blit8
        1+ r> d# 8 rshift r> 1-
    repeat
    2drop
;

: draw-char ( c x y -- )
    over fb-w @ d# 8 - u> over fb-h @ d# 16 - u> or if drop 2drop exit then
    rot d# 32 - dup d# 94 u> if drop d# 0 then
    d# 4 lshift font +                              ( x y glyph )
    dup cell+ @ >r @
    italic @ slant !
    rows8
    d# 0 slant !
    r> rows8
    2drop
;

create tx 0 ,
create ty 0 ,

: draw-text ( addr u x y -- )
    ty ! tx !
    bounds
    begin
        2dupxor
    while
        dup c@ tx @ ty @ draw-char
        d# 8 tx +!
        1+
    repeat
    2drop
;

\ Text built with the ordinary output words: sb-begin sends emit to a
\ buffer, sb-end puts it back, sb-draw puts the buffer on the screen.

create sb #64 allot
create sbn 0 ,
create sb-old 0 ,

: sb-emit ( c -- )
    sbn @ d# 63 < if sb sbn @ + c! d# 1 sbn +! else drop then
;

: sb-begin ( -- ) d# 0 sbn !  'emit @ sb-old !  ['] sb-emit 'emit ! ;
: sb-end   ( -- ) sb-old @ 'emit ! ;
: sb-draw  ( x y -- ) >r >r sb sbn @ r> r> draw-text ;
