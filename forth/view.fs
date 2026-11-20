\ view.fs - the image viewer

\ The image is dithered once into a bitmap laid out as the
\ display is.  Painting the window is then copying the bitmap to
\ the screen.

\ Reading and dithering take a while.  So a task does it, one
\ image row per turn, while the desktop carries on.  Only the
\ row being dithered is read.  The window shows the rows as they
\ are made, so the picture comes down the window.  Rows are
\ marked for repaint four at a time rather than one, since a
\ mark costs about half of what dithering a row does, and the
\ picture is finished sooner the more rows a mark is of.
\ After the last row the task takes itself out of the ring.

\ A viewer is a record: its bitmap, how much is made and shown,
\ its window, its image and dither, and the task that makes it,
\ whose own record is the end of the viewer's.  So several
\ viewers can load pictures at once.  `vw` is the viewer in use.
\ The task finds its viewer from its own record's address; a
\ window that shows a viewer keeps it in win-data.

\ Each task needs its own part of the two stacks.  Above what
\ the desktop's task uses there are two parts of 32 cells, at
\ cell 64 and cell 96, so two pictures can load at once and a
\ third must wait.

\ If the picture is bigger than its window, it is shown at half
\ size, each pixel the gray of four of the image's
\ (dither-half).  The 16-bit machine needs this: the Mandrill is
\ 512 by 512, a 16,384-cell bitmap it cannot spare, and its
\ screen is 512 by 342; at half size the whole picture is the
\ 256 by 256 its window shows, in 4,096 cells.  If even half
\ size is bigger than the window, only the middle part that
\ shows is kept: the rows above it are dithered but not kept,
\ since each pixel depends on the rows above, and the rows below
\ are not read.

\ The bitmap is not in memory, if the disk can take it (see
\ below): only the block of it whose rows are being made is,
\ and is written when they are.  That block, or the whole
\ bitmap, is allocated first.  Reading and dithering allocate
\ their memory after it, all when the picture starts, and free
\ it after the last row; the task allocates and frees nothing
\ while it works.  Reading's memory comes from freed memory if
\ there is enough (tiff-open); the dither's is new.  When the
\ dither's memory is freed: if nothing was allocated after it,
\ himem goes back; otherwise it goes to the free run (hfree),
\ where the next picture's reading will take it.

create vw 0 ,           \ the viewer in use: its record

$2f constant VIEW-CELLS
#36 constant VTASK-AT  \ where the viewer's task record starts:
                       \ the last eleven cells

: bm     ( -- a ) vw @ ;          \ the bitmap: address and
                                  \ cells per row, two cells
: bmh    ( -- a ) vw @ CELL2 + ;  \ rows of the picture made
                                  \ so far,
: vseen  ( -- a ) vw @ c# 3 + ;   \ and how many bitmap rows
                                  \ have been marked for repaint
: vwin   ( -- a ) vw @ c# 4 + ;   \ the window it is shown in,
                                  \ once known
: vpart  ( -- a ) vw @ c# 5 + ;   \ which part of the stacks its
                                  \ task has: 1 or 2
: vimg   ( -- a ) vw @ c# 6 + ;   \ its image, while being read,
: vdth   ( -- a ) vw @ c# 7 + ;   \ and its dither
: vy0    ( -- a ) vw @ c# 8 + ;   \ the row of the picture as
                                  \ shown where the
                                  \ bitmap starts,

: vkw    ( -- a ) vw @ c# 9 + ;   \ and the bitmap's width and
                                  \ height in pixels
: vkh    ( -- a ) vw @ c# 10 + ;
: vhalf  ( -- a ) vw @ c# 11 + ;  \ true if the picture is shown
                                  \ at half size
\ How the picture is drawn, as chosen in its menu (below):
\ dithered, or kept as gray and drawn through a mask.
: vmode  ( -- a ) vw @ c# 12 + ;  \ 0 dithered, 1 Bayer's mask,
                                  \ 2 blue noise
: vd     ( -- a ) vw @ c# 13 + ;  \ pixels per side of a gray
                                  \ sample as shown, as a shift
: vbr    ( -- a ) vw @ c# 14 + ;  \ levels added to the mask's
                                  \ thresholds: more is darker
: vgray  ( -- a ) vw @ c# 15 + ;  \ the gray, if kept, or 0
: vmask  ( -- a ) vw @ c# 16 + ;  \ the mask

: vblk   ( -- a ) vw @ c# 17 + ;  \ the block the picture's file
                                  \ starts at
: vleft  ( -- a ) vw @ c# 18 + ;  \ the pixel across the picture
                                  \ as shown where the
                                  \ bitmap starts
: vbusy  ( -- a ) vw @ c# 19 + ;  \ true while its task
                                  \ is working
: vband  ( -- a ) vw @ c# 20 + ;  \ the band the gray is drawn
                                  \ through the mask into,
: vrow   ( -- a ) vw @ c# 21 + ;  \ and how many rows of samples
                                  \ of the gray are kept
\ The menu's work uses the memory from varena up to vtop,
\ allocated as one run as long as nothing else is allocated
\ after it.  The gray comes first; the mask, the band and the
\ drawing from vmark; and then what making the gray uses.
: varena ( -- a ) vw @ c# 22 + ;

: vtop   ( -- a ) vw @ c# 23 + ;
: vmark  ( -- a ) vw @ c# 24 + ;
: vks    ( -- a ) vw @ c# 25 + ;     \ pixels per side of a
                                     \ sample of the gray as
                                     \ kept, as a shift
: vwhole ( -- a ) vw @ c# 26 + ;     \ true once every row of
                                     \ the bitmap has been made
: vsaved ( -- a ) vw @ c# 27 + ;     \ rows of the bitmap that
                                     \ are on the disk
: vpw    ( -- a ) vw @ c# 28 + ;     \ the picture's width
                                     \ and height
: vph    ( -- a ) vw @ c# 29 + ;
: vdrw   ( -- a ) vw @ c# 30 + ;     \ the drawing of the gray
                                     \ through the mask

\ The gray on the disk (below): the block of it that is read for
\ painting, or 0 if the gray is all in memory; which row of
\ samples that block starts at, or a row there is none of; the
\ block its rows are made in; how many rows of samples are on
\ the disk; and how many a block holds.
: vgpb    ( -- a ) vw @ c# 31 + ;
: vgin    ( -- a ) vw @ c# 32 + ;
: vgmk    ( -- a ) vw @ c# 33 + ;
: vgsaved ( -- a ) vw @ c# 34 + ;
: vgrpb   ( -- a ) vw @ c# 35 + ;

: vtask  ( -- a ) vw @ VTASK-AT + ;  \ its task record:
                                     \ eleven cells

create vparts 3 ,  \ the parts of the stacks no task is using:
                   \ one bit each

: retire ( -- )
    \ take the running task out of the ring after this turn
    \ the task before it in the ring
    up @ begin dup @ up @ <> while @ repeat
    up @ @ swap !
;

: made ( -- n ) \ rows of the bitmap made so far
    bmh @ vy0 @ -  d# 0 max  vkh @ min
;

: view-shown ( -- )
    \ mark the rows made since the last call for repaint
    made vseen @ > if
        vwin @ ?dup if
            >r  d# 0 vseen @  vkw @  made vseen @ -
            r> win-inval
        then
        made vseen !
    then
;

\ free the viewer's memory from a up to vtop: lower himem if
\ nothing came after, else hfree
: vback ( a -- )
    himem @ vtop @ = if dup himem ! vtop ! exit then
    vtop @ over - hfree
;

\ done reading: free the dither's memory from a, and
\ reading's (tiff-close)
: vdone ( a -- )
    himem @ vtop @ = >r  vback  vimg @ tiff-close
    r> if himem @ vtop ! then  \ the viewer's memory now ends
                               \ here, if it was the most recent
;

\ true if the menu's memory is still the most recent allocation;
\ if not, start a new one at himem
: vkept? ( -- f )
    varena @ if
        himem @ vtop @ = if true exit then
        varena @ vtop @ over - hfree  \ something was allocated
                                      \ after it: free it
    then
    himem @ dup varena ! vtop !  d# 0 vgray !  false
;

\ ---- the bitmap on the disk

\ The bitmap's place is the disk, in the blocks from VIEW-BLK,
\ each of which holds as many whole rows as fit.  The dither
\ makes its rows in one block's worth of memory, and each block
\ is written when its rows are made (block-out): the whole
\ bitmap is never in memory.  The viewer's record of the bitmap
\ says where the rows in memory would be if all of it were
\ there, so the dither, which is told the row, puts each in the
\ block; vsaved is how many rows are on the disk.

\ The window is painted from the disk, and while the dither is
\ at work the rows of the block being made from memory: the
\ first and last blocks needed are worked out from the clip's
\ top and bottom, and only those are read.  A block can be
\ copied to the screen as a bitmap of its own.  Blocks are read
\ into memory taken just for the paint (with-room), not the
\ block buffer, so that the editor's block is left alone.

\ The menu does not draw into the bitmap.  A picture kept as
\ gray is painted from the gray, straight to the window where
\ it can be seen (gray-shown, below), and the bitmap stays as
\ the dither made it, for when the picture is shown dithered
\ again: it is not read back, and nothing is dithered twice.

\ There is room on the disk for one bitmap.  If another viewer's
\ is there, or the bitmap is too big, or there is no disk, the
\ whole bitmap is in memory and stays there; and if a write
\ fails, the picture is dithered again into one (in-memory).
\ The disk copy has no header: it is only trusted by the viewer
\ that wrote it, while the machine runs.

create vondisk 0 ,  \ the viewer whose bitmap is on the disk,
                    \ or 0

: vcells ( -- n ) \ cells in the bitmap
    bm 1+ @ vkh @ * ;
: vrpb ( -- n ) \ rows in a block
    BLKCELLS bm 1+ @ u/mod nip ;
: vrows ( row -- n ) \ rows in the block that starts at this row
    vkh @ swap - vrpb min ;

\ the block's rows in the bitmap: their address, and how many
\ cells they are
: v-chunk ( row -- a n )
    dup bm 1+ @ * bm @ +
    swap vrows bm 1+ @ *
;

\ copy the block in bitmap b, which starts at that row, to its
\ place in the window
: vshow ( row b -- )
    swap >r  d# 0 d# 0 d# 0 r@  vkw @  r> vrows  copy-bits
;

: vblock ( row -- blk ) \ the block a row of the bitmap is in
    vrpb u/mod nip VIEW-BLK + ;

\ paint the blocks that overlap the clip, from the block that
\ row `top` of the bitmap is in; a is a bitmap record, then room
\ for a block

: (view-show) ( top a -- )
    >r  r@ CELL2 + r@ !  bm 1+ @ r@ 1+ !      ( top ) ( R: a )
    clip-rows vsaved @ min  swap rot max      ( bottom top )
    \ no rows of the bitmap to paint
    2dup > 0= if 2drop rdrop exit then
    \ the blocks those rows are in
    vblock  swap 1- vblock 1+  swap           ( end blk )
    begin
        2dup xor
    while
        r@ @ over blk-read
        dup VIEW-BLK - vrpb *  r@ vshow
        1+
    repeat
    2drop rdrop
;

\ can the disk take the bitmap, a block as its rows are made?
: to-disk? ( -- f )
    'blk-read @ 0<>
    vondisk @ dup 0= swap vw @ = or  and
    vrpb VIEW-BLKS * vkh @ < 0=  and
;

: whole-bitmap ( -- ) \ memory for all of it
    vcells >c halloc bm ! ;

\ The bitmap's cells in a row, and its memory: a block, which is
\ where its first rows are, if the disk takes it; or all of it.
: vbitmap ( -- )
    vkw @ PXMASK + PXSHIFT rshift bm 1+ !  d# 0 vsaved !
    to-disk? if
        vw @ vondisk !  BLKCELLS >c halloc bm !  exit
    then
    whole-bitmap
;

\ start dithering the picture into the viewer's bitmap, from the
\ first row
: dither-begin ( -- )
    vblk @ tiff-open  dup vimg ! image!
    d# 0 bmh !  d# 0 vseen !
    bm vimg @ vleft @ vy0 @ vkw @ vkh @
    vhalf @ if dither-half else dither-part then  vdth !
    himem @ vtop !
;

: in-block ( -- a ) \ where the block being made is in memory
    vsaved @ v-chunk drop ;

\ The disk did not take a block.  What the dither and its block
\ had is given back, and the picture is dithered again from its
\ first row into a bitmap in memory, which stays there.
: in-memory ( -- )
    d# 0 vondisk !
    vdth @ vdone  in-block BLKCELLS hgive
    d# 0 vsaved !  whole-bitmap  dither-begin
;

\ The rows of the block being made are made: it is written, and
\ the rows after them are the ones in memory.
: block-out ( -- )
    in-block dup vsaved @ vblock ['] blk-write catch
    if 2drop drop in-memory exit then
    vsaved @ vrows vsaved +!
    vsaved @ bm 1+ @ * - bm !
;

\ the task's last turn: free its part of the stacks and take it
\ out of the ring
: view-done ( -- )
    view-shown  true vwhole !  vpart @ vparts or!  d# 0 vbusy !
    retire
;

\ The last kept row is made: what reading and dithering used is
\ given back, and the block, if the bitmap is on the disk.
: view-made ( -- )
    vimg @ if vdth @ vdone  d# 0 vimg !  d# 0 vdth ! then
    vondisk @ vw @ = if
        in-block BLKCELLS hgive  d# 0 bm !
    then
    view-done
;

: view-row ( -- ) \ one turn of a viewer's task
    up @ VTASK-AT - vw !         \ the viewer whose record
                                 \ contains this task's
    bmh @ vy0 @ vkh @ + < 0= if view-made exit then
    vdth @ dither-row
    d# 1 bmh +!
    vondisk @ vw @ = if
        made vsaved @ -  vsaved @ vrows = if block-out then
    then
    bmh @ d# 3 and 0= if view-shown then
;

: pic-w ( -- n ) \ the picture's size as shown
    vpw @ vhalf @ if d# 1 rshift then ;
: pic-h ( -- n ) vph @ vhalf @ if d# 1 rshift then ;

\ start the viewer's task, running xt each turn, in a free part
\ of the stacks
: view-spawn ( xt -- )
    \ the higher part, if it is free
    vparts @ d# 1 and if d# 1 else d# 2 then
    dup vpart !  dup vparts xor!
    d# 1 = if d# 96 else d# 64 then
    dup vtask spawn
    true vbusy !
;

\ The picture's size, read and the reader freed before the
\ bitmap is allocated; and whether it is bigger than the window,
\ and so shown at half size.  A gray sample, if one is kept,
\ covers 4 by 4 of the picture's pixels.
: vsize ( w h img -- w h )
    dup image!  tw @ vpw !  th @ vph !  tiff-close
    over vpw @ <  over vph @ <  or vhalf !
    vhalf @ if d# 1 else d# 2 then vd !
;

\ The middle rows of the picture that fit, and the middle
\ columns, from a cell boundary.
: vkeep ( w h -- )
    pic-h min dup vkh !  pic-h swap - d# 1 rshift vy0 !
    pic-w min dup vkw !  pic-w swap - d# 1 rshift
    PXMASK invert and vleft !
;

header view-load
: view-load ( blk# w h -- viewer )
    \ start loading the TIFF at a block, to be shown in w by h
    \ no part of the stacks is free for its task
    vparts @ 0= d# 8 -throw
    \ allocate the block buffer now, so it is not among what
    \ is freed
    blk-need
    VIEW-CELLS >c halloc  dup vw !
    d# 0 swap VIEW-CELLS cells-fill
    rot dup vblk ! tiff-open  vsize  vkeep  vbitmap
    dither-begin  ['] view-row view-spawn
    vw @
;

\ ---- the gray on the disk

\ A picture kept as gray has its place on the disk too, in the
\ blocks from GRAY-BLK, if the viewer's bitmap is on the disk
\ and the gray fits there.  A block holds as many whole rows of
\ samples as fit, a row being its four plane rows.  It is made
\ a block at a time, as the bitmap is (gray-out), and painted
\ from the blocks the clip asks for, read into a block that is
\ kept for that.  The gray's record says where its planes would
\ start if all of them were in memory: whoever is about to make
\ a row or draw one says first which block has it (make-in,
\ gray-in), and the dither and the drawing, which are told the
\ row, find it there.  If the disk does not take the gray, all
\ of it is in memory, and these do nothing.

: gs-cells ( -- n ) \ cells in a row of samples: four plane rows
    vgray @ 1+ @ d# 2 lshift ;
: gs-rows ( row -- n ) \ the rows in the block that starts at it
    vgray @ gray-rows swap - vgrpb @ min ;
: gs-blk ( row -- blk ) \ the block a row of samples is in
    vgrpb @ u/mod nip GRAY-BLK + ;
\ the rows of samples from `row` are the ones in the block at a
: gs-at ( a row -- ) gs-cells * - vgray @ ! ;

: make-in ( -- ) \ the rows being made are in their block
    vgpb @ if vgmk @ vgsaved @ gs-at then ;

\ The planes of row sy of the samples are where the gray says:
\ in the block being made, or in the block for painting, which
\ is read if it is not the one that is there.
: gray-in ( sy -- )
    vgpb @ 0= if drop exit then
    dup vgsaved @ < 0= if drop make-in exit then
    dup vgin @ - vgrpb @ u< 0= if
        dup vgrpb @ u/mod drop -       ( its block's first row )
        vgpb @ over gs-blk blk-read  dup vgin !
    then
    drop  vgpb @ vgin @ gs-at
;

\ The rows of the block being made are made: it is written, and
\ the rows after them are the ones made in it.
: gray-out ( -- )
    vgmk @ vgsaved @ gs-blk blk-write
    vgsaved @ gs-rows vgsaved +!
;

\ ---- painting the window

\ The dithered bitmap, from row top of it down, where the clip
\ asks for it: the rows that are in memory, as many as have been
\ made, and the rows that are on the disk.
: dithered ( top -- )
    bm @ if
        dup vsaved @ max >r
        bm d# 0 r@  d# 0 r@  vkw @
        vwhole @ if vkh @ else made then r> -  copy-bits
    then
    vondisk @ vw @ =  clip-rows drop vsaved @ < and if
        d# 1024 CELLB 2* +  ['] (view-show) with-room
    then
    drop
;

\ A picture kept as gray is painted from the gray.  The rows the
\ clip asks for are drawn through the mask into a band, a bitmap
\ of BAND-ROWS rows as wide as the picture's, and each bandful
\ is copied to the window, which shows what of it can be seen.
\ The band's record is a bitmap's, with its rows after it, and
\ says its rows start where row y of the whole bitmap would be:
\ so what draws a row of the picture (gray-draw-row) and what
\ copies from one (copy-bits) are told the row of the picture,
\ and find it in the band.
#16 constant BAND-ROWS

: band-at ( y -- ) \ the band holds the rows from y
    vband @ >r  r@ 1+ @ * negate  r@ CELL2 + +  r> ! ;

: gray-line ( y -- ) \ row y of the bitmap, drawn into the band
    dup vy0 @ + vd @ rshift gray-in  vdrw @ gray-draw-row ;

\ n rows of the picture from row y, no more than a bandful
: band-draw ( y n -- )
    over band-at
    2dup bounds
    begin 2dup xor while dup gray-line 1+ repeat
    2drop
    >r >r  vband @ d# 0 r@  d# 0 r>  vkw @ r>  copy-bits
;

\ What is made of the gray, through the mask; and under it, on
\ the rows not made yet, the picture as it was dithered, if any
\ of those rows is asked for.
: gray-shown ( -- )
    clip-rows nip made >
    made vkh @ < and if made dithered then
    clip-rows made min  swap d# 0 max       ( bottom top )
    begin
        2dup >
    while
        2dup - BAND-ROWS min  2dup band-draw  +
    repeat
    2drop
;

header view-draw : view-draw ( win -- )
    dup win-data @ ?dup 0= if drop exit then
    vw !  vwin !
    copy-mode
    vmode @ if vgray @ if gray-shown exit then then
    d# 0 dithered
;

header ignore-key : ignore-key ( c win -- ) 2drop ;

\ ---- the picture's menu: how the picture is drawn

\ The menu is in the bar while the picture's window is in front.
\ It chooses how the picture is drawn: dithered (Atkinson), or
\ kept as gray and drawn through a mask (gray.fs, which is
\ included before this file), Bayer's or blue noise from the
\ disk.  Finer and Coarser change how many of the picture's
\ pixels a gray sample covers; Darker and Lighter move the
\ mask's thresholds a level.  These only matter when drawing
\ through a mask.

\ A picture drawn through a mask is not drawn into the viewer's
\ bitmap, and takes no memory for one.  It is kept as gray, and
\ the window is painted from the gray, where it can be seen
\ (gray-shown, above).  Making the gray is the viewer's task's
\ work: each turn reads and keeps one row of samples, and marks
\ the rows of the picture that makes for repaint, so the picture
\ comes down the window as it does when dithered, over the
\ picture as it was dithered.  Darker, Lighter and another mask
\ are no work at all: the mask is loaded again, and the window
\ is marked.  Atkinson shows the bitmap the dither made, which
\ is where it was, on the disk or in memory.

\ The memory used is the viewer's arena, all allocated when a
\ line is picked: the gray, then the mask, the band and the
\ drawing, then what making the gray needs, which is freed after
\ the last row.  Each pick starts the arena again from its
\ start, so making one picture after another takes no more
\ memory; Atkinson frees it all.  If there is not enough memory
\ for the gray as fine as asked, it is kept coarser.  The gray,
\ mask and drawing are per-viewer records, so viewers can work
\ at the same time; a pick while the viewer's task is busy does
\ nothing.

create t-picture ," Picture"
create t-atkinson ," Atkinson"
create t-ordered ," Ordered"
create t-blue ," Blue noise"
create t-finer ," Finer"
create t-coarser ," Coarser"
create t-darker ," Darker"
create t-lighter ," Lighter"

create pic-menu 0 , 0 , 0 , 0 , 0 ,  \ the menu (made
                                     \ by view-menu)
create pic-lines 0 , 0 , 0 , 0 , 0 , 0 , 0 ,

: pic-vw ( -- viewer|0 )
    \ make the front window's viewer the one in use
    top win-data @ dup vw ! ;
: pic-n ( -- n ) d# 7 ;
: pic-line ( i -- title ) pic-lines + @ ;
: pic-mark ( i -- f ) \ marks the way it is drawn now
    pic-vw if vmode @ = else drop false then ;

: pic-ks ( -- shift )
    \ pixels of the picture per side of a sample, as a shift
    vd @ vhalf @ if 1+ then ;

W32+? [if]
: mask-blue ( bright -- mask ) mask-blue64 ;
[else]
: mask-blue ( bright -- mask )
    \ a quarter the size of the 64 by 64, to save memory
    mask-blue32 ;
[then]

\ ---- what the task does

\ The rows of the picture that the samples kept so far make are
\ made, and marked for repaint.
: pic-made ( -- )
    vrow @ vd @ lshift  vy0 @ vkh @ + min  bmh @ max bmh !
    view-shown
;

\ The reader freed, if it came from freed memory outside the
\ arena.
: pic-unread ( -- )
    vimg @ ?dup if
        dup varena @ u<  over himem @ u< 0=
        or if tiff-close else drop then
    then
;

\ Show the picture as it was dithered: the bitmap is where the
\ dither left it, whole.  The gray's memory is given back.
: pic-dither ( -- )
    vkept? drop  varena @ himem !  d# 0 varena !  d# 0 vgray !
    vy0 @ vkh @ + bmh !  d# 0 vseen !  view-shown
;

\ The disk did not take a block of the gray: the gray is given
\ up, and the picture is shown as it was dithered.
: gray-lost ( -- )
    pic-unread  d# 0 vimg !  d# 0 vmode !  pic-dither
    view-done
;

\ the block the gray's rows were made in is given back
: make-free ( -- )
    vgmk @ ?dup if
        BLKCELLS hgive  d# 0 vgmk !  himem @ vtop !
    then
;

\ When the rows of the block being made are all made, it is
\ written: false if the disk did not take it.
: block-kept ( -- f )
    vgpb @ 0= if true exit then
    vrow @ vgsaved @ -  vgsaved @ gs-rows <> if true exit then
    ['] gray-out catch 0=
;

: pic-turn ( -- )
    \ one turn of a viewer's task making the gray: a row of
    \ samples is kept, and its block written when it is full
    up @ VTASK-AT - vw !
    make-in  vgray @ gray-row  d# 1 vrow +!
    block-kept 0= if gray-lost exit then
    pic-made
    vrow @ vgray @ gray-rows < if exit then
    \ free what making it used, if nothing was allocated since
    himem @ vtop @ = if
        vgray @ gray-dither vdone make-free
    then
    d# 0 vimg !
    view-done
;

\ ---- what a pick does

: band-new ( -- band ) \ a band as wide as the bitmap is
    bm 1+ @ BAND-ROWS * CELL2 + >c halloc
    bm 1+ @ over 1+ !
;

\ load the mask, and start a drawing through it into a band,
\ after what is allocated: no row of the picture is made
: pic-mask ( -- )
    vbr @
    vmode @ d# 1 = if mask-bayer else mask-blue then vmask !
    band-new dup vband !
    vmask @ vgray @ vd @ vleft @ vy0 @
    gray-draw-start vdrw !
    vy0 @ bmh !  d# 0 vseen !
;

\ The gray's record; and its planes, all of them in memory, or
\ on the disk with a block in memory to paint from, if the
\ viewer's bitmap is on the disk and the gray fits there.
: gray-plan ( w h shift -- )
    gray-record vgray !
    d# 0 vgsaved !  d# -1 u2/ vgin !  d# 0 vgmk !  d# 0 vgpb !
    BLKCELLS gs-cells u/mod nip vgrpb !
    vondisk @ vw @ =
    vgrpb @ GRAY-BLKS * vgray @ gray-rows < 0=  and if
        BLKCELLS >c halloc vgpb !  exit
    then
    vgray @ gray-planes
;

\ from the start of the arena: allocate the gray, the mask, the
\ block its rows are made in if it goes to the disk, and what
\ making the gray needs
: pic-start ( -- )
    varena @ himem !
    vpw @ vph @ pic-ks gray-plan  pic-ks vks !
    himem @ vmark !
    pic-mask
    vgpb @ if BLKCELLS >c halloc vgmk ! then
    vblk @ tiff-open  dup vimg !  vgray @ gray-open
    himem @ vtop !
;

\ The gray through the mask again, if it is still kept at this
\ fineness: true if it is.  Every row of it is made, so the
\ window is marked, and is painted through the mask as it now
\ is.
: pic-again ( -- f )
    vkept? if vgray @ if vks @ pic-ks = if
        vmark @ himem !  pic-mask  himem @ vtop !
        vgray @ gray-rows vrow !  pic-made  true exit
    then then then
    false
;

\ show the picture through the mask, making the gray first
\ unless it is already kept at this fineness
: pic-gray ( -- )
    pic-again if exit then
    begin               \ as fine as memory allows
        d# 0 vimg !
        ['] pic-start catch
    while
        pic-unread
        vd @ d# 2 < 0= if d# -8 throw then
        d# 1 vd +!
    repeat
    d# 0 vrow !  ['] pic-turn view-spawn
;

: changed! ( x a -- f ) \ x stored at a: true if that changed it
    2dup @ = if 2drop false exit then  ! true ;

: pic-way ( i -- ) \ dithered, or through one of the masks
    dup vmode changed! 0= if drop exit then
    if pic-gray else pic-dither then
;

: pic-do ( i -- )
    \ do what line i asks, if it changes what is shown
    dup d# 3 < if pic-way exit then
    dup d# 5 < if
        d# 3 = if d# -1 else d# 1 then  vd @ +  d# 0 max
        d# 2 min  vd changed!
    else
        d# 5 = if d# 1 else d# -1 then  vbr @ +  d# -15 max
        d# 15 min  vbr changed!
    then
    vmode @ and if pic-gray then
;

: pic-pick ( i -- )
    pic-vw 0= if drop exit then
    \ ignore it while busy, or with no free part of the stacks
    vbusy @  vparts @ 0= or if drop exit then
    vmode @ >r  vd @ >r  vbr @ >r
    \ out of memory: put the settings back
    ['] pic-do catch if
        drop  r> vbr !  r> vd !  r> vmode !  d# 0 vgray !
        exit
    then
    rdrop rdrop rdrop
;

header view-menu
: view-menu ( win -- ) \ give the window the picture's menu
    t-atkinson pic-lines !  t-ordered pic-lines 1+ !
    t-blue pic-lines CELL2 + !
    t-finer pic-lines c# 3 + !  t-coarser pic-lines c# 4 + !
    t-darker pic-lines c# 5 + !  t-lighter pic-lines c# 6 + !
    t-picture pic-menu !  ['] pic-n pic-menu 1+ !
    ['] pic-line pic-menu CELL2 + !
    ['] pic-pick pic-menu c# 3 + !
    ['] pic-mark pic-menu c# 4 + !
    pic-menu swap win-menu !
;

\ The picture is on whichever disk has it under this label: the
\ pictures are on a disk of their own, and not on the one the
\ machine starts from (doc/disk.md).  Its first block, whose
\ number says its disk, or nothing if no disk has it.
create t-mandrill ," mandrill.tiff"

header picture-blk
: picture-blk ( -- blk|0 ) t-mandrill >c count labelled drop ;
