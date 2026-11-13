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
\ CELL, CELLB, BYTESHIFT, TOPBIT, HALFBITS and the W64? W32? W32+? tests
\ come from cross.fs, which takes the machine's sizes from basewords.fs.
\
\ An address is the number of a cell, and @ and ! take one.  cell+ is the
\ next cell and cells turns a count of cells into a difference of addresses.
\ (While the machine can still be built to count bytes, CELL is the bytes
\ in a cell there, and cells multiplies by it.)
header cell+    : cell+     CELL + ;
CELLADDR? [if]
header cells    : cells ;
[else]
header cells    : cells     BYTESHIFT lshift ;
[then]

\ Text and code are reached by the address of a byte, which is not an
\ address @ can take: it is the cell's address times the bytes in a cell,
\ plus the byte's place in the cell.  >c gives the byte address of the
\ start of a cell, and c> the cell a byte is in.
header >c       : >c        (>c) ;
header c>       : c>        (c>) ;

header <>       :noname     <>       ;
header >        :noname     >        ;
header 0<       :noname     0<       ;
header 0>       : 0>        d# 0 > ;
header 0<>      : 0<>       d# 0 <> ;
header u>       :noname     u>       ;

header key      :noname     key      ;

header key?
: key? ( -- f ) \ is a character waiting on the console?
    h# e1 io@
;

\ Output goes through a vector, so it can be sent to a window.

header 'emit create 'emit 0 ,
header uart-emit : uart-emit ( c -- ) emit ;

header emit
: emit ( c -- ) 'emit @ >r ;      \ as execute: return into the vector

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
header hex1 : hex1
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
header lo32  : lo32 ( dw -- w ) h# ffffffff and ;
header hi16  : hi16 ( dw -- s ) d# 16 rshift h# ffff and ;
header lo16  : lo16 ( dw -- s ) h# ffff and ;

\ Memory is fetched and stored a cell at a time.  A byte or a 16-bit field
\ is part of a cell: these take the address of a byte, fetch the cell it is
\ in, and shift the field into place.  Instructions are 16-bit fields too,
\ so this is also how the compiler reads and writes code.

header field@ : field@ ( c-addr mask -- x )
    >r
    dup (c>) @
    swap OFFSET-MASK and d# 3 lshift rshift
    r> and
;

header field! : field! ( x c-addr mask -- )
    -rot                                    ( mask x a )
    dup (c>) >r                             \ the cell
    OFFSET-MASK and d# 3 lshift             ( mask x shift )
    >r over and r>                          ( mask x' shift )
    tuck lshift                             ( mask shift x'' )
    -rot lshift invert                      ( x'' ~mask' )
    r@ @ and or
    r> !
;

header c@
: c@ ( c-addr -- c )
    dup (c>) @
    swap OFFSET-MASK and d# 3 lshift rshift
    h# ff and
;

header c!
: c! ( c c-addr -- ) h# ff field! ;

header uw@
: uw@ ( c-addr -- w ) h# ffff field@ ;

header w!
: w! ( w c-addr -- ) h# ffff field! ;

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
create labels   0 ,         \ names of definitions that have no header
create cp       0 ,         \ Code pointer, grows up
create dp       0 ,         \ Data pointer, grows up
create lastword 0 ,
create thisxt   0 ,
create syncpt   0 ,
create sourceC  0 , 0 ,
header sourceid create sourceid 0 ,
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
header labels :noname labels ;

header words : words
    forth @
    begin
        dup
    while
        dup CELL2 + >c count type
        space
        @
    repeat
    drop
;

header negate   :noname     negate   ;
header -        : -         negate + ; 
header abs      : abs       dup 0< if negate then ; 
header 2*       :noname     2*       ;
header 2/       : 2/        dup 0< if invert d# 1 rshift invert else d# 1 rshift then ; 
\ The data pointer counts bytes, so that bytes can be laid down one at a
\ time.  here is the address of the cell it is in, which is the next free
\ cell once the pointer has been aligned.
header here     : here      dp @ (c>) ;
header align    : align     dp @ CELLB 1- + ALIGN-MASK and dp ! ;
\ The data stack depth.  adepth is the true depth; depth is what a program
\ sees, measured from sbase, which the desktop raises while it runs a line
\ of Forth above its own work.
header sbase create sbase 0 ,
header adepth : adepth ( -- n ) IO-SP io@ 1- ;            \ less the port number itself
header depth    : depth     adepth sbase @ - ;

header /string
: /string
    dup >r - swap r> + swap
; 

header aligned
: aligned ( a -- a ) \ the address of a whole cell, at or after a
    (>c) CELLB 1- + (c>)
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
    TOPBIT lshift
    or r> 2/
;

create scratch 0 ,

\ One step per bit of a cell.  Each DOUBLE runs what follows twice, so four
\ of them are 16 steps, five 32 and six 64.

: mulstep ( ud u1 -- ud u1 )
W64? [if]
    DOUBLE
[then]
W32+? [if]
    DOUBLE
[then]
    DOUBLE DOUBLE DOUBLE DOUBLE
    >r
    d# 1 lshift over TOPBIT rshift + swap d# 1 lshift swap
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

: divstep  \ ( y x z )   a step for every bit of a cell
W64? [if]
    DOUBLE
: div32                  \ 32 steps
[then]
W32+? [if]
    DOUBLE
: div16                  \ 16 steps
[then]
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
header u/mod : u/mod ( u1 u2 -- rem quot )
    >r
W32+? [if]
    dup d# 16 rshift 0= if
        TOP16 lshift d# 0 r> div16
    else
W64? [if]
        dup d# 32 rshift 0= if
            d# 32 lshift d# 0 r> div32
        else
            d# 0 r> divstep
        then
[else]
        d# 0 r> divstep
[then]
    then
[else]
    d# 0 r> divstep
[then]
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
header 3dup : 3dup  3rd 3rd 3rd ;

: lower ( c1 -- c2 ) \ c2 is the lower-case of c1
    h# 40 over <
    over h# 5b < and
    h# 20 and +
;

header caligned
: caligned
    1+ d# -2 and
;

header calign
: calign
    dp @
    caligned
    dp !
;

\ A dictionary header is cells: a link, an xt whose top bit marks an
\ immediate word, then the name as a count byte and lower-case characters,
\ zero padded to whole cells.  To look a name up it is packed the same way
\ into skey, and then compared with each header a cell at a time; the first
\ cell holds the length and the first characters, which settles nearly
\ every entry with one aligned fetch.

create skey #264 allot
create skn 0 ,              \ cells in the key

: >key ( c-addr u -- n ) \ pack a name into skey; n is its size in cells
    d# 255 min
    dup BYTESHIFT rshift 1+ skn !
    skey skn @
    begin
        dup
    while
        >r d# 0 over ! cell+ r> 1-
    repeat
    2drop
    dup skey >c c!
    skey >c 1+ swap                         ( src dst u )
    begin
        dup
    while
        >r
        over c@ lower over c!
        1+ swap 1+ swap
        r> 1-
    repeat
    drop 2drop
    skn @
;

: samename? ( wp -- f ) \ do the name cells after the first match the key?
    CELL2 + cell+ skey cell+ skn @ 1-
    begin
        dup
    while
        >r
        over @ over @ <> if rdrop 2drop false exit then
        cell+ swap cell+ swap
        r> 1-
    repeat
    drop 2drop true
;

header sfind
: sfind ( c-addr u -- c-addr u 0 | xt flag ) \ flag: 1 immediate, -1 not
    2dup >key drop
    skey @ >r
    forth @
    begin
        dup
    while
        dup CELL2 + @ r@ = if
            dup samename? if
                rdrop nip nip
                cell+ @
                dup d# 1 lshift d# 1 rshift         \ the xt, less the flag
                swap 0< if d# 1 else d# -1 then
                exit
            then
        then
        @
    repeat
    rdrop
;

: digit? ( c -- u f )
   lower
   dup h# 39 > h# 100 and +
   dup h# 160 > h# 127 and - h# 30 -
   dup base @ u<
;

: ud* ( ud1 u -- ud2 ) \ ud2 is the product of ud1 and u
    tuck * >r
    2dup or HALFBITS rshift if  \ both under half a cell: the product fits one
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
    >r
    2dup or OFFSET-MASK and 0= if           \ both at the start of a cell: whole cells first
        begin
            r@ CELLB u< 0=
        while
            over (c>) @ over (c>) !
            CELLB + swap CELLB + swap
            r> ALIGN-MASK + >r
        repeat
    then
    r>
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
    sourceC 2@
;

: source! ( addr u -- ) \ set the source
    sourceC 2!
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
    align
    here !
    CELLB tallot
;

header w,
: w,
    dp @ w!
    d# 2 tallot
;

header c,
: tc,
    dp @ c!
    d# 1 tallot
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
        d# 0 state !
        d# 0 sourceid !
        ['] uart-emit 'emit !
        d# 0 sbase !
        d# 0 IO-RSP io!     \ empty both stacks before restarting
        d# 0 IO-SP io!
        d# 1 execute        \ word 1 of the boot vector: quit
    then
;

header -throw : -throw ( a b -- ) \ if a is nonzero, throw -b
    negate and throw
;

\ Code is paged: 4096 words (8KB) to a page, page 0 being the kernel.  A
\ branch reaches the kernel or its own page, so a definition has to fit in
\ one page.  New definitions start in a fresh page when the current one is
\ nearly full, and compile, picks a near or far call, so none of this shows
\ in ordinary use.

\ Run-time code goes in CODE-SIZE bytes from CODE-BASE, both from cross.fs.

: code,
    cp @ d# 2 + h# 1fff and 0= if   \ the last slot of a page stays empty
        d# -8 throw                 \ ran off the end of the page
    then
    cp @ w!
    d# 2 cp +!
;

: newpage? ( -- ) \ before a definition: move on if under 1KB is left here
    cp @ h# 1fff and h# 1c00 u> if
        cp @ h# 1fff or 1+ cp !
    then
    cp @ CODE-BASE - CODE-SIZE u< 0= d# 8 -throw
;

: target ( -- field ) \ branch target field for the code about to be compiled
    cp @ d# 1 rshift
    dup d# 12 rshift if h# fff and h# 1000 or then
;

: attach
    lastword @ ?dup if
        forth !
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
    newpage?
    align
    here lastword !
    forth @ t,                              \ link
    cp @ d# 1 rshift dup thisxt ! t,        \ xt: the word address, on any page
    parse-name >key skey swap               \ name
    begin
        dup
    while
        >r dup @ t, cell+ r> 1-
    repeat
    2drop
    sync
;

header immediate
:noname
    lastword @ cell+
    dup @ d# 1 TOPBIT lshift or
    swap !
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
    newpage?
    calign cp @ d# 1 rshift
    dup thisxt !
    d# 0 lastword !
    sync
    t]
;

header inline: : inline: \ copy the instruction after the call, and skip it
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

\ Where the most recent created word's exit is.  The literal in front of it
\ is 1-4 instructions long, so `create` records the spot.
create doesexit 0 ,

\ Runs inside a defining word: the rest of that word becomes the behaviour
\ of the word just created.  Its exit is replaced by a jump to the code after
\ does>, and the defining word returns to its own caller.
header does>
: (does>)
    r>                                  ( xt of the code after does> )
    dup d# 12 rshift 0= if              \ in the kernel
        doesexit @ w! exit
    then
    dup doesexit @ d# 1 rshift xor d# 12 rshift 0= if    \ on the created word's page
        h# fff and h# 1000 or doesexit @ w! exit
    then
    \ On another page: replace the exit with a jump by xt.  That is longer,
    \ so the created word has to be the last code compiled.
    doesexit @ d# 2 + cp @ <> d# 22 -throw
    doesexit @ cp !
    tliteral
    inline: >r
    h# 7403 code,
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
    cp @ d# 1 rshift thisxt @ <> if
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
    target or
    swap w!
    sync
;

header-imm begin
: tbegin
    target
;

header-imm again
: tagain
    code,
;

header-imm until
: tuntil
    h# 2000 or code,
;

\ else is ahead, then the if's then.  while is an if kept under the loop's
\ start, and repeat is again followed by that if's then.
header-imm else
:noname
    cp @  h# 0000 code,
    swap tthen
;

header-imm while
:noname
    tif swap
;

header-imm repeat
:noname
    tagain tthen
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
            dup d# 12 rshift 0= if              \ kernel: reachable from anywhere
                h# 4000 or code,
            else
                dup cp @ d# 1 rshift xor d# 12 rshift 0= if  \ this page
                    h# fff and h# 5000 or code,
                else                            \ another page: go by xt
                    tliteral
                    ['] execute h# 4000 or code,
                then
            then
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
        dup uw@ dup if     \ link: page offset of the previous leave, marked
            h# 1fff and cp @ h# 1fff invert and or
        then
        swap                ( next leafptr )
        target swap w!
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
;fallthru
\ Reserve a forward jump and chain it for resolveleaves.  The slot holds the
\ page offset of the previous one in the chain, marked so it is never zero.
: leaf,
    cp @
    leaves @ dup if h# 1fff and h# 4000 or then code,
    leaves !
;

header-imm ?do
:noname
    leaves @ d# 0 leaves !
    ['] (?do) compile,
    tif
        leaf,
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

header hex
: hex
    d# 16 setbase
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
header-imm >r   :noname     inline: >r ;
header-imm r>   :noname     inline: r> ;
header-imm r@   :noname     inline: r@ noop ;

: jumptable ( u -- ) \ add u to the return address
    r> + >r ;

\   (doubleAlso) ( c-addr u -- x 1 | x x 2 )
\               If the string is legal, give a single or double cell number
\               and size of the number.

\ A created word is the literal address of its data field, then exit.

header create
: tcreate
    mkheader attach
    align
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
    adepth >r
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
        tib >c dup d# 128 accept
        source!
        d# 0 >in !
    then
;

header evaluate
: tevaluate
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
    ['] uart-emit 'emit !
    decimal
    "cold" >c d# 4 sfind if
        execute
    else
       2drop
    then
    quit
;

\ Everything below is compiled into code page 1.  The kernel never calls it
\ directly: these words are reached by name from the prompt, or through an
\ execution token.
\ It is written for 64-bit cells: the drivers and the display work in
\ 64-bit quantities throughout.
\ It has not yet been moved to cell addresses, so it is left out of a
\ machine built with them.
W64? [if]
CELLADDR? [if] [else]
meta
    $2000 org
target

include system.fs
[then]
[then]

meta
    link @ t,
    link @ t' forth tvar!
    llink @ t' labels tvar!
    there  t' dp tvar!
    tcp @ code-base max $1fff + $1fff invert and
    t' cp tvar!             \ run-time definitions start on the next free code page
target
