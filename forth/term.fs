\ term.fs - a scrolling text area, and the Forth window built on it

$32 constant TCOLS
$10 constant TROWS

create tcells #800 allot
: tbuf ( -- a ) tcells >c ;             \ the text, by the address of its first byte
create tcol 0 ,
create trow 0 ,
create ti 0 ,
create twin 0 ,         \ the window it is in, once that is known

\ What has changed is put out of date a character at a time.
: tmark ( -- ) \ the character the cursor is at
    twin @ ?dup if
        >r  tcol @ d# 3 lshift d# 2 +  trow @ d# 4 lshift d# 2 +  d# 8 d# 16  r> win-inval
    then
;

header tclear : tclear ( -- ) tbuf d# 800 d# 32 fill  d# 0 tcol !  d# 0 trow ! ;

\ The text moves up a line.  The picture of it is moved too, where the
\ window can be seen, and what could not be moved is put out of date: the
\ new line, the line the cursor was on, and whatever would have come from
\ under another window.  What was already out of date is painted first, so
\ that what moves is the text as it was.
: tscroll ( -- )
    twin @ if refresh then
    tbuf TCOLS + tbuf TCOLS TROWS 1- * cmove
    tbuf TCOLS TROWS 1- * + TCOLS d# 32 fill
    twin @ 0= if exit then
    cursor-off
    twin @ c# 8 + @ port!
    twin @ content origin!
    noclip  d# 2 d# 2 TCOLS d# 3 lshift TROWS d# 4 lshift clip-to
    twin @ c# 9 + @ rgn-clip!                       \ where the window can be seen
    d# 2 d# 2 TCOLS d# 3 lshift TROWS d# 4 lshift  d# 0 d# -16  scroll-rect
    screen-port @ port!
    cursor-show
    scroll-update @ inval-rgn
    d# 2  TROWS d# 2 - d# 4 lshift d# 2 +  TCOLS d# 3 lshift  d# 16  twin @ win-inval
;

: tnewline ( -- )
    d# 0 tcol !
    trow @ 1+ dup TROWS = if drop tscroll else trow ! then
;

: (term-emit) ( c -- )
    dup d# 10 = if drop tnewline exit then
    dup d# 13 = if drop exit then
    dup d# 8 = if
        drop
        tcol @ if
            d# -1 tcol +!
            d# 32 trow @ TCOLS * tcol @ + tbuf + c!
        then
        exit
    then
    trow @ TCOLS * tcol @ + tbuf + c!
    tcol @ 1+ dup TCOLS = if drop tnewline else tcol ! then
;

: term-emit ( c -- ) tmark (term-emit) tmark ; \ where the cursor was, and where it is

header term-draw : term-draw ( win -- )
    drop
    d# 0 ti !
    begin
        ti @ TROWS <
    while
        ti @ TCOLS * tbuf + TCOLS
        d# 2  d# 2 ti @ d# 4 lshift +  draw-text
        d# 1 ti +!
    repeat
    true inverse !                                  \ the cursor: a solid cell
    d# 32
    d# 2 tcol @ d# 3 lshift +
    d# 2 trow @ d# 4 lshift +
    draw-char
    d# 0 inverse !
;

\ ---- the Forth window
\
\ The window's Forth is a task of its own, with the upper half of each
\ stack, so what is on its data stack stays there from one line to the
\ next.  The desktop's task takes the keys: it puts them in `line`, and at
\ the end of a line says that one is ready.  The Forth task runs it when
\ its turn comes, with its output going to the terminal, and the desktop
\ goes on running meanwhile for as long as the line says pause.  Keys
\ typed while a line is running are dropped.

create lcells #80 allot
: line ( -- a ) lcells >c ;
create llen 0 ,
create lready 0 ,       \ true while a line is waiting or running
create rtask  0 , 0 , 0 , 0 ,  0 , 0 , 0 , 0 ,  0 , 0 , 0 , 0 ,  0 ,
create rgoing 0 ,       \ true once the task has been started

\ The prompt: ok, and a new line to type on.  The window opens with one.
header term-ok : term-ok ( -- )
    [char] o term-emit [char] k term-emit d# 10 term-emit
;

: repl ( -- ) \ what the Forth task does on each of its turns
    lready @ 0= if exit then
    ['] term-emit 'emit !
    line llen @ ['] tevaluate catch
    ?dup if
        >r 2drop
        d# 0 state !  d# 0 sourceid !
        [char] ? emit space r> .
    then
    space term-ok
    d# 0 llen !  d# 0 lready !
;

header repl-start : repl-start ( -- ) \ once
    rgoing @ if exit then
    true rgoing !
    ['] repl d# 128 d# 128 rtask spawn
;

header repl-key : repl-key ( c win -- )
    twin !
    lready @ if drop exit then
    dup d# 13 = if
        drop
        d# 10 term-emit
        true lready !
        exit
    then
    dup d# 8 = over d# 127 = or if
        drop
        llen @ if d# -1 llen +! d# 8 term-emit then
        exit
    then
    dup d# 32 < over d# 126 > or if drop exit then
    llen @ d# 78 < if
        dup line llen @ + c!
        d# 1 llen +!
        term-emit
    else
        drop
    then
;
