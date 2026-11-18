\ viewmenu.fs - the picture's menu: how the picture is drawn

\ The menu is in the bar while the picture's window is in front.
\ It chooses how the picture is drawn: dithered (Atkinson), or
\ kept as gray and drawn through a mask (gray.fs), Bayer's or
\ blue noise from the disk.  Finer and Coarser change how many
\ of the picture's pixels a gray sample covers; Darker and
\ Lighter move the mask's thresholds a level.  These only matter
\ when drawing through a mask.

\ The viewer's task does the work, a part per turn, into the
\ viewer's bitmap, over what was there.  When keeping the gray,
\ each turn reads and keeps one row of samples and draws the
\ bitmap rows it makes at once, so the picture comes down the
\ window as it does when dithered.  Darker, Lighter and
\ switching masks redraw from the kept gray, sixteen rows a
\ turn, without reading the picture.

\ The memory used is the viewer's arena (view.fs), all allocated
\ when a line is picked: the gray, then the mask and drawing,
\ then what making the gray needs, which is freed after the last
\ row.  Each pick starts the arena again from its start, so
\ making one picture after another takes no more memory;
\ dithering again frees it all except the bitmap.  If there is
\ not enough memory for the gray as fine as asked, it is kept
\ coarser.  The gray, mask and drawing are per-viewer records,
\ so viewers can work at the same time; a pick while the
\ viewer's task is busy does nothing.

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

\ draw up to n more bitmap rows, as far as the samples kept so
\ far allow
: pic-rows ( n -- )
    begin
        dup  made vkh @ < and  bmh @ vd @ rshift vrow @ < and
    while
        made vdrw @ gray-draw-row  d# 1 bmh +!  1-
    repeat
    drop  view-shown
;

: pic-turn ( -- )
    \ one turn of a viewer's task doing the menu's work
    up @ VTASK-AT - vw !
    vstate @ d# 1 = if  \ making the gray: keep a row of
                        \ samples, and draw what it allows
        vgray @ gray-row  d# 1 vrow +!  d# 16 pic-rows
        vrow @ vgray @ gray-rows < if exit then
        \ free what making it used, if nothing was
        \ allocated since
        himem @ vtop @ = if vgray @ gray-dither vdone then
        d# 0 vimg !  d# 2 vstate !  exit
    then
    d# 16 pic-rows
    made vkh @ < if exit then
    view-done
;

\ ---- what a pick does

\ load the mask and start a drawing, after what is allocated,
\ from the bitmap's first row
: pic-mask ( -- )
    vbr @
    vmode @ d# 1 = if mask-bayer else mask-blue then vmask !
    bm vmask @ vgray @ vd @ vleft @ vy0 @
    gray-draw-start vdrw !
    vy0 @ bmh !  d# 0 vseen !
;

\ from the start of the arena: allocate the gray, the mask, and
\ what making the gray needs
: pic-start ( -- )
    varena @ himem !
    vpw @ vph @ pic-ks gray-new vgray !  pic-ks vks !
    himem @ vmark !
    pic-mask
    vblk @ tiff-open  dup vimg !  vgray @ gray-open
    himem @ vtop !
;

: pic-go ( state -- ) vstate !  ['] pic-turn view-spawn ;

\ The gray drawn through the mask again, if it is still kept at
\ this fineness: true if it is.
: pic-again ( -- f )
    vkept? if vgray @ if vks @ pic-ks = if
        vmark @ himem !  pic-mask  himem @ vtop !
        vgray @ gray-rows vrow !  d# 2 pic-go  true exit
    then then then
    false
;

\ The reader freed, if it came from freed memory outside the
\ arena.
: pic-unread ( -- )
    vimg @ ?dup if
        dup varena @ u<  over himem @ u< 0=
        or if tiff-close else drop then
    then
;

\ draw the picture through the mask, making the gray first
\ unless it is already kept at this fineness
: pic-gray ( -- )
    view-in             \ the gray is drawn into the bitmap
    pic-again if exit then
    begin               \ as fine as memory allows
        d# 0 vimg !
        ['] pic-start catch
    while
        pic-unread
        vd @ d# 2 < 0= if d# -8 throw then
        d# 1 vd +!
    repeat
    d# 0 vrow !  d# 1 pic-go
;

: pic-dither ( -- ) \ dither the picture again
    view-in  \ the dither writes into the bitmap
    vkept? drop  varena @ himem !  d# 0 vgray !
    view-dither
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
