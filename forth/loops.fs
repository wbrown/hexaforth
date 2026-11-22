\ loops.fs - loops that end other ways than by repeat: a while
\ ended by until, and a loop with two whiles.  321B0 is what it
\ is to print (the test `loops`).
meta
    4 org
target

\ counts n down to 0 or to 2, whichever is first
: to-two ( n -- n' ) begin dup while 1- dup d# 2 = until then ;
\ counts n down while it is odd and not 0: 64 more than what it
\ stopped at, or 0 if it was given 0
: odds ( n -- n' )
    begin dup while dup d# 1 and while 1- repeat d# 64 + then ;

header main : main
    d# 5 to-two [char] 1 + emit         \ 2, so 3
    d# 1 to-two [char] 2 + emit         \ 0, so 2
    d# 0 to-two [char] 1 + emit         \ 0, so 1
    d# 3 odds emit                      \ stops at 2: 66, which is B
    d# 0 odds [char] 0 + emit           \ 0: it left by the first while
    d# 10 emit
    halt
;
header quit : quit halt ;
