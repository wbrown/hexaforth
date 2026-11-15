\ wm.fs - windows, the pointer, and the event loop
\
\ A window is ten cells: x, y, width, height, title (a counted string),
\ three routines, its port, and the region of it that can be seen:
\     draw   ( win -- )         paint the content, in the window's port
\     key    ( c win -- )       a key, when this window is in front
\     click  ( x y win -- )     a click in the content, or 0 for none
\
\ `wins` holds the windows from back to front.  The front one has the
\ keyboard.
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

create wins #64 allot
header nwin create nwin 0 ,
create cw 0 ,           \ the window being drawn

header top : top ( -- win ) nwin @ 1- cells wins + @ ;

\ A window is eleven cells: where it is and how big, its title, the words
\ that draw it and take its keys and clicks, its port, what can be seen of
\ it, and a cell for whoever draws it to keep what it is a window on.
header win-data : win-data ( win -- a ) c# 10 + ;

header window
: window ( x y w h title draw key click -- win )
    d# 11 >c halloc >r
    new-port r@ c# 8 + !
    new-rgn r@ c# 9 + !
    d# 0 r@ win-data !
    r@ c# 7 + !  r@ c# 6 + !  r@ c# 5 + !  r@ c# 4 + !
    r@ c# 3 + !  r@ CELL2 + !  r@ CELL + !  r@ !
    r@ nwin @ cells wins + !
    d# 1 nwin +!
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
create arrow
    $80 p, $c0 p, $e0 p, $f0 p, $f8 p, $fc p,
    $fe p, $f0 p, $d8 p, $18 p, $0c p, $0c p,

: cursor-xor ( -- )
    d# 0
    begin
        dup d# 12 <
    while
        dup arrow >c + c@  over my @ +  mx @ swap  xor8
        1+
    repeat
    drop
;

header cursor-off : cursor-off  ( -- ) cur-on @ if cursor-xor d# 0 cur-on ! then ;
header cursor-show : cursor-show ( -- ) cur-on @ 0= if cursor-xor true cur-on ! then ;

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

\ The title in the bar is a menu: the windows, the one in front first.
\ The button going down on the title brings it down under the title.  If
\ the button goes up on a line of it, that window comes to the front; if
\ it goes up on the title the menu stays, and the next press picks a line
\ or, anywhere else, takes the menu away.  So it works held down and
\ dragged, or with a click and then a click.
\
\ While it is down it is in front of every window.  It is counted with
\ what covers them, so that none of them paints or scrolls where it is,
\ and it is painted after them where it is out of date.
header menu-open create menu-open 0 ,   \ true while it is down
create menu-held 0 ,    \ true while the button that brought it down is still down
create menu-hot -1 ,    \ the line of it the pointer is on, or -1
create menu-w 0 ,       \ how wide it is,
create menu-tw 0 ,      \ and how much of the bar its title has

: menu-box ( -- x y w h ) d# 4 d# 19  menu-w @  nwin @ d# 4 lshift d# 2 + ;

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

: wm-regions ( -- )
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
    cw @ c# 4 + @ >c count  cw.x d# 8 + cw.y d# 2 + draw-text
    mono font !
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

: draw-desk ( -- ) \ menu bar, then grey
    true d# 0 d# 0 fb-w @ d# 19 fill-rect
    false d# 0 d# 19 fb-w @ hline
    sans font !
    t-menu >c count text-width d# 16 + menu-tw !
    menu-open @ if                                  \ its title is white on black while it is down
        false d# 4 d# 1 menu-tw @ d# 8 - d# 17 fill-rect
        true inverse !
    then
    t-menu >c count d# 8 d# 2 draw-text
    d# 0 inverse !
    mono font !
    p-gray pattern !
    d# 0 d# 20 fb-w @ fb-h @ d# 20 - pat-rect
;

: draw-menu ( -- ) \ a line for each window, the one in front first
    true menu-box fill-rect  false menu-box frame-rect
    sans font !
    d# 0
    begin
        dup nwin @ <
    while
        >r                                          ( R: the line )
        r@ menu-hot @ = if                          \ the one the pointer is on
            false  d# 5  r@ d# 4 lshift d# 20 +  menu-w @ d# 2 -  d# 16  fill-rect
            true inverse !
        then
        nwin @ 1- r@ - cells wins + @ c# 4 + @ >c count
        d# 12  r@ d# 4 lshift d# 20 +  draw-text
        d# 0 inverse !
        r> 1+
    repeat
    drop
    mono font !
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
    cursor-off
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
    cursor-show
;

: repaint ( -- ) d# 0 d# 0 fb-w @ fb-h @ inval-rect ;
: redraw-top ( -- ) top inval-window ;

\ ---- which window, and bringing one to the front

: inside? ( win -- f ) \ is the pointer in it?
    dup @ mx @ swap - over CELL2 + @ u<
    swap dup CELL + @ my @ swap - swap c# 3 + @ u<
    and
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

\ A window may want to know when the one in front has changed: to take
\ something into memory while it is in front, and give it back after.
header 'front create 'front 0 ,     \ ( -- )
: fronted ( -- ) 'front @ ?dup if execute then ;

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

\ ---- the menu: bringing it down, and what the button does while it is

: menu-inval ( -- ) menu-box inval-rect  d# 0 d# 0 menu-tw @ d# 19 inval-rect ;     \ it, and its title

: menu-line ( -- i ) \ the line of it the pointer is on, or -1
    mx @ d# 4 - menu-w @ u<  my @ d# 20 - nwin @ d# 4 lshift u<  and
    if my @ d# 20 - d# 4 rshift else d# -1 then
;

: on-title? ( -- f ) my @ d# 19 <  mx @ menu-tw @ <  and ;

: menu-down ( -- ) \ as wide as the longest title wants
    sans font !
    d# 0  wins nwin @ bounds
    begin
        2dupxor
    while
        dup @ c# 4 + @ >c count text-width  >r rot r> max -rot
        1+
    repeat
    2drop
    mono font !
    d# 24 + menu-w !
    true menu-open !  true menu-held !  d# -1 menu-hot !
    menu-inval
;

: menu-up ( -- ) menu-inval  d# 0 menu-open !  d# 0 menu-held ! ;

: menu-pick ( i -- ) \ the window on that line comes to the front
    menu-up
    nwin @ 1- swap -                                ( where it is among the windows )
    dup 1+ nwin @ <> if
        redraw-top raise redraw-top  fronted
    else
        drop
    then
;

: menu-track ( -- ) \ the pointer has moved: the line it is on is the one shown so
    menu-open @ if
        menu-line dup menu-hot @ <> if menu-hot ! menu-inval else drop then
    then
;

\ ---- the mouse button: the menu, or front a window, drag its title, or click in it

create dragging 0 ,
create dx0 0 ,
create dy0 0 ,

: press ( -- )
    menu-open @ if                                  \ a line of it, or away with it
        menu-line dup 0< if drop menu-up else menu-pick then
        exit
    then
    on-title? if menu-down exit then
    win-at dup 0< if drop exit then
    dup 1+ nwin @ <> if
        redraw-top raise redraw-top                 \ the one that was in front, and the one that is
        fronted
    else
        drop
    then
    top cw !
    my @ cw.y - d# 19 < if
        true dragging !  mx @ dx0 !  my @ dy0 !
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
    dragging @ if
        d# 0 dragging !
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
    dup d# 17 = if drop true quitting ! exit then   \ ^Q: leave the desktop
    dup d# 23 = if                                  \ ^W: next window
        drop
        nwin @ d# 1 > if redraw-top d# 0 raise redraw-top fronted then
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
    d# 1 <> if 2drop exit then                      \ otherwise keys only
    over h# 110 = if nip if press else release then exit then
    over shift-key? if nip 0<> shift ! exit then
    over ctrl-key?  if nip 0<> ctrl ! exit then
    0= if drop exit then                            \ a key going up
    dup d# 127 u> if drop exit then
    shift @ if d# 128 + then keymap >c + c@
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
    wm-regions
    repaint
    begin
        'poll @ execute
        refresh
        pause                                       \ the other tasks' turn
        quitting @
    until
;
