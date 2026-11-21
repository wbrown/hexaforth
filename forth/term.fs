\ term.fs - the Forth window: what is typed and printed, kept as
\ a text that rolls (text.fs)

meta
#50 constant term-cols
#16 constant term-rows
term-cols cell-bytes + cell-bytes / constant term-line  \ cells
target

meta term-cols target constant TCOLS
meta term-rows target constant TROWS
meta term-line term-rows * target constant TROOM  \ its cells

bulk
create tcells
    meta term-line term-rows * cell-bytes * target allot
unbulk
create ttext 0 , 0 , 0 , 0 , 0 , 0 ,  \ the text,
create tback 0 ,  \ how far back from its end it is shown,
create twin 0 ,   \ and the window, once that is known

: tlines ( -- n ) ttext tb.lines @ ;
: tfirst ( -- n ) \ the first line shown
    tlines TROWS - tback @ - d# 0 max ;
: tcur ( -- col row ) \ the cursor, at the end of the text
    ttext tb.len @  tlines 1- tfirst - ;

: tmark ( -- ) \ the cursor's character put out of date
    twin @ ?dup if
        >r  tcur char-xy  d# 8 d# 16  r> win-inval
    then ;

: tarea ( -- x y w h ) \ the text's place in its window
    d# 2 d# 2 TCOLS d# 3 lshift TROWS d# 4 lshift ;

: tall ( -- ) \ all the text put out of date
    twin @ ?dup if >r tarea r> win-inval then ;

: trow-inval ( row -- ) \ a row of it put out of date
    >r d# 0 r> char-xy  TCOLS d# 3 lshift d# 16
    twin @ win-inval
;

header tclear : tclear ( -- )
    tcells TROOM true ttext text-new  d# 0 tback ! ;

: tslide ( -- ) \ the picture of the text up a row
    cursor-off
    twin @ c# 8 + @ port!
    twin @ content origin!
    noclip  tarea clip-to
    twin @ c# 9 + @ rgn-clip!
    tarea  d# 0 d# -16  scroll-rect
    screen-port @ port!
    cursor-show
;

: tnewline ( -- )
    tlines TROWS < if ttext text-nl exit then
    twin @ if refresh then  \ what is out of date, first
    ttext text-nl
    twin @ 0= if exit then
    tslide
    scroll-update @ inval-rgn
    TROWS d# 2 - trow-inval  d# 0 trow-inval
;

: (term-emit) ( c -- )
    dup d# 10 = if drop tnewline exit then
    dup d# 13 = if drop exit then
    dup d# 8 = if drop ttext text-back exit then
    dup bl < if drop bl then    \ not a code of the text
    ttext text-code
    ttext tb.len @ TCOLS = if tnewline then
;

: term-emit ( c -- )
    tback @ if d# 0 tback !  tall then
    tmark (term-emit) tmark
;

: tcursor ( -- ) \ a solid cell, if the end of the text is shown
    tcur dup TROWS < if
        true inverse !
        >r >r d# 32 r> r> char-xy draw-char  d# 0 inverse !
        exit
    then
    2drop
;

header term-draw : term-draw ( win -- )
    drop
    tfirst dup ttext text-line  tlines rot - TROWS min
    d# 0 d# 0 char-xy draw-lines  tcursor
    tfirst if TCOLS d# 0 char-xy mark-up then
    tback @ if TCOLS TROWS 1- char-xy mark-down then
;

: tback+ ( n -- ) \ shown from n lines further back
    tback @ +  tlines TROWS - d# 0 max min  d# 0 max  tback !
    tall
;

\ ---- the Forth window

\ Its Forth is a task with the upper half of each stack.

create lcells #80 allot
: line ( -- a ) lcells >c ;
create llen 0 ,
create lready 0 ,  \ true while a line is waiting or running
create rtask  0 , 0 , 0 ,  0 , 0 , 0 , 0 ,  0 , 0 , 0 , 0 ,
create rgoing 0 ,  \ true once the task has been started

header term-ok : term-ok ( -- )
    [char] o term-emit [char] k term-emit d# 10 term-emit
;

: repl ( -- ) \ a turn of the Forth task
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

: lback ( -- )
    llen @ if d# -1 llen +! d# 8 term-emit then ;

: ladd ( c -- ) \ if the line has room for it
    llen @ d# 78 < if
        dup line llen @ + c!
        d# 1 llen +!
        term-emit
    else
        drop
    then
;

header repl-key : repl-key ( c win -- )
    twin !
    dup d# 132 = if drop TROWS 1- tback+ exit then
    dup d# 133 = if drop TROWS 1- negate tback+ exit then
    lready @ if drop exit then
    dup d# 13 = if
        drop d# 10 term-emit  true lready ! exit
    then
    dup d# 8 = over d# 127 = or if drop lback exit then
    dup d# 32 < over d# 126 > or if drop exit then
    ladd
;
