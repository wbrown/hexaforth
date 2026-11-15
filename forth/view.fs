\ view.fs - the image viewer
\
\ The image is dithered once, into a bitmap as the display is.  Painting
\ the window is then a matter of copying it to the screen.
\
\ Reading the image and dithering it take a while: 10 million
\ instructions for the mandrill.  So a task does them, a row of the image
\ at each of its turns, and the desktop goes on meanwhile.  Only the row
\ being dithered is read, so what is kept is the bitmap.  The window
\ shows the rows that are made, so the picture comes down the window.  The
\ rows are put out of date four at a time and not one, since each time
\ costs about ten thousand instructions, which is half of what a row
\ does: at 1 MIPS the picture is finished at 18.8 seconds with eight at a
\ time, 19.6 with four, 20.8 with two and 23.2 with one.  When the last
\ row is made the task takes itself out of the ring.
\
\ A viewer is a record: its bitmap, how much of it is made and shown, its
\ window, its image and its dither, and the task that makes it, whose own
\ record is the last part of it.  So there may be more than one, each
\ loading its own picture at once.  `vw` is the viewer in hand.  The task
\ finds its viewer from where its own record is; a window that shows one
\ has it in the cell a window keeps for whoever draws it (win-data).
\
\ Each task wants its own part of the two stacks.  There are two parts
\ above anything the desktop's own task reaches, of 32 cells each, from
\ cell 64 and from cell 96: so two pictures may be loading at once, and a
\ third must wait for one of them.
\
\ A window may be smaller than its picture.  Then the picture is shown at
\ half its size, each pixel the gray of four of the image's (dither-half):
\ the Mandrill is 512 by 512 and 32KB as a bitmap, which a machine with
\ cells of 16 bits has not to spare, and its screen is 512 by 342; at
\ half, the whole of it is the 256 by 256 that its window shows, and 8KB.
\ If half is still more than the window shows, what is kept is the bitmap
\ of what it does show, the middle: the rows above that part are dithered
\ and not kept, since what a pixel is depends on those above it, and the
\ rows below are not read.

create vw 0 ,           \ the viewer in hand: its record

$19 constant VIEW-CELLS

: bm     ( -- a ) vw @ ;                \ the bitmap, and the cells in a row of it: two cells
: bmh    ( -- a ) vw @ CELL2 + ;        \ rows of the picture that are dithered,
: vseen  ( -- a ) vw @ c# 3 + ;         \ and how many of the bitmap's have been put out of date
: vwin   ( -- a ) vw @ c# 4 + ;         \ the window it is shown in, once that is known
: vpart  ( -- a ) vw @ c# 5 + ;         \ which part of the stacks its task has: 1 or 2
: vimg   ( -- a ) vw @ c# 6 + ;         \ its image,
: vdth   ( -- a ) vw @ c# 7 + ;         \ and the dither of it
: vy0    ( -- a ) vw @ c# 8 + ;         \ the row of the picture that the bitmap's first is,
: vkw    ( -- a ) vw @ c# 9 + ;         \ and the bitmap's width and height in pixels
: vkh    ( -- a ) vw @ c# 10 + ;
: vhalf  ( -- a ) vw @ c# 11 + ;        \ true if the picture is shown at half its size
: vtask  ( -- a ) vw @ c# 12 + ;        \ its task: thirteen cells

create vparts 3 ,       \ the parts of the stacks that no task has: a bit each

: retire ( -- ) \ the task that is running takes no more turns after this one
    up @ begin dup @ up @ <> while @ repeat         \ the task before it in the ring
    up @ @ swap !
;

: made ( -- n ) \ rows of the bitmap that are made
    bmh @ vy0 @ -  d# 0 max  vkh @ min
;

: view-shown ( -- ) \ the rows made since the last that were shown are out of date
    made vseen @ > if
        vwin @ ?dup if >r  d# 0 vseen @  vkw @  made vseen @ -  r> win-inval then
        made vseen !
    then
;

: view-row ( -- ) \ what a viewer's task does on each of its turns
    up @ c# 12 - vw !                               \ its viewer is what its own record is part of
    bmh @ vy0 @ vkh @ + < 0= if                     \ the last row that is kept is made
        view-shown  vpart @ vparts or!  retire  exit
    then
    vdth @ dither-row
    d# 1 bmh +!
    bmh @ d# 3 and 0= if view-shown then
;

: pic-w ( -- n ) tw @ vhalf @ if d# 1 rshift then ; \ the picture's size as it is shown
: pic-h ( -- n ) th @ vhalf @ if d# 1 rshift then ;

header view-load
: view-load ( blk# w h -- viewer ) \ start on the TIFF that starts at a block, to be shown w by h
    vparts @ 0= d# 8 -throw                         \ no part of the stacks is left for its task
    rot tiff-open                                   ( w h img )
    VIEW-CELLS >c halloc  dup vw !
    d# 0 swap VIEW-CELLS cells-fill
    dup vimg ! image!                               ( w h )
    over tw @ <  over th @ <  or vhalf !            \ more than the window shows: at half its size
    pic-h min dup vkh !  pic-h swap - d# 1 rshift vy0 !     \ the rows in the middle of that,
    pic-w min dup vkw !  pic-w swap - d# 1 rshift           \ and across from the middle,
    PXMASK invert and >r                            \ or the start of the cell that is in
    bm vimg @ r> vy0 @ vkw @ vkh @ vhalf @ (dither) vdth !
    vparts @ d# 1 and if d# 1 else d# 2 then        \ a part of the stacks: the higher if it is free
    dup vpart !  dup vparts xor!
    d# 1 = if d# 96 else d# 64 then
    ['] view-row swap dup vtask spawn
    vw @
;

header view-draw : view-draw ( win -- )
    dup win-data @ ?dup 0= if drop exit then
    vw !  vwin !
    bm @ 0= if exit then
    copy-mode
    bm d# 0 d# 0  d# 0 d# 0  vkw @ made  copy-bits
;

header ignore-key : ignore-key ( c win -- ) 2drop ;
