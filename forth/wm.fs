\ wm.fs - windows, the pointer, and the event loop
\
\ A window is fourteen cells: x, y, width, height, title (a counted
\ string), three routines, its port, its visible region, a cell for its
\ program's own use (win-data), its menu or 0 (win-menu), and two optional
\ hooks, each a word or 0 (win-front, win-back).  The routines:
\     draw   ( win -- )         paint the content, in the window's port
\     key    ( c win -- )       a key, when this window is in front
\     click  ( x y win -- )     a click in the content, or 0 for none
\ and the two hooks:
\     front  ( win -- )         the window has come to the front
\     back   ( win -- )         another window has come in front of it
\
\ `awins` holds every window that was made, in that order, for the menu.
\ `wins` holds the ones that are open, from back to front.  The front one
\ has the keyboard.
\
\ Nothing draws when something changes.  What has changed is added to
\ the region of the screen that is out of date (inval-rect, win-inval),
\ and the event loop then paints it.  Each window is painted only where it
\ can be seen: its visible region is its rectangle less the windows in
\ front of it, and what is painted of it is the part of that which is out
\ of date.  So no window is painted over another, and the order they are
\ painted in does not matter.  A key or a click
\ puts the whole of the front window out of date, unless the window's
\ routine said more exactly what changed.

meta #16 constant max-wins target
meta max-wins target constant MAX-WINS

create wins  meta max-wins cell-bytes * target allot
create awins meta max-wins cell-bytes * target allot
header nwin create nwin 0 ,
create nawin 0 ,
create cw 0 ,           \ the window being drawn

header no-windows : no-windows ( -- ) d# 0 nwin !  d# 0 nawin ! ;
header top : top ( -- win|0 ) nwin @ if nwin @ 1- cells wins + @ else d# 0 then ;

\ A window is fourteen cells: position and size, title, the words that
\ draw it and take its keys and clicks, its port, its visible region, a
\ cell for its program's own use, the menu shown in the bar while it is in
\ front, and the hooks run when it comes to the front and goes behind.
header win-data  : win-data  ( win -- a ) c# 10 + ;
header win-menu  : win-menu  ( win -- a ) c# 11 + ;
header win-front : win-front ( win -- a ) c# 12 + ;
header win-back  : win-back  ( win -- a ) c# 13 + ;

header window
: window ( x y w h title draw key click -- win )
    nawin @ MAX-WINS = d# 8 -throw
    d# 14 >c halloc >r
    new-port r@ c# 8 + !
    new-rgn r@ c# 9 + !
    d# 0 r@ win-data !  d# 0 r@ win-menu !  d# 0 r@ win-front !  d# 0 r@ win-back !
    r@ c# 7 + !  r@ c# 6 + !  r@ c# 5 + !  r@ c# 4 + !
    r@ c# 3 + !  r@ CELL2 + !  r@ CELL + !  r@ !
    r@ nwin @ cells wins + !  d# 1 nwin +!
    r@ nawin @ cells awins + !  d# 1 nawin +!
    r>
;

header content : content ( win -- x y ) \ top left of the area inside the frame
    dup @ 1+ swap CELL + @ d# 19 +
;

: cw.x cw @ @ ;
: cw.y cw @ CELL + @ ;
: cw.w cw @ CELL2 + @ ;
: cw.h cw @ c# 3 + @ ;

\ ---- the pointer, drawn by inverting, so drawing it again removes it

create mx 0 ,
create my 0 ,
create cur-on 0 ,
create arrow            \ twelve rows of eight pixels, a cell each
    $80 pcell, $c0 pcell, $e0 pcell, $f0 pcell, $f8 pcell, $fc pcell,
    $fe pcell, $f0 pcell, $d8 pcell, $18 pcell, $0c pcell, $0c pcell,

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

header cursor-off : cursor-off  ( -- ) cur-on @ if cursor-xor d# 0 cur-on ! then ;
header cursor-show : cursor-show ( -- ) cur-on @ 0= if cursor-xor true cur-on ! then ;

\ ---- the outline of a window being dragged
\
\ While a window is dragged by its title, a gray outline of its frame
\ follows the pointer, as on the classic Macintosh, and the window itself
\ moves only when the button is released.  Like the pointer, the outline
\ is drawn straight onto the screen by inverting, so drawing it again
\ removes it.  refresh takes it off before painting and puts it back after,
\ so a window repainted under it does not leave pieces of it behind.
\
\ The outline is moved once a turn of the event loop (drag-follow), to
\ where the pointer is by then, not once for every report the pointer
\ sends.  QEMU's driver hands over every report that is waiting in one
\ call, and a pointer moved quickly sends more reports than the outline
\ can be redrawn for; redrawing for each kept the event loop from coming
\ round, and the screen froze.

create dragging 0 ,     \ true while a window is being dragged by its title
create dx0 0 ,          \ where the pointer was when the drag started
create dy0 0 ,
create drag-on 0 ,      \ true while the outline is on the screen
create drag-x 0 ,       \ the outline's top left
create drag-y 0 ,

: drag-w ( -- w ) top CELL2 + @ ;                   \ the dragged window's size: it is the front window
: drag-h ( -- h ) top c# 3 + @ ;

: frame-xor ( -- ) \ invert a one-pixel gray frame the size of the dragged window, at drag-x drag-y
    pattern @ >r  'mode @ >r
    xor-mode  p-gray pattern !
    drag-x @  drag-y @  drag-w  d# 1  pat-rect                         \ top
    drag-x @  drag-y @ drag-h + 1-  drag-w  d# 1  pat-rect             \ bottom
    drag-x @  drag-y @ 1+  drag-h d# 2 -  xor-column                  \ left
    drag-x @ drag-w + 1-  drag-y @ 1+  drag-h d# 2 -  xor-column      \ right
    r> 'mode !  r> pattern !
;

: drag-off ( -- ) drag-on @ if frame-xor d# 0 drag-on ! then ;

: drag-at ( -- x y ) \ where the pointer has taken the dragged window's top left
    mx @ dx0 @ -  top @ +
    my @ dy0 @ -  top CELL + @ +
;

: drag-show ( -- ) \ draw the outline where the pointer has taken the window, if a window is being dragged
    dragging @  drag-on @ 0=  and if
        drag-at drag-y ! drag-x !
        frame-xor  true drag-on !
    then
;

: drag-follow ( -- ) \ once a turn of the event loop: move the outline, if the pointer has moved it
    dragging @ 0= if exit then
    drag-at  drag-y @ =  swap drag-x @ =  and  drag-on @ and  if exit then
    drag-off drag-show
;

\ A window may want to know that the pointer has moved, to show something
\ about what it is on.
header 'moved create 'moved 0 ,     \ ( -- )

: set-mouse ( x y -- )
    cursor-off
    fb-h @ d# 12 - min d# 0 max my !
    fb-w @ d# 8 - min d# 0 max mx !
    cursor-show
    'moved @ ?dup if execute then
;

\ ---- painting

create t-menu ," hexaforth"

\ Each title in the bar is a menu.  The first lists the windows, the front
\ one first; after it comes the front window's own menu, if it has one.
\ Pressing the button on a title drops its menu.  Releasing on a line
\ picks it; releasing on the title leaves the menu open, and the next
\ press picks a line or, anywhere else, closes it.  So it works by holding
\ and dragging, or by two clicks.
\
\ A menu is five cells: its title (a counted string), and words that give
\ its number of lines ( -- n ), a line's text and whether it is in bold
\ ( i -- title bold? ), the action for a picked line ( i -- ), and whether
\ a line is marked ( i -- f ), or 0 if no line is ever marked.
\
\ While it is down it is in front of every window.  It is counted with
\ what covers them, so that none of them paints or scrolls where it is,
\ and it is painted after them where it is out of date.
header menu-open create menu-open 0 ,   \ the menu that is open, or 0
create menu-held 0 ,    \ true while the button that brought it down is still down
create menu-hot -1 ,    \ the line of it the pointer is on, or -1
create menu-w 0 ,       \ how wide it is,
create menu-x 0 ,       \ and where its title starts in the bar
create menu-tw 0 ,      \ the width of the windows' title in the bar,
create menu-tw2 0 ,     \ and of the front window's title after it

: menu-n ( -- n ) menu-open @ 1+ @ execute ;    \ lines in the open menu
: menu-box ( -- x y w h ) menu-x @ d# 4 +  d# 19  menu-w @  menu-n d# 4 lshift d# 2 + ;

\ ---- what is out of date

create dirty 0 , 0 , 0 , 0 ,    \ the rectangle around it: left, top, and past the right
                                \ and bottom; empty if no wider than 0
create urect 0 , 0 , 0 , 0 ,    \ the same, for what is being painted
create auto 0 ,                 \ true if the front window is to be put out of date

create rd 0 ,           \ the region that is out of date
create ru 0 ,           \ the region being painted
create rpaint 0 ,       \ what is being painted of one window, or of the desk
create rdesk 0 ,        \ the desk that can be seen
create rcov 0 ,         \ what the windows in front cover
create rtmp 0 ,
create rwin 0 ,         \ a rectangle

header wm-regions
: wm-regions ( -- ) \ allocate the window manager's regions, the first time only
    rd @ if exit then
    new-rgn rd !  new-rgn ru !  new-rgn rpaint !  new-rgn rdesk !
    new-rgn rcov !  new-rgn rtmp !  new-rgn rwin !
;

: dirty-rgn ( rgn -- ) \ add it to the region that is out of date
    rd @ swap rtmp @ rgn-or
    rd @ rtmp @ rd ! rtmp !
;

: dirty-box ( x y w h -- ) \ and to the rectangle around that
    >r >r
    dirty CELL2 + @ dirty @ > if                    \ as well as what is there
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

header inval-window : inval-window ( win -- ) \ frame and all
    >r r@ @ r@ CELL + @ r@ CELL2 + @ r> c# 3 + @ inval-rect
;

: uclip ( -- ) \ clip to what is being painted
    urect @  urect CELL + @  urect CELL2 + @  urect c# 3 + @  clip-screen!
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
    dup if false cw.x 1+ cw.y 1+ cw.w d# 2 - d# 17 fill-rect then
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
    cw @ c# 8 + @ port!                             \ the window's own port:
    cw @ content origin!                            \ its content, and no more
    uclip  d# 0 d# 0 cw.w d# 2 - cw.h d# 20 - clip-to
    rpaint @ rgn-clip!
    true text-or !                                  \ it has just been made white
    cw @ dup c# 5 + @ execute
    screen-port @ port!
;

create sysmenu 0 , 0 , 0 , 0 , 0 ,      \ the windows' menu (filled in by sysmenu!)

: top-menu ( -- menu|0 ) nwin @ if top win-menu @ else d# 0 then ;

create bt 0 , 0 ,       \ the title being drawn: its x in the bar, and its width

: bar-title ( menu x -- w ) \ draw a menu's title in the bar, inverted while it is open
    bt !  dup >r  @ ctext chars-width d# 16 + bt 1+ !
    r@ menu-open @ = if
        false  bt @ d# 4 +  d# 1  bt 1+ @ d# 8 -  d# 17  fill-rect
        true inverse !
    then
    r> @ ctext  bt @ d# 8 +  d# 2  draw-chars
    d# 0 inverse !
    bt 1+ @
;

\ The disk light, at the right of the bar: a floppy (the font spin) whose
\ spindle turns a quarter each time the event loop sees that blocks have
\ moved (blocks.fs counts them), with an up arrow to its left after a read
\ and a down arrow after a write.  The arrow stays until the event loop has
\ gone round eight times with no blocks moved.  There is no clock, so a
\ turn of the loop is however long the tasks run between pauses.
\
\ disk-corner is how far from the right of the bar the light starts, and
\ how tall it is.  The desktop's checks read it from here
\ (test/deskcheck.c), to leave the light out of their comparisons.
create disk-corner
    #28 , #19 ,
create disk-seen 0 ,    \ blk-moved when the event loop last looked
create disk-held 0 ,    \ minus the turns the arrow is still shown for, or 0
create disk-turn 0 ,    \ which of the floppy's glyphs is shown

meta
:: IN-USE-TURNS  #-8 literal ;
target

: disk-x ( -- x ) fb-w @ disk-corner @ - ;          \ left edge of the light: the arrow, then the floppy
: disk-inval ( -- ) disk-x d# 0 disk-corner @ disk-corner 1+ @ inval-rect ;

: disk-light ( -- ) \ called each turn of the event loop: mark the light for repaint if it changed
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

: draw-disk ( -- ) \ draw the floppy, and the arrow while the disk is in use
    'blk-read @ 0= if exit then
    spin font !
    disk-turn @  disk-x d# 10 +  d# 2  draw-char
    disk-held @ if
        blk-wrote @ if d# 5 else d# 4 then  disk-x  d# 2  draw-char
    then
;

: draw-desk ( -- ) \ menu bar, then grey
    true d# 0 d# 0 fb-w @ d# 19 fill-rect
    false d# 0 d# 19 fb-w @ hline
    sans font !
    sysmenu d# 0 bar-title menu-tw !
    top-menu ?dup if menu-tw @ bar-title else d# 0 then menu-tw2 !
    draw-disk
    plain-font font !
    p-gray pattern !
    d# 0 d# 20 fb-w @ fb-h @ d# 20 - pat-rect
;

: draw-menu ( -- ) \ draw the open menu's lines, with a mark by any that are marked
    true menu-box fill-rect  false menu-box frame-rect
    sans font !
    d# 0
    begin
        dup menu-n <
    while
        >r                                          ( R: the line )
        r@ menu-hot @ = if                          \ the one the pointer is on
            false  menu-x @ d# 5 +  r@ d# 4 lshift d# 20 +  menu-w @ d# 2 -  d# 16  fill-rect
            true inverse !
        then
        menu-open @ c# 4 + @ ?dup if                \ a mark before the line, if it is marked
            r@ swap execute if
                inverse @  menu-x @ d# 6 +  r@ d# 4 lshift d# 26 +  d# 4 d# 4  fill-rect
            then
        then
        r@ menu-open @ CELL2 + @ execute bold ! ctext
        menu-x @ d# 12 +  r@ d# 4 lshift d# 20 +  draw-chars
        d# 0 bold !  d# 0 inverse !
        r> 1+
    repeat
    drop
    plain-font font !
;

\ What can be seen of each window changes only when a window is made,
\ moved or brought to the front, and is worked out again only then.  Which
\ windows there are, in what order, and where each is, are summed, and the
\ sum is kept: it is a few instructions a window to see that nothing has
\ changed, whoever might have changed it.
create vsum -1 ,
: v+ ( sum x -- sum' ) swap d# 31 * + ;

: win-sum ( sum win -- sum' ) \ which it is, and where
    >r  r@ v+  r@ @ v+  r@ CELL + @ v+  r@ CELL2 + @ v+  r> c# 3 + @ v+
;

: layout ( -- sum )
    nwin @ menu-open @ + >r                         ( R: the sum so far: the menu covers them too )
    wins nwin @ bounds
    begin
        2dupxor
    while
        dup @ r> swap win-sum >r
        1+
    repeat
    2drop r>
;

: visible ( -- ) \ work out what can be seen of each window, and of the desk
    layout dup vsum @ = if drop exit then
    vsum !
    menu-open @ if menu-box rcov @ rect-rgn else rcov @ rgn-none then
    nwin @
    begin
        dup
    while
        1-
        dup cells wins + @ >r
        r@ @ r@ CELL + @ r@ CELL2 + @ r@ c# 3 + @ rwin @ rect-rgn
        rwin @ rcov @ r> c# 9 + @ rgn-diff
        rcov @ rwin @ rtmp @ rgn-or
        rcov @ rtmp @ rcov ! rtmp !
    repeat
    drop
    d# 0 d# 0 fb-w @ fb-h @ rwin @ rect-rgn
    rwin @ rcov @ rdesk @ rgn-diff
;

\ What a window says has changed is out of date only where the window can
\ be seen.  Where another window lies over it there is nothing of this one
\ to paint, and to put that out of date would have the one on top painted
\ again for nothing.
header win-inval : win-inval ( x y w h win -- ) \ in a window's content
    >r 2swap r@ content
    rot + >r + r> 2swap                             ( x y w h ) \ on the screen
    d# 0 auto !
    rd @ 0= if rdrop inval-rect exit then
    visible
    rwin @ rect-rgn
    rwin @  r> c# 9 + @  rcov @ rgn-and             \ rcov is free once visible is done
    rcov @ inval-rgn
;

header refresh : refresh ( -- ) \ paint what is out of date
    dirty CELL2 + @ dirty @ > 0= if exit then
    dirty @ d# 0 max urect !
    dirty CELL + @ d# 0 max urect CELL + !
    dirty CELL2 + @ fb-w @ min urect CELL2 + !
    dirty c# 3 + @ fb-h @ min urect c# 3 + !
    d# 0 dirty !  d# 0 dirty CELL2 + !
    rd @ ru @ rd ! ru !                             \ paint this; start another
    rd @ rgn-none
    visible
    cursor-off  drag-off                            \ while the clip is still the whole screen
    ru @ rdesk @ rpaint @ rgn-and
    rpaint @ rgn-empty? 0= if
        uclip  rpaint @ rgn-clip!
        draw-desk
    then
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
    menu-open @ if                                  \ the menu, over all of them
        menu-box rwin @ rect-rgn
        ru @ rwin @ rpaint @ rgn-and
        rpaint @ rgn-empty? 0= if
            uclip  rpaint @ rgn-clip!
            draw-menu
        then
    then
    noclip
    drag-show  cursor-show
;

: repaint ( -- ) d# 0 d# 0 fb-w @ fb-h @ inval-rect ;
: redraw-top ( -- ) top ?dup if inval-window then ;

\ ---- which window, and bringing one to the front or closing it

: inside? ( win -- f ) \ is the pointer in it?
    dup @ mx @ swap - over CELL2 + @ u<
    swap dup CELL + @ my @ swap - swap c# 3 + @ u<
    and
;

: close-box? ( win -- f ) \ is the pointer on the x in its title bar?
    dup CELL + @ my @ swap - d# 19 u<
    swap dup @ swap CELL2 + @ + mx @ - d# 13 u< and
;

: win-i ( win -- i ) \ where it is in wins, or -1 if it is closed
    nwin @
    begin
        dup
    while
        1-  2dup cells wins + @ = if nip exit then
    repeat
    1- nip
;

: win-at ( -- i ) \ the frontmost window under the pointer, or -1
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

\ A window is told of the button going down in it (its click routine).  One
\ that wants to follow the pointer after that, to draw with it, stores a
\ word in 'track then: it is run with where the pointer is, in the window
\ as a click is, each time the pointer has moved while the button is down,
\ and once more when the button goes up, when `held` is no longer true.
header held create held 0 ,         \ true while the button is down
header 'track create 'track 0 ,     \ ( x y win -- )
create closing 0 ,                  \ the window whose x the button went down on, or 0

: track ( -- ) \ the pointer has moved with the button down, or the button has gone up
    'track @ ?dup if
        >r  top cw !
        mx @ cw.x - 1-  my @ cw.y - d# 19 -  top
        r> execute
    then
;

: end-press ( -- ) \ end any press or drag before the front window changes
    drag-off  d# 0 dragging !  d# 0 closing !
    d# 0 held !  track  d# 0 'track !
;

\ A program can be told when its window comes to the front and when
\ another window comes in front of it, for instance to load something into
\ memory only while it is in front.  It stores a word in the window's
\ win-front or win-back hook; the word is called with the window.
: told ( win a -- ) @ ?dup if execute else drop then ;      \ call a window's hook, if it has one

: front-changed ( old -- ) \ repaint the new front window and the bar, and call the old and new hooks
    redraw-top
    d# 0 d# 0 fb-w @ d# 19 inval-rect               \ the bar shows the front window's menu
    ?dup if dup win-back told then
    top ?dup if dup win-front told then
;

: forward ( i -- ) \ bring window i to the front: repaint it and the old front window, and call their hooks
    end-press
    top >r  redraw-top raise  r> front-changed
;

: close-at ( i -- ) \ take window i out of wins
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

\ ---- the menu: bringing it down, and what the button does while it is

: menu-inval ( -- ) menu-box inval-rect  d# 0 d# 0 fb-w @ d# 19 inval-rect ;      \ the menu and the bar

: menu-line ( -- i ) \ the line of it the pointer is on, or -1
    mx @ menu-x @ - d# 4 - menu-w @ u<  my @ d# 20 - menu-n d# 4 lshift u<  and
    if my @ d# 20 - d# 4 rshift else d# -1 then
;

: title-at ( -- x menu | 0 ) \ the bar title under the pointer: its x and its menu, or 0
    my @ d# 19 < 0= if d# 0 exit then
    mx @ menu-tw @ < if d# 0 sysmenu exit then
    mx @ menu-tw @ - menu-tw2 @ u< if top-menu ?dup if menu-tw @ swap exit then then
    d# 0
;

: on-title? ( -- f ) title-at ?dup if nip menu-open @ = else false then ;    \ is the pointer on the open menu's title?

: menu-down ( x menu -- ) \ open a menu, as wide as its longest line
    menu-open !  menu-x !
    sans font !
    d# 0 d# 0
    begin
        dup menu-n <
    while
        dup menu-open @ CELL2 + @ execute drop ctext chars-width  rot max swap
        1+
    repeat
    drop
    plain-font font !
    d# 24 + menu-w !
    true menu-held !  d# -1 menu-hot !
    menu-inval
;

: menu-up ( -- ) menu-inval  d# 0 menu-open !  d# 0 menu-held ! ;

: menu-pick ( i -- ) \ close the menu and run its action for line i
    menu-open @ >r  menu-up  r> c# 3 + @ execute
;

\ The windows' menu: a line per window, in the order they were made, the
\ last first, in bold if the window is open and plain if it is closed;
\ picking one brings it to the front, opening it if it was closed.
: menu-win ( i -- win ) nawin @ 1- swap - cells awins + @ ;
: wins-n ( -- n ) nawin @ ;
: wins-line ( i -- title bold? ) menu-win dup c# 4 + @  swap win-i 0< 0= ;
: wins-pick ( i -- )
    menu-win dup win-i
    dup 0< if
        drop  end-press
        top >r  redraw-top
        nwin @ cells wins + !  d# 1 nwin +!
        r> front-changed  exit
    then
    nip
    dup 1+ nwin @ <> if forward else drop then
;
: sysmenu! ( -- )
    t-menu sysmenu !  ['] wins-n sysmenu 1+ !  ['] wins-line sysmenu CELL2 + !  ['] wins-pick sysmenu c# 3 + !
;

: menu-track ( -- ) \ the pointer has moved: the line it is on is the one shown so
    menu-open @ if
        menu-line dup menu-hot @ <> if menu-hot ! menu-inval else drop then
    then
;

\ ---- the mouse button: the menu, or front a window, drag its title, or click in it

: press ( -- )
    menu-open @ if                                  \ a line of it, or away with it
        menu-line dup 0< if drop menu-up else menu-pick then
        exit
    then
    title-at ?dup if menu-down exit then
    win-at dup 0< if drop exit then
    dup cells wins + @ dup close-box? if nip closing ! exit then
    drop
    true held !  d# 0 'track !
    dup 1+ nwin @ <> if forward else drop then      \ bring it to the front, unless it is already
    top cw !
    my @ cw.y - d# 19 < if
        true dragging !  mx @ dx0 !  my @ dy0 !  drag-show
        exit
    then
    top c# 7 + @ ?dup if
        >r
        true auto !
        mx @ cw.x - 1-  my @ cw.y - d# 19 -  top
        r> execute
        auto @ if redraw-top then
    then
;

: release ( -- )
    menu-held @ if                                  \ the button that brought the menu down
        d# 0 menu-held !
        menu-line dup 0< if
            drop  on-title? 0= if menu-up then      \ let go on the title, it stays
        else
            menu-pick
        then
        exit
    then
    closing @ ?dup if
        d# 0 closing !
        dup close-box? if
            win-at dup 0< 0= if
                dup cells wins + @ rot = if close-at exit then
            then
            drop
        then
        drop exit
    then
    d# 0 held !  track  d# 0 'track !               \ whoever was following the pointer is told it is over
    dragging @ if
        drag-off  d# 0 dragging !
        redraw-top                                  \ where it was
        mx @ dx0 @ - top +!
        my @ dy0 @ - top CELL + +!
        redraw-top                                  \ and where it is
    then
;

\ ---- keys

create shift 0 ,
create ctrl 0 ,
create quitting 0 ,

: on-key ( c -- )
    menu-open @ if menu-up then                     \ a key takes the menu away, and is then a key
    dup d# 17 = if                                  \ ^Q: close the front window, or leave the desktop
        drop
        nwin @ if nwin @ 1- close-at else true quitting ! then
        exit
    then
    nwin @ 0= if drop exit then
    dup d# 23 = if                                  \ ^W: next window
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

\ A pointer says where it is in 32,768ths of the screen.  That times the
\ screen's size is more than a cell of 16 bits holds, and there it is
\ multiplied into two cells.
W32+? [if]
: scaled ( v n -- x ) * d# 15 rshift ;
[else]
: scaled ( v n -- x ) um* d# 1 lshift swap d# 15 rshift or ;
[then]

header on-event : on-event ( type code value -- )
    rot
    dup d# 3 = if                                   \ absolute position
        drop
        swap if
            fb-h @ scaled mx @ swap set-mouse
        else
            fb-w @ scaled my @ set-mouse
        then
        menu-track
        exit
    then
    \ A device says where the pointer is an axis at a time, and then that it
    \ has said all there is to say (an event of type 0): only then is the
    \ pointer somewhere it has been, and whoever follows it is told.
    dup 0= if drop 2drop  held @ if track then  exit then
    d# 1 <> if 2drop exit then                      \ otherwise keys only
    over h# 110 = if nip if press else release then exit then
    over shift-key? if nip 0<> shift ! exit then
    over ctrl-key?  if nip 0<> ctrl ! exit then
    0= if drop exit then                            \ a key going up
    dup d# 127 u> if drop exit then
    shift @ if d# 128 + then                        \ the keys, and after them the keys shifted:
    d# 3 lshift  keymap swap char@                  \ characters packed into cells, as text is
    ctrl @ if h# 1f and then
    ?dup if on-key then
;

\ Keys and pointer events come from whatever the machine has, which is
\ asked for them each time round: a driver stores the word that asks in
\ 'poll, and hands what it has to on-event.  A machine with nothing but
\ its console has the console's keys.
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
        pause                                       \ the other tasks' turn
        quitting @
    until
;
