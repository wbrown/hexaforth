\ term.fs - a scrolling text area, and the Forth window built on it

$32 constant TCOLS
$10 constant TROWS

create tbuf #800 allot
create tcol 0 ,
create trow 0 ,
create ti 0 ,
create tx0 0 ,
create ty0 0 ,

: tclear ( -- ) tbuf d# 800 d# 32 fill  d# 0 tcol !  d# 0 trow ! ;

: tscroll ( -- )
    tbuf TCOLS + tbuf TCOLS TROWS 1- * cmove
    tbuf TCOLS TROWS 1- * + TCOLS d# 32 fill
;

: tnewline ( -- )
    d# 0 tcol !
    trow @ 1+ dup TROWS = if drop tscroll else trow ! then
;

: term-emit ( c -- )
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

: term-draw ( win -- )
    content ty0 ! tx0 !
    d# 0 ti !
    begin
        ti @ TROWS <
    while
        ti @ TCOLS * tbuf + TCOLS
        tx0 @ d# 2 +  ty0 @ d# 2 + ti @ d# 4 lshift +  draw-text
        d# 1 ti +!
    repeat
    true inverse !                                  \ the cursor: a solid cell
    d# 32
    tx0 @ d# 2 + tcol @ d# 3 lshift +
    ty0 @ d# 2 + trow @ d# 4 lshift +
    draw-char
    d# 0 inverse !
;

\ ---- the Forth window
\
\ A line is evaluated with output going to the terminal.  The desktop is in
\ the middle of its own work on the data stack at that point, so the user's
\ stack is kept in `ust` between lines: it is pushed above eight spare
\ cells before the line runs and collected again afterwards.

create line #80 allot
create llen 0 ,
create ust #256 allot
create un 0 ,
create ubase 0 ,

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
    space [char] o emit [char] k emit cr
    ['] uart-emit 'emit !
;

: repl-key ( c win -- )
    drop
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
