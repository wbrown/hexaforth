\ view.fs - the image viewer
\
\ The image is dithered once into a bitmap laid out as the display is.
\ Painting the window is then copying the bitmap to the screen.
\
\ Reading and dithering take a while: 10 million instructions for the
\ Mandrill.  So a task does it, one image row per turn, while the desktop
\ carries on.  Only the row being dithered is read.  The window shows the
\ rows as they are made, so the picture comes down the window.  Rows are
\ marked for repaint four at a time rather than one, since each mark costs
\ about ten thousand instructions, half of what dithering a row does: at 1
\ MIPS the picture finished at 18.8 seconds marking eight at a time, 19.6
\ with four, 20.8 with two and 23.2 with one.  After the last row the task
\ takes itself out of the ring.
\
\ A viewer is a record: its bitmap, how much is made and shown, its window,
\ its image and dither, and the task that makes it, whose own record is
\ the end of the viewer's.  So several viewers can load pictures at once.
\ `vw` is the viewer in use.  The task finds its viewer from its own
\ record's address; a window that shows a viewer keeps it in win-data.
\
\ Each task needs its own part of the two stacks.  Above what the desktop's
\ task uses there are two parts of 32 cells, at cell 64 and cell 96, so two
\ pictures can load at once and a third must wait.
\
\ If the picture is bigger than its window, it is shown at half size, each
\ pixel the gray of four of the image's (dither-half).  The 16-bit machine
\ needs this: the Mandrill is 512 by 512, a 16,384-cell bitmap it cannot
\ spare, and its screen is 512 by 342; at half size the whole picture is
\ the 256 by 256 its window shows, in 4,096 cells.  If even half size is
\ bigger than the window, only the middle part that shows is kept: the
\ rows above it are dithered but not kept, since each pixel depends on the
\ rows above, and the rows below are not read.
\
\ The bitmap is allocated first.  When the dither finishes, it is written
\ to the disk and its memory freed, if the disk can take it (see below).
\ Reading and dithering allocate their memory after the bitmap, all when
\ the picture starts, and free it after the last row; the task allocates
\ and frees nothing while it works.  Reading's memory comes from freed
\ memory if there is enough (tiff-open); the dither's is new.  When the
\ dither's memory is freed: if nothing was allocated after it, himem goes
\ back; otherwise it goes to the free run (hfree), where the next picture's
\ reading will take it.

create vw 0 ,           \ the viewer in use: its record

$2a constant VIEW-CELLS
#31 constant VTASK-AT   \ where the viewer's task record starts: the last eleven cells

: bm     ( -- a ) vw @ ;                \ the bitmap: address and cells per row, two cells
: bmh    ( -- a ) vw @ CELL2 + ;        \ rows of the picture made so far,
: vseen  ( -- a ) vw @ c# 3 + ;         \ and how many bitmap rows have been marked for repaint
: vwin   ( -- a ) vw @ c# 4 + ;         \ the window it is shown in, once known
: vpart  ( -- a ) vw @ c# 5 + ;         \ which part of the stacks its task has: 1 or 2
: vimg   ( -- a ) vw @ c# 6 + ;         \ its image, while being read,
: vdth   ( -- a ) vw @ c# 7 + ;         \ and its dither
: vy0    ( -- a ) vw @ c# 8 + ;         \ the row of the picture as shown where the bitmap starts,
: vkw    ( -- a ) vw @ c# 9 + ;         \ and the bitmap's width and height in pixels
: vkh    ( -- a ) vw @ c# 10 + ;
: vhalf  ( -- a ) vw @ c# 11 + ;        \ true if the picture is shown at half size
\ How the picture is drawn, as chosen in its menu (viewmenu.fs): dithered,
\ or kept as gray and drawn through a mask.
: vmode  ( -- a ) vw @ c# 12 + ;        \ 0 dithered, 1 Bayer's mask, 2 blue noise
: vd     ( -- a ) vw @ c# 13 + ;        \ pixels per side of a gray sample as shown, as a shift
: vbr    ( -- a ) vw @ c# 14 + ;        \ levels added to the mask's thresholds: more is darker
: vgray  ( -- a ) vw @ c# 15 + ;        \ the gray, if kept, or 0
: vmask  ( -- a ) vw @ c# 16 + ;        \ the mask
: vblk   ( -- a ) vw @ c# 17 + ;        \ the block the picture's file starts at
: vleft  ( -- a ) vw @ c# 18 + ;        \ the pixel across the picture as shown where the bitmap starts
: vbusy  ( -- a ) vw @ c# 19 + ;        \ true while its task is working
: vstate ( -- a ) vw @ c# 20 + ;        \ what the task is doing, when it is doing the menu's work,
: vrow   ( -- a ) vw @ c# 21 + ;        \ and how many rows of samples are kept
\ The menu's work uses the memory from varena up to vtop, allocated as one
\ run as long as nothing else is allocated after it.  The gray comes first;
\ the mask and drawing from vmark; and what making the gray uses from vtemp.
: varena ( -- a ) vw @ c# 22 + ;
: vtop   ( -- a ) vw @ c# 23 + ;
: vmark  ( -- a ) vw @ c# 24 + ;
: vks    ( -- a ) vw @ c# 25 + ;        \ pixels per side of a sample of the gray as kept, as a shift
: vwhole ( -- a ) vw @ c# 26 + ;        \ true once every row of the bitmap has been made
: vtemp  ( -- a ) vw @ c# 27 + ;
: vpw    ( -- a ) vw @ c# 28 + ;        \ the picture's width and height
: vph    ( -- a ) vw @ c# 29 + ;
: vdrw   ( -- a ) vw @ c# 30 + ;        \ the drawing of the gray through the mask
: vtask  ( -- a ) vw @ VTASK-AT + ;     \ its task record: eleven cells

create vparts 3 ,       \ the parts of the stacks no task is using: one bit each

: retire ( -- ) \ take the running task out of the ring after this turn
    up @ begin dup @ up @ <> while @ repeat         \ the task before it in the ring
    up @ @ swap !
;

: made ( -- n ) \ rows of the bitmap made so far
    bmh @ vy0 @ -  d# 0 max  vkh @ min
;

: view-shown ( -- ) \ mark the rows made since the last call for repaint
    made vseen @ > if
        vwin @ ?dup if >r  d# 0 vseen @  vkw @  made vseen @ -  r> win-inval then
        made vseen !
    then
;

: vback ( a -- ) \ free the viewer's memory from a up to vtop: lower himem if nothing came after, else hfree
    himem @ vtop @ = if dup himem ! vtop ! exit then
    vtop @ over - hfree
;

: vdone ( a -- ) \ done reading: free the dither's memory from a, and reading's (tiff-close)
    himem @ vtop @ = >r  vback  vimg @ tiff-close
    r> if himem @ vtop ! then                       \ the viewer's memory now ends here, if it was the most recent
;

: vkept? ( -- f ) \ true if the menu's memory is still the most recent allocation; if not, start a new one at himem
    varena @ if
        himem @ vtop @ = if true exit then
        varena @ vtop @ over - hfree                \ something was allocated after it: free it
    then
    himem @ dup varena ! vtop !  d# 0 vgray !  false
;

\ ---- the bitmap on the disk
\
\ When the dither finishes, the bitmap is written to the blocks from
\ VIEW-BLK and its memory is freed.  The window is then painted from the
\ disk: the first and last blocks needed are worked out from the clip's top
\ and bottom, and only those are read.
\ Each block holds as many whole rows as fit, so a block can be copied to
\ the screen as a bitmap of its own.  Blocks are read into memory taken
\ just for the paint (with-room), not the block buffer, so that the
\ editor's block is left alone.
\
\ The menu draws into the bitmap, so before it does, view-in allocates the
\ bitmap again and reads it back from the disk.
\
\ There is room on the disk for one bitmap.  If another viewer's is there,
\ or the bitmap is too big, or there is no disk, or a write fails, the
\ bitmap stays in memory.  The disk copy has no header: it is only trusted
\ by the viewer that wrote it, while the machine runs.

create vondisk 0 ,      \ the viewer whose bitmap is on the disk, or 0

: vcells ( -- n ) bm 1+ @ vkh @ * ;                 \ cells in the bitmap
: vrpb ( -- n ) BLKCELLS bm 1+ @ u/mod nip ;        \ rows in a block
: vrows ( row -- n ) vkh @ swap - vrpb min ;        \ rows in the block that starts at this row

: v-chunk ( row -- a n ) \ the block's rows in the bitmap: their address, and how many cells they are
    dup bm 1+ @ * bm @ +
    swap vrows bm 1+ @ *
;

: (view-write) ( x a -- ) \ write the bitmap to the disk, a block at a time, copying each through a
    nip >r  VIEW-BLK d# 0                           ( blk row ) ( R: a )
    begin
        dup vkh @ <
    while
        dup v-chunk r@ swap cells-move
        over r@ swap blk-write
        vrpb +  swap 1+ swap
    repeat
    2drop rdrop
;

: (view-read) ( x a -- ) \ read the bitmap back from the disk, through a
    nip >r  VIEW-BLK d# 0                           ( blk row ) ( R: a )
    begin
        dup vkh @ <
    while
        over r@ swap blk-read
        dup v-chunk r@ -rot cells-move
        vrpb +  swap 1+ swap
    repeat
    2drop rdrop
;

: vshow ( row b -- ) \ copy the block in bitmap b, which starts at that row, to its place in the window
    swap >r  d# 0 d# 0 d# 0 r@  vkw @  r> vrows  copy-bits
;

: vblock ( row -- blk ) vrpb u/mod nip VIEW-BLK + ;   \ the block a row of the bitmap is in

: (view-show) ( x a -- ) \ paint the blocks that overlap the clip; a is a bitmap record, then room for a block
    nip >r  r@ CELL2 + r@ !  bm 1+ @ r@ 1+ !        ( R: a )
    clip c# 3 + @ oy @ -  vkh @ min                 ( bottom ) \ the clip's rows, in the bitmap
    clip CELL + @ oy @ -  d# 0 max                  ( bottom top )
    2dup > 0= if 2drop rdrop exit then              \ no rows of the bitmap to paint
    vblock  swap 1- vblock 1+  swap                 ( end blk ) \ the blocks those rows are in
    begin
        2dupxor
    while
        r@ @ over blk-read
        dup VIEW-BLK - vrpb *  r@ vshow
        1+
    repeat
    2drop rdrop
;

: view-out ( -- ) \ write the bitmap to the disk and free its memory, if the disk can take it
    bm @ 0=  'blk-read @ 0=  or if exit then
    vondisk @ ?dup if vw @ <> if exit then then
    vrpb VIEW-BLKS *  vkh @  < if exit then
    d# 0 d# 1024 ['] (view-write) with-room 0= if exit then
    vw @ vondisk !
    bm @ vcells hgive  d# 0 bm !
;

\ If the viewer's memory is still the most recent allocation, the bitmap
\ goes at its start and the menu's work after it.  Otherwise view-out's
\ hgive left the bitmap's old memory in the free run, and hreuse takes it
\ from there if it is still free.
: view-in ( -- ) \ allocate the bitmap again and read it back from the disk
    bm @ if exit then
    vkept? if
        varena @ himem !  vcells >c halloc bm !  himem @ dup varena ! vtop !
    else
        vcells >c hreuse bm !
    then
    d# 0 vgray !
    d# 0 d# 1024 ['] (view-read) with-room drop
    d# 0 vondisk !
;

: view-done ( -- ) \ the task's last turn: free its part of the stacks and take it out of the ring
    view-shown  true vwhole !  vpart @ vparts or!  d# 0 vbusy !  retire
;

: view-row ( -- ) \ one turn of a viewer's task
    up @ VTASK-AT - vw !                            \ the viewer whose record contains this task's
    bmh @ vy0 @ vkh @ + < 0= if                     \ the last kept row is made
        vimg @ if                                   \ free what reading and dithering used,
            vdth @ vdone                            \ which is the whole arena if it was the menu's
            d# 0 varena !  d# 0 vimg !  d# 0 vdth !
        then
        view-out  view-done  exit
    then
    vdth @ dither-row
    d# 1 bmh +!
    bmh @ d# 3 and 0= if view-shown then
;

: pic-w ( -- n ) vpw @ vhalf @ if d# 1 rshift then ; \ the picture's size as shown
: pic-h ( -- n ) vph @ vhalf @ if d# 1 rshift then ;

: view-spawn ( xt -- ) \ start the viewer's task, running xt each turn, in a free part of the stacks
    vparts @ d# 1 and if d# 1 else d# 2 then        \ the higher part, if it is free
    dup vpart !  dup vparts xor!
    d# 1 = if d# 96 else d# 64 then
    dup vtask spawn
    true vbusy !
;

: view-dither ( -- ) \ start dithering the picture into the viewer's bitmap, from the first row
    vblk @ tiff-open  dup vimg ! image!
    d# 0 bmh !  d# 0 vseen !
    bm vimg @ vleft @ vy0 @ vkw @ vkh @  vhalf @ if dither-half else dither-part then  vdth !
    himem @ vtop !
    ['] view-row view-spawn
;

header view-load
: view-load ( blk# w h -- viewer ) \ start loading the TIFF at a block, to be shown in w by h
    vparts @ 0= d# 8 -throw                         \ no part of the stacks is free for its task
    blk-need                                        \ allocate the block buffer now, so it is not among what is freed
    VIEW-CELLS >c halloc  dup vw !
    d# 0 swap VIEW-CELLS cells-fill
    rot dup vblk ! tiff-open                        ( w h img ) \ read the picture's size,
    dup image!  tw @ vpw !  th @ vph !  tiff-close  \ and free the reader before allocating the bitmap
    over vpw @ <  over vph @ <  or vhalf !          \ bigger than the window: show it at half size
    pic-h min dup vkh !  pic-h swap - d# 1 rshift vy0 !     \ keep the middle rows that fit,
    pic-w min dup vkw !  pic-w swap - d# 1 rshift           \ and the middle columns,
    PXMASK invert and vleft !                       \ starting on a cell boundary
    vkw @ PXMASK + PXSHIFT rshift  dup bm 1+ !      \ the bitmap
    vkh @ * >c halloc bm !
    vhalf @ if d# 1 else d# 2 then vd !             \ a gray sample, if one is kept, covers 4 by 4 of the picture's pixels
    view-dither
    vw @
;

header view-draw : view-draw ( win -- )
    dup win-data @ ?dup 0= if drop exit then
    vw !  vwin !
    copy-mode
    bm @ 0= if                                      \ not in memory: paint it from the disk
        vondisk @ vw @ = if  d# 0  d# 1024 CELLB 2* +  ['] (view-show) with-room drop  then
        exit
    then                                            \ once all rows have been made, show them all: a remake
    bm d# 0 d# 0  d# 0 d# 0  vkw @  vwhole @ if vkh @ else made then  copy-bits    \ draws over the old picture
;

header ignore-key : ignore-key ( c win -- ) 2drop ;
