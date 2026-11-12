\ view.fs - the image viewer
\
\ The image is dithered once, into a 1-bit bitmap, by pointing the display's
\ plot vector at the bitmap while atkinson runs.  Painting the window is
\ then a matter of copying bytes to the screen.

create bm  0 ,          \ the bitmap, or 0
create bmw 0 ,          \ bytes per row
create bmh 0 ,          \ rows

: bm-plot ( on? x y -- )
    bmw @ * over d# 3 rshift + bm @ +               ( on? x a )
    swap d# 7 and h# 80 swap rshift                 ( on? a mask )
    rot if over c@ or else invert over c@ and then
    swap c!
;

: view-load ( blk# -- ) \ dither the TIFF starting at a block
    tiff-open tiff-decode
    tw @ d# 7 + d# 3 rshift bmw !
    th @ bmh !
    bmw @ bmh @ * halloc bm !
    'plot @ >r
    ['] bm-plot 'plot !
    d# 0 d# 0 atkinson
    r> 'plot !
;

: view-draw ( win -- )
    content ty0 ! tx0 !
    bm @ 0= if exit then
    bm @
    d# 0 ti !
    begin
        ti @ bmh @ <
    while
        d# 0
        begin
            dup bmw @ <
        while
            over c@  over d# 3 lshift tx0 @ +  ty0 @ ti @ +  cblit8
            swap 1+ swap 1+
        repeat
        drop
        d# 1 ti +!
    repeat
    drop
;

: ignore-key ( c win -- ) 2drop ;
