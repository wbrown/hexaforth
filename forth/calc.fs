\ calc.fs - an RPN calculator
\
\ Digits build a number.  Enter pushes it, or duplicates the top if no
\ number is being entered.  + - * / work on the top two, n negates, x swaps
\ and d drops.  The buttons send the same keys.

create ci 0 ,           \ a loop counter

create cst #128 allot
create cn 0 ,
create cacc 0 ,
create centry 0 ,
create cbtn ," 789/456*123-0nE+dx< "

: cpush ( x -- )
    cn @ d# 15 < if cn @ cells cst + !  d# 1 cn +! else drop then
;

: cpop ( -- x )
    cn @ if d# -1 cn +!  cn @ cells cst + @ else d# 0 then
;

: centered ( -- ) \ push the number being entered, if there is one
    centry @ if cacc @ cpush  d# 0 cacc !  d# 0 centry ! then
;

: sdiv ( a b -- q ) \ signed, towards zero; anything over zero is zero
    dup 0= if 2drop d# 0 exit then
    2dup xor >r
    abs swap abs swap u/mod nip
    r> 0< if negate then
;

header calc-key : calc-key ( c win -- )
    drop
    dup [char] 0 - d# 10 u< if
        [char] 0 - cacc @ d# 10 * + cacc !
        true centry !
        exit
    then
    dup d# 13 = over d# 32 = or if
        drop
        centry @ if
            centered
        else
            cn @ if cpop dup cpush cpush then
        then
        exit
    then
    dup [char] + = if drop centered cpop cpop + cpush exit then
    dup [char] - = if drop centered cpop cpop swap - cpush exit then
    dup [char] * = if drop centered cpop cpop * cpush exit then
    dup [char] / = if drop centered cpop cpop swap sdiv cpush exit then
    dup [char] n = if drop centered cpop negate cpush exit then
    dup [char] x = if drop centered cpop cpop swap cpush cpush exit then
    dup [char] d = if
        drop
        centry @ if d# 0 cacc !  d# 0 centry ! else cpop drop then
        exit
    then
    dup d# 8 = if drop cacc @ d# 10 u/mod nip cacc ! exit then
    drop
;

header calc-draw : calc-draw ( win -- )
    drop
    d# 0 ci !                                       \ the top four, oldest first
    begin
        ci @ d# 4 <
    while
        cn @ d# 4 - ci @ + dup 0< 0= if
            sb-begin cells cst + @ . sb-end
            ox @ d# 6 +  oy @ d# 4 + ci @ d# 4 lshift +  sb-draw
        else
            drop
        then
        d# 1 ci +!
    repeat
    sb-begin
    [char] > emit space
    centry @ if cacc @ . then
    sb-end
    ox @ d# 6 + oy @ d# 72 + sb-draw
    false ox @ oy @ d# 92 + d# 136 hline
    d# 0 ci !                                       \ 4 x 5 buttons of 34 x 24
    begin
        ci @ d# 20 <
    while
        false
        ox @ ci @ d# 3 and d# 34 * +
        oy @ d# 96 + ci @ d# 2 rshift d# 24 * +
        d# 33 d# 23 frame-rect
        cbtn 1+ ci @ + c@
        ox @ ci @ d# 3 and d# 34 * + d# 13 +
        oy @ d# 96 + ci @ d# 2 rshift d# 24 * + d# 4 +
        draw-char
        d# 1 ci +!
    repeat
;

header calc-click : calc-click ( x y win -- )
    drop
    d# 96 - dup 0< if 2drop exit then
    d# 24 u/mod nip dup d# 4 > if 2drop exit then   ( x row )
    swap dup 0< if 2drop exit then
    d# 34 u/mod nip d# 3 min                        ( row col )
    swap d# 2 lshift + cbtn 1+ + c@
    dup [char] E = if drop d# 13 then
    dup [char] < = if drop d# 8 then
    d# 0 calc-key
;
