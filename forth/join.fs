\ join.fs - the nucleus's peephole
\
\ What is typed is plain words: `2dup xor`, `over +`, `dup @`.  Each of
\ those is one instruction of this machine, and this is what makes it one:
\ an instruction about to be compiled is offered to the one or two before
\ it, and if there is a rule for them they become one, which is then
\ offered again.  So the source is what reads well and the code is what the
\ machine does best, and nobody needs a name for the instruction.
\
\ The rules are in joins.fs, which the build makes from the image: they
\ are the decompiler's table the other way round.  A rule is four
\ instructions, in order: nothing or the instruction two before, the one
\ before, the one being compiled, and what they are together.  Every
\ instruction a rule gives is one the image itself has, so the decompiler
\ can show whatever is compiled here, as the words that were typed.
\
\ Nothing is joined across a place a branch lands on (syncpt), or to an
\ instruction with a return in it.  A word of up to three instructions
\ that has no call or branch and leaves the return stack alone is compiled
\ in place, so that its instructions can be joined: 2dup is two.
\
\ This is compiled into code page 1, not the kernel, whose ROM is nearly
\ full: the kernel's compile, goes through a vector, which the build sets
\ to pcompile, here.

create jk 0 , 0 , 0 ,   \ the three instructions looked for
create jlo 0 ,          \ where the rules still to look in start,
create jn# 0 ,          \ and how many they are
create jn 0 ,           \ the instruction being compiled

: jcmp ( rule -- n ) \ less than nothing if the rule is before what is looked for, nothing if it is it
    dup i@ jk @ - ?dup if nip exit then
    dup 1+ i@ jk CELL + @ - ?dup if nip exit then
    d# 2 + i@ jk CELL2 + @ -
;

: jfind ( x a b -- y|0 ) \ by halving: the rules are in order
    jk CELL2 + !  jk CELL + !  jk !
    join-rules @ jlo !  join-rules CELL + @ jn# !
    begin
        jn# @
    while
        jn# @ d# 1 rshift                           ( half )
        dup d# 2 lshift jlo @ +                     ( half rule )
        dup jcmp                                    ( half rule n )
        dup 0= if drop nip d# 3 + i@ exit then
        0< if                                       \ it is after this rule,
            drop  dup 1+ d# 2 lshift jlo +!  invert jn# +!
        else                                        \ or before it
            drop  jn# !
        then
    repeat
    d# 0
;

: plain? ( ia -- f ) \ an ALU instruction with no return, that no branch lands after
    dup syncpt @ u< if drop false exit then
    i@ h# f000 and h# 6000 =
;

\ The two before are tried first: a rule for three leaves less than one for
\ two, and `2dup xor` is three (over over xor), of which the last two alone
\ are `over xor`.
: join1 ( -- f ) \ join the instruction in jn to the two before, or the one, if there is a rule
    prev plain? 0= if false exit then
    prev 1- plain? if
        prev 1- i@ prev i@ jn @ jfind ?dup if
            jn !  d# -2 cp +!  true exit
        then
    then
    d# 0 prev i@ jn @ jfind ?dup if
        jn !  d# -1 cp +!  true exit
    then
    false
;

: join, ( insn -- ) jn !  begin join1 0= until  jn @ code, ;

\ One instruction of a word that is being compiled in place.
: emit1 ( w -- )
    dup h# 8000 and if code, exit then              \ a literal
    dup h# 1000 and if h# effc and then             \ without the return
    prev syncpt @ u< 0= if
        prev i@ h# 8000 and if                      \ after a literal:
            dup h# 643c =  prev i@ h# 8000 lit?  and if     \ + makes the push an add,
                drop  prev i@ h# 4000 or prev i!  exit
            then
            dup h# 6800 = over h# 6308 = or if var, exit then   \ @ or ! may be a variable's,
            code, exit                              \ and nothing else is joined to it
        then
    then
    join,
;

\ Does an instruction leave the return stack alone but for how it moves?
: quiet? ( w -- f ) dup h# c00 and h# c00 <>  swap h# 300 and h# 100 <>  and ;

: small ( xt -- n ) \ how many instructions, if the word can be compiled in place; or nothing
    d# 0 swap                                       ( n ia )
    begin
        over d# 3 > if 2drop d# 0 exit then
        dup i@                                      ( n ia w )
        dup h# 7403 = if 2drop exit then            \ its exit
        dup h# 8000 and 0= if
            dup h# e000 and h# 6000 <> if drop 2drop d# 0 exit then    \ a call or a branch
            dup h# 1000 and if                      \ the return is in it: the last
                dup quiet? swap d# 3 and d# 3 = and if drop 1+ exit then
                2drop d# 0 exit
            then
            dup quiet? swap d# 3 and 0= and 0= if 2drop d# 0 exit then
        else
            drop
        then
        1+ swap 1+ swap
    again
;

: pcompile, ( xt -- )
    dup thisxt @ = if (compile,) exit then          \ the word being made is not all there
    dup small ?dup 0= if (compile,) exit then       ( xt n )
    bounds
    begin
        2dupxor
    while
        dup i@ emit1
        1+
    repeat
    2drop
;

\ The rules come last, so that they are the end of what their page has:
\ they are not code, and nothing that reads the image as code goes that far.
include joins.fs
