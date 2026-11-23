\ compose.fs - the one instruction that two or three are

\ The instructions are stepped on a model of the machine whose
\ cells hold symbols: where each value came from.  The one
\ instruction that would leave the same is built from what is
\ left (jleft) and stepped too: it is the answer only if it
\ leaves the same (jcand).

meta
: low-bit ( mask -- n )
    0 swap begin dup 1 and 0= while 2/ swap 1+ swap repeat
    drop ;
target

\ where each field starts, and the operations the model knows
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
meta 0 IN^N 0 io[IN] low-bit rshift target constant OP-XOR
meta 0 IN>>T 0 io[IN] low-bit rshift target constant OP-SHIFT
meta 0 [IN] 0 io[IN] low-bit rshift target constant OP-LOAD
meta 0 io[IN] 0 io[IN] low-bit rshift target constant OP-IO
meta 0 ->[T] 0 ->[T] low-bit rshift target constant OUT-MEM

: w-in ( w -- n ) IN-AT rshift d# 3 and ;
: w-op ( w -- n ) OP-AT rshift d# 15 and ;
: w-out ( w -- n ) OUT-AT rshift d# 3 and ;
: w-moves ( field -- n ) d# 3 and  dup d# 2 and 2* - ;
: w-d ( w -- n ) D-AT rshift w-moves ;

\ The model, jm, in order: the two depths, T, R, the address
\ stored to and the value, a bit for each input fetched
\ through, whether it is lost, and eight cells of each stack.
\ A cell of the data stack starts as its index and one of the
\ return stack as 16 more; the depths start at 5.  So T
\ starts as 4, the cell under it 3 and R 20; 96 is the cell T
\ points to.  The result of one operation on an input is
\ 64 + 16 * the input + the operation.  What the model cannot
\ follow is lost (jbad).

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
#96 constant M-WAS      \ what T pointed to
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

\ which input a symbol is: the cell under T, T, [T] or R (0-3)
: jsrc ( sym -- in | -1 )
    dup N-WAS = if drop d# 0 exit then
    dup T-WAS = if drop d# 1 exit then
    dup M-WAS = if drop d# 2 exit then
    R-WAS = if d# 3 exit then
    true ;

: jmade ( in op -- sym ) \ what one operation makes of an input
    over 0< if 2drop jbad exit then
    swap d# 4 lshift + d# 64 + ;

\ what is at the address a cell holds; lost after a store
: jfetched ( sym -- sym' )
    JSA @ NO-CELL <> if drop jbad exit then
    dup jsrc dup 0< if 2drop jbad exit then     ( sym in )
    d# 1 over lshift JF or!
    swap T-WAS = if drop M-WAS exit then
    OP-LOAD jmade ;

: jstored ( sym addr -- )
    JSA @ NO-CELL <> if 2drop jbad drop exit then
    JSA ! JSV ! ;

\ is op one of + and or xor * = , whose two inputs can swap?
: jeither? ( op -- f )
    dup OP-SAME = swap d# 3 - d# 5 u< or ;

: jself ( op -- sym | 0 )
    \ the symbol for x op x when every x gives the same, else 0
    dup OP-SAME = if d# 0 swap jmade exit then
    dup OP-XOR =  swap OP-SAME - 1- d# 2 u<  or
    if d# 0 OP-XOR jmade exit then
    d# 0 ;

\ an operation of IN and the cell under T, either way round
: jof-two ( a n op -- sym )
    >r
    2dup = if r@ jself ?dup if nip nip rdrop exit then then
    dup N-WAS = if drop jsrc r> jmade exit then
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

: jout ( in-sym w -- out-sym ) \ the operation (vm_step.h)
    w-op
    ?dup 0= if exit then
    dup OP-UNDER = if drop  JT @ jnos !  exit then
    dup OP-SWAP = if drop  jnos @  JT @ jnos !  JT !  exit then
    dup OP-LOAD = if drop jfetched exit then
    dup OP-IO = if 2drop jbad exit then
    dup OP-NOT = if swap jsrc swap jmade exit then
    dup OP-SHIFT - d# 2 u< if >r JT @ r> jof-shift exit then
    >r jnos @ r> jof-two ;

: jdeep ( n a -- ) \ a depth moves; lost past the eight cells
    tuck +!
    dup @ d# 2 - d# 7 u< if drop exit then
    d# 5 swap !  jbad drop ;

: jd-shown ( w -- ) w-d 0< if jm @ 1- jds @ JT ! then ;
: jr-shown ( w -- )
    w-moves 0< if JRSP @ 1- jrs @ JR ! then ;

: jgoes ( out-sym w -- ) \ the stacks move; the result goes
    dup w-d jm jdeep  dup w-moves JRSP jdeep
    dup w-d 0> if JT @ jnos ! then
    dup w-moves 0> if JR @ jrnos ! then
    dup w-out                                   ( out w o )
    ?dup 0= if swap JT ! jr-shown exit then
    dup d# 1 = if drop swap JR ! jd-shown exit then
    OUT-MEM <> if 2drop jbad drop exit then     \ a port
    swap JT @ jstored  dup jd-shown jr-shown ;

\ A return goes to what R holds as the instruction starts: that
\ must be R's first value.  No instruction is stepped after it.
#16 constant J-RETURNED \ the bit of JF a return sets
: jreturns ( w -- )
    h# 1000 and 0= if exit then
    JR @ R-WAS <> if jbad drop then
    J-RETURNED JF or! ;

: jstep ( w -- ) \ one instruction
    dup h# e000 and h# 6000 <>  JF @ J-RETURNED and  or if
        drop jbad drop exit
    then
    dup jreturns
    dup >r jin r@ jout r> jgoes ;

\ a cell that was stored to does not count as fetched from
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

: jsame? ( -- f ) \ is the machine as it was kept in jwas?
    jm jwas d# 8 jsame-run? 0= if false exit then
    d# 0 jds  jwas d# 8 +  jm @ 1- jsame-run? 0= if
        false exit
    then
    d# 0 jrs  jwas d# 16 +  JRSP @ 1- jsame-run? ;

\ of these fields, moving the stacks and returning as jwas did
: jfielded ( in op out -- w )
    OUT-AT lshift >r  OP-AT lshift >r  IN-AT lshift r> or r> or
    jwas @ d# 5 - d# 3 and D-AT lshift or
    jwas 1+ @ d# 5 - d# 3 and or
    jwas d# 6 + @ J-RETURNED and if h# 1000 or then
    h# 6000 or ;

\ that instruction, if it leaves the model as jwas has it
: jcand ( in op out -- w | 0 )
    jfielded  dup jreset jstep jsettled  jsame? and ;

: jpasses ( in out -- w | 0 ) \ `in` goes to `out` unchanged
    2dup d# 0 swap jcand ?dup if nip nip exit then
    2dup OP-SWAP swap jcand ?dup if nip nip exit then
    OP-UNDER swap jcand ;

: jout-as ( sym out -- w | 0 ) \ whose result is that symbol
    >r
    dup d# 64 u< if
        jsrc dup 0< if rdrop drop d# 0 exit then
        r> jpasses exit
    then
    d# 64 - dup d# 4 rshift swap d# 15 and      ( in op )
    ?dup 0= if r> jpasses exit then
    r> jcand ;

\ has a stack moved by -2 to 1, as one instruction can move it?
: jfits? ( depth -- f ) d# 5 - d# 2 + d# 4 u< ;

\ the one instruction that leaves what was stepped: its result
\ is what was stored, or T, or R
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

: jtwo ( a b -- w | 0 ) \ the one instruction that a then b are
    jreset swap jstep jstep jleft ;

: jthree ( a b c -- w | 0 )
    jreset rot jstep swap jstep jstep jleft ;
