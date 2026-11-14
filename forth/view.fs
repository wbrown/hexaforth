\ view.fs - the image viewer
\
\ The image is dithered once, into a bitmap as the display is.  Painting
\ the window is then a matter of copying it to the screen.

create bm 0 , 0 ,        \ the bitmap, or 0, and the cells in a row of it
create bmh 0 ,          \ rows

header view-load : view-load ( blk# -- ) \ dither the TIFF starting at a block
    tiff-open tiff-decode
    bm atkinson
    th @ bmh !
;

header view-draw : view-draw ( win -- )
    drop
    bm @ 0= if exit then
    copy-mode
    bm d# 0 d# 0  d# 0 d# 0  tw @ bmh @  copy-bits
;

header ignore-key : ignore-key ( c win -- ) 2drop ;
