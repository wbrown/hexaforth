\ calc.fs - an RPN calculator

\ Enter pushes the number, or the top again; n negates, x swaps
\ and d drops.

create ci 0 ,           \ a loop counter

create cst #128 allot
create cn 0 ,
create cacc 0 ,
create centry 0 ,
create cbtn ," 789/456*123-0nE+dx< "  \ the buttons' keys

: button ( i -- c )
    1+ d# 3 lshift cbtn swap char@ ;

: cpush ( x -- )
    cn @ d# 15 < if
        cn @ cells cst + !  d# 1 cn +!
    else
        drop
    then
;

: cpop ( -- x )
    cn @ if d# -1 cn +!  cn @ cells cst + @ else d# 0 then
;

: centered ( -- ) \ the number being entered is pushed
    centry @ if cacc @ cpush  d# 0 cacc !  d# 0 centry ! then
;

: sdiv ( a b -- q ) \ towards zero; anything over zero is zero
    dup 0= if 2drop d# 0 exit then
    2dup xor >r
    abs swap abs swap u/mod nip
    r> 0< if negate then
;

: cdigit ( n -- )
    cacc @ d# 10 * + cacc !  true centry ! ;

: center ( -- )
    centry @ if centered exit then
    cn @ if cpop dup cpush cpush then
;

: ctwo ( -- a b ) centered cpop cpop swap ;

: cdrop ( -- ) \ the number being entered, or else the top
    centry @ if d# 0 cacc !  d# 0 centry ! exit then
    cpop drop
;

: calc-do ( c -- )
    dup [char] 0 - d# 10 u< if [char] 0 - cdigit exit then
    dup d# 13 = over d# 32 = or if drop center exit then
    dup [char] + = if drop ctwo + cpush exit then
    dup [char] - = if drop ctwo - cpush exit then
    dup [char] * = if drop ctwo * cpush exit then
    dup [char] / = if drop ctwo sdiv cpush exit then
    dup [char] n = if drop centered cpop negate cpush exit then
    dup [char] x = if drop ctwo cpush cpush exit then
    dup [char] d = if drop cdrop exit then
    dup d# 8 = if drop cacc @ d# 10 u/mod nip cacc ! exit then
    drop
;

header calc-key : calc-key ( c win -- )
    >r calc-do
    d# 0 d# 0 d# 136 d# 92 r> win-inval
;

: cstack ( -- ) \ the top four numbers, oldest first
    d# 0 ci !
    begin
        ci @ d# 4 <
    while
        cn @ d# 4 - ci @ + dup 0< 0= if
            sb-begin cells cst + @ . sb-end
            d# 6  d# 4 ci @ d# 4 lshift +  sb-draw
        else
            drop
        then
        d# 1 ci +!
    repeat
;

: cline ( -- ) \ the number being entered
    sb-begin
    [char] > emit space
    centry @ if cacc @ . then
    sb-end
    d# 6 d# 72 sb-draw
;

: cbutton-at ( i -- x y ) \ where button i's frame is
    dup d# 3 and d# 34 *  swap d# 2 rshift d# 24 * d# 96 + ;

: cbuttons ( -- ) \ 4 x 5 buttons of 34 x 24
    d# 0 ci !
    begin
        ci @ d# 20 <
    while
        false  ci @ cbutton-at  d# 33 d# 23 frame-rect
        ci @ button  ci @ cbutton-at d# 4 + swap d# 13 + swap
        draw-char
        d# 1 ci +!
    repeat
;

header calc-draw : calc-draw ( win -- )
    drop  cstack  cline
    false d# 0 d# 92 d# 136 hline
    cbuttons
;

create cwin 0 ,

header calc-click : calc-click ( x y win -- )
    cwin !
    d# 96 - dup 0< if 2drop exit then
    d# 24 u/mod nip dup d# 4 > if 2drop exit then   ( x row )
    swap dup 0< if 2drop exit then
    d# 34 u/mod nip d# 3 min                        ( row col )
    swap d# 2 lshift + button
    dup [char] E = if drop d# 13 then
    dup [char] < = if drop d# 8 then
    cwin @ calc-key
;
