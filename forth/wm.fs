\ wm.fs - windows, the pointer, and the event loop
\
\ A window is nine cells: x, y, width, height, title (a counted string),
\ three routines, and its port:
\     draw   ( win -- )         paint the content, in the window's port
\     key    ( c win -- )       a key, when this window is in front
\     click  ( x y win -- )     a click in the content, or 0 for none
\
\ `wins` holds the windows from back to front.  The front one has the
\ keyboard.
\
\ Nothing draws when something changes.  What has changed is added to a
\ rectangle of the screen that is out of date (inval-rect, win-inval), and
\ the event loop then paints that rectangle: the desk and every window
\ that touches it, from back to front, clipped to it.  A key or a click
\ puts the whole of the front window out of date, unless the window's
\ routine said more exactly what changed.

create wins #64 allot
header nwin create nwin 0 ,
create cw 0 ,           \ the window being drawn

: top ( -- win ) nwin @ 1- cells wins + @ ;

header window
: window ( x y w h title draw key click -- win )
    d# 72 halloc >r
    new-port r@ c# 8 + !
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

: cursor-off  ( -- ) cur-on @ if cursor-xor d# 0 cur-on ! then ;
: cursor-show ( -- ) cur-on @ 0= if cursor-xor true cur-on ! then ;

: set-mouse ( x y -- )
    cursor-off
    fb-h @ d# 12 - min d# 0 max my !
    fb-w @ d# 8 - min d# 0 max mx !
    cursor-show
;

\ ---- painting

create t-menu ," hexaforth"

\ ---- what is out of date

create dirty 0 , 0 , 0 , 0 ,    \ left, top, and past the right and bottom; empty if no wider than 0
create urect 0 , 0 , 0 , 0 ,    \ the same, for what is being painted
create auto 0 ,                 \ true if the front window is to be put out of date

header inval-rect : inval-rect ( x y w h -- ) \ on the screen
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

header inval-window : inval-window ( win -- ) \ frame and all
    >r r@ @ r@ CELL + @ r@ CELL2 + @ r> c# 3 + @ inval-rect
;

: uclip ( -- ) \ clip to what is being painted
    urect @ clip !  urect CELL + @ clip CELL + !
    urect CELL2 + @ clip CELL2 + !  urect c# 3 + @ clip c# 3 + !
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

: draw-window ( i -- )
    dup cells wins + @ cw !
    1+ nwin @ = draw-frame
    cw @ c# 8 + @ port!                             \ the window's own port:
    cw @ content origin!                            \ its content, and no more
    uclip  d# 0 d# 0 cw.w d# 2 - cw.h d# 20 - clip-to
    cw @ dup c# 5 + @ execute
    screen-port @ port!
;

: draw-desk ( -- ) \ menu bar, then grey
    true d# 0 d# 0 fb-w @ d# 19 fill-rect
    false d# 0 d# 19 fb-w @ hline
    sans font !
    t-menu >c count d# 8 d# 2 draw-text
    mono font !
    p-gray pattern !
    d# 0 d# 20 fb-w @ fb-h @ d# 20 - pat-rect
;

header win-inval : win-inval ( x y w h win -- ) \ in a window's content
    >r 2swap r> content
    rot + >r + r> 2swap inval-rect
    d# 0 auto !
;

header refresh : refresh ( -- ) \ paint what is out of date
    dirty CELL2 + @ dirty @ > 0= if exit then
    dirty @ d# 0 max urect !
    dirty CELL + @ d# 0 max urect CELL + !
    dirty CELL2 + @ fb-w @ min urect CELL2 + !
    dirty c# 3 + @ fb-h @ min urect c# 3 + !
    d# 0 dirty !  d# 0 dirty CELL2 + !
    cursor-off
    uclip
    draw-desk
    d# 0
    begin
        dup nwin @ <
    while
        dup cells wins + @ touches? if dup draw-window uclip then
        1+
    repeat
    drop
    noclip
    cursor-show
;

: repaint ( -- ) d# 0 d# 0 fb-w @ fb-h @ inval-rect ;
: redraw-top ( -- ) top inval-window ;

\ ---- which window, and bringing one to the front

create hx 0 ,
create hy 0 ,

: inside? ( win -- f ) \ is hx,hy in it?
    dup @ hx @ swap - over CELL2 + @ u<
    swap dup CELL + @ hy @ swap - swap c# 3 + @ u<
    and
;

: win-at ( -- i ) \ the frontmost window under hx,hy, or -1
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

\ ---- the mouse button: front a window, drag its title, or click in it

create dragging 0 ,
create dx0 0 ,
create dy0 0 ,

: press ( -- )
    mx @ hx !  my @ hy !
    win-at dup 0< if drop exit then
    dup 1+ nwin @ <> if raise redraw-top else drop then
    top cw !
    hy @ cw.y - d# 19 < if
        true dragging !  mx @ dx0 !  my @ dy0 !
        exit
    then
    top c# 7 + @ ?dup if
        >r
        true auto !
        hx @ cw.x - 1-  hy @ cw.y - d# 19 -  top
        r> execute
        auto @ if redraw-top then
    then
;

: release ( -- )
    dragging @ if
        d# 0 dragging !
        redraw-top refresh                          \ where it was
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
    dup d# 17 = if drop true quitting ! exit then   \ ^Q: leave the desktop
    dup d# 23 = if                                  \ ^W: next window
        drop
        nwin @ d# 1 > if d# 0 raise redraw-top then
        exit
    then
    true auto !
    top dup c# 6 + @ execute
    auto @ if redraw-top then
;

: shift-key? ( code -- f ) dup d# 42 = swap d# 54 = or ;
: ctrl-key?  ( code -- f ) dup d# 29 = swap d# 97 = or ;

\ ---- events from the input devices

header on-event : on-event ( type code value -- )
    rot
    dup d# 3 = if                                   \ absolute position
        drop
        swap if
            fb-h @ * d# 15 rshift mx @ swap set-mouse
        else
            fb-w @ * d# 15 rshift my @ set-mouse
        then
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

: uart-poll ( -- ) key? if key on-key then ;

header event-loop : event-loop ( -- )
    d# 0 quitting !
    repaint
    begin
        input-poll
        uart-poll
        refresh
        quitting @
    until
;
