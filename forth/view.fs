\ view.fs - the image viewer

\ A viewer is a record with its task's record at its end; a
\ window keeps its viewer in win-data.

create vw 0 ,           \ the viewer in use: its record

$2f constant VIEW-CELLS
#36 constant VTASK-AT   \ where its task's record starts

: bm     ( -- a ) vw @ ;          \ the bitmap: two cells
: bmh    ( -- a ) vw @ CELL2 + ;  \ rows of the picture made,
: vseen  ( -- a ) vw @ c# 3 + ;   \ and rows put out of date
: vwin   ( -- a ) vw @ c# 4 + ;   \ its window, once known
: vpart  ( -- a ) vw @ c# 5 + ;   \ its task's part of stacks
: vimg   ( -- a ) vw @ c# 6 + ;   \ its image, while read,
: vdth   ( -- a ) vw @ c# 7 + ;   \ and its dither
: vy0    ( -- a ) vw @ c# 8 + ;   \ the bitmap's first row,
: vkw    ( -- a ) vw @ c# 9 + ;   \ its width
: vkh    ( -- a ) vw @ c# 10 + ;  \ and height in pixels
: vhalf  ( -- a ) vw @ c# 11 + ;  \ true if at half size
: vmode  ( -- a ) vw @ c# 12 + ;  \ 0 dither, 1 Bayer, 2 blue
: vd     ( -- a ) vw @ c# 13 + ;  \ a sample's side, a shift
: vbr    ( -- a ) vw @ c# 14 + ;  \ added to the mask: darker
: vgray  ( -- a ) vw @ c# 15 + ;  \ the gray, if kept, or 0

: vmask  ( -- a ) vw @ c# 16 + ;  \ the mask
: vblk   ( -- a ) vw @ c# 17 + ;  \ the picture's first block
: vleft  ( -- a ) vw @ c# 18 + ;  \ the bitmap's first pixel
: vbusy  ( -- a ) vw @ c# 19 + ;  \ true while its task works
: vband  ( -- a ) vw @ c# 20 + ;  \ the band the gray draws in
: vrow   ( -- a ) vw @ c# 21 + ;  \ rows of samples kept
: varena ( -- a ) vw @ c# 22 + ;  \ the menu's memory: start,
: vtop   ( -- a ) vw @ c# 23 + ;  \ end,
: vmark  ( -- a ) vw @ c# 24 + ;  \ and where its gray ends
: vks    ( -- a ) vw @ c# 25 + ;  \ a kept sample's side
: vwhole ( -- a ) vw @ c# 26 + ;  \ true once all rows made
: vsaved ( -- a ) vw @ c# 27 + ;  \ bitmap rows on the disk
: vpw    ( -- a ) vw @ c# 28 + ;  \ the picture's width
: vph    ( -- a ) vw @ c# 29 + ;  \ and height
: vdrw   ( -- a ) vw @ c# 30 + ;  \ the gray's drawing

: vgpb    ( -- a ) vw @ c# 31 + ; \ the gray's paint block,
: vgin    ( -- a ) vw @ c# 32 + ; \ or 0; its first row;
: vgmk    ( -- a ) vw @ c# 33 + ; \ the block rows are made in
: vgsaved ( -- a ) vw @ c# 34 + ; \ rows of samples on disk;
: vgrpb   ( -- a ) vw @ c# 35 + ; \ and how many to a block
: vtask  ( -- a ) vw @ VTASK-AT + ;

create vparts 3 ,  \ the parts of the stacks free: a bit each

: retire ( -- ) \ the running task out of the ring
    up @ begin dup @ up @ <> while @ repeat
    up @ @ swap !
;

: made ( -- n ) \ rows of the bitmap made so far
    bmh @ vy0 @ -  d# 0 max  vkh @ min
;

: view-shown ( -- ) \ the rows made since are out of date
    made vseen @ > if
        vwin @ ?dup if
            >r  d# 0 vseen @  vkw @  made vseen @ -
            r> win-inval
        then
        made vseen !
    then
;

\ the viewer's memory from a up to vtop is freed: by setting
\ himem back if it was the last given out, and if not by hfree
: vback ( a -- )
    himem @ vtop @ = if dup himem ! vtop ! exit then
    vtop @ over - hfree
;

: vdone ( a -- ) \ the dither's memory from a, and the reader's
    himem @ vtop @ = >r  vback  vimg @ tiff-close
    r> if himem @ vtop ! then
;

\ is the menu's memory still the last given out?  If not, it
\ starts again at himem.
: vkept? ( -- f )
    varena @ if
        himem @ vtop @ = if true exit then
        varena @ vtop @ over - hfree
    then
    himem @ dup varena ! vtop !  d# 0 vgray !  false
;

\ ---- the bitmap on the disk

\ The blocks from VIEW-BLK, whole rows to a block.  bm says
\ where the rows would be were they all in memory.

create vondisk 0 ,  \ the viewer whose bitmap is on disk, or 0

: vcells ( -- n ) \ cells in the bitmap
    bm 1+ @ vkh @ * ;
: vrpb ( -- n ) \ rows in a block
    BLKCELLS bm 1+ @ u/mod nip ;
: vrows ( row -- n ) \ rows in the block that starts at this row
    vkh @ swap - vrpb min ;

: v-chunk ( row -- a n ) \ the block's rows in the bitmap
    dup bm 1+ @ * bm @ +
    swap vrows bm 1+ @ *
;

: vshow ( row b -- ) \ the block in bitmap b, to the window
    swap >r  d# 0 d# 0 d# 0 r@  vkw @  r> vrows  copy-bits
;

: vblock ( row -- blk ) \ the block a row of the bitmap is in
    vrpb u/mod nip VIEW-BLK + ;

\ the blocks the clip asks for, from row `top` of the bitmap; a
\ is a bitmap's record and then room for a block
: (view-show) ( top a -- )
    >r  r@ CELL2 + r@ !  bm 1+ @ r@ 1+ !      ( top ) ( R: a )
    clip-rows vsaved @ min  swap rot max      ( bottom top )
    2dup > 0= if 2drop rdrop exit then
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

: to-disk? ( -- f ) \ can the disk take the bitmap?
    'blk-read @ 0<>
    vondisk @ dup 0= swap vw @ = or  and
    vrpb VIEW-BLKS * vkh @ < 0=  and
;

: whole-bitmap ( -- ) \ memory for all of it
    vcells >c halloc bm ! ;

: vbitmap ( -- ) \ its memory: a block, or all of it
    vkw @ PXMASK + PXSHIFT rshift bm 1+ !  d# 0 vsaved !
    to-disk? if
        vw @ vondisk !  BLKCELLS >c halloc bm !  exit
    then
    whole-bitmap
;

: dither-begin ( -- )
    vblk @ tiff-open  dup vimg ! image!
    d# 0 bmh !  d# 0 vseen !
    bm vimg @ vleft @ vy0 @ vkw @ vkh @
    vhalf @ if dither-half else dither-part then  vdth !
    himem @ vtop !
;

: in-block ( -- a ) \ where the block being made is in memory
    vsaved @ v-chunk drop ;

\ the disk did not take a block: dithered again, into memory
: in-memory ( -- )
    d# 0 vondisk !
    vdth @ vdone  in-block BLKCELLS hgive
    d# 0 vsaved !  whole-bitmap  dither-begin
;

: block-out ( -- ) \ the block's rows are made: it is written
    in-block dup vsaved @ vblock ['] blk-write catch
    if 2drop drop in-memory exit then
    vsaved @ vrows vsaved +!
    vsaved @ bm 1+ @ * - bm !
;

: view-done ( -- ) \ the task's last turn
    view-shown  true vwhole !  vpart @ vparts or!  d# 0 vbusy !
    retire
;

: view-made ( -- ) \ the last row is made: its memory given back
    vimg @ if vdth @ vdone  d# 0 vimg !  d# 0 vdth ! then
    vondisk @ vw @ = if
        in-block BLKCELLS hgive  d# 0 bm !
    then
    view-done
;

: view-row ( -- ) \ one turn of a viewer's task
    up @ VTASK-AT - vw !
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

: view-spawn ( xt -- ) \ its task, in a free part of the stacks
    vparts @ d# 1 and if d# 1 else d# 2 then
    dup vpart !  dup vparts xor!
    d# 1 = if d# 96 else d# 64 then
    dup vtask spawn
    true vbusy !
;

\ the picture's size; at half size if it is bigger than w by h
: vsize ( w h img -- w h )
    dup image!  tw @ vpw !  th @ vph !  tiff-close
    over vpw @ <  over vph @ <  or vhalf !
    vhalf @ if d# 1 else d# 2 then vd !
;

: vkeep ( w h -- ) \ the middle of the picture that fits is kept
    pic-h min dup vkh !  pic-h swap - d# 1 rshift vy0 !
    pic-w min dup vkw !  pic-w swap - d# 1 rshift
    PXMASK invert and vleft !
;

header view-load
: view-load ( blk# w h -- viewer ) \ the TIFF at a block
    vparts @ 0= d# 8 -throw
    blk-need    \ before anything that is freed
    VIEW-CELLS >c halloc  dup vw !
    d# 0 swap VIEW-CELLS cells-fill
    rot dup vblk ! tiff-open  vsize  vkeep  vbitmap
    dither-begin  ['] view-row view-spawn
    vw @
;

\ ---- the gray on the disk

\ The blocks from GRAY-BLK, whole rows of samples to a block, a
\ row being its four plane rows.

: gs-cells ( -- n ) \ cells in a row of samples
    vgray @ 1+ @ d# 2 lshift ;
: gs-rows ( row -- n ) \ the rows in the block that starts at it
    vgray @ gray-rows swap - vgrpb @ min ;
: gs-blk ( row -- blk ) \ the block a row of samples is in
    vgrpb @ u/mod nip GRAY-BLK + ;
: gs-at ( a row -- ) \ the rows from `row` are in the block at a
    gs-cells * - vgray @ ! ;

: make-in ( -- ) \ the rows being made are in their block
    vgpb @ if vgmk @ vgsaved @ gs-at then ;

: gray-in ( sy -- ) \ row sy is where the gray says
    vgpb @ 0= if drop exit then
    dup vgsaved @ < 0= if drop make-in exit then
    dup vgin @ - vgrpb @ u< 0= if
        dup vgrpb @ u/mod drop -       ( its block's first row )
        vgpb @ over gs-blk blk-read  dup vgin !
    then
    drop  vgpb @ vgin @ gs-at
;

: gray-out ( -- ) \ the block's rows are made: it is written
    vgmk @ vgsaved @ gs-blk blk-write
    vgsaved @ gs-rows vgsaved +!
;

\ ---- painting the window

: dithered ( top -- ) \ the bitmap, from row top
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

\ the gray is drawn through the mask into a band, a bitmap
#16 constant BAND-ROWS

: band-at ( y -- ) \ the band holds the rows from y
    vband @ >r  r@ 1+ @ * negate  r@ CELL2 + +  r> ! ;

: gray-line ( y -- ) \ row y of the bitmap, drawn into the band
    dup vy0 @ + vd @ rshift gray-in  vdrw @ gray-draw-row ;

: band-draw ( y n -- ) \ n rows from y, no more than a bandful
    over band-at
    2dup bounds
    begin 2dup xor while dup gray-line 1+ repeat
    2drop
    >r >r  vband @ d# 0 r@  d# 0 r>  vkw @ r>  copy-bits
;

\ what is made of the gray, and the dithered rows under it
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

create t-picture ," Picture"
create t-atkinson ," Atkinson"
create t-ordered ," Ordered"
create t-blue ," Blue noise"
create t-finer ," Finer"
create t-coarser ," Coarser"
create t-darker ," Darker"
create t-lighter ," Lighter"

create pic-menu 0 , 0 , 0 , 0 , 0 ,
create pic-lines 0 , 0 , 0 , 0 , 0 , 0 , 0 ,

: pic-vw ( -- viewer|0 ) \ the front window's, in use
    top win-data @ dup vw ! ;
: pic-n ( -- n ) d# 7 ;
: pic-line ( i -- title ) pic-lines + @ ;
: pic-mark ( i -- f ) \ marks the way it is drawn now
    pic-vw if vmode @ = else drop false then ;

: pic-ks ( -- shift ) \ the picture's pixels a side of a sample
    vd @ vhalf @ if 1+ then ;

W32+? [if]
: mask-blue ( bright -- mask ) mask-blue64 ;
[else]
: mask-blue ( bright -- mask ) mask-blue32 ;
[then]

\ ---- what the task does

: pic-made ( -- ) \ the rows the samples kept so far make
    vrow @ vd @ lshift  vy0 @ vkh @ + min  bmh @ max bmh !
    view-shown
;

: pic-unread ( -- ) \ the reader freed, if outside the arena
    vimg @ ?dup if
        dup varena @ u<  over himem @ u< 0=
        or if tiff-close else drop then
    then
;

: pic-dither ( -- ) \ as it was dithered; the gray given back
    vkept? drop  varena @ himem !  d# 0 varena !  d# 0 vgray !
    vy0 @ vkh @ + bmh !  d# 0 vseen !  view-shown
;

: gray-lost ( -- ) \ the disk did not take a block of the gray
    pic-unread  d# 0 vimg !  d# 0 vmode !  pic-dither
    view-done
;

: make-free ( -- ) \ the block the gray's rows were made in
    vgmk @ ?dup if
        BLKCELLS hgive  d# 0 vgmk !  himem @ vtop !
    then
;

: block-kept ( -- f ) \ false if the disk did not take a block
    vgpb @ 0= if true exit then
    vrow @ vgsaved @ -  vgsaved @ gs-rows <> if true exit then
    ['] gray-out catch 0=
;

: pic-turn ( -- ) \ a turn of the task: a row of samples
    up @ VTASK-AT - vw !
    make-in  vgray @ gray-row  d# 1 vrow +!
    block-kept 0= if gray-lost exit then
    pic-made
    vrow @ vgray @ gray-rows < if exit then
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

: pic-mask ( -- ) \ the mask, a band, and a drawing into it
    vbr @
    vmode @ d# 1 = if mask-bayer else mask-blue then vmask !
    band-new dup vband !
    vmask @ vgray @ vd @ vleft @ vy0 @
    gray-draw-start vdrw !
    vy0 @ bmh !  d# 0 vseen !
;

\ the gray's record, and its planes in memory or on the disk
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

: pic-start ( -- ) \ the arena again, from its start
    varena @ himem !
    vpw @ vph @ pic-ks gray-plan  pic-ks vks !
    himem @ vmark !
    pic-mask
    vgpb @ if BLKCELLS >c halloc vgmk ! then
    vblk @ tiff-open  dup vimg !  vgray @ gray-open
    himem @ vtop !
;

\ the gray through the mask again, if it is kept as fine as this
: pic-again ( -- f )
    vkept? if vgray @ if vks @ pic-ks = if
        vmark @ himem !  pic-mask  himem @ vtop !
        vgray @ gray-rows vrow !  pic-made  true exit
    then then then
    false
;

: pic-gray ( -- ) \ through the mask, the gray made first
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

: pic-do ( i -- ) \ what line i asks, if that changes anything
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
    vbusy @  vparts @ 0= or if drop exit then
    vmode @ >r  vd @ >r  vbr @ >r
    ['] pic-do catch if     \ out of memory: as it was
        drop  r> vbr !  r> vd !  r> vmode !  d# 0 vgray !
        exit
    then
    rdrop rdrop rdrop
;

header view-menu
: view-menu ( win -- )
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

create t-mandrill ," mandrill.tiff"  \ the picture's label

header picture-blk
: picture-blk ( -- blk|0 ) t-mandrill >c count labelled drop ;
