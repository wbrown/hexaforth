$00f1 constant IO-OUT
$00e0 constant IO-IN
$00d0 constant IO-SP    \ data stack depth
$00d1 constant IO-RSP   \ return stack depth

meta
    4 org 
target

header 1-       : 1-        d# -1 + ;
\ Words the cross-compiler already knows as one or two instructions get a
\ header only: `:noname` leaves the name bound to the inline form, so code
\ below compiles the instructions themselves, where the peephole can fuse
\ them, instead of a call.
header 0=       :noname     0=       ;
header cell+    : cell+     d# 8 + ;        \ 64-bit cells = 8 bytes
header cells    : cells     d# 3 lshift ;   \ multiply by 8 for 64-bit

header <>       :noname     <>       ;
header >        :noname     >        ;
header 0<       :noname     0<       ;
header 0>       : 0>        d# 0 > ;
header 0<>      : 0<>       d# 0 <> ;
header u>       :noname     u>       ;

header key      :noname     key      ;
header emit     :noname     emit     ;

header cr
: cr
    d# 13 emit
    d# 10 emit
;

header space
: space
    d# 32 emit
;

header bl
: bl
    d# 32
;

: hex8 dup d# 16 rshift DOUBLE
: hex4 dup d# 8 rshift DOUBLE
: hex2 dup d# 4 rshift DOUBLE
: hex1
    h# f and
    dup d# 10 < if
        [char] 0
    else
        d# 55
    then
    +
    emit
;

header .x       : .x hex8 space ;
header .x2      : .x2 hex2 space ;

header true     : true  d# -1 ;
header false    : false d# 0 ;
header rot      : rot   >r swap r> swap ; 
header -rot     :noname     -rot     ;
header tuck     :noname     tuck     ;
header 2drop    :noname     2drop    ;
header ?dup     : ?dup  dup if dup then ;

header 2dup     :noname     2dup     ;
header +!       :noname     +!       ;
header 2swap    : 2swap rot >r rot r> ;
header 2over    : 2over >r >r 2dup r> r> 2swap ;

header min      : min   2dup< if drop else nip then ;
header max      : max   2dup< if nip else drop then ;

header hi32  :noname hi32 ;
header lo32  : lo32 ( dw -- w ) d# 32 lshift d# 32 rshift ;
header hi16  : hi16 ( dw -- s ) d# 32 lshift d# 48 rshift ;
header lo16  : lo16 ( dw -- s ) d# 48 lshift d# 48 rshift ;

header uw@
: uw@
    @ h# ffff and
;

header w!
: w! ( u c-addr -- ) \ store 16 bits, leaving the other 48 bits of the cell alone
    >r lo16
    r@ @ d# -1 d# 16 lshift and
    or r> !
;

header count
: count
    dup 1+ swap c@
;

header bounds   :noname     bounds   ;

header type
: type
    bounds
    begin
        2dupxor
    while
        dup c@ emit
        1+
    repeat
    2drop
;

create "cold"
    'c' c, 'o' c, 'l' c, 'd' c,

create base     $a ,
create forth    0 ,
create cp       0 ,         \ Code pointer, grows up
create dp       0 ,         \ Data pointer, grows up
create lastword 0 ,
create thisxt   0 ,
create syncpt   0 ,
create sourceC  0 , 0 ,
create sourceid 0 ,
create >in      0 ,
create state    0 ,
create delim    0 ,
create rO       0 ,
create leaves   0 ,
create tib $80 allot

header dp    :noname dp ;
header cp    :noname cp ;
header state :noname state ;
header base  :noname base ;
header >in   :noname >in  ;
header forth :noname forth ;

: nextword
    uw@ d# -2 and
;

header words : words
    forth uw@
    begin
        dup
    while
        dup d# 2 +
        count type
        space
        nextword
    repeat
    drop
;

header negate   :noname     negate   ;
header -        : -         negate + ; 
header abs      : abs       dup 0< if negate then ; 
header 2*       :noname     2*       ;
header 2/       : 2/        dup 0< if invert d# 1 rshift invert else d# 1 rshift then ; 
header here     : here      dp @ ;
header depth    : depth     IO-SP io@ 1- ;  \ less the port number itself

header /string
: /string
    dup >r - swap r> + swap
; 

header aligned
: aligned
    d# 7 + d# -8 and
; 

header d+
: d+                              ( augend . addend . -- sum . ) 
    rot + >r                      ( augend addend) 
    over+                         ( augend sum) 
    swap over swap                ( sum sum augend)
    u< if                         ( sum) 
        r> 1+ 
    else 
        r> 
    then                          ( sum . ) 
; 

header dnegate
: dnegate 
    invert swap invert swap 
    d# 1. d+
; 

header dabs
: dabs ( d -- ud )
    dup 0< if dnegate then
; 

header s>d
: s>d dup 0< ;

header 2@
: 2@ \ ( a -- lo hi )
    dup cell+ @ swap @
;

header 2!
: 2! \ ( lo hi a -- )
    tuck !
    cell+ !
;

header 2rot
: 2rot
    >r >r 2swap r> r> 2swap
;

header d>s
: d>s   drop ;

header d=
: d=                        \ a b c d -- f ) 
    >r                      \ a b c 
    rot =                   \ b a=c 
    swap r> =               \ a=c b=d 
    and
; 

header d<
: d<            \ ( al ah bl bh -- flag ) 
    rot         \ al bl bh ah 
    2dup = 
    if 
        2drop u< 
    else 
        > nip nip
    then 
; 

header du<
: du<           \ ( al ah bl bh -- flag ) 
    rot         \ al bl bh ah 
    2dup = 
    if 
        2drop u< 
    else 
        u> nip nip
    then 
; 

header d-
: d-
    dnegate d+
;

header d0<
: d0<
    nip 0<
;

header d0=
: d0=
    or 0=
;

header d2*
: d2*
    2dup d+
; 

header d2/
: d2/
    >r d# 1 rshift r@
    d# 63 lshift
    or r> 2/
;

create scratch 0 ,

\ One step per bit of a cell: six DOUBLEs = 64 steps.

: mulstep ( ud u1 -- ud u1 )
    DOUBLE DOUBLE DOUBLE DOUBLE DOUBLE DOUBLE
    >r
    d# 1 lshift over d# 63 rshift + swap d# 1 lshift swap
    r@ d# 0 < if 
        scratch @ d# 0 d+ 
    then 
    r> d# 1 lshift
;

header um*
: um*  ( u1 u2 -- ud ) 
    scratch ! 
    d# 0. rot
    mulstep
    drop 
; 

header *        :noname     *        ;

\ see Hacker's Delight (2nd ed) 9-4 "Unsigned Long Division"

\ One step per bit.  The DOUBLEs nest, so entering part-way down runs fewer
\ steps: div32 and div16 are for dividends already shifted to the top of y.

: divstep  \ ( y x z )   64 steps
    DOUBLE
: div32                  \ 32 steps
    DOUBLE
: div16                  \ 16 steps
    DOUBLE DOUBLE DOUBLE DOUBLE
    >r
    dup 0< >r
    2dup d+
    dup r> or r@ u< invert if
        r@ -
        swap 1+ swap
    then
    r>
;

header um/mod
: um/mod \ ( ud u1 -- u2 u3 ) ( 6.1.2370 ) 
    divstep
    drop swap 
; 

\ Single-cell divide.  A dividend that fits in 16 or 32 bits is shifted to
\ the top of the cell first, so only that many steps are needed.
: u/mod ( u1 u2 -- rem quot )
    >r
    dup d# 16 rshift 0= if
        d# 48 lshift d# 0 r> div16
    else
        dup d# 32 rshift 0= if
            d# 32 lshift d# 0 r> div32
        else
            d# 0 r> divstep
        then
    then
    drop swap
;

\ Unicode-friendly ACCEPT contibuted by Matthias Koch

: delchar ( addr len -- addr len )
    dup if d# 8 emit d# 32 emit d# 8 emit then

    begin
        dup 0= if exit then
        1- 2dup + c@
        h# C0 and h# 80 <>
      until
;

header accept
: accept
    >r d# 0  ( addr len R: maxlen )

    begin
        key    ( addr len key R: maxlen )
        
        d# 9 over= if drop d# 32 then
        d# 127 over= if drop d# 8 then
        
        dup d# 31 u>
        if
            over r@ u<
            if
                dup emit
                >r 2dup + r@ swap c! 1+ r>
            then
        then
        
        d# 8 over= if >r delchar r> then
          
        d# 10 over= swap d# 13 = or
    until
    
    rdrop nip
    space
;

: 3rd   >r over r> swap ;
: 3dup  3rd 3rd 3rd ;

: lower ( c1 -- c2 ) \ c2 is the lower-case of c1
    h# 40 over <
    over h# 5b < and
    h# 20 and +
;

: i<> ( c1 c2 -- f ) \ case-insensitive difference
    2dupxor h# 1f and if
        drop exit
    then
    lower swap lower xor
;

: sameword ( c-addr u wp -- c-addr u wp flag )
    2dup d# 2 + c@ = if
        3dup
        d# 3 + >r
        bounds
        begin
            2dupxor
        while
            dup c@ r@ c@ i<> if
                2drop rdrop false exit
            then
            1+
            r> 1+ >r
        repeat
        2drop rdrop true
    else
        false
    then
;

header caligned
: caligned
    1+ d# -2 and
;

: >xt
    d# 2 +
    count +
    caligned
    uw@
;

header calign
: calign
    dp @
    caligned
    dp !
;

\ lsb 0 means non-immediate, return -1
\     1 means immediate,     return  1
: isimmediate ( wp -- -1 | 1 )
    uw@ d# 1 and 2* 1-
;

\ A header starts with a 16-bit link, the name length, and the name.  One
\ 64-bit fetch therefore sees the link, the length and the first five
\ characters together.  namekey builds the value those bytes must have for a
\ name, and a mask selecting the length and as many characters as the name
\ has (up to five), ignoring letter case.  Both are shifted past the link.

: namekey ( c-addr u -- mask key )
    dup d# 5 min d# 3 lshift
    d# 1 swap lshift 1-
    h# dfdfdfdfdf and
    d# 8 lshift h# ff or d# 16 lshift       ( c-addr u mask )
    >r
    swap @ d# 8 lshift or                   \ characters above the length
    d# 16 lshift r@ and
    r> swap
;

\ Most entries are rejected by one fetch, mask and compare.  Survivors are
\ confirmed by sameword, which also checks names longer than five.

header sfind
: sfind ( c-addr u -- c-addr u 0 | xt flag )
    2dup namekey >r                         ( c-addr u mask ) ( r: key )
    forth uw@
    begin
        dup
    while
        2dup @and r@ = if
            >r -rot r>                      ( mask c-addr u wp )
            sameword if
                >r 2drop drop r>
                rdrop
                dup >xt
                swap isimmediate
                exit
            then
            >r rot r>                       ( c-addr u mask wp )
        then
        @ h# fffe and                       \ next entry, less its immediate flag
    repeat
    nip rdrop
;

: digit? ( c -- u f )
   lower
   dup h# 39 > h# 100 and +
   dup h# 160 > h# 127 and - h# 30 -
   dup base @ u<
;

: ud* ( ud1 u -- ud2 ) \ ud2 is the product of ud1 and u
    tuck * >r
    2dup or d# 32 rshift if     \ both under 2^32: the product fits one cell
        um*
    else
        * d# 0
    then
    r> +
;

header >number
: >number ( ud1 c-addr1 u1 -- ud2 c-addr2 u2 )
    begin
        dup
    while
        over c@ digit?
        0= if drop exit then
        >r 2swap base @ ud*
        r> s>d d+ 2swap
        d# 1 /string
    repeat
;

header fill
: fill ( c-addr u char -- ) ( 6.1.1540 ) 
  >r  bounds 
  begin
    2dupxor 
  while
    r@ over c! 1+ 
  repeat
  r> drop 2drop
; 

header cmove
: cmove ( addr1 addr2 u -- )
    bounds rot >r
    begin
        2dupxor
    while
        r@ c@ over c!
        r> 1+ >r
        1+
    repeat
    rdrop 2drop
;

header cmove>
: cmove> \ ( addr1 addr2 u -- )
    begin
        dup
    while
        1- >r
        over r@ + c@
        over r@ + c!
        r>
    repeat
    drop 2drop
;

header execute
: execute
    >r
;

header source
: source
    sourceC 2w@
;

: source! ( addr u -- ) \ set the source
    sourceC 2w!
;

header source-id
: source-id
    sourceid @
;

\ From Forth200x - public domain

header isspace?
: isspace? ( c -- f )
    h# 21 u< ;

header isnotspace?
: isnotspace? ( c -- f )
    isspace? 0= ;

: xt-skip   ( addr1 n1 xt -- addr2 n2 ) \ gforth
    \ skip all characters satisfying xt ( c -- f )
    >r
    BEGIN
        over c@ r@ execute
        overand
    WHILE
        d# 1 /string
    REPEAT
    r> drop ;

header parse-name
: parse-name ( "name" -- c-addr u )
    source >in @ /string
    ['] isspace? xt-skip over >r
    ['] isnotspace? xt-skip ( end-word restlen r: start-word )
    2dup d# 1 min + source drop - >in !
    drop r> tuck -
;

header isnotdelim
: isnotdelim
    delim @ <>
;

header parse
: parse ( "ccc<char" -- c-addr u )
    delim !
    source >in @ /string
    over >r
    ['] isnotdelim xt-skip
    2dup d# 1 min + source drop - >in !
    drop r> tuck -
;


header allot
: tallot
    dp +!
;

header ,
: t,
    here !
    d# 8 tallot
;

header w,
: w,
    here w!
    d# 2 tallot
;

header c,
: tc,
    here c!
    d# 1 tallot
;

: code,
    cp @ w!
    d# 2 cp +!
;

: attach
    lastword @ ?dup if
        forth w!
    then
;

: sync
    cp @ syncpt !
;

header s,
: s,
    dup tc,
    bounds
    begin
        2dupxor
    while
        count tc,
    repeat
    2drop
;

: mkheader
    calign
    here lastword !
    forth w@ w,
    parse-name
    s,
    calign
    cp @ 2/ w,              \ xt: word address
    cp @ 2/ thisxt !
    sync
;

header immediate
:noname
    lastword @
    dup uw@ d# 1 or
    swap w!
;

header ]
: t]
    d# 3 state !
;

header-imm [
: t[
    d# 0 state !
;

header :
:noname
    mkheader t]
;

header :noname
:noname
    calign cp @ 2/
    dup thisxt !
    d# 0 lastword !
    sync
    t]
;

: inline: \ copy the instruction after the call, and skip it
    r>
    dup 2* uw@ code,
    1+ >r
;
inline-operand

: prev
    cp @ d# -2 +
;

: (loopdone)  ( 0 -- )
    drop
    r> r> rO ! >r
;

\ Where the most recent created word's exit is.  The literal in front of it
\ is 1-4 instructions long, so `create` records the spot.
create doesexit 0 ,

\ Runs inside a defining word: the rest of that word becomes the behaviour
\ of the word just created.  Its exit is replaced by a jump to the code after
\ does>, and the defining word returns to its own caller.
header does>
: (does>)
    r> doesexit @ w!
;

\ May ; turn this call into a jump?  Not for words that take their own
\ return address.
: jumpable ( op -- f )
    dup h# e000 and h# 4000 =       \ is a call
    swap h# 1fff and
    dup ['] (loopdone) <>
    swap ['] (does>) <> and and
;

header-imm exit
: texit
    cp @ 2/ thisxt @ <> if
        prev uw@ jumpable if
            prev
            uw@
            h# 4000 xor
            prev w!
            \ cp @ syncpt @ <> if
                inline: exit
            \ then
            exit
        then
    then
    inline: exit
;

header-imm ;
:noname
    attach
    texit
    t[
;

header-imm if
: tif
    cp @
    h# 2000 code,
;

header-imm ahead
:noname
    cp @
    h# 0000 code,
;

header-imm then
: tthen
    dup uw@
    cp @ 2/ or
    swap w!
    sync
;

header-imm begin
: tbegin
    cp @ 2/
;

header-imm again
: tagain
    code,
;

header-imm until
: tuntil
    h# 2000 or code,
;

: isreturn ( opcode -- f ) \ ALU instruction with RET set
    h# f000 and
    h# 7000 =
;

: isliteral ( ptr -- f) \ a literal followed by exit
    dup uw@ h# 8000 and 0<>
    swap d# 2 + uw@ h# 7403 = and
;

header compile,
: compile, ( xt -- ) \ xt is a word address; 2* gives the byte address of its code
    dup 2* uw@ isreturn if
        2* uw@ h# effc and  \ single-instruction word: inline it without RET r-1
        code,
    else
        dup 2* isliteral if
            2* uw@ code,
        else
            h# 4000 or
            code,
        then
    then
;

header-imm recurse
:noname
    thisxt @ compile,
;

\
\ How DO...LOOP is implemented
\
\ Uses top of R-stack (R) and a variable rO:
\    R is the counter; it starts negative and counts up. When it reaches 0, loop exits
\    rO is the offset. It is set up at loop start so that I can be computed from (rC+rO)
\
\ So DO receives ( limit start ) on the stack. It needs to compute:
\      R = start - limit
\      rO = limit
\
\ E.g. for "13 3 DO"
\      rC = -10
\      rO = 13
\
\ So the loop runs:
\      R      -10 -9 -8 -7 -6 -5 -4 -3 -2 -1
\      I        3  4  5  6  7  8  9 10 11 12
\
\

: (do)  ( limit start -- start-limit )
    r> rO @ >r >r
    over rO !
    swap -
;

: (?do)  ( limit start -- start-limit start!=limit )
    r> rO @ >r >r
    over rO !
    swap -
    dup 0=
;

: (loopnext)
    d# 1 + dup
;

header-imm do
:noname
    leaves @ d# 0 leaves !
    ['] (do) compile,
    tbegin
    inline: >r
;

\ Finish compiling DO..LOOP
\ resolve each LEAVE by walking the chain starting at 'leaves'
\ compile a call to (loopdone)
: resolveleaves
    leaves @
    begin
        dup
    while
        dup uw@ swap        ( next leafptr )
        cp @ 2/ swap w!
    repeat
    drop
    leaves !
    ['] (loopdone) compile,
;

header-imm loop
:noname
    inline: r>
    ['] (loopnext) compile,
    tif
    swap tagain
    tthen
    resolveleaves
;

: (+loopnext) ( inc R -- R finished )
    over 0< if
        dup>r +
        r> over ( R old new )
        u<
    else
        dup>r +
        r> over ( R old new )
        u>
    then
;

header-imm +loop
:noname
    inline: r>
    ['] (+loopnext) compile,
    tuntil
    resolveleaves
;

header-imm leave
: leave
    inline: r>
    cp @
    leaves @ code,
    leaves !
;

header-imm ?do
:noname
    leaves @ d# 0 leaves !
    ['] (?do) compile,
    tif
        cp @
        leaves @ code,
        leaves !
    tthen
    tbegin
    inline: >r
;

header i
: i
    r>
    r@ rO @ +
    swap >r
;

header j
: j
    r> r>
    r> r> 2dup + -rot
    >r >r
    -rot
    >r >r
;

header-imm unloop
:noname
    inline: r>
    ['] (loopdone) compile,
;

header decimal
: decimal
    d# 10
;fallthru
: setbase
    base !
;

: (.) ( u -- )
    base @ u/mod
    ?dup if (.) then
    dup d# 10 < if [char] 0 else d# 55 then + emit
;

header .
: . ( n -- )
    dup 0< if [char] - emit negate then
    (.) space
;

header !        :noname     !        ;
header +        :noname     +        ;
header xor      :noname     xor      ;
header and      :noname     and      ;
header or       :noname     or       ;
header invert   :noname     invert   ;
header =        :noname     =        ;
header <        :noname     <        ;
header u<       :noname     u<       ;
header swap     :noname     swap     ;
header dup      :noname     dup      ;
header drop     :noname     drop     ;
header over     :noname     over    ;
header nip      :noname     nip      ;
header @        :noname     @        ;
header io!      :noname     io!      ;
header io@      :noname     io@      ;
header rshift   :noname     rshift   ;
header lshift   :noname     lshift   ;
header 1+       :noname     1+       ;
header c@       :noname     c@       ;
header c!       :noname     c!       ;
header-imm >r   :noname     inline: >r ;
header-imm r>   :noname     inline: r> ;
header-imm r@   :noname     inline: r@ noop ;

: jumptable ( u -- ) \ add u to the return address
    r> + >r ;

\   (doubleAlso) ( c-addr u -- x 1 | x x 2 )
\               If the string is legal, give a single or double cell number
\               and size of the number.

\ catch saves the data stack depth, the previous handler and the DO loop
\ offset on the return stack, and records the return stack depth at that
\ point in `handler`.  throw cuts the return stack back to that depth, which
\ leaves those cells on top, then restores the data stack depth with the
\ xt's cell replaced by the throw code.

create handler 0 ,

header throw
: throw ( n -- )
    ?dup if
        handler @ ?dup if
            IO-RSP io!
            r> rO !
            r> handler !
            r> swap >r
            IO-SP io!
            drop r>
            exit
        then
        [char] e emit
        [char] r emit
        [char] r emit
        [char] o emit
        [char] r emit
        [char] : emit
        space
        .
        t[
        d# 0 sourceid !
        d# 0 IO-RSP io!     \ empty both stacks before restarting
        d# 0 IO-SP io!
        d# 1 execute        \ word 1 of the boot vector: quit
    then
;

: -throw ( a b -- ) \ if a is nonzero, throw -b
    negate and throw
;

\ Literals: 12-bit immediates with a shift selector (0/12/24/36 bits) and an
\ add flag.  The first non-zero segment pushes, later ones add.

: segs, ( u48 addflag -- ) \ addflag: h# 4000 to add to T, 0 to push
    >r d# 0
    begin ( u shift )
        over h# fff and ?dup if
            over d# 12 lshift or
            r> or h# 8000 or code,
            h# 4000 >r
        then
        1+ swap d# 12 rshift swap
        over 0=
    until
    2drop
    r> 0= if h# 8000 code, then \ nothing emitted: push zero
;

header-imm 2literal
: 2literal
    swap DOUBLE
header-imm literal
: tliteral
    dup 0< if
        invert tliteral
        inline: invert
        exit
    then
    dup d# 48 rshift ?dup if \ wider than 48 bits: high part, shift, add the rest
        tliteral
        d# 48 tliteral
        inline: lshift
        d# 16 lshift d# 16 rshift
        h# 4000 segs,
        exit
    then
    d# 0 segs,
;

\ A created word is the literal address of its data field, then exit.

header create
: tcreate
    mkheader attach
    here aligned dp !
    here tliteral
    cp @ doesexit !
    h# 7403 code,           \ exit
;

header variable
: variable
    tcreate d# 0 t,
;

header constant
: tconstant ( x "name" -- )
    mkheader attach
    tliteral
    h# 7403 code,
;


: isvoid ( caddr u -- ) \ any char remains, throw -13
    nip 0<> d# 13 -throw
;

: consume1 ( caddr u ch -- caddr' u' f )
    >r over c@ r> =
    over 0<> and
    dup>r d# 1 and /string r>
;

: ((doubleAlso))
    h# 0. 2swap
    [char] - consume1 >r
    >number
    [char] . consume1 if
        isvoid              \ double number
        r> if dnegate then
        ['] 2literal exit
    then
                            \ single number
    isvoid drop
    r> if negate then
: single
    ['] tliteral
;

: base((doubleAlso))
    base @ >r setbase
    ((doubleAlso))
    r> setbase
;

: is'c' ( caddr u -- f )
    d# 3 =
    over c@ [char] ' = and
    swap 1+ 1+ c@ [char] ' = and
;

: (doubleAlso)
    [char] $ consume1 if
        d# 16 base((doubleAlso)) exit
    then
    [char] # consume1 if
        d# 10 base((doubleAlso)) exit
    then
    [char] % consume1 if
        d# 2 base((doubleAlso)) exit
    then
    2dup is'c' if
        drop 1+ c@ single exit
    then
    ((doubleAlso))
;

: doubleAlso
    (doubleAlso) drop
;

header-imm postpone
:noname
    parse-name sfind
    dup 0= d# 13 -throw
    0< if
        tliteral
        ['] compile,
    then
    compile,
;

: doubleAlso,
    (doubleAlso)
    execute
;

: dispatch
    jumptable ;fallthru
    jmp execute                 \      -1      0       non-immediate
    jmp doubleAlso              \      0       0       number
    jmp execute                 \      1       0       immediate

    jmp compile,                \      -1      2       non-immediate
    jmp doubleAlso,             \      0       2       number
    jmp execute                 \      1       2       immediate

: interpret
    begin
        parse-name dup
    while
        sfind
        state @ +
        1+ dispatch         \ table index in words, one jmp each
    repeat
    2drop
;

header catch
: catch ( xt -- 0 | n )
    depth >r
    handler @ >r
    rO @ >r
    IO-RSP io@ handler !
    execute
    rdrop
    r> handler !
    rdrop
    d# 0
;

header '
: tick ( "name" -- xt )
    parse-name sfind
    0= d# 13 -throw
;

header-imm [']
:noname
    tick tliteral
;

header refill
: refill
    source-id 0= dup
    if
        tib dup d# 128 accept
        source!
        d# 0 >in !
    then
;

header evaluate
:noname
    source >r >r >in @ >r
    source-id >r d# -1 sourceid !
    source! d# 0 >in !
    interpret
    r> sourceid !
    r> >in ! r> r> source!
;

header quit
: quit
    begin
        refill drop
        interpret
        space
        [char] o emit
        [char] k emit
        cr
    again
;

: main
    decimal
    "cold" d# 4 sfind if
        execute
    else
       2drop
    then
    quit
;

meta
    link @ t,
    link @ t' forth t!
    there  t' dp t!
    tcp @  t' cp t!
target
