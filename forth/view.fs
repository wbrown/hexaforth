\ view.fs - the image viewer
\
\ The image is dithered once, into a bitmap as the display is.  Painting
\ the window is then a matter of copying it to the screen.
\
\ Reading the image and dithering it take a while: 37 million
\ instructions for the mandrill.  So a task does them, a row of the image
\ at each of its turns, and the desktop goes on meanwhile.  The window
\ shows the rows that are made, so the picture comes down the window.  The
\ rows are put out of date eight at a time and not one: whatever is out of
\ date, every window that lies over it is painted again, and that for each
\ row made the picture half as long again in coming.  The task has cells
\ 96 to 127 of each stack, above anything the desktop's own task reaches,
\ and when the last row is made it takes itself out of the ring.

create bm 0 , 0 ,       \ the bitmap, or 0, and the cells in a row of it
create bmh 0 ,          \ rows of it that are made,
create vseen 0 ,        \ and how many of those have been put out of date
create vwin 0 ,         \ the window it is shown in, once that is known
create vbusy 0 ,        \ true while the task is at work
create vtask  0 , 0 , 0 , 0 ,  0 , 0 , 0 , 0 ,  0 , 0 , 0 , 0 ,  0 ,

: retire ( -- ) \ the task that is running takes no more turns after this one
    up @ begin dup @ up @ <> while @ repeat         \ the task before it in the ring
    up @ @ swap !
;

: view-shown ( -- ) \ the rows made since the last that were shown are out of date
    vwin @ ?dup if >r  d# 0 vseen @  tw @  bmh @ vseen @ -  r> win-inval then
    bmh @ vseen !
;

: view-row ( -- ) \ what the task does on each of its turns
    bmh @ th @ < 0= if view-shown  d# 0 vbusy !  retire exit then
    tiff-row dither-row
    d# 1 bmh +!
    bmh @ d# 7 and 0= if view-shown then
;

header view-load : view-load ( blk# -- ) \ start on the TIFF that starts at a block
    vbusy @ if drop exit then                       \ one picture at a time
    tiff-open tiff-start
    bm dither-start
    d# 0 bmh !  d# 0 vseen !
    true vbusy !
    ['] view-row d# 96 d# 96 vtask spawn
;

header view-draw : view-draw ( win -- )
    vwin !
    bm @ 0= if exit then
    copy-mode
    bm d# 0 d# 0  d# 0 d# 0  tw @ bmh @  copy-bits
;

header ignore-key : ignore-key ( c win -- ) 2drop ;
