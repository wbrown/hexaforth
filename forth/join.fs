\ join.fs - the nucleus's peephole

\ An instruction being compiled is combined with the one or two
\ before it, if one instruction does the same (compose.fs).  No
\ instruction before syncpt, where a branch lands, is changed.
\ 7403 exit, 643c +, 6800 @, 6308 !, 6400 does nothing, 8000 a
\ literal pushed, c000 added, b000 f000 a variable (MODEL.md).

create jn 0 ,          \ the instruction being compiled

\ an ALU instruction with no return, that no branch lands after
: plain? ( ia -- f )
    dup syncpt @ u< if drop false exit then
    i@ h# f000 and h# 6000 =
;

include compose.fs

: join1 ( -- f ) \ jn and the two before, or the one, as one
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

: join, ( insn -- ) \ compiled, unless it comes to 6400
    jn !  begin join1 0= until
    jn @ dup h# 6400 = if drop exit then  code, ;

: lval ( insn -- n ) \ the number a literal has
    dup h# fff and swap d# 12 rshift d# 3 and d# 12 * lshift ;

: lit? ( insn kind -- f ) \ h# 8000 a push, h# c000 an add
    over h# c000 and =  swap h# 3000 and h# 3000 <>  and ;

\ the address just pushed, and how many instructions push it
: var-lit ( -- address n | 0 )
    prev i@ dup h# 8000 lit? if lval d# 1 exit then
    dup h# c000 lit?  prev 1- i@ h# 8000 lit?  and if
        lval prev 1- i@ lval + d# 2 exit
    then
    drop d# 0
;

: var, ( insn -- ) \ @ or ! after a variable's address is one
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

\ ---- numbers: code that only computes a number is run, and
\ the number compiled.  folds.fs lists the instructions that
\ compute one number from one or from two.

: folds ( -- ia ) folds-at @ ;

: makes ( w -- n ) \ numbers w computes one from: 1, 2 or 0
    folds d# 4 +  folds i@ folds 1+ i@ +  bounds    ( w end a )
    begin
        2dup xor
    while
        >r  over r@ i@ = if
            2drop  r> folds d# 4 + -  folds i@ u<  d# 2 +  exit
        then
        r> 1+
    repeat
    2drop drop d# 0
;

\ how many numbers are needed before w, going back: a push
\ gives one, and what `makes` n takes n and gives one
: wants ( wanted w -- wanted' true | false )
    dup h# 8000 lit? if drop 1- true exit then
    dup h# c000 lit? if drop true exit then
    makes ?dup 0= if drop false exit then
    1- + true
;

: number-at ( e -- s | 0 )
    \ where the number that ends at e starts
    d# 1 swap                                       ( wanted a )
    begin
        1-  dup syncpt @ u< if 2drop d# 0 exit then
        >r  r@ i@ wants 0= if rdrop d# 0 exit then  r>
        over 0=
    until
    nip
;

: number@ ( s e -- n ) \ run, with an exit put at e
    dup i@ >r  ( R: the instruction at e )
    cp @ >r  dup cp !  exit,  r> cp !
    >r execute
    r> r> swap i!
;

\ n replaces the code from s, if that is no longer
: renumber ( n s -- )
    cp @ >r  over tliteral  cp @ r@ -  r@ cp !
    ( n s len ) ( R: where the code ends )
    over r> swap - u> if 2drop exit then
    cp ! tliteral
;

: room? ( -- f ) \ past the code, for a number to be tried in
    cp @ h# fff and h# ff0 u< ;

: tidy ( -- f ) \ a number just compiled, as few instructions
    cp @ number-at dup if
        dup cp @ number@ swap renumber  true
    then
;

: fold ( -- f ) \ jn compiled, if what ends with it is a number
    jn @ code,  tidy dup 0= if d# -1 cp +! then
;

\ ---- after dup, r@ or r>, a push and an instruction are the
\ push and another instruction.  folds.fs has a set of rules
\ for each of the three, and says what a rule is.
: set-of ( x -- set | 0 ) \ the set for dup, r@ or r>
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

\ w as the first rule of the set that it matches changes it
: crossed ( w set -- y | 0 )
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

: pushed ( -- n ) \ how many instructions pushed what was pushed
    prev i@ dup h# c000 and h# 8000 = if drop d# 1 exit then
    h# c000 lit?  prev 1- i@ h# 8000 lit?  and  d# 2 and
;

\ the n instructions after the one at `at` go down over it
: push-down ( n at y -- end end )
    jn !  swap bounds                               ( end at )
    begin 2dup xor while
        dup 1+ i@ over i!  1+
    repeat
    d# -1 cp +!
;

\ a dup, r@ or r>, what is pushed and jn become what is pushed
\ and another instruction
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
\ The last set's instruction is = and each rule's third is
\ invert, so y is w xor invert and ( w y ) xor is invert.
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

: emit-lit ( w -- ) \ a literal: after an add, tidy
    dup code,  h# c000 lit? if room? if tidy drop then then ;

: add-to ( -- f ) \ jn, a +, is added to the number before it
    jn @ h# 643c <> if false exit then
    cp @ number-at ?dup 0= if false exit then
    dup cp @ number@                ( s n )
    dup d# 36 rshift if 2drop false exit then
    swap cp !  h# c000 segs,  true
;

: after-lit ( -- ) \ jn, with a literal before it
    jn @  dup h# 6800 = over h# 6308 = or if var, exit then
    drop  flag-not if exit then  across  jn @ code,
;

: emit0 ( w -- ) \ an instruction of a word compiled inline
    dup h# 8000 and if emit-lit exit then
    dup h# 1000 and if h# effc and then     \ without the return
    jn !
    prev syncpt @ u< 0= if
        room? if
            jn @ makes if fold if exit then then
            add-to if exit then
        then
        prev i@ h# 8000 and if after-lit exit then
    then
    jn @ join,
;

: lays ( xt n -- ) \ n instructions of a word, inline
    bounds begin 2dup xor while dup i@ emit0 1+ repeat 2drop ;

: quiet? ( w -- f ) \ does it neither read R nor write it?
    dup h# c00 and h# c00 <>  swap h# 300 and h# 100 <>  and ;

\ of a word inline: w goes on (0), is the last (1), or cannot
\ be inline (-1): a call, a branch, or what uses or moves R
: inline-kind ( w -- k )
    dup h# 8000 and if drop d# 0 exit then
    dup h# e000 and h# 6000 <> if drop d# -1 exit then
    dup h# 1000 and if
        dup quiet? swap d# 3 and d# 3 = and if d# 1 exit then
        d# -1 exit
    then
    dup quiet? swap d# 3 and 0= and if d# 0 exit then
    d# -1
;

\ how many instructions a word is inline, a bare exit not
\ counted, or -1 if it cannot be inline or is too long
: small ( xt -- n )
    d# 0 swap                                       ( n ia )
    begin
        over d# 3 > if 2drop d# -1 exit then
        dup i@                                      ( n ia w )
        dup h# 7403 = if 2drop exit then
        inline-kind ?dup if
            nip 0< if drop d# -1 exit then  1+ exit
        then
        1+ swap 1+ swap
    again
;

\ A small word is tried inline, and called if that is longer.
\ keep copies the eight instructions before it to 16 past cp
\ and moves syncpt up to them; fits puts them back.
: imove ( from to n -- ) \ instructions, lowest first
    >r over - r> swap >r  ( from n ) ( R: far )
    bounds begin 2dup xor while
        dup i@ over r@ + i!  1+
    repeat
    2drop rdrop ;

: roomy? ( -- f ) \ past the code, for what is kept
    cp @ h# fff and h# fe0 u< ;

: keep ( -- syncpt cp ) \ before a word is tried
    syncpt @ cp @
    dup d# 8 -  dup syncpt @ max syncpt !
    over d# 16 + d# 8 imove ;

\ true if no more than `more` instructions were added; if not,
\ the code is as it was
: fits ( syncpt cp more -- f )
    over + cp @ u< if
        dup cp !  dup d# 16 + swap d# 8 - d# 8 imove
        syncpt !  false exit
    then
    drop syncpt !  true ;

\ the word just called, to be tried again with what comes next
create waits 0 ,        \ its xt, or 0

\ w is what comes next.  If the waiting word's call is still
\ the last instruction, the word and w are tried inline in its
\ place.  True if w was compiled.
: again? ( w -- w false | true )
    waits @ ?dup 0= if false exit then      ( w xt )
    d# 0 waits !
    prev i@ over near h# 4000 or <>
    syncpt @ cp @ = or  roomy? 0= or if drop false exit then
    d# -1 cp +!
    2dup >r >r  keep 2swap                  ( syncpt cp w xt )
    dup small lays  emit0
    d# 2 fits if rdrop rdrop true exit then
    r> r> (compile,)  false ;

: emit1 ( w -- ) \ emit0, after a waiting word is tried again
    dup h# 8000 and 0= if
        dup h# 1000 and if h# effc and then
        again? if exit then
    then
    emit0 ;

\ compiles the instruction after the call of it, and returns to
\ its caller's caller: each word below compiles one instruction
: offered ( -- )
    r> i@ emit1 ;
inline-operand
header-imm >r      :noname  offered >r ;fallthru
header-imm r>      :noname  offered r> ;fallthru
header-imm r@      :noname  offered r@ ;fallthru
header-imm rdrop   :noname  offered rdrop ;fallthru

\ an exit: texit makes a call before it a jump; an exit it
\ compiles instead is taken back and given to join,
: returns ( -- )
    cp @  texit  cp @ = if exit then
    d# -1 cp +!  h# 7403 join, ;
header-imm exit    :noname  returns ;
header-imm ;       :noname  returns attach t[ ;

: placed ( xt n -- ) \ inline, whatever comes of it
    dup 0= if 2drop exit then
    over i@ emit1  1- swap 1+ swap lays ;

: number? ( xt n -- f ) \ does the word only push a number?
    over i@ h# 8000 lit? >r  + 1- i@ h# 8000 and  r> and ;

: near? ( xt -- f ) \ does one instruction call it from here?
    dup d# 12 rshift 0=  swap cp @ xor d# 12 rshift 0=  or ;

create placing 0 ,      \ true after in-place
header-imm in-place  :noname  true placing ! ;

\ a trial is for a word of two instructions or more that one
\ instruction calls, not a number and not after in-place; any
\ other small word is inlined outright
: tried? ( xt n -- f )
    placing @  d# 0 placing !  if 2drop false exit then
    dup d# 2 < if 2drop false exit then
    over near? 0= if 2drop false exit then
    number? 0=  roomy? and ;

\ a call, if xt is being defined or is not small; inline, by
\ a trial if tried? says.  A word whose trial failed waits.
: pcompile, ( xt -- )
    dup thisxt @ = if (compile,) exit then
    dup small dup 0< if drop (compile,) exit then   ( xt n )
    2dup tried? 0= if placed exit then
    d# 0 waits !
    over >r  keep 2swap lays
    d# 1 fits if rdrop exit then
    r> dup (compile,) waits ! ;
