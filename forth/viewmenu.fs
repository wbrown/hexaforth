\ viewmenu.fs - the picture's menu: how the picture is drawn
\
\ While the picture's window is in front its menu is in the bar.  The
\ picture is dithered (Atkinson), or kept as gray and drawn through a mask
\ (gray.fs): Bayer's, or blue noise from the disk.  Finer and Coarser are
\ how many of the picture's pixels a sample of the gray is, and Darker and
\ Lighter a level more or less on the mask's thresholds; they change the
\ picture when it is drawn through a mask.
\
\ The work is done by the viewer's task, a part of it at each turn, into
\ the bitmap the viewer has, over what was there.  Kept as gray, a row of
\ samples is read and kept at a turn, and the rows of the bitmap it makes
\ are drawn at once: so the picture comes down the window as it does when
\ it is dithered.  Darker, Lighter, and the other mask draw the gray that
\ is kept again, sixteen rows at a turn, and do not read the picture.
\
\ The memory the work has is the viewer's (view.fs), all had when a line is
\ picked: the gray, then the mask and what drawing has, then what keeping
\ the gray has, which is given back when the last row is kept.  It is had
\ again from where it started each time, so that one picture made after
\ another takes no more, and is given back when the picture is dithered
\ again, which keeps nothing but the bitmap.  If there is not enough for
\ the gray as fine as it is asked for, it is kept coarser.  What is in hand
\ while the gray is kept or drawn is gray.fs's and atkinson.fs's, in
\ variables, so one viewer at a time has it: a pick while another's is at
\ work does nothing, as does one while the viewer's own task is.

create t-picture ," Picture"
create t-atkinson ," Atkinson"
create t-ordered ," Ordered"
create t-blue ," Blue noise"
create t-finer ," Finer"
create t-coarser ," Coarser"
create t-darker ," Darker"
create t-lighter ," Lighter"

create pic-menu 0 , 0 , 0 , 0 , 0 ,     \ the menu (made by view-menu)
create pic-lines 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
create pic-busy 0 ,     \ the viewer whose picture is being kept or drawn as gray, or 0

: pic-vw ( -- viewer|0 ) top win-data @ dup vw ! ;      \ the viewer whose menu it is, in hand
: pic-n ( -- n ) d# 7 ;
: pic-line ( i -- title ) pic-lines + @ ;
: pic-mark ( i -- f ) pic-vw if vmode @ = else drop false then ;   \ how it is drawn

: pic-ks ( -- shift ) vd @ vhalf @ if 1+ then ;     \ pixels of the picture a side to a sample, as a shift

W32+? [if]
: mask-blue ( -- mask ) mask-blue64 ;
[else]
: mask-blue ( -- mask ) mask-blue32 ;               \ a quarter of the 64 by 64, where memory is less
[then]

\ ---- what the task does

: pic-rows ( n -- ) \ as many as n rows more of the bitmap, of those whose samples are kept
    begin
        dup  made vkh @ < and  bmh @ vd @ rshift vrow @ < and
    while
        made gray-draw-row  d# 1 bmh +!  1-
    repeat
    drop  view-shown
;

: pic-turn ( -- ) \ what a viewer's task does at each of its turns, for its menu
    up @ VTASK-AT - vw !
    vstate @ d# 1 = if                              \ keeping the gray: a row of samples, and what it draws
        vgray @ gray-row  d# 1 vrow +!  d# 16 pic-rows
        vrow @ grows @ < if exit then
        himem @ vtop @ = if gdth @ vdone then       \ what keeping it had, given back if nothing has been had since
        d# 0 vimg !  d# 2 vstate !  exit
    then
    d# 16 pic-rows
    made vkh @ < if exit then
    d# 0 pic-busy !  view-done
;

\ ---- what a pick does

: pic-mask ( -- ) \ the mask, and what drawing has, after what is there: and to draw from the bitmap's first row
    vbr @ gbright !
    vmode @ d# 1 = if mask-bayer else mask-blue then vmask !
    bm vmask @ vgray @ vd @ vleft @ vy0 @ gray-draw-start
    vy0 @ bmh !  d# 0 vseen !
;

: pic-start ( -- ) \ from the start of the menu's memory: the gray, the mask, and what keeping the gray has
    varena @ himem !
    vpw @ vph @ pic-ks gray-new vgray !  pic-ks vks !
    himem @ vmark !
    pic-mask
    vblk @ tiff-open  dup vimg !  vgray @ gray-open
    himem @ vtop !
;

: pic-go ( state -- ) vstate !  vw @ pic-busy !  ['] pic-turn view-spawn ;

: pic-gray ( -- ) \ the picture to be drawn through the mask: the gray kept, unless it is kept already as it is to be
    vkept? if vgray @ if vks @ pic-ks = if
        vmark @ himem !  pic-mask  himem @ vtop !
        vgray @ gry !  grows @ vrow !  d# 2 pic-go  exit
    then then then
    begin                                           \ as finely as there is memory for
        d# 0 vimg !
        ['] pic-start catch
    while
        vimg @ ?dup if                              \ what reading had, if it was had where memory was
            dup varena @ u<  over himem @ u< 0=  or if tiff-close else drop then    \ given back
        then
        vd @ d# 2 < 0= if d# -8 throw then
        d# 1 vd +!
    repeat
    d# 0 vrow !  d# 1 pic-go
;

: pic-dither ( -- ) \ the picture to be dithered again
    vkept? drop  varena @ himem !  d# 0 vgray !
    view-dither
;

: pic-do ( i -- ) \ what line i asks, if it changes what is shown
    dup d# 3 < if
        dup vmode @ = if drop exit then
        dup vmode !  if pic-gray else pic-dither then
        exit
    then
    dup d# 5 < if
        d# 3 = if d# -1 else d# 1 then  vd @ +  d# 0 max  d# 2 min
        dup vd @ = if drop exit then
        vd !
    else
        d# 5 = if d# 1 else d# -1 then  vbr @ +  d# -15 max  d# 15 min
        dup vbr @ = if drop exit then
        vbr !
    then
    vmode @ if pic-gray then
;

: pic-pick ( i -- )
    pic-vw 0= if drop exit then
    vbusy @  pic-busy @ or  vparts @ 0= or if drop exit then    \ one thing at a time
    vmode @ >r  vd @ >r  vbr @ >r
    ['] pic-do catch if                             \ there was no memory for it: it is as it was
        drop  r> vbr !  r> vd !  r> vmode !  d# 0 vgray !
        exit
    then
    rdrop rdrop rdrop
;

header view-menu
: view-menu ( win -- ) \ the picture's menu, in the bar while its window is in front
    t-atkinson pic-lines !  t-ordered pic-lines 1+ !  t-blue pic-lines CELL2 + !
    t-finer pic-lines c# 3 + !  t-coarser pic-lines c# 4 + !
    t-darker pic-lines c# 5 + !  t-lighter pic-lines c# 6 + !
    t-picture pic-menu !  ['] pic-n pic-menu 1+ !  ['] pic-line pic-menu CELL2 + !
    ['] pic-pick pic-menu c# 3 + !  ['] pic-mark pic-menu c# 4 + !
    pic-menu swap win-menu !
;
