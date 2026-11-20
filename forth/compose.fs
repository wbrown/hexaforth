\ compose.fs - what two instructions are together, worked out

\ An instruction is its fields: where the ALU's input is from,
\ the operation, where the result goes, and how each stack
\ moves.  Two are one instruction when one of them only moves
\ cells about, so that what the other makes is still made of
\ what one instruction can reach, and what is left is as one
\ instruction leaves it.  That is not looked up here: it is
\ worked out.  The two are run on a machine whose cells hold
\ not numbers but where each came from; the instruction that
\ would leave the same is made from what is left, and run on
\ that machine too; and if the two machines are left the same,
\ that instruction is the two.

\ The machine is vm.c's, as vm_step.h says it, and no more of
\ it than an instruction of the ALU that does not return.  The
\ target `pairs` puts every pair of instructions the nucleus
\ can meet to this and to the step itself (util/pairs.c).

meta
: low-bit ( mask -- n )
    0 swap begin dup 1 and 0= while 2/ swap 1+ swap repeat
    drop ;
target

\ where each field is, and the operations that are looked at
meta R->IN low-bit target constant IN-AT
meta 0 io[IN] low-bit target constant OP-AT
meta 0 ->[T] low-bit target constant OUT-AT
meta 0 d-1 low-bit target constant D-AT
meta 0 T<>N,IN-> 0 io[IN] low-bit rshift target
    constant OP-SWAP
meta 0 T->N,IN-> 0 io[IN] low-bit rshift target
    constant OP-UNDER
meta 0 ~IN 0 io[IN] low-bit rshift target constant OP-NOT
meta 0 IN==N 0 io[IN] low-bit rshift target constant OP-SAME
meta 0 IN>>T 0 io[IN] low-bit rshift target constant OP-SHIFT
meta 0 [IN] 0 io[IN] low-bit rshift target constant OP-LOAD
meta 0 io[IN] 0 io[IN] low-bit rshift target constant OP-IO
meta 0 ->[T] 0 ->[T] low-bit rshift target constant OUT-MEM

: w-in ( w -- n ) IN-AT rshift d# 3 and ;
: w-op ( w -- n ) OP-AT rshift d# 15 and ;
: w-out ( w -- n ) OUT-AT rshift d# 3 and ;
\ how far a stack moves: of the field's four, 2 and 3 are two
\ and one down
: w-moves ( field -- n ) d# 3 and  dup d# 2 and 2* - ;
: w-d ( w -- n ) D-AT rshift w-moves ;

\ The machine: the two depths, T and R, where it stored and
\ what, where it fetched from, whether it did what cannot be
\ said, and eight cells of each stack.  A cell of the data
\ stack holds its own number as it starts and a cell of the
\ return stack 16 more; the depths start at 5.  So T is 4 as it
\ was, the cell under it 3, and R 20.  What an instruction can
\ make of those in one operation is 64 and more: 16 for each
\ input and the operation's number.  What T points to, as it
\ is, is M-WAS.
\ It is not among the variables: there is too little room
\ there, and what is set aside at here needs it (display.fs).

#24 constant JM-CELLS
high
create jm   meta #24 cell-bytes * target allot
create jwas meta #24 cell-bytes * target allot
create jm0  5 , 5 , 4 , #20 , #255 , 0 , 0 , 0 ,
    0 , 1 , 2 , 3 , 4 , 5 , 6 , 7 ,
    #16 , #17 , #18 , #19 , #20 , #21 , #22 , #23 ,
unhigh

#3 constant N-WAS
#4 constant T-WAS
#20 constant R-WAS
#96 constant M-WAS
#255 constant NO-CELL

: JRSP ( -- a ) jm 1+ ;
: JT ( -- a ) jm d# 2 + ;
: JR ( -- a ) jm d# 3 + ;
: JSA ( -- a ) jm d# 4 + ;
: JSV ( -- a ) jm d# 5 + ;
: JF ( -- a ) jm d# 6 + ;
: JLOST ( -- a ) jm d# 7 + ;
: jds ( i -- a ) jm d# 8 + + ;
: jrs ( i -- a ) jm d# 16 + + ;
: jnos ( -- a ) jm @ d# 2 - jds ;   \ the cell under T
: jrnos ( -- a ) JRSP @ d# 2 - jrs ;

: jreset ( -- ) jm0 jm JM-CELLS cells-move ;
: jbad ( -- sym ) true JLOST !  NO-CELL ;

\ which input of an instruction a cell is, as the machine
\ started: the cell under T, T, what T points to, or R; or
\ less than nothing
: jsrc ( sym -- in | -1 )
    dup N-WAS = if drop d# 0 exit then
    dup T-WAS = if drop d# 1 exit then
    dup M-WAS = if drop d# 2 exit then
    R-WAS = if d# 3 exit then
    true ;

: jmade ( in op -- sym ) \ what one operation makes of an input
    over 0< if 2drop jbad exit then
    swap d# 4 lshift + d# 64 + ;

\ What is at the address a cell holds.  After a store nothing
\ is said of memory.  It is noted where it fetched from.
: jfetched ( sym -- sym' )
    JSA @ NO-CELL <> if drop jbad exit then
    dup jsrc dup 0< if 2drop jbad exit then     ( sym in )
    d# 1 over lshift JF or!
    swap T-WAS = if drop M-WAS exit then
    OP-LOAD jmade ;

: jstored ( sym addr -- )
    JSA @ NO-CELL <> if 2drop jbad drop exit then
    JSA ! JSV ! ;

\ + and or xor * and = are the same either way round
: jeither? ( op -- f )
    dup OP-SAME = swap d# 3 - d# 5 u< or ;

\ an operation of IN and the cell under T: that cell is to be
\ the one that was under T, or IN is, and the operation is the
\ same either way round
: jof-two ( a n op -- sym )
    >r  dup N-WAS = if drop jsrc r> jmade exit then
    over N-WAS = r@ jeither? and if nip jsrc r> jmade exit then
    2drop rdrop jbad ;

: jof-shift ( a t op -- sym ) \ a shift is of IN by T
    >r  T-WAS = if jsrc r> jmade exit then
    drop rdrop jbad ;

: jin ( w -- sym ) \ the input
    w-in
    dup 0= if drop jnos @ exit then
    dup d# 1 = if drop JT @ exit then
    d# 2 = if JT @ jfetched exit then
    JR @ ;

\ the operation, as vm_step.h has it: what it makes, and what
\ two of them do to T and the cell under it
: jout ( in-sym w -- out-sym )
    w-op
    ?dup 0= if exit then
    dup OP-UNDER = if drop  JT @ jnos !  exit then
    dup OP-SWAP = if drop  jnos @  JT @ jnos !  JT !  exit then
    dup OP-LOAD = if drop jfetched exit then
    dup OP-IO = if 2drop jbad exit then
    dup OP-NOT = if swap jsrc swap jmade exit then
    dup OP-SHIFT - d# 2 u< if >r JT @ r> jof-shift exit then
    >r jnos @ r> jof-two ;

\ a depth that the eight cells do not have is not gone on with
: jdeep ( n a -- ) \ a depth moves
    tuck +!
    dup @ d# 2 - d# 7 u< if drop exit then
    d# 5 swap !  jbad drop ;

: jd-shown ( w -- ) w-d 0< if jm @ 1- jds @ JT ! then ;
: jr-shown ( w -- )
    w-moves 0< if JRSP @ 1- jrs @ JR ! then ;

\ the stacks move, and the result goes where it goes
: jgoes ( out-sym w -- )
    dup w-d jm jdeep  dup w-moves JRSP jdeep
    dup w-d 0> if JT @ jnos ! then
    dup w-moves 0> if JR @ jrnos ! then
    dup w-out                                   ( out w o )
    ?dup 0= if swap JT ! jr-shown exit then
    dup d# 1 = if drop swap JR ! jd-shown exit then
    OUT-MEM <> if 2drop jbad drop exit then     \ a port
    swap JT @ jstored  dup jd-shown jr-shown ;

: jstep ( w -- ) \ one instruction
    dup h# f000 and h# 6000 <> if drop jbad drop exit then
    dup >r jin r@ jout r> jgoes ;

\ a place that was stored to is not among those fetched from:
\ where the store does not stop the machine, the fetch does
\ not
: jsettled ( -- )
    JSA @ jsrc dup 0< if drop exit then
    d# 1 swap lshift invert JF and! ;

: jsame-run? ( a b n -- f ) \ are n cells of each the same?
    >r over - r> swap >r bounds  ( end a ) ( R: b less a )
    begin 2dup xor while
        dup @ over r@ + @ xor if 2drop rdrop false exit then
        1+
    repeat
    2drop rdrop true ;

\ is the machine as it was kept?  The depths and the rest, and
\ the cells of each stack that are under its top
: jsame? ( -- f )
    jm jwas d# 8 jsame-run? 0= if false exit then
    d# 0 jds  jwas d# 8 +  jm @ 1- jsame-run? 0= if
        false exit
    then
    d# 0 jrs  jwas d# 16 +  JRSP @ 1- jsame-run? ;

\ the instruction of these fields that moves the stacks as far
\ as the two did
: jfielded ( in op out -- w )
    OUT-AT lshift >r  OP-AT lshift >r  IN-AT lshift r> or r> or
    jwas @ d# 5 - d# 3 and D-AT lshift or
    jwas 1+ @ d# 5 - d# 3 and or
    h# 6000 or ;

: jcand ( in op out -- w | 0 ) \ is that instruction the two?
    jfielded  dup jreset jstep jsettled  jsame? and ;

\ OUT is one of the inputs as it is: there are three such
\ instructions, by what else the operation does
: jpasses ( in out -- w | 0 )
    2dup d# 0 swap jcand ?dup if nip nip exit then
    2dup OP-SWAP swap jcand ?dup if nip nip exit then
    OP-UNDER swap jcand ;

\ the instruction whose result is that cell, sent to `out`
: jout-as ( sym out -- w | 0 )
    >r
    dup d# 64 u< if
        jsrc dup 0< if rdrop drop d# 0 exit then
        r> jpasses exit
    then
    d# 64 - dup d# 4 rshift swap d# 15 and      ( in op )
    ?dup 0= if r> jpasses exit then
    r> jcand ;

: jfits? ( depth -- f ) d# 5 - d# 2 + d# 4 u< ;

\ what was run is one instruction, or nothing: its result is
\ what was stored, if anything was, or what T or R has
: jleft ( -- w | 0 )
    jsettled
    JLOST @ if d# 0 exit then
    jm @ jfits?  JRSP @ jfits?  and 0= if d# 0 exit then
    jm jwas JM-CELLS cells-move
    jwas d# 4 + @ NO-CELL <> if
        jwas d# 5 + @ OUT-MEM jout-as exit
    then
    jwas d# 2 + @ d# 0 jout-as ?dup if exit then
    jwas d# 3 + @ d# 1 jout-as ;

: jtwo ( a b -- w | 0 ) \ the instruction that two are
    jreset swap jstep jstep jleft ;

: jthree ( a b c -- w | 0 )
    jreset rot jstep swap jstep jstep jleft ;
