\ view.fs - the image viewer
\
\ The image is dithered once, into a 1-bit bitmap, by pointing the display's
\ plot vector at the bitmap while atkinson runs.  Painting the window is
\ then a matter of copying it to the screen, a cell of 64 pixels at a time.

create vi 0 ,           \ a loop counter

create bm  0 ,          \ the bitmap, or 0
create bmw 0 ,          \ bytes per row, a whole number of cells
create bmh 0 ,          \ rows

: bm-plot ( on? x y -- )
    bmw @ * over d# 3 rshift + bm @ >c +            ( on? x a )
    swap d# 7 and h# 80 swap rshift                 ( on? a mask )
    rot if over c@ or else invert over c@ and then
    swap c!
;

header view-load : view-load ( blk# -- ) \ dither the TIFF starting at a block
    tiff-open tiff-decode
    tw @ d# 63 + d# 6 rshift d# 3 lshift bmw !
    th @ bmh !
    bmw @ bmh @ * halloc bm !
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
            over @                                  ( a i cell )
            d# 8                                    \ its eight bytes, lowest first
            begin
                dup
            while
                >r >r
                r@ h# ff and  over d# 3 lshift ox @ +  oy @ vi @ +  cblit8
                1+ r> d# 8 rshift r> 1-
            repeat
            2drop
            swap cell+ swap
        repeat
        drop
        d# 1 vi +!
    repeat
    drop
;

header ignore-key : ignore-key ( c win -- ) 2drop ;
