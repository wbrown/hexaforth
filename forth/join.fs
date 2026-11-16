\ join.fs - the nucleus's peephole
\
\ What is typed is plain words: `2dup xor`, `over +`, `dup @`.  Each of
\ those is one instruction of this machine, and this is what makes it one:
\ an instruction about to be compiled is offered to the one or two before
\ it, and if there is a rule for them they become one, which is then
\ offered again.  So the source is what reads well and the code is what the
\ machine does best, and nobody needs a name for the instruction.
\
\ The rules are in joins.fs, which says which they are and why those.  A
\ rule is four instructions, in order: nothing or the instruction two
\ before, the one before, the one being compiled, and what they are
\ together.  Every instruction a rule gives is one the image itself has,
\ so the decompiler can show whatever is compiled here, as the words that
\ were typed.
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

\ Each rule has a count of how often it has been used since the image was
\ made, so that it can be seen which rules earn their place.  The counts
\ are after the rules and in their order, an instruction's place each:
\ join-rules' fourth cell says where.
: used ( rule -- ) \ count it
    join-rules @ - d# 2 rshift  join-rules c# 3 + @ +
    dup i@ 1+ swap i!
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
        dup 0= if drop nip  dup used  d# 3 + i@ exit then
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

\ Numbers.  Most of what is typed has a number in it, and nothing is joined
\ to a number by a rule.  But the nucleus is on the machine: what some
\ instructions make of numbers it finds by running them, and compiles the
\ number that comes of it.  `16 3 lshift` is 128, and `100 30 - 2 *` is
\ 140.  What was just compiled is a number if it is pushes, and
\ instructions that make one number of one or of two, and leaves one: so
\ it is read back from the code, and nothing is kept beside it.
\
\ Which instructions make one number of one or of two and touch nothing
\ else, and what a dup, a push and an instruction are together (`dup 16
\ rshift` is the push and one instruction), the machine was asked when
\ this was built: folds.fs, which says its own form.

: folds ( -- ia ) join-rules CELL2 + @ ;

: makes ( w -- n ) \ how many numbers an instruction makes one of: one, two, or none if it is not such
    folds d# 4 +  folds i@ folds 1+ i@ +  bounds    ( w end a )
    begin
        2dupxor
    while
        >r  over r@ i@ = if
            2drop  r> folds d# 4 + -  folds i@ u<  d# 2 +  exit     \ those of one are first
        then
        r> 1+
    repeat
    2drop drop d# 0
;

\ What ends at e is a number if, going back from there, every instruction
\ is a push, or adds to what is there, or makes one number of one or of
\ two, and at some push nothing more is wanted: that is where it starts.
: number-at ( e -- s | 0 )
    d# 1 swap                                       ( wanted a )
    begin
        1-  dup syncpt @ u< if 2drop d# 0 exit then
        dup i@                                      ( wanted a w )
        dup h# 8000 lit? if
            drop swap 1- swap                       \ a push: one fewer is wanted
        else
            dup h# c000 lit? if
                drop                                \ it adds to one
            else
                makes  dup 0= if nip nip exit then  \ it is neither: there is no number
                1- rot + swap                       \ and for one of two, one more is wanted
            then
        then
        over 0=
    until
    nip
;

\ What number it is, the machine is asked: an exit is put where it ends,
\ and it is run.
: number@ ( s e -- n )
    dup i@ >r                                       ( R: what is where it ends )
    cp @ >r  dup cp !  exit,  r> cp !
    >r execute
    r> r> swap i!
;

\ The number n in place of the code from s, if it is no more instructions
\ than are there: it is compiled past the end first, to see how many it is.
: renumber ( n s -- )
    cp @ >r  over tliteral  cp @ r@ -  r@ cp !      ( n s len ) ( R: where the code ends )
    over r> swap - u> if 2drop exit then            \ more: what is there stays
    cp ! tliteral
;

: room? ( -- f ) cp @ h# fff and h# ff0 u< ;        \ past the code, for a number to be tried in

: tidy ( -- f ) \ if what was just compiled is a number, it is made as few instructions as it can be
    cp @ number-at dup if  dup cp @ number@ swap renumber  true  then
;

: fold ( -- f ) \ jn is compiled: if what ends with it is a number that is the end of it, and if not it is taken off again
    jn @ code,  tidy dup 0= if d# -1 cp +! then
;

: crossed ( w -- y | 0 ) \ what w is after a dup and a push, by the rules
    folds dup i@ over 1+ i@ + + d# 4 +              ( w rules )
    folds d# 2 + i@ d# 3 *  bounds
    begin
        2dupxor
    while
        >r  over r@ i@ and  r@ 1+ i@ = if
            drop  r> d# 2 + i@ xor  exit
        then
        r> d# 3 +
    repeat
    2drop drop d# 0
;

: across ( -- ) \ a dup, an instruction that pushes and jn are that instruction and another: make them so
    prev 1- syncpt @ u< if exit then
    prev i@ h# c000 and h# 8000 <> if exit then     \ what pushes: a number, or a variable fetched
    prev 1- i@  folds d# 3 + i@ <> if exit then
    jn @ crossed ?dup 0= if exit then  jn !
    prev i@ prev 1- i!  d# -1 cp +!
;

\ One instruction of a word that is being compiled in place.
: emit1 ( w -- )
    dup h# 8000 and if                              \ a literal
        dup code,  h# c000 lit? if room? if tidy drop then then  exit
    then
    dup h# 1000 and if h# effc and then             \ without the return
    jn !
    prev syncpt @ u< 0= if
        room? if
            jn @ makes if fold if exit then then    \ it and the numbers before it are a number;
            jn @ h# 643c = if                       \ a number that + comes after is added;
                cp @ number-at ?dup if
                    dup cp @ number@                ( s n )
                    dup d# 36 rshift 0= if  swap cp !  h# 4000 segs,  exit  then
                    2drop
                then
            then
        then
        prev i@ h# 8000 and if                      \ and after a literal:
            jn @  dup h# 6800 = over h# 6308 = or if var, exit then  drop  \ @ or ! may be a variable's;
            across                                  \ a dup before it may be taken up;
            jn @ code, exit                         \ and nothing else is joined to it
        then
    then
    jn @ join,
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
meta                                                \ a count for each rule
    t' join-rules 1+ byte-shift lshift tcell@
    dup table-room  tcp @ 2/ t' join-rules 3 + tvar!
    0 swap is,
target
include folds.fs
