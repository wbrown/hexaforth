\ term.fs - the Forth window: what is typed and printed, kept as
\ a text that rolls (text.fs)

\ What is printed goes onto the end of the text's last line, and
\ a line is ended by a newline or by being TCOLS characters
\ long.  The window shows TROWS lines: the last of them, or
\ lines further back, which PgUp and PgDn go to and whatever is
\ printed comes back from.  The text has room for as many lines
\ as are shown when they are all TCOLS long, and for more when
\ they are shorter, since a line takes only the cells of its own
\ characters.  When it is full its first lines go.
meta
#50 constant term-cols
#16 constant term-rows
\ the cells of a line of term-cols characters and its EOL
term-cols cell-bytes + cell-bytes / constant term-line
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
: tcur ( -- col row )
    \ where the cursor is, at the end of the text
    ttext tb.len @  tlines 1- tfirst - ;

\ What has changed is put out of date a character at a time.
: tmark ( -- ) \ the character the cursor is at
    twin @ ?dup if
        >r  tcur char-xy  d# 8 d# 16  r> win-inval
    then ;

: tarea ( -- x y w h ) \ the text's place in its window
    d# 2 d# 2 TCOLS d# 3 lshift TROWS d# 4 lshift ;

: tall ( -- ) \ all the text put out of date
    twin @ ?dup if >r tarea r> win-inval then ;

: trow-inval ( row -- ) \ a row of it put out of date
    >r d# 0 r> char-xy  TCOLS d# 3 lshift d# 16  twin @ win-inval
;

header tclear : tclear ( -- )
    tcells TROOM true ttext text-new  d# 0 tback ! ;

\ The text moves up a line.  The picture of it is moved too,
\ where the window can be seen, and what could not be moved is
\ put out of date: the new line, the line the cursor was on, the
\ first line, whose [PGUP] went up with the rest, and whatever
\ would have come from under another window.  What was already
\ out of date is painted first, so that what moves is the text
\ as it was.

: tslide ( -- ) \ the picture of the text up a row
    cursor-off
    twin @ c# 8 + @ port!
    twin @ content origin!
    noclip  tarea clip-to
    twin @ c# 9 + @ rgn-clip!  \ where the window can be seen
    tarea  d# 0 d# -16  scroll-rect
    screen-port @ port!
    cursor-show
;

\ A new line below the last row shown moves the text up a row.
: tnewline ( -- )
    tlines TROWS < if ttext text-nl exit then
    twin @ if refresh then
    ttext text-nl
    twin @ 0= if exit then
    tslide
    scroll-update @ inval-rgn
    TROWS d# 2 - trow-inval  d# 0 trow-inval
;

\ A character below a space that is not one of these is printed
\ as a space, so that it is not a code of the text.
: (term-emit) ( c -- )
    dup d# 10 = if drop tnewline exit then
    dup d# 13 = if drop exit then
    dup d# 8 = if drop ttext text-back exit then
    dup bl < if drop bl then
    ttext text-code
    ttext tb.len @ TCOLS = if tnewline then
;

\ where the cursor was, and where it is; and if lines further
\ back were shown, the end again
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

\ PgUp and PgDn: the text shown from n lines further back, as
\ far as there are lines.  A page is a window less a line, so
\ that the line at its edge is still to be seen.
: tback+ ( n -- )
    tback @ +  tlines TROWS - d# 0 max min  d# 0 max  tback !
    tall
;

\ ---- the Forth window

\ The window's Forth is a task of its own, with the upper half
\ of each stack, so what is on its data stack stays there from
\ one line to the next.  The desktop's task takes the keys: it
\ puts them in `line`, and at the end of a line says that one is
\ ready.  The Forth task runs it when its turn comes, with its
\ output going to the terminal, and the desktop goes on running
\ meanwhile for as long as the line says pause.  Keys typed
\ while a line is running are dropped, but for PgUp and PgDn.

create lcells #80 allot
: line ( -- a ) lcells >c ;
create llen 0 ,
create lready 0 ,  \ true while a line is waiting or running
create rtask  0 , 0 , 0 ,  0 , 0 , 0 , 0 ,  0 , 0 , 0 , 0 ,
create rgoing 0 ,  \ true once the task has been started

\ The prompt: ok, and a new line to type on.  The window opens
\ with one.
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

: lback ( -- ) \ the character before the cursor is taken back
    llen @ if d# -1 llen +! d# 8 term-emit then ;

: ladd ( c -- ) \ a character typed, if the line has room for it
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
