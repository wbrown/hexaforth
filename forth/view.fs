\ view.fs - the image viewer
\
\ The image is dithered once, into a 1-bit bitmap, by pointing the display's
\ plot vector at the bitmap while atkinson runs.  Painting the window is
\ then a matter of copying it to the screen, eight pixels at a time.

create vi 0 ,           \ a loop counter

create bm  0 ,          \ the bitmap, or 0
create bmw 0 ,          \ cells per row
create bmh 0 ,          \ rows

\ The bitmap has its pixels in the display's order, and a set bit is white.
: bm-plot ( on? x y -- )
    bmw @ * over PXSHIFT rshift + bm @ +            ( on? x a )
    swap PXMASK and LEFT1 swap PIXR                 ( on? a mask )
    rot if over @ or else invert over @ and then
    swap !
;

header view-load : view-load ( blk# -- ) \ dither the TIFF starting at a block
    tiff-open tiff-decode
    tw @ PXMASK + PXSHIFT rshift bmw !
    th @ bmh !
    bmw @ bmh @ * >c halloc bm !
    'plot @ >r
    ['] bm-plot 'plot !
    d# 0 d# 0 atkinson
    r> 'plot !
;

header view-draw : view-draw ( win -- )
    drop
    bm @ 0= if exit then
    bm @
    d# 0 vi !
    begin
        vi @ bmh @ <
    while
        d# 0
        begin
            dup bmw @ <
        while
            over @  over PXSHIFT lshift ox @ +      ( a i cell x )
            CELLB                                   \ eight pixels at a time, from the left
            begin
                dup
            while
                >r
                2dup swap LEFT8 swap  oy @ vi @ +  cblit8
                d# 8 + swap d# 8 PIXL swap
                r> 1-
            repeat
            drop 2drop
            swap 1+ swap 1+
        repeat
        drop
        d# 1 vi +!
    repeat
    drop
;

header ignore-key : ignore-key ( c win -- ) 2drop ;
