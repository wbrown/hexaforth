\ wm.fs - windows, the pointer, and the event loop
\
\ A window is eight cells: x, y, width, height, title (a counted string),
\ and three routines:
\     draw   ( win -- )         paint the content
\     key    ( c win -- )       a key, when this window is in front
\     click  ( x y win -- )     a click in the content, or 0 for none
\
\ `wins` holds the windows from back to front.  The front one has the
\ keyboard.  Painting is back to front with no clipping between windows, so
\ only the front window may draw outside a full repaint.

create wins #64 allot
create nwin 0 ,
create cw 0 ,           \ the window being drawn

: top ( -- win ) nwin @ 1- cells wins + @ ;

header window
: window ( x y w h title draw key click -- win )
    d# 64 halloc >r
    r@ d# 56 + !  r@ d# 48 + !  r@ d# 40 + !  r@ d# 32 + !
    r@ d# 24 + !  r@ d# 16 + !  r@ d# 8 + !   r@ !
    r@ nwin @ cells wins + !
    d# 1 nwin +!
    r>
;

: content ( win -- x y ) \ top left of the area inside the frame
    dup @ 1+ swap d# 8 + @ d# 19 +
;

: cw.x cw @ @ ;
: cw.y cw @ d# 8 + @ ;
: cw.w cw @ d# 16 + @ ;
: cw.h cw @ d# 24 + @ ;

\ ---- the pointer, drawn by inverting, so drawing it again removes it

create mx 0 ,
create my 0 ,
create cur-on 0 ,
create arrow
    $80 c, $c0 c, $e0 c, $f0 c, $f8 c, $fc c,
    $fe c, $f0 c, $d8 c, $18 c, $0c c, $0c c,

: cursor-xor ( -- )
    d# 0
    begin
        dup d# 12 <
    while
        dup arrow + c@  over my @ +  mx @ swap  xor8
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

: draw-frame ( front? -- ) \ frame and title of cw
    true  cw.x cw.y cw.w cw.h fill-rect
    false cw.x cw.y cw.w cw.h frame-rect
    false cw.x cw.y d# 18 + cw.w hline
    dup if false cw.x 1+ cw.y 1+ cw.w d# 2 - d# 17 fill-rect then
    inverse !
    cw @ d# 32 + @ count  cw.x d# 8 + cw.y d# 2 + draw-text
    d# 0 inverse !
;

: draw-window ( i -- )
    dup cells wins + @ cw !
    1+ nwin @ = draw-frame
    cw @ dup d# 40 + @ execute
;

create dy 0 ,

: draw-desk ( -- ) \ menu bar, then a grey made of alternating pixels
    true d# 0 d# 0 fb-w @ d# 19 fill-rect
    false d# 0 d# 19 fb-w @ hline
    t-menu count d# 8 d# 2 draw-text
    d# 20 dy !
    begin
        dy @ fb-h @ <
    while
        d# 0
        begin
            dup fb-w @ <
        while
            dy @ d# 1 and if h# aa else h# 55 then over dy @ cblit8
            d# 8 +
        repeat
        drop
        d# 1 dy +!
    repeat
;

: repaint ( -- )
    cursor-off
    draw-desk
    d# 0
    begin
        dup nwin @ <
    while
        dup draw-window
        1+
    repeat
    drop
    cursor-show
;

: redraw-top ( -- ) cursor-off nwin @ 1- draw-window cursor-show ;

\ ---- which window, and bringing one to the front

create hx 0 ,
create hy 0 ,

: inside? ( win -- f ) \ is hx,hy in it?
    dup @ hx @ swap - over d# 16 + @ u<
    swap dup d# 8 + @ hy @ swap - swap d# 24 + @ u<
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
    dup 1+ nwin @ <> if raise repaint else drop then
    top cw !
    hy @ cw.y - d# 19 < if
        true dragging !  mx @ dx0 !  my @ dy0 !
        exit
    then
    top d# 56 + @ ?dup if
        >r
        hx @ cw.x - 1-  hy @ cw.y - d# 19 -  top
        r> execute
        redraw-top
    then
;

: release ( -- )
    dragging @ if
        d# 0 dragging !
        mx @ dx0 @ - top +!
        my @ dy0 @ - top d# 8 + +!
        repaint
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
        nwin @ d# 1 > if d# 0 raise repaint then
        exit
    then
    top dup d# 48 + @ execute
    redraw-top
;

: shift-key? ( code -- f ) dup d# 42 = swap d# 54 = or ;
: ctrl-key?  ( code -- f ) dup d# 29 = swap d# 97 = or ;

\ ---- events from the input devices

: on-event ( type code value -- )
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
    shift @ if d# 128 + then keymap + c@
    ctrl @ if h# 1f and then
    ?dup if on-key then
;

: uart-poll ( -- ) key? if key on-key then ;

: event-loop ( -- )
    d# 0 quitting !
    repaint
    begin
        input-poll
        uart-poll
        quitting @
    until
;
