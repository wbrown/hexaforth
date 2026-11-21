\ wm.fs - windows, the pointer, and the event loop

\ A window is x, y, width, height, its title (a counted string),
\   draw  ( win -- )      its content, in its port
\   key   ( c win -- )    a key, when it is in front
\   click ( x y win -- )  a click in its content, or 0
\ its port, its visible region, and the four cells below.

meta #16 constant max-wins target
meta max-wins target constant MAX-WINS

\ the open windows, from back to front; and every window made,
\ in the order they were made, for the menu
create wins  meta max-wins cell-bytes * target allot
create awins meta max-wins cell-bytes * target allot
header nwin create nwin 0 ,
header nawin create nawin 0 ,   \ how many were made
create cw 0 ,           \ the window being drawn

header no-windows
: no-windows ( -- ) d# 0 nwin !  d# 0 nawin ! ;

nearby
header top : top ( -- win|0 ) \ the front window
    nwin @ if nwin @ 1- cells wins + @ else d# 0 then ;
nearby-end

header win-data  : win-data  ( win -- a ) c# 10 + ;  \ its own
header win-menu  : win-menu  ( win -- a ) c# 11 + ;  \ or 0
\ ( win -- ), or 0: it has come to the front, or left it
header win-front : win-front ( win -- a ) c# 12 + ;
header win-back  : win-back  ( win -- a ) c# 13 + ;

header window
: window ( x y w h title draw key click -- win )
    nawin @ MAX-WINS = d# 8 -throw
    d# 14 >c halloc >r
    new-port r@ c# 8 + !
    new-rgn r@ c# 9 + !
    d# 0 r@ win-data !  d# 0 r@ win-menu !  d# 0 r@ win-front !
    d# 0 r@ win-back !
    r@ c# 7 + !  r@ c# 6 + !  r@ c# 5 + !  r@ c# 4 + !
    r@ c# 3 + !  r@ CELL2 + !  r@ CELL + !  r@ !
    r@ nwin @ cells wins + !  d# 1 nwin +!
    r@ nawin @ cells awins + !  d# 1 nawin +!
    r>
;

nearby
header content : content ( win -- x y ) \ inside the frame
    dup @ 1+ swap CELL + @ d# 19 +
;
nearby-end

: cw.x cw @ @ ;
: cw.y cw @ CELL + @ ;
: cw.w cw @ CELL2 + @ ;
: cw.h cw @ c# 3 + @ ;

\ ---- the pointer, drawn by inverting

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

\ ---- the outline of the front window while it is dragged

create dragging 0 ,  \ true while it is dragged by its title
create dx0 0 ,       \ where the pointer was when that started
create dy0 0 ,
create drag-on 0 ,   \ true while the outline is on the screen
create drag-x 0 ,    \ the outline's top left
create drag-y 0 ,

: drag-w ( -- w ) top CELL2 + @ ;
: drag-h ( -- h ) top c# 3 + @ ;

: frame-xor ( -- ) \ a gray frame inverted, at drag-x drag-y
    pattern @ >r  'mode @ >r
    xor-mode  p-gray pattern !
    drag-x @  drag-y @  drag-w  d# 1  pat-rect
    drag-x @  drag-y @ drag-h + 1-  drag-w  d# 1  pat-rect
    drag-x @  drag-y @ 1+  drag-h d# 2 -  xor-column
    drag-x @ drag-w + 1-  drag-y @ 1+  drag-h d# 2 -
    xor-column
    r> 'mode !  r> pattern !
;

: drag-off ( -- ) drag-on @ if frame-xor d# 0 drag-on ! then ;

: drag-at ( -- x y ) \ where the pointer has taken its top left
    mx @ dx0 @ -  top @ +
    my @ dy0 @ -  top CELL + @ +
;

: drag-show ( -- )
    dragging @  drag-on @ 0=  and if
        drag-at drag-y ! drag-x !
        frame-xor  true drag-on !
    then
;

: drag-follow ( -- ) \ once each pass of the event loop
    dragging @ 0= if exit then
    drag-at  drag-y @ =  swap drag-x @ =  and  drag-on @ and
    if exit then
    drag-off drag-show
;

: pointer-in ( -- x y win ) \ in the front window's content
    top >r  mx @ r@ @ - 1-  my @ r@ CELL + @ - d# 19 -  r> ;

\ ( x y win -- ), or 0: run when the pointer has moved
header 'moved create 'moved 0 ,

: set-mouse ( x y -- )
    cursor-off
    fb-h @ d# 12 - min d# 0 max my !
    fb-w @ d# 8 - min d# 0 max mx !
    cursor-show
    'moved @ ?dup if >r pointer-in r> execute then
;

\ ---- painting

create t-menu ," hexaforth"

\ A menu is its title (a counted string), its lines ( -- n ), a
\ line's text and whether it is bold ( i -- title bold? ), what
\ picking one does ( i -- ), and whether a line is marked
\ ( i -- f ), or 0.
header menu-open \ the menu that is open, or 0
    create menu-open 0 ,
create menu-held 0 ,  \ true while its button is still down
create menu-hot -1 ,  \ the line of it the pointer is on, or -1
create menu-w 0 ,     \ how wide it is,
create menu-x 0 ,     \ and where its title starts in the bar
create menu-tw 0 ,    \ the width of the windows' title,
create menu-tw2 0 ,   \ and of the front window's title after it

nearby
: menu-n ( -- n ) \ lines in the open menu
    menu-open @ 1+ @ execute ;
: menu-box ( -- x y w h )
    menu-x @ d# 4 +  d# 19  menu-w @
    menu-n d# 4 lshift d# 2 + ;
nearby-end

\ ---- what is out of date

\ the rectangle around it: left, top, past the right and bottom
create dirty 0 , 0 , 0 , 0 ,
create urect 0 , 0 , 0 , 0 ,  \ and around what is being painted
create auto 0 ,    \ true if a key redraws the front window

create rd 0 ,      \ the region that is out of date
create ru 0 ,      \ the region being painted
create rpaint 0 ,  \ what of it one window has, or the desk
create rdesk 0 ,   \ the desk that can be seen
create rcov 0 ,    \ what the windows in front cover
create rtmp 0 ,
create rwin 0 ,    \ a rectangle

header wm-regions
: wm-regions ( -- )
    rd @ if exit then
    new-rgn rd !  new-rgn ru !  new-rgn rpaint !
    new-rgn rdesk !
    new-rgn rcov !  new-rgn rtmp !  new-rgn rwin !
;

nearby
: dirty-rgn ( rgn -- ) \ out of date as well
    rd @ swap rtmp @ rgn-or
    rd @ rtmp @ rd ! rtmp !
;

: dirty-box ( x y w h -- ) \ and the rectangle around it
    >r >r
    dirty CELL2 + @ dirty @ > if
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
    [char] x  cw.x cw.w + d# 12 -  cw.y d# 2 +  draw-char
    plain-font font !
    d# 0 inverse !
;

: draw-window ( i -- ) \ what is in rpaint of it
    dup cells wins + @ cw !
    uclip  rpaint @ rgn-clip!
    1+ nwin @ = draw-frame
    cw @ c# 8 + @ port!
    cw @ content origin!
    uclip  d# 0 d# 0 cw.w d# 2 - cw.h d# 20 - clip-to
    rpaint @ rgn-clip!
    true text-or !        \ it has just been made white
    cw @ dup c# 5 + @ execute
    screen-port @ port!
;

create sysmenu 0 , 0 , 0 , 0 , 0 ,  \ the windows' menu

: top-menu ( -- menu|0 )
    nwin @ if top win-menu @ else d# 0 then ;

create bt 0 , 0 ,  \ the title being drawn: its x, and its width

: bar-title ( menu x -- w ) \ inverted while the menu is open
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

\ ---- the disk lights, at the right of the bar: a floppy a
\ disk, and an arrow by the one in use, up for a read

\ one light's width and height (test/deskcheck.c reads it here)
create disk-corner
    #28 , #19 ,
create disk-seen 0 ,  \ blk-moved when the loop last looked
create disk-held 0 ,  \ minus the passes the arrow has, or 0
create disk-turn 0 ,  \ which of the floppy's glyphs is shown

meta
:: IN-USE-TURNS  #-8 literal ;
target

: disk-x ( d -- x ) \ left edge of disk d's light
    1+ disk-corner @ *  fb-w @ swap - ;

: disks-lit ( -- mask ) \ the disks that have a light
    disks-there @  'blk-read @ if d# 1 or then ;

: lights ( -- n ) \ one more than the last disk that has one
    d# 0 disks-lit
    dup if begin >r 1+ r> u2/  dup 0= until then
    drop
;
: disk-inval ( -- ) \ all of them
    lights 1- disk-x  d# 0
    lights disk-corner @ *  disk-corner 1+ @  inval-rect ;

: disk-light ( -- ) \ once each pass of the event loop
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

: hot-line ( i -- ) \ inverse, if the pointer is on it
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
    r@ menu-open @ CELL2 + @ execute bold ! ctext
    menu-x @ d# 12 +  r> d# 4 lshift d# 20 +  draw-chars
    d# 0 bold !  d# 0 inverse !
;

: draw-menu ( -- )
    true menu-box fill-rect  false menu-box frame-rect
    sans font !
    d# 0 begin dup menu-n < while dup menu-row 1+ repeat drop
    plain-font font !
;

\ of which windows there are, in what order, and where each is
create vsum -1 ,
nearby
: v+ ( sum x -- sum' ) swap d# 31 * + ;

: win-sum ( sum win -- sum' )
    >r  r@ v+  r@ @ v+  r@ CELL + @ v+  r@ CELL2 + @ v+
    r> c# 3 + @ v+
;

: layout ( -- sum )
    nwin @ menu-open @ + >r  ( R: the sum so far )
    wins nwin @ bounds
    begin
        2dup xor
    while
        dup @ r> swap win-sum >r
        1+
    repeat
    2drop r>
;

\ its visible region is its rectangle less what covers it
: covered ( win -- )
    >r
    r@ @ r@ CELL + @ r@ CELL2 + @ r@ c# 3 + @
    rwin @ rect-rgn
    rwin @ rcov @ r> c# 9 + @ rgn-diff
    rcov @ rwin @ rtmp @ rgn-or
    rcov @ rtmp @ rcov ! rtmp !
;

: visible ( -- ) \ of each window and the desk, if vsum changed
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

\ ( on? -- ), or 0: takes what a window drew straight onto the
\ screen off before anything is painted, and puts it back after
header 'overlay create 'overlay 0 ,

: overlay ( on? -- )
    'overlay @ ?dup if execute else drop then ;
header overlays-off : overlays-off ( -- )
    cursor-off drag-off false overlay ;
header overlays-on  : overlays-on  ( -- )
    true overlay drag-show cursor-show ;

\ its content where it can be seen, until screen-port @ port!
header win-port : win-port ( win -- )
    visible  dup cw !
    dup c# 8 + @ port!  dup content origin!
    d# 0 d# 0 cw.w d# 2 - cw.h d# 20 - clip!
    c# 9 + @ rgn-clip!
;

nearby
header win-inval : win-inval ( x y w h win -- ) \ in its content
    >r 2swap r@ content
    rot + >r + r> 2swap                             ( x y w h )
    d# 0 auto !
    rd @ 0= if rdrop inval-rect exit then
    visible
    rwin @ rect-rgn
    rwin @  r> c# 9 + @  rcov @ rgn-and
    rcov @ inval-rgn
;
nearby-end

: take-dirty ( -- ) \ what is out of date is what is painted now
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
    rd @ ru @ rd ! ru !
    rd @ rgn-none
    visible
    overlays-off         \ while the clip is the whole screen
    paint-desk  paint-windows  paint-menu
    noclip
    overlays-on
;

: repaint ( -- ) d# 0 d# 0 fb-w @ fb-h @ inval-rect ;
: redraw-top ( -- ) top ?dup if inval-window then ;

\ ---- which window, and bringing one to the front or closing it

: inside? ( win -- f ) \ is the pointer in it?
    dup @ mx @ swap - over CELL2 + @ u<
    swap dup CELL + @ my @ swap - swap c# 3 + @ u<
    and
;

: close-box? ( win -- f ) \ is the pointer on its title's x?
    dup CELL + @ my @ swap - d# 19 u<
    swap dup @ swap CELL2 + @ + mx @ - d# 13 u< and
;

: win-i ( win -- i ) \ where it is in wins, or -1 if closed
    nwin @
    begin
        dup
    while
        1-  2dup cells wins + @ = if nip exit then
    repeat
    1- nip
;

: win-at ( -- i ) \ the frontmost under the pointer, or -1
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

header held create held 0 ,      \ true while the button is down
\ ( x y win -- ), or 0: what a click stores to be run as the
\ pointer moves, and once more when the button has gone up
header 'track create 'track 0 ,
create closing 0 ,  \ the window whose x was pressed, or 0

: track ( -- )
    'track @ ?dup if >r pointer-in r> execute then
;

: end-press ( -- ) \ before the front window changes
    drag-off  d# 0 dragging !  d# 0 closing !
    d# 0 held !  track  d# 0 'track !
;

: told ( win a -- ) \ a window's hook, if it has one
    @ ?dup if execute else drop then ;

\ the new front window and the bar are out of date, and the
\ old front window and the new are told
: front-changed ( old -- )
    redraw-top
    d# 0 d# 0 fb-w @ d# 19 inval-rect  \ the bar has its menu
    ?dup if dup win-back told then
    top ?dup if dup win-front told then
;

: forward ( i -- ) \ window i to the front
    end-press
    top >r  redraw-top raise  r> front-changed
;

: close-at ( i -- ) \ window i out of wins
    end-press
    top >r
    dup cells wins + @ dup inval-window c# 9 + @ rgn-none
    d# -1 nwin +!
    begin
        dup nwin @ <
    while
        dup 1+ cells wins + @ over cells wins + !
        1+
    repeat
    drop
    r> dup top <> if front-changed else drop then
;

\ ---- the menu

: menu-inval ( -- ) \ the menu and the bar
    menu-box inval-rect  d# 0 d# 0 fb-w @ d# 19 inval-rect ;

: menu-line ( -- i ) \ the line of it the pointer is on, or -1
    mx @ menu-x @ - d# 4 - menu-w @ u<
    my @ d# 20 - menu-n d# 4 lshift u<  and
    if my @ d# 20 - d# 4 rshift else d# -1 then
;

: title-at ( -- x menu | 0 ) \ the bar title under the pointer
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
        dup menu-open @ CELL2 + @ execute drop
        ctext chars-width  rot max swap
        1+
    repeat
    drop
    plain-font font !
;

: menu-down ( x menu -- )
    menu-open !  menu-x !
    menu-width d# 24 + menu-w !
    true menu-held !  d# -1 menu-hot !
    menu-inval
;

: menu-up ( -- )
    menu-inval  d# 0 menu-open !  d# 0 menu-held ! ;

: menu-pick ( i -- )
    menu-open @ >r  menu-up  r> c# 3 + @ execute
;

\ the windows' menu: a line a window, the last made first, bold
\ if it is open.  Picking one brings it to the front, and opens
\ it if it was closed.
: menu-win ( i -- win ) nawin @ 1- swap - cells awins + @ ;
: wins-n ( -- n ) nawin @ ;
: wins-line ( i -- title bold? )
    menu-win dup c# 4 + @  swap win-i 0< 0= ;
: reopen ( win -- ) \ a closed window, at the front
    end-press
    top >r  redraw-top
    nwin @ cells wins + !  d# 1 nwin +!
    r> front-changed
;

: wins-pick ( i -- )
    menu-win dup win-i
    dup 0< if drop reopen exit then
    nip
    dup 1+ nwin @ <> if forward else drop then
;
: sysmenu! ( -- )
    t-menu sysmenu !  ['] wins-n sysmenu 1+ !
    ['] wins-line sysmenu CELL2 + !
    ['] wins-pick sysmenu c# 3 + !
;

: menu-track ( -- ) \ the pointer has moved
    menu-open @ if
        menu-line dup menu-hot @ <> if
            menu-hot ! menu-inval
        else
            drop
        then
    then
;

\ ---- the mouse button

: press-window ( i -- ) \ to the front; dragged, or clicked
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
    menu-open @ if
        menu-line dup 0< if drop menu-up else menu-pick then
        exit
    then
    title-at ?dup if menu-down exit then
    win-at dup 0< if drop exit then
    dup cells wins + @ dup close-box? if
        nip closing ! exit
    then
    drop
    true held !  d# 0 'track !
    press-window
;

: release-menu ( -- ) \ on a line, its title, or elsewhere
    d# 0 menu-held !
    menu-line dup 0< if
        drop  on-title? 0= if menu-up then
    else
        menu-pick
    then
;

\ the window whose x was pressed is closed, if the pointer is
\ still on its x and it is the frontmost window there
: release-close ( win -- )
    d# 0 closing !
    dup close-box? 0= if drop exit then
    win-at dup 0< if 2drop exit then
    tuck cells wins + @ = if close-at else drop then
;

: release-drag ( -- )
    drag-off  d# 0 dragging !
    redraw-top
    mx @ dx0 @ - top +!
    my @ dy0 @ - top CELL + +!
    redraw-top
;

: release ( -- )
    menu-held @ if release-menu exit then
    closing @ ?dup if release-close exit then
    d# 0 held !  track  d# 0 'track !
    dragging @ if release-drag then
;

\ ---- keys

create shift 0 ,
create ctrl 0 ,
create quitting 0 ,

\ ^Q closes the front window, or with none open leaves the
\ desktop; ^W is the next window
: on-key ( c -- )
    menu-open @ if menu-up then
    dup d# 17 = if
        drop
        nwin @ if nwin @ 1- close-at else true quitting ! then
        exit
    then
    nwin @ 0= if drop exit then
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

\ A pointer says where it is in 32,768ths of the screen.
W32+? [if]
: scaled ( v n -- x ) * d# 15 rshift ;
[else]
: scaled ( v n -- x ) um* d# 1 lshift swap d# 15 rshift or ;
[then]

: pointer-event ( code value -- ) \ an axis of where it is
    swap if
        fb-h @ scaled mx @ swap set-mouse
    else
        fb-w @ scaled my @ set-mouse
    then
    menu-track
;

: key-event ( code value -- ) \ a key or the button, down or up
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

\ type 3: an axis of the pointer; 0: its report ends; 1: a key
header on-event : on-event ( type code value -- )
    rot
    dup d# 3 = if drop pointer-event exit then
    dup 0= if drop 2drop  held @ if track then  exit then
    d# 1 <> if 2drop exit then
    key-event
;

\ ( -- ): asks a driver's devices, which hand on-event what they
\ have, each pass of the event loop
header 'poll create 'poll 0 ,

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
        pause
        quitting @
    until
;
