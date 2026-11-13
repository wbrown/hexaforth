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

\ The text moves up a line.  If the window is in front, so that all of it
\ is on the screen, the picture of the text is moved too, and only the last
\ two lines are put out of date: the new one, and the one the cursor was
\ on.  What was already out of date is painted first, so that what moves is
\ the text as it was.
: tscroll ( -- )
    twin @ dup if top = then
    dup if refresh then
    tbuf TCOLS + tbuf TCOLS TROWS 1- * cmove
    tbuf TCOLS TROWS 1- * + TCOLS d# 32 fill
    if
        cursor-off
        twin @ c# 8 + @ port!
        twin @ content origin!
        noclip  d# 2 d# 2 TCOLS d# 3 lshift TROWS d# 4 lshift clip-to
        d# 2 d# 2 TCOLS d# 3 lshift TROWS d# 4 lshift  d# 0 d# -16  scroll-rect
        screen-port @ port!
        cursor-show
        d# 2  TROWS d# 2 - d# 4 lshift d# 2 +  TCOLS d# 3 lshift  d# 32  twin @ win-inval
    else
        twin @ ?dup if inval-window then
    then
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
\ A line is evaluated with output going to the terminal.  The desktop is in
\ the middle of its own work on the data stack at that point, so the user's
\ stack is kept in `ust` between lines: it is pushed above eight spare
\ cells before the line runs and collected again afterwards.

create lcells #80 allot
: line ( -- a ) lcells >c ;
create llen 0 ,
create ust #256 allot
create un 0 ,
create ubase 0 ,

\ The prompt: ok, and a new line to type on.  The window opens with one.
header term-ok : term-ok ( -- )
    [char] o term-emit [char] k term-emit d# 10 term-emit
;

: repl-run ( -- )
    ['] term-emit 'emit !
    adepth ubase !
    d# 0 d# 0 d# 0 d# 0 d# 0 d# 0 d# 0 d# 0
    ubase @ d# 8 + sbase !
    d# 0
    begin
        dup un @ <
    while
        dup cells ust + @ swap 1+
    repeat
    drop
    line llen @ ['] tevaluate catch
    ?dup if
        >r 2drop
        d# 0 state !  d# 0 sourceid !
        [char] ? emit space r> .
    then
    depth d# 0 max d# 32 min un !
    d# 0 sbase !
    un @
    begin
        dup
    while
        1- tuck cells ust + !
    repeat
    drop
    ubase @ IO-SP io!
    space term-ok
    ['] uart-emit 'emit !
;

header repl-key : repl-key ( c win -- )
    twin !
    dup d# 13 = if
        drop
        d# 10 term-emit
        repl-run
        d# 0 llen !
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
