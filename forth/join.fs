\ join.fs - the nucleus's peephole

\ What is typed is plain words: `2dup xor`, `over +`, `dup @`.
\ Each of those is one instruction of this machine, and this is
\ what makes it one: an instruction about to be compiled is
\ offered to the one or two before it, and if they are together
\ what one instruction does they become that one, which is then
\ offered again.  So the source is what reads well and the code
\ is what the machine does best, and nobody needs a name for the
\ instruction.

\ What they are together is not looked up: it is worked out, by
\ stepping them on a machine whose cells hold where each thing
\ came from (compose.fs).  The targets `pairs` and `peephole`
\ hold what it works out to the machine's own step.

\ Nothing is joined across a place a branch lands on (syncpt),
\ or to an instruction with a return in it.  A word of up to
\ three instructions that has no call or branch and leaves the
\ return stack alone is compiled in place, so that its
\ instructions can be joined: 2dup is two.

\ This is the first thing on code page 1, straight after the
\ kernel, whose ROM is nearly full: the kernel's compile, goes
\ through a vector, which the build sets to pcompile, here.  It
\ uses nothing but the kernel.

create jn 0 ,          \ the instruction being compiled

\ an ALU instruction with no return, that no branch lands after
: plain? ( ia -- f )
    dup syncpt @ u< if drop false exit then
    i@ h# f000 and h# 6000 =
;

\ What two instructions are together, and three, is worked out
\ and not looked up (compose.fs).  The two before are tried
\ first: three that are one instruction leave less than two
\ that are.  What is made is offered again.
include compose.fs

\ join the instruction in jn to the two before, or the one, if
\ they are one instruction
: join1 ( -- f )
    prev plain? 0= if false exit then
    prev 1- plain? if
        prev 1- i@ prev i@ jn @ jthree ?dup if
            jn !  d# -2 cp +!  true exit
        then
    then
    prev i@ jn @ jtwo ?dup if
        jn !  d# -1 cp +!  true exit
    then
    false
;

\ What is left may be the instruction that does nothing, as
\ `>r r>` is together: then nothing is compiled, as cross.fs
\ compiles nothing for it.
: join, ( insn -- )
    jn !  begin join1 0= until
    jn @ dup h# 6400 = if drop exit then  code, ;

\ @ or ! straight after a literal that is the address of one of
\ the 4,096 cells from VARS: the machine has one instruction for
\ the two together.  The literal is one instruction, or a push
\ and an add.  Nothing is joined across a place a branch
\ lands on.

\ These three are here and not in the kernel because only this
\ file calls them: the kernel's own (compile,) compiles a call
\ and nothing else.
: lval ( insn -- n )
    dup h# fff and swap d# 12 rshift d# 3 and d# 12 * lshift ;

\ Is it a literal of this kind: h# 8000 for a push, h# c000 for
\ an add?
: lit? ( insn kind -- f )
    over h# c000 and =  swap h# 3000 and h# 3000 <>  and ;

\ the address the literal just compiled pushes, and how many
\ instructions it is (a push, or a push and an add); or nothing
\ if the instruction before is not such a literal
: var-lit ( -- address n | 0 )
    prev i@ dup h# 8000 lit? if lval d# 1 exit then
    dup h# c000 lit?  prev 1- i@ h# 8000 lit?  and if
        lval prev 1- i@ lval + d# 2 exit
    then
    drop d# 0
;

: var, ( insn -- )
    >r
    var-lit ?dup 0= if r> code, exit then
    ( address instructions )
    cp @ over - syncpt @ <
    rot VARS - dup d# 4095 u> rot or if  ( instructions place )
        2drop r> code, exit
    then
    swap negate cp +!
    r> h# 6308 = if h# f000 else h# b000 then or code,
;

\ Numbers.  Most of what is typed has a number in it, and
\ nothing is joined to a number by a rule.  But the nucleus is
\ on the machine: what some instructions make of numbers it
\ finds by running them, and compiles the number that comes of
\ it.  `16 3 lshift` is 128, and `100 30 - 2 *` is 140.  What
\ was just compiled is a number if it is pushes, and
\ instructions that make one number of one or of two, and leaves
\ one: so it is read back from the code, and nothing is kept
\ beside it.

\ Which instructions make one number of one or of two and touch
\ nothing else, and what a dup, a push and an instruction are
\ together (`dup 16 rshift` is the push and one instruction),
\ the machine was asked when this was built: folds.fs, which
\ says its own form.

: folds ( -- ia ) folds-at @ ;

\ how many numbers an instruction makes one of: one, two, or
\ none if it is not such
: makes ( w -- n )
    folds d# 4 +  folds i@ folds 1+ i@ +  bounds    ( w end a )
    begin
        2dup xor
    while
        >r  over r@ i@ = if
            \ those of one are first
            2drop  r> folds d# 4 + -  folds i@ u<  d# 2 +  exit
        then
        r> 1+
    repeat
    2drop drop d# 0
;

\ What ends at e is a number if, going back from there, every
\ instruction is a push, or adds to what is there, or makes one
\ number of one or of two, and at some push nothing more is
\ wanted: that is where it starts.
\ How many numbers are wanted before instruction w, going back:
\ one fewer before a push, as many before an add, and one more
\ before one that makes one number of two; or, before any other,
\ there is no number.
: wants ( wanted w -- wanted' true | false )
    dup h# 8000 lit? if drop 1- true exit then
    dup h# c000 lit? if drop true exit then
    makes ?dup 0= if drop false exit then
    1- + true
;

: number-at ( e -- s | 0 )
    d# 1 swap                                       ( wanted a )
    begin
        1-  dup syncpt @ u< if 2drop d# 0 exit then
        >r  r@ i@ wants 0= if rdrop d# 0 exit then  r>
        over 0=
    until
    nip
;

\ What number it is, the machine is asked: an exit is put where
\ it ends, and it is run.
: number@ ( s e -- n )
    dup i@ >r  ( R: what is where it ends )
    cp @ >r  dup cp !  exit,  r> cp !
    >r execute
    r> r> swap i!
;

\ The number n in place of the code from s, if it is no more
\ instructions than are there: it is compiled past the end
\ first, to see how many it is.
: renumber ( n s -- )
    cp @ >r  over tliteral  cp @ r@ -  r@ cp !
    ( n s len ) ( R: where the code ends )
    \ more: what is there stays
    over r> swap - u> if 2drop exit then
    cp ! tliteral
;

: room? ( -- f ) \ past the code, for a number to be tried in
    cp @ h# fff and h# ff0 u< ;

\ if what was just compiled is a number, it is made as few
\ instructions as it can be
: tidy ( -- f )
    cp @ number-at dup if
        dup cp @ number@ swap renumber  true
    then
;

\ jn is compiled: if what ends with it is a number that is the
\ end of it, and if not it is taken off again
: fold ( -- f )
    jn @ code,  tidy dup 0= if d# -1 cp +! then
;

\ The rules are in sets, one for each instruction a push is
\ taken across (dup, r@, r>): the instruction, how many rules,
\ and the rules.
: set-of ( x -- set | 0 ) \ the set for that instruction
    folds dup i@ over 1+ i@ + + d# 4 +              ( x set )
    folds d# 2 + i@ negate                          ( x set -n )
    begin
        dup
    while
        >r  2dup i@ = if rdrop nip exit then
        dup 1+ i@ d# 3 * + d# 2 +  r> 1+
    repeat
    drop 2drop d# 0
;

: crossed ( w set -- y | 0 )
    \ what w is after that set's instruction and a push
    dup d# 2 +  swap 1+ i@ d# 3 *  bounds  ( w end rule )
    begin
        2dup xor
    while
        >r  over r@ i@ and  r@ 1+ i@ = if
            drop  r> d# 2 + i@ xor  exit
        then
        r> d# 3 +
    repeat
    2drop drop d# 0
;

\ How many instructions what was just pushed is: a number or a
\ variable fetched is one, and a number that is a push and an
\ add is two.
: pushed ( -- n )
    prev i@ dup h# c000 and h# 8000 = if drop d# 1 exit then
    h# c000 lit?  prev 1- i@ h# 8000 lit?  and  d# 2 and
;

\ What is pushed, the n instructions after the one at `at`, goes
\ down over that instruction, and jn is y.
: push-down ( n at y -- end end )
    jn !  swap bounds                               ( end at )
    begin 2dup xor while
        dup 1+ i@ over i!  1+
    repeat
    d# -1 cp +!
;

\ a dup, what is pushed and jn are what is pushed and another
\ instruction: make them so.  So are an r@ or an r> in the dup's
\ place: r> 8 rshift is the 8 pushed, and a shift that has the
\ return stack for its input.
: across ( -- )
    pushed ?dup 0= if exit then                     ( n )
    prev over -                                     ( n at )
    dup syncpt @ u< 0= if
        dup i@ set-of ?dup if                       ( n at set )
            jn @ swap crossed ?dup if push-down then
        then
    then
    2drop
;

\ `0 =` after an instruction that leaves a flag is invert.
\ Which leave one is the last set of what is told (folds.fs): it
\ is the set of `=`, and what its rules change is `invert`.
\ True if jn, an `=` after a push of nothing, was compiled so.
: flag-set ( -- set ) folds dup d# 3 + i@ + ;
: flag-not ( -- f )
    prev i@ h# 8000 =  flag-set i@ jn @ =  and if
        prev 1- plain? if
            prev 1- i@ dup flag-set crossed ?dup if ( w y )
                xor  d# -1 cp +!  join,  true exit
            then
            drop
        then
    then
    false
;

\ A literal, which an add may make one number with those before.
: emit-lit ( w -- )
    dup code,  h# c000 lit? if room? if tidy drop then then ;

\ a number that jn, a +, comes after is added to it: true if it
\ was
: add-to ( -- f )
    jn @ h# 643c <> if false exit then
    cp @ number-at ?dup 0= if false exit then
    dup cp @ number@                ( s n )
    dup d# 36 rshift if 2drop false exit then
    swap cp !  h# c000 segs,  true
;

\ After a literal: @ or ! may be a variable's; `0 =` may follow
\ a flag; a dup before it may be taken up; and nothing else is
\ joined to it.
: after-lit ( -- )
    jn @  dup h# 6800 = over h# 6308 = or if var, exit then
    drop  flag-not if exit then  across  jn @ code,
;

\ One instruction of a word that is being compiled in place.
: emit1 ( w -- )
    dup h# 8000 and if emit-lit exit then
    dup h# 1000 and if h# effc and then     \ without the return
    jn !
    prev syncpt @ u< 0= if
        room? if
            \ it and the numbers before it are a number;
            jn @ makes if fold if exit then then
            add-to if exit then
        then
        prev i@ h# 8000 and if after-lit exit then
    then
    jn @ join,
;

\ A word that moves the return stack cannot be called: it is
\ immediate, and where it is written it offers its one
\ instruction to the one before, as emit1 offers any other.
: offered ( -- ) \ the instruction after the call of this
    r> i@ emit1 ;
inline-operand
header-imm >r      :noname  offered >r ;fallthru
header-imm r>      :noname  offered r> ;fallthru
header-imm r@      :noname  offered r@ ;fallthru
header-imm rdrop   :noname  offered rdrop ;fallthru

\ Does an instruction leave the return stack alone but for how
\ it moves?
: quiet? ( w -- f )
    dup h# c00 and h# c00 <>  swap h# 300 and h# 100 <>  and ;

\ What instruction w of a word says of compiling the word in
\ place: it goes on (0), it returns as it is and is the last
\ (1), or it cannot be (-1): a call or a branch, or one that
\ moves the return stack otherwise.
: inline-kind ( w -- k )
    dup h# 8000 and if drop d# 0 exit then  \ a literal
    dup h# e000 and h# 6000 <> if drop d# -1 exit then
    dup h# 1000 and if                  \ the return is in it
        dup quiet? swap d# 3 and d# 3 = and if d# 1 exit then
        d# -1 exit
    then
    dup quiet? swap d# 3 and 0= and if d# 0 exit then
    d# -1
;

\ how many instructions, if the word can be compiled in place,
\ which for a word that is only its exit is none (cells); or
\ less than nothing
: small ( xt -- n )
    d# 0 swap                                       ( n ia )
    begin
        over d# 3 > if 2drop d# -1 exit then
        dup i@                                      ( n ia w )
        dup h# 7403 = if 2drop exit then            \ its exit
        inline-kind ?dup if
            nip 0< if drop d# -1 exit then  1+ exit
        then
        1+ swap 1+ swap
    again
;

: pcompile, ( xt -- )
    \ the word being made is not all there
    dup thisxt @ = if (compile,) exit then
    dup small dup 0< if drop (compile,) exit then   ( xt n )
    bounds
    begin
        2dup xor
    while
        dup i@ emit1
        1+
    repeat
    2drop
;

\ What is told of numbers (folds.fs) is a table, and whoever
\ includes this includes that after all the code of its page:
\ nothing that reads the image as code goes as far as a table.
