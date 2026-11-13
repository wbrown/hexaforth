\ view.fs - the image viewer
\
\ The image is dithered once, into a 1-bit bitmap, by pointing the display's
\ plot vector at the bitmap while atkinson runs.  Painting the window is
\ then a matter of copying it to the screen.

create bm 0 , 0 ,        \ the bitmap, or 0, and the cells in a row of it
create bmh 0 ,          \ rows

\ The bitmap is in the form the display is.
: bm-plot ( on? x y -- )
    bm CELL + @ * over PXSHIFT rshift + bm @ +      ( on? x a )
    swap PXMASK and LEFT1 swap PIXR swap            ( on? mask a )
    rot if paper else ink then
;

header view-load : view-load ( blk# -- ) \ dither the TIFF starting at a block
    tiff-open tiff-decode
    tw @ PXMASK + PXSHIFT rshift bm CELL + !
    th @ bmh !
    bm CELL + @ bmh @ * >c halloc bm !
    'plot @ >r
    ['] bm-plot 'plot !
    d# 0 d# 0 atkinson
    r> 'plot !
;

header view-draw : view-draw ( win -- )
    drop
    bm @ 0= if exit then
    copy-mode
    bm d# 0 d# 0  ox @ oy @  tw @ bmh @  copy-bits
;

header ignore-key : ignore-key ( c win -- ) 2drop ;
