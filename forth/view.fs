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

create vw 0 ,           \ the viewer in hand: its record

$15 constant VIEW-CELLS

: bm     ( -- a ) vw @ ;                \ the bitmap, and the cells in a row of it: two cells
: bmh    ( -- a ) vw @ CELL2 + ;        \ rows of it that are made,
: vseen  ( -- a ) vw @ c# 3 + ;         \ and how many of those have been put out of date
: vwin   ( -- a ) vw @ c# 4 + ;         \ the window it is shown in, once that is known
: vpart  ( -- a ) vw @ c# 5 + ;         \ which part of the stacks its task has: 1 or 2
: vimg   ( -- a ) vw @ c# 6 + ;         \ its image,
: vdth   ( -- a ) vw @ c# 7 + ;         \ and the dither of it
: vtask  ( -- a ) vw @ c# 8 + ;         \ its task: thirteen cells

create vparts 3 ,       \ the parts of the stacks that no task has: a bit each

: retire ( -- ) \ the task that is running takes no more turns after this one
    up @ begin dup @ up @ <> while @ repeat         \ the task before it in the ring
    up @ @ swap !
;

: view-shown ( -- ) \ the rows made since the last that were shown are out of date
    vimg @ image!
    vwin @ ?dup if >r  d# 0 vseen @  tw @  bmh @ vseen @ -  r> win-inval then
    bmh @ vseen !
;

: view-row ( -- ) \ what a viewer's task does on each of its turns
    up @ c# 8 - vw !                                \ its viewer is what its own record is part of
    vimg @ image!
    bmh @ th @ < 0= if
        view-shown  vpart @ vparts or!  retire  exit
    then
    vdth @ dither-row
    d# 1 bmh +!
    bmh @ d# 3 and 0= if view-shown then
;

header view-load : view-load ( blk# -- viewer ) \ start on the TIFF that starts at a block
    vparts @ 0= d# 8 -throw                         \ no part of the stacks is left for its task
    tiff-open                                       ( img )
    VIEW-CELLS >c halloc  dup vw !
    d# 0 swap VIEW-CELLS cells-fill
    dup vimg !
    bm swap dither-start vdth !
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
    vimg @ image!
    copy-mode
    bm d# 0 d# 0  d# 0 d# 0  tw @ bmh @  copy-bits
;

header ignore-key : ignore-key ( c win -- ) 2drop ;
