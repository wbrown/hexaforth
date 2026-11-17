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
\ the Mandrill is 512 by 512, a bitmap of 16,384 cells where a cell is 16
\ bits, which that machine has not to spare, and its screen is 512 by 342;
\ at half, the whole of it is the 256 by 256 that its window shows, and
\ 4,096 cells.  If half is still more than the window shows, what is kept
\ is the bitmap of what it does show, the middle: the rows above that part
\ are dithered and not kept, since what a pixel is depends on those above
\ it, and the rows below are not read.
\
\ The bitmap is had first, and kept.  What reading and dithering have is
\ had after it, all when the picture is started, and given back when the
\ last row is made: the task has no memory and gives none back while it
\ works.  What reading has is had where memory was given back, if there is
\ enough of that (tiff-open); what dithering has is had new.  If nothing
\ else has been had since, what the dither had is had again from where it
\ was; if something has, it is nobody's (hfree), where the next picture to
\ be read is read.

create vw 0 ,           \ the viewer in hand: its record

$29 constant VIEW-CELLS
#30 constant VTASK-AT   \ where in the record its task is: the last eleven cells

: bm     ( -- a ) vw @ ;                \ the bitmap, and the cells in a row of it: two cells
: bmh    ( -- a ) vw @ CELL2 + ;        \ rows of the picture that are made,
: vseen  ( -- a ) vw @ c# 3 + ;         \ and how many of the bitmap's have been put out of date
: vwin   ( -- a ) vw @ c# 4 + ;         \ the window it is shown in, once that is known
: vpart  ( -- a ) vw @ c# 5 + ;         \ which part of the stacks its task has: 1 or 2
: vimg   ( -- a ) vw @ c# 6 + ;         \ its image, while it is read,
: vdth   ( -- a ) vw @ c# 7 + ;         \ and the dither of it
: vy0    ( -- a ) vw @ c# 8 + ;         \ the row of the picture as shown that the bitmap's first is,
: vkw    ( -- a ) vw @ c# 9 + ;         \ and the bitmap's width and height in pixels
: vkh    ( -- a ) vw @ c# 10 + ;
: vhalf  ( -- a ) vw @ c# 11 + ;        \ true if the picture is shown at half its size
\ How the picture is drawn, which its menu says (viewmenu.fs): dithered, or
\ kept as gray and drawn through a mask.
: vmode  ( -- a ) vw @ c# 12 + ;        \ 0 dithered, 1 Bayer's mask, 2 blue noise
: vd     ( -- a ) vw @ c# 13 + ;        \ pixels a side to a sample of the gray as shown, as a shift
: vbr    ( -- a ) vw @ c# 14 + ;        \ levels added to the mask's thresholds: darker, or less is lighter
: vgray  ( -- a ) vw @ c# 15 + ;        \ the gray, if it is kept, or 0
: vmask  ( -- a ) vw @ c# 16 + ;        \ the mask
: vblk   ( -- a ) vw @ c# 17 + ;        \ the block the picture's file starts at
: vleft  ( -- a ) vw @ c# 18 + ;        \ the pixel of the picture as shown that the bitmap's first is
: vbusy  ( -- a ) vw @ c# 19 + ;        \ true while its task is at work
: vstate ( -- a ) vw @ c# 20 + ;        \ what the task is doing, when it is the menu's,
: vrow   ( -- a ) vw @ c# 21 + ;        \ and the rows of samples that are kept
\ The memory the menu's work has, from varena up to vtop: one run of it,
\ which is where memory is given out from while nothing else has had any
\ since.  The gray is first, and what the mask and drawing have is from
\ vmark, and what keeping the gray has is from vtemp.
: varena ( -- a ) vw @ c# 22 + ;
: vtop   ( -- a ) vw @ c# 23 + ;
: vmark  ( -- a ) vw @ c# 24 + ;
: vks    ( -- a ) vw @ c# 25 + ;        \ pixels of the picture a side to a sample of the gray kept, as a shift
: vwhole ( -- a ) vw @ c# 26 + ;        \ true once every row of the bitmap has been made
: vtemp  ( -- a ) vw @ c# 27 + ;
: vpw    ( -- a ) vw @ c# 28 + ;        \ the picture's width and height
: vph    ( -- a ) vw @ c# 29 + ;
: vtask  ( -- a ) vw @ VTASK-AT + ;     \ its task: eleven cells

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

: vback ( a -- ) \ what the viewer had from a up to vtop given back: to be had again if nothing has been had since
    himem @ vtop @ = if dup himem ! vtop ! exit then
    vtop @ over - hfree
;

: vdone ( a -- ) \ done reading: what the dither had, from a, and what reading had (tiff-close), given back
    himem @ vtop @ = >r  vback  vimg @ tiff-close
    r> if himem @ vtop ! then                       \ where the viewer's memory ends, if it was the last to have any
;

: vkept? ( -- f ) \ whether what the menu's work has is still its own, to be had again; if not, it has memory from here on
    varena @ if
        himem @ vtop @ = if true exit then
        varena @ vtop @ over - hfree                \ one run of its own, which others have had memory after
    then
    himem @ dup varena ! vtop !  d# 0 vgray !  false
;

: view-done ( -- ) \ its task's last turn: the stacks it had are free, and it takes no more turns
    view-shown  true vwhole !  vpart @ vparts or!  d# 0 vbusy !  retire
;

: view-row ( -- ) \ what a viewer's task does on each of its turns
    up @ VTASK-AT - vw !                            \ its viewer is what its own record is part of
    bmh @ vy0 @ vkh @ + < 0= if                     \ the last row that is kept is made
        vimg @ if                                   \ what it was read and dithered in is done with,
            vdth @ vdone                            \ which is all the menu's work had, if it was that
            d# 0 varena !  d# 0 vimg !  d# 0 vdth !
        then
        view-done  exit
    then
    vdth @ dither-row
    d# 1 bmh +!
    bmh @ d# 3 and 0= if view-shown then
;

: pic-w ( -- n ) vpw @ vhalf @ if d# 1 rshift then ; \ the picture's size as it is shown
: pic-h ( -- n ) vph @ vhalf @ if d# 1 rshift then ;

: view-spawn ( xt -- ) \ the viewer's task, to run xt at each of its turns, in a part of the stacks of its own
    vparts @ d# 1 and if d# 1 else d# 2 then        \ the higher part if it is free
    dup vpart !  dup vparts xor!
    d# 1 = if d# 96 else d# 64 then
    dup vtask spawn
    true vbusy !
;

\ Dither the picture into the bitmap the viewer has, from its first row.
: view-dither ( -- )
    vblk @ tiff-open  dup vimg ! image!
    d# 0 bmh !  d# 0 vseen !
    bm vimg @ vleft @ vy0 @ vkw @ vkh @ vhalf @ (dither) vdth !
    himem @ vtop !
    ['] view-row view-spawn
;

header view-load
: view-load ( blk# w h -- viewer ) \ start on the TIFF that starts at a block, to be shown w by h
    vparts @ 0= d# 8 -throw                         \ no part of the stacks is left for its task
    blk-need                                        \ the block buffer is not to be among what is given back
    VIEW-CELLS >c halloc  dup vw !
    d# 0 swap VIEW-CELLS cells-fill
    rot dup vblk ! tiff-open                        ( w h img ) \ how big the picture is,
    dup image!  tw @ vpw !  th @ vph !  tiff-close  \ and what reading it had given back, before the bitmap is had
    over vpw @ <  over vph @ <  or vhalf !          \ more than the window shows: at half its size
    pic-h min dup vkh !  pic-h swap - d# 1 rshift vy0 !     \ the rows in the middle of that,
    pic-w min dup vkw !  pic-w swap - d# 1 rshift           \ and across from the middle,
    PXMASK invert and vleft !                       \ or the start of the cell that is in
    vkw @ PXMASK + PXSHIFT rshift  dup bm 1+ !      \ the bitmap, which is kept
    vkh @ * >c halloc bm !
    vhalf @ if d# 1 else d# 2 then vd !             \ a sample of the gray, if it is kept, for 4 by 4 of the picture's pixels
    view-dither
    vw @
;

header view-draw : view-draw ( win -- )
    dup win-data @ ?dup 0= if drop exit then
    vw !  vwin !
    bm @ 0= if exit then
    copy-mode                                       \ once all its rows have been made, what is being made
    bm d# 0 d# 0  d# 0 d# 0  vkw @  vwhole @ if vkh @ else made then  copy-bits    \ again is over what was
;

header ignore-key : ignore-key ( c win -- ) 2drop ;
