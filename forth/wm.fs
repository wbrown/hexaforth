\ wm.fs - windows, the pointer, and the event loop

\ A window is fourteen cells: x, y, width, height, title (a
\ counted string), three routines, its port, its visible region,
\ a cell for its program's own use (win-data), its menu or 0
\ (win-menu), and two optional hooks, each a word or 0
\ (win-front, win-back).  The routines:
\   draw  ( win -- )      paint the content in the window's port
\   key   ( c win -- )    a key, when this window is in front
\   click ( x y win -- )  a click in the content, or 0 for none
\ and the two hooks:
\   front ( win -- )      the window has come to the front
\   back  ( win -- )      another window has come in front of it

\ `wins` holds the windows from back to front.  The front one
\ has the keyboard.

\ Nothing draws when something changes.  What has changed is
\ added to the region of the screen that is out of date
\ (inval-rect, win-inval), and the event loop then paints it.
\ Each window is painted only where it can be seen: its visible
\ region is its rectangle less the windows in front of it, and
\ what is painted of it is the part of that which is out of
\ date.  So no window is painted over another, and the order
\ they are painted in does not matter.  A key or a click puts
\ the whole of the front window out of date, unless the window's
\ routine said more exactly what changed.  The pointer, a
\ dragged window's outline and a window's overlay are drawn
\ straight onto the screen instead, and are taken off before
\ anything is painted and put back after.

create wins #64 allot
header nwin create nwin 0 ,
create cw 0 ,           \ the window being drawn

nearby
header top : top ( -- win ) nwin @ 1- cells wins + @ ;
nearby-end

\ A window is fourteen cells: position and size, title, the
\ words that draw it and take its keys and clicks, its port, its
\ visible region, a cell for its program's own use, the menu
\ shown in the bar while it is in front, and the hooks run when
\ it comes to the front and goes behind.
header win-data  : win-data  ( win -- a ) c# 10 + ;
header win-menu  : win-menu  ( win -- a ) c# 11 + ;
header win-front : win-front ( win -- a ) c# 12 + ;
header win-back  : win-back  ( win -- a ) c# 13 + ;

header window
: window ( x y w h title draw key click -- win )
    d# 14 >c halloc >r
    new-port r@ c# 8 + !
    new-rgn r@ c# 9 + !
    d# 0 r@ win-data !  d# 0 r@ win-menu !  d# 0 r@ win-front !
    d# 0 r@ win-back !
    r@ c# 7 + !  r@ c# 6 + !  r@ c# 5 + !  r@ c# 4 + !
    r@ c# 3 + !  r@ CELL2 + !  r@ CELL + !  r@ !
    r@ nwin @ cells wins + !
    d# 1 nwin +!
    r>
;

\ What is between `nearby` and `nearby-end` is laid in the
\ kernel's page, where a call from any page is one instruction:
\ every window's code, on the pages after this, says what of it
\ is out of date through these (cross.fs, nearby).

nearby
header content : content ( win -- x y )
    \ top left of the area inside the frame
    dup @ 1+ swap CELL + @ d# 19 +
;
nearby-end

: cw.x cw @ @ ;
: cw.y cw @ CELL + @ ;
: cw.w cw @ CELL2 + @ ;
: cw.h cw @ c# 3 + @ ;

\ ---- the pointer, drawn by inverting, so drawing it again
\ removes it

create mx 0 ,
create my 0 ,
create cur-on 0 ,
create arrow  \ twelve rows of eight pixels, a cell each
    $80 pcell, $c0 pcell, $e0 pcell, $f0 pcell, $f8 pcell,
    $fc pcell,
    $fe pcell, $f0 pcell, $d8 pcell, $18 pcell, $0c pcell,
    $0c pcell,

: cursor-xor ( -- )
    d# 0
    begin
        dup d# 12 <
    while
        dup arrow + @  over my @ +  mx @ swap  xor8
        1+
    repeat
    drop
;

header cursor-off : cursor-off  ( -- )
    cur-on @ if cursor-xor d# 0 cur-on ! then ;
header cursor-show : cursor-show ( -- )
    cur-on @ 0= if cursor-xor true cur-on ! then ;

\ ---- the outline of a window being dragged

\ While a window is dragged by its title, a gray outline of its
\ frame follows the pointer, as on the classic Macintosh, and
\ the window itself moves only when the button is released.
\ Like the pointer, the outline is drawn straight onto the
\ screen by inverting, so drawing it again removes it.  refresh
\ takes it off before painting and puts it back after, so a
\ window repainted under it does not leave pieces of it behind.

\ The outline is moved once a turn of the event loop
\ (drag-follow), to where the pointer is by then, not once for
\ every report the pointer sends.  QEMU's driver hands over
\ every report that is waiting in one call, and a pointer moved
\ quickly sends more reports than the outline can be redrawn
\ for; redrawing for each kept the event loop from coming round,
\ and the screen froze.

create dragging 0 ,  \ true while a window is being dragged by
                     \ its title
create dx0 0 ,       \ where the pointer was when the
                     \ drag started
create dy0 0 ,
create drag-on 0 ,   \ true while the outline is on the screen
create drag-x 0 ,    \ the outline's top left
create drag-y 0 ,

: drag-w ( -- w )
    \ the dragged window's size: it is the front window
    top CELL2 + @ ;
: drag-h ( -- h ) top c# 3 + @ ;

\ invert a one-pixel gray frame the size of the dragged window,
\ at drag-x drag-y
: frame-xor ( -- )
    pattern @ >r  'mode @ >r
    xor-mode  p-gray pattern !
    \ top
    drag-x @  drag-y @  drag-w  d# 1  pat-rect
    \ bottom
    drag-x @  drag-y @ drag-h + 1-  drag-w  d# 1  pat-rect
    \ left
    drag-x @  drag-y @ 1+  drag-h d# 2 -  xor-column
    \ right
    drag-x @ drag-w + 1-  drag-y @ 1+  drag-h d# 2 -
    xor-column
    r> 'mode !  r> pattern !
;

: drag-off ( -- ) drag-on @ if frame-xor d# 0 drag-on ! then ;

: drag-at ( -- x y )
    \ where the pointer has taken the dragged window's top left
    mx @ dx0 @ -  top @ +
    my @ dy0 @ -  top CELL + @ +
;

\ draw the outline where the pointer has taken the window, if a
\ window is being dragged
: drag-show ( -- )
    dragging @  drag-on @ 0=  and if
        drag-at drag-y ! drag-x !
        frame-xor  true drag-on !
    then
;

\ once a turn of the event loop: move the outline, if the
\ pointer has moved it
: drag-follow ( -- )
    dragging @ 0= if exit then
    drag-at  drag-y @ =  swap drag-x @ =  and  drag-on @ and
    if exit then
    drag-off drag-show
;

\ Where the pointer is, as a window is told it: in the front
\ window's content, as a click is, and that window.  Nothing
\ outside this file knows where the pointer is on the screen.
: pointer-in ( -- x y win )
    top >r  mx @ r@ @ - 1-  my @ r@ CELL + @ - d# 19 -  r> ;

\ A window may want to know that the pointer has moved, to show
\ something about what it is on.
header 'moved create 'moved 0 ,     \ ( x y win -- )

: set-mouse ( x y -- )
    cursor-off
    fb-h @ d# 12 - min d# 0 max my !
    fb-w @ d# 8 - min d# 0 max mx !
    cursor-show
    'moved @ ?dup if >r pointer-in r> execute then
;

\ ---- painting

create t-menu ," hexaforth"

\ Each title in the bar is a menu.  The first lists the windows,
\ the front one first; after it comes the front window's own
\ menu, if it has one.  Pressing the button on a title drops its
\ menu.  Releasing on a line picks it; releasing on the title
\ leaves the menu open, and the next press picks a line or,
\ anywhere else, closes it.  So it works by holding and
\ dragging, or by two clicks.

\ A menu is five cells: its title (a counted string), and words
\ that give its number of lines ( -- n ), a line's text ( i --
\ title ), the action for a picked line ( i -- ), and whether a
\ line is marked ( i -- f ), or 0 if no line is ever marked.

\ While it is down it is in front of every window.  It is
\ counted with what covers them, so that none of them paints or
\ scrolls where it is, and it is painted after them where it is
\ out of date.
header menu-open \ the menu that is open, or 0
    create menu-open 0 ,
create menu-held 0 ,  \ true while the button that brought it
                      \ down is still down
create menu-hot -1 ,  \ the line of it the pointer is on, or -1
create menu-w 0 ,     \ how wide it is,
create menu-x 0 ,     \ and where its title starts in the bar
create menu-tw 0 ,    \ the width of the windows' title in
                      \ the bar,
create menu-tw2 0 ,   \ and of the front window's title after it

nearby
: menu-n ( -- n ) \ lines in the open menu
    menu-open @ 1+ @ execute ;
: menu-box ( -- x y w h )
    menu-x @ d# 4 +  d# 19  menu-w @
    menu-n d# 4 lshift d# 2 + ;
nearby-end

\ ---- what is out of date

create dirty 0 , 0 , 0 , 0 ,  \ the rectangle around it: left,
                              \ top, and past the right and
                              \ bottom; empty if no wider than 0
create urect 0 , 0 , 0 , 0 ,  \ the same, for what is
                              \ being painted
create auto 0 ,               \ true if the front window is to
                              \ be put out of date

create rd 0 ,      \ the region that is out of date
create ru 0 ,      \ the region being painted
create rpaint 0 ,  \ what is being painted of one window, or of
                   \ the desk
create rdesk 0 ,   \ the desk that can be seen
create rcov 0 ,    \ what the windows in front cover
create rtmp 0 ,
create rwin 0 ,    \ a rectangle

header wm-regions
: wm-regions ( -- )
    \ allocate the window manager's regions, the first time only
    rd @ if exit then
    new-rgn rd !  new-rgn ru !  new-rgn rpaint !
    new-rgn rdesk !
    new-rgn rcov !  new-rgn rtmp !  new-rgn rwin !
;

nearby
: dirty-rgn ( rgn -- )
    \ add it to the region that is out of date
    rd @ swap rtmp @ rgn-or
    rd @ rtmp @ rd ! rtmp !
;

: dirty-box ( x y w h -- ) \ and to the rectangle around that
    >r >r
    dirty CELL2 + @ dirty @ > if  \ as well as what is there
        over dirty @ min dirty !
        dup dirty CELL + @ min dirty CELL + !
        swap r> + dirty CELL2 + @ max dirty CELL2 + !
        r> + dirty c# 3 + @ max dirty c# 3 + !
    else
        over dirty !
        dup dirty CELL + !
        swap r> + dirty CELL2 + !
        r> + dirty c# 3 + !
    then
;

header inval-rect : inval-rect ( x y w h -- ) \ on the screen
    rd @ if 2over 2over rwin @ rect-rgn  rwin @ dirty-rgn then
    dirty-box
;

header inval-rgn : inval-rgn ( rgn -- ) \ on the screen
    dup rgn-empty? if drop exit then
    dup dirty-rgn
    rgn-box dirty-box
;
nearby-end

header inval-window : inval-window ( win -- ) \ frame and all
    >r r@ @ r@ CELL + @ r@ CELL2 + @ r> c# 3 + @ inval-rect
;

: uclip ( -- ) \ clip to what is being painted
    urect @  urect CELL + @  urect CELL2 + @  urect c# 3 + @
    clip-screen!
;

: touches? ( win -- f ) \ is any of it being painted?
    >r
    r@ @ urect CELL2 + @ <
    r@ @ r@ CELL2 + @ + urect @ > and
    r@ CELL + @ urect c# 3 + @ < and
    r@ CELL + @ r> c# 3 + @ + urect CELL + @ > and
;

: draw-frame ( front? -- ) \ frame and title of cw
    true  cw.x cw.y cw.w cw.h fill-rect
    false cw.x cw.y cw.w cw.h frame-rect
    false cw.x cw.y d# 18 + cw.w hline
    dup if
        false cw.x 1+ cw.y 1+ cw.w d# 2 - d# 17 fill-rect
    then
    inverse !
    sans font !
    cw @ c# 4 + @ ctext  cw.x d# 8 + cw.y d# 2 + draw-chars
    plain-font font !
    d# 0 inverse !
;

: draw-window ( i -- ) \ what is in rpaint of it
    dup cells wins + @ cw !
    uclip  rpaint @ rgn-clip!
    1+ nwin @ = draw-frame
    cw @ c# 8 + @ port!   \ the window's own port:
    cw @ content origin!  \ its content, and no more
    uclip  d# 0 d# 0 cw.w d# 2 - cw.h d# 20 - clip-to
    rpaint @ rgn-clip!
    true text-or !        \ it has just been made white
    cw @ dup c# 5 + @ execute
    screen-port @ port!
;

create sysmenu 0 , 0 , 0 , 0 , 0 ,  \ the windows' menu (filled
                                    \ in by sysmenu!)

: top-menu ( -- menu|0 )
    nwin @ if top win-menu @ else d# 0 then ;

create bt 0 , 0 ,  \ the title being drawn: its x in the bar,
                   \ and its width

: bar-title ( menu x -- w )
    \ draw a menu's title in the bar, inverted while it is open
    bt !  dup >r  @ ctext chars-width d# 16 + bt 1+ !
    r@ menu-open @ = if
        false  bt @ d# 4 +  d# 1  bt 1+ @ d# 8 -  d# 17
        fill-rect
        true inverse !
    then
    r> @ ctext  bt @ d# 8 +  d# 2  draw-chars
    d# 0 inverse !
    bt 1+ @
;

\ The disk lights, at the right of the bar: a floppy (the font
\ spin) for each disk there is, disk 0's at the right and each
\ disk after it two glyphs to the left of the one before, which
\ are its arrow and its floppy.  The disk the last block moved
\ was of (blocks.fs says which, from the block's number) has its
\ spindle turned a quarter each time the event loop sees that
\ blocks have moved, with an up arrow to its left after a read
\ and a down arrow after a write.  The arrow stays until the
\ event loop has gone round eight times with no blocks moved.
\ There is no clock, so a turn of the loop is however long the
\ tasks run between pauses.

\ disk-corner is how wide one disk's light is, its arrow and its
\ floppy, and how tall: disk d's starts d and one more of those
\ from the right of the bar.  The desktop's checks read it from
\ here (test/deskcheck.c), to leave the lights out of their
\ comparisons.
create disk-corner
    #28 , #19 ,
create disk-seen 0 ,  \ blk-moved when the event loop
                      \ last looked
create disk-held 0 ,  \ minus the turns the arrow is still shown
                      \ for, or 0
create disk-turn 0 ,  \ which of the floppy's glyphs is shown

meta
:: IN-USE-TURNS  #-8 literal ;
target

: disk-x ( d -- x )
    \ left edge of disk d's light: the arrow, then the floppy
    1+ disk-corner @ *  fb-w @ swap - ;

\ the disks that have a light: those known to be there, and
\ disk 0 if there is a driver at all
: disks-lit ( -- mask )
    disks-there @  'blk-read @ if d# 1 or then ;

: lights ( -- n ) \ one more than the last disk that has one
    d# 0 disks-lit
    \ as nwords counts (see.fs): the mask on R while the count
    \ is stepped, and the shift takes it from there
    dup if begin >r 1+ r> u2/  dup 0= until then
    drop
;
: disk-inval ( -- ) \ all of them
    lights 1- disk-x  d# 0
    lights disk-corner @ *  disk-corner 1+ @  inval-rect ;

\ called each turn of the event loop: mark the light for repaint
\ if it changed
: disk-light ( -- )
    blk-moved @ dup disk-seen @ <> if
        disk-seen !  IN-USE-TURNS disk-held !
        disk-turn @ 1+ d# 3 and  disk-turn !
        disk-inval  exit
    then
    drop
    disk-held @ if
        d# 1 disk-held +!  disk-held @ 0= if disk-inval then
    then
;

\ disk d's light: its floppy, turned if it is the disk in use,
\ and the arrow while it is
: draw-light ( d -- )
    dup blk-disk @ = >r
    r@ if disk-turn @ else d# 0 then
    over disk-x d# 10 +  d# 2  draw-char
    r> disk-held @ and if
        blk-wrote @ if d# 5 else d# 4 then
        over disk-x  d# 2  draw-char
    then
    drop
;

: draw-disk ( -- ) \ the light of each disk there is
    spin font !
    d# 0 disks-lit                          ( d mask )
    begin
        dup
    while
        dup d# 1 and if over draw-light then
        u2/ swap 1+ swap
    repeat
    2drop
;

: draw-desk ( -- ) \ menu bar, then grey
    true d# 0 d# 0 fb-w @ d# 19 fill-rect
    false d# 0 d# 19 fb-w @ hline
    sans font !
    sysmenu d# 0 bar-title menu-tw !
    top-menu ?dup if menu-tw @ bar-title else d# 0 then
    menu-tw2 !
    draw-disk
    plain-font font !
    p-gray pattern !
    d# 0 d# 20 fb-w @ fb-h @ d# 20 - pat-rect
;

: hot-line ( i -- )
    \ inverse, if it is the line the pointer is on
    dup menu-hot @ = if
        false  menu-x @ d# 5 +  rot d# 4 lshift d# 20 +
        menu-w @ d# 2 -  d# 16  fill-rect
        true inverse !  exit
    then
    drop
;

: line-mark ( i -- ) \ a mark before line i, if it is marked
    menu-open @ c# 4 + @ ?dup 0= if drop exit then
    over swap execute 0= if drop exit then
    inverse @  menu-x @ d# 6 +  rot d# 4 lshift d# 26 +
    d# 4 d# 4  fill-rect
;

: menu-row ( i -- ) \ line i of the open menu
    dup hot-line  dup line-mark  >r
    r@ menu-open @ CELL2 + @ execute ctext
    menu-x @ d# 12 +  r> d# 4 lshift d# 20 +  draw-chars
    d# 0 inverse !
;

\ draw the open menu's lines, with a mark by any that are marked
: draw-menu ( -- )
    true menu-box fill-rect  false menu-box frame-rect
    sans font !
    d# 0 begin dup menu-n < while dup menu-row 1+ repeat drop
    plain-font font !
;

\ What can be seen of each window changes only when a window is
\ made, moved or brought to the front, and is worked out again
\ only then.  Which windows there are, in what order, and where
\ each is, are summed, and the sum is kept: it is a few
\ instructions a window to see that nothing has changed, whoever
\ might have changed it.
create vsum -1 ,
nearby
: v+ ( sum x -- sum' ) swap d# 31 * + ;

: win-sum ( sum win -- sum' ) \ which it is, and where
    >r  r@ v+  r@ @ v+  r@ CELL + @ v+  r@ CELL2 + @ v+
    r> c# 3 + @ v+
;

: layout ( -- sum )
    nwin @ menu-open @ + >r
    ( R: the sum so far: the menu covers them too )
    wins nwin @ bounds
    begin
        2dup xor
    while
        dup @ r> swap win-sum >r
        1+
    repeat
    2drop r>
;

\ what can be seen of a window: its rectangle less what covers
\ it; and it then covers what is behind it too
: covered ( win -- )
    >r
    r@ @ r@ CELL + @ r@ CELL2 + @ r@ c# 3 + @
    rwin @ rect-rgn
    rwin @ rcov @ r> c# 9 + @ rgn-diff
    rcov @ rwin @ rtmp @ rgn-or
    rcov @ rtmp @ rcov ! rtmp !
;

: visible ( -- )
    \ work out what can be seen of each window, and of the desk
    layout dup vsum @ = if drop exit then
    vsum !
    menu-open @ if
        menu-box rcov @ rect-rgn
    else
        rcov @ rgn-none
    then
    nwin @ begin dup while 1- dup cells wins + @ covered repeat
    drop
    d# 0 d# 0 fb-w @ fb-h @ rwin @ rect-rgn
    rwin @ rcov @ rdesk @ rgn-diff
;
nearby-end

\ A window may draw over its content straight onto the screen,
\ as the pointer and a dragged window's outline are drawn over
\ everything: the Words window's box.  It stores a word in
\ 'overlay, which is told false before anything is painted, to
\ take off what it drew, and true after, to put it back.  It
\ draws in its port with win-port, which shows only where the
\ window can be seen.
header 'overlay create 'overlay 0 ,     \ ( on? -- )

: overlay ( on? -- )
    'overlay @ ?dup if execute else drop then ;
header overlays-off : overlays-off ( -- )
    cursor-off drag-off false overlay ;
header overlays-on  : overlays-on  ( -- )
    true overlay drag-show cursor-show ;

\ draw in its content where it can be seen, until screen-port
\ @ port!
header win-port : win-port ( win -- )
    visible  dup cw !
    dup c# 8 + @ port!  dup content origin!
    d# 0 d# 0 cw.w d# 2 - cw.h d# 20 - clip!
    c# 9 + @ rgn-clip!
;

\ What a window says has changed is out of date only where the
\ window can be seen.  Where another window lies over it there
\ is nothing of this one to paint, and to put that out of date
\ would have the one on top painted again for nothing.

nearby
header win-inval : win-inval ( x y w h win -- )
    \ in a window's content
    >r 2swap r@ content
    \ on the screen
    rot + >r + r> 2swap                             ( x y w h )
    d# 0 auto !
    rd @ 0= if rdrop inval-rect exit then
    visible
    rwin @ rect-rgn
    \ rcov is free once visible is done
    rwin @  r> c# 9 + @  rcov @ rgn-and
    rcov @ inval-rgn
;
nearby-end

\ What is out of date, cut to the screen, is what is painted
\ now, and nothing is out of date after it.
: take-dirty ( -- )
    dirty @ d# 0 max urect !
    dirty CELL + @ d# 0 max urect CELL + !
    dirty CELL2 + @ fb-w @ min urect CELL2 + !
    dirty c# 3 + @ fb-h @ min urect c# 3 + !
    d# 0 dirty !  d# 0 dirty CELL2 + !
;

: paint-desk ( -- ) \ where the desk is out of date
    ru @ rdesk @ rpaint @ rgn-and
    rpaint @ rgn-empty? 0= if
        uclip rpaint @ rgn-clip! draw-desk
    then
;

: paint-windows ( -- ) \ where each window is out of date
    d# 0
    begin
        dup nwin @ <
    while
        dup cells wins + @ touches? if
            ru @ over cells wins + @ c# 9 + @ rpaint @ rgn-and
            rpaint @ rgn-empty? 0= if dup draw-window then
        then
        1+
    repeat
    drop
;

: paint-menu ( -- ) \ the menu, over all of them
    menu-open @ 0= if exit then
    menu-box rwin @ rect-rgn
    ru @ rwin @ rpaint @ rgn-and
    rpaint @ rgn-empty? 0= if
        uclip rpaint @ rgn-clip! draw-menu
    then
;

header refresh : refresh ( -- ) \ paint what is out of date
    dirty CELL2 + @ dirty @ > 0= if exit then
    take-dirty
    rd @ ru @ rd ! ru !  \ paint this; start another
    rd @ rgn-none
    visible
    overlays-off         \ while the clip is still the
                         \ whole screen
    paint-desk  paint-windows  paint-menu
    noclip
    overlays-on
;

: repaint ( -- ) d# 0 d# 0 fb-w @ fb-h @ inval-rect ;
: redraw-top ( -- ) top inval-window ;

\ ---- which window, and bringing one to the front

: inside? ( win -- f ) \ is the pointer in it?
    dup @ mx @ swap - over CELL2 + @ u<
    swap dup CELL + @ my @ swap - swap c# 3 + @ u<
    and
;

: win-at ( -- i )
    \ the frontmost window under the pointer, or -1
    nwin @
    begin
        dup
    while
        1- dup cells wins + @ inside? if exit then
    repeat
    1-
;

: raise ( i -- )
    dup cells wins + @ >r
    begin
        dup 1+ nwin @ <
    while
        dup 1+ cells wins + @ over cells wins + !
        1+
    repeat
    cells wins + r> swap !
;

\ A program can be told when its window comes to the front and
\ when another window comes in front of it, for instance to load
\ something into memory only while it is in front.  It stores a
\ word in the window's win-front or win-back hook; the word is
\ called with the window.
: told ( win a -- ) \ call a window's hook, if it has one
    @ ?dup if execute else drop then ;

\ bring window i to the front: repaint it and the old front
\ window, and call their hooks
: forward ( i -- )
    drag-off                           \ the outline is the old
                                       \ front window's size
    top >r  redraw-top raise redraw-top
    d# 0 d# 0 fb-w @ d# 19 inval-rect  \ and the bar, which
                                       \ shows the front
                                       \ window's menu
    r@ dup win-back told  rdrop
    top dup win-front told
;

\ ---- the menu: bringing it down, and what the button does
\ while it is

: menu-inval ( -- ) \ the menu and the bar
    menu-box inval-rect  d# 0 d# 0 fb-w @ d# 19 inval-rect ;

: menu-line ( -- i ) \ the line of it the pointer is on, or -1
    mx @ menu-x @ - d# 4 - menu-w @ u<
    my @ d# 20 - menu-n d# 4 lshift u<  and
    if my @ d# 20 - d# 4 rshift else d# -1 then
;

: title-at ( -- x menu | 0 )
    \ the bar title under the pointer: its x and its menu, or 0
    my @ d# 19 < 0= if d# 0 exit then
    mx @ menu-tw @ < if d# 0 sysmenu exit then
    mx @ menu-tw @ - menu-tw2 @ u< if
        top-menu ?dup if menu-tw @ swap exit then
    then
    d# 0
;

: on-title? ( -- f ) \ is the pointer on the open menu's title?
    title-at ?dup if nip menu-open @ = else false then ;

: menu-width ( -- n ) \ of the open menu's longest line
    sans font !
    d# 0 d# 0
    begin
        dup menu-n <
    while
        dup menu-open @ CELL2 + @ execute ctext chars-width
        rot max swap
        1+
    repeat
    drop
    plain-font font !
;

: menu-down ( x menu -- )
    \ open a menu, as wide as its longest line
    menu-open !  menu-x !
    menu-width d# 24 + menu-w !
    true menu-held !  d# -1 menu-hot !
    menu-inval
;

: menu-up ( -- )
    menu-inval  d# 0 menu-open !  d# 0 menu-held ! ;

: menu-pick ( i -- )
    \ close the menu and run its action for line i
    menu-open @ >r  menu-up  r> c# 3 + @ execute
;

\ The windows' menu: a line per window, the front one first;
\ picking one brings it to the front.
: wins-n ( -- n ) nwin @ ;
: wins-line ( i -- title )
    nwin @ 1- swap - cells wins + @ c# 4 + @ ;
: wins-pick ( i -- )
    nwin @ 1- swap -  ( where it is among the windows )
    dup 1+ nwin @ <> if forward else drop then
;
: sysmenu! ( -- )
    t-menu sysmenu !  ['] wins-n sysmenu 1+ !
    ['] wins-line sysmenu CELL2 + !
    ['] wins-pick sysmenu c# 3 + !
;

\ the pointer has moved: the line it is on is the one shown so
: menu-track ( -- )
    menu-open @ if
        menu-line dup menu-hot @ <> if
            menu-hot ! menu-inval
        else
            drop
        then
    then
;

\ ---- the mouse button: the menu, or front a window, drag its
\ title, or click in it

\ A window is told of the button going down in it (its click
\ routine).  One that wants to follow the pointer after that, to
\ draw with it, stores a word in 'track then: it is run with
\ where the pointer is, in the window as a click is, each time
\ the pointer has moved while the button is down, and once more
\ when the button goes up, when `held` is no longer true.
header held create held 0 ,      \ true while the button is down
header 'track create 'track 0 ,  \ ( x y win -- )


\ the pointer has moved with the button down, or the button has
\ gone up
: track ( -- )
    'track @ ?dup if >r pointer-in r> execute then
;

\ The button down on window i: it comes to the front, unless it
\ is there already; on its title it is dragged, and in its
\ content it is clicked.

: press-window ( i -- )
    dup 1+ nwin @ <> if forward else drop then
    top cw !
    my @ cw.y - d# 19 < if
        true dragging !  mx @ dx0 !  my @ dy0 !  drag-show
        exit
    then
    top c# 7 + @ ?dup if
        >r
        true auto !
        pointer-in
        r> execute
        auto @ if redraw-top then
    then
;

: press ( -- )
    \ a line of it, or away with it
    menu-open @ if
        menu-line dup 0< if drop menu-up else menu-pick then
        exit
    then
    title-at ?dup if menu-down exit then
    true held !  d# 0 'track !
    win-at dup 0< if drop exit then
    press-window
;

\ The button that brought the menu down is let go: on a line, it
\ is picked; on the title, the menu stays; anywhere else
\ it goes.
: release-menu ( -- )
    d# 0 menu-held !
    menu-line dup 0< if
        drop  on-title? 0= if menu-up then
    else
        menu-pick
    then
;

\ A window dragged is let go: what was where it was and what is
\ where it is are out of date.
: release-drag ( -- )
    drag-off  d# 0 dragging !
    redraw-top
    mx @ dx0 @ - top +!
    my @ dy0 @ - top CELL + +!
    redraw-top
;

: release ( -- )
    menu-held @ if release-menu exit then
    \ whoever was following the pointer is told it is over
    d# 0 held !  track  d# 0 'track !
    dragging @ if release-drag then
;

\ ---- keys

create shift 0 ,
create ctrl 0 ,
create quitting 0 ,

: on-key ( c -- )
    \ a key takes the menu away, and is then a key
    menu-open @ if menu-up then
    \ ^Q: leave the desktop
    dup d# 17 = if drop true quitting ! exit then
    \ ^W: next window
    dup d# 23 = if
        drop
        nwin @ d# 1 > if d# 0 forward then
        exit
    then
    true auto !
    top dup c# 6 + @ execute
    auto @ if redraw-top then
;

: shift-key? ( code -- f ) dup d# 42 = swap d# 54 = or ;
: ctrl-key?  ( code -- f ) dup d# 29 = swap d# 97 = or ;

\ ---- events from the input devices

\ A pointer says where it is in 32,768ths of the screen.  That
\ times the screen's size is more than a cell of 16 bits holds,
\ and there it is multiplied into two cells.
W32+? [if]
: scaled ( v n -- x ) * d# 15 rshift ;
[else]
: scaled ( v n -- x ) um* d# 1 lshift swap d# 15 rshift or ;
[then]

: pointer-event ( code value -- ) \ where it is, an axis of it
    swap if
        fb-h @ scaled mx @ swap set-mouse
    else
        fb-w @ scaled my @ set-mouse
    then
    menu-track
;

\ A key or the button, down or up.  The keys are characters
\ packed into cells, as text is: the keys, and after them the
\ keys shifted.
: key-event ( code value -- )
    over h# 110 = if nip if press else release then exit then
    over shift-key? if nip 0<> shift ! exit then
    over ctrl-key?  if nip 0<> ctrl ! exit then
    0= if drop exit then            \ a key going up
    dup d# 127 u> if drop exit then
    shift @ if d# 128 + then
    d# 3 lshift  keymap swap char@
    ctrl @ if h# 1f and then
    ?dup if on-key then
;

\ A device says where the pointer is an axis at a time, and then
\ that it has said all there is to say (an event of type 0):
\ only then is the pointer somewhere it has been, and whoever
\ follows it is told.  Of the rest, only keys are taken.
header on-event : on-event ( type code value -- )
    rot
    dup d# 3 = if drop pointer-event exit then
    dup 0= if drop 2drop  held @ if track then  exit then
    d# 1 <> if 2drop exit then
    key-event
;

\ Keys and pointer events come from whatever the machine has,
\ which is asked for them each time round: a driver stores the
\ word that asks in 'poll, and hands what it has to on-event.  A
\ machine with nothing but its console has the console's keys.
header 'poll create 'poll 0 ,       \ ( -- )

header uart-poll : uart-poll ( -- ) key? if key on-key then ;

header event-loop : event-loop ( -- )
    d# 0 quitting !
    'poll @ 0= if ['] uart-poll 'poll ! then
    sysmenu!
    wm-regions
    repaint
    begin
        'poll @ execute
        drag-follow
        disk-light
        refresh
        pause  \ the other tasks' turn
        quitting @
    until
;
