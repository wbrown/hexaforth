\ nuc.fs - the nucleus: the Forth that runs on the machine.  Its
\ kernel is on code page 0, and the first krom-bytes of that may
\ be a ROM.  The peephole (join.fs) and the system (system.fs)
\ follow on the later pages.  MODEL.md describes the machine:
\ its stacks, memory, instructions and ports.
\
\ An address is the number of a cell; @ and ! move one cell.
\ c@ c! uw@ w! take the address of a byte: the cell's address
\ times the bytes in a cell, plus the byte's place in the cell,
\ lowest byte first.  >c and c> convert between the two.
\ dp counts bytes; here is the cell dp is in.
\ An execution token and a branch target are the number of an
\ instruction; i@ and i! fetch and store one.
\ A code page is 4,096 instructions.  A branch or a call reaches
\ its own page and page 0.

\ ports (MODEL.md)
$00f1 constant IO-OUT
$00e0 constant IO-IN
$00d0 constant IO-SP    \ data stack depth
$00d1 constant IO-RSP   \ return stack depth

meta
    4 org
target

\ ---- tasks: a record is the next task, the depth of the return
\ stack it stopped at, the word it runs, and its own variables.
\ The tasks share the two stacks: each has its own range of depths.

\ The build sets the first cell to main-task itself (end of file).
create main-task 0 , 0 , 0 ,   0 , 0 , $a , 0 ,   0 , 0 , 0 , 0 ,
header up create up 0 ,                     \ the task that is running

#4 constant HANDLER-CELL                    \ where a record has its handler,
#6 constant EMIT-CELL                       \ and where its output goes

header sbase    : sbase ( -- a )    up @ c# 3 + ;   \ where its data stack starts
: handler ( -- a )                  up @ HANDLER-CELL + ;   \ the catch that a throw goes to
header base     : base ( -- a )     up @ c# 5 + ;
header 'emit    : 'emit ( -- a )    up @ EMIT-CELL + ;
header sourceid : sourceid ( -- a ) up @ c# 7 + ;
header >in      : >in ( -- a )      up @ c# 8 + ;
: sourceC ( -- a )                  up @ c# 9 + ;   \ two cells: the text being interpreted

\ A baseword is a macro of cross.fs: one or two instructions,
\ compiled inline.  To be typed at the prompt it needs a header
\ and a body here.  `header X :noname X ;` gives it both.
\ `header X callable X` does too, and also makes uses of X in the
\ kernel a call instead of inline.
\ A word defined here with a baseword's name (1-, rot, -) takes
\ the name from there on: a call, where the baseword was inline.
\
\ `;into Y` ends a definition with no exit: it runs on into Y,
\ the definition after it.
header 0=       callable 0=         ( x -- f )
header 0<>      : 0<> ( x -- f )    d# 0 ;into <>
header <>       callable <>         ( x1 x2 -- f )
header 0>       : 0> ( n -- f )     d# 0 ;into >
header >        callable >          ( n1 n2 -- f )
header 0<       callable 0<         ( n -- f )
header isnotspace? : isnotspace? ( c -- f ) d# 32 ;into u>
header u>       callable u>         ( u1 u2 -- f )
header 2*       callable 2*         ( x -- x' )
header 2dup     callable 2dup       ( x1 x2 -- x1 x2 x1 x2 )
header bounds   callable bounds     ( a n -- a+n a )
header -rot     callable -rot       ( x1 x2 x3 -- x3 x1 x2 )
header 2drop    callable 2drop      ( x1 x2 -- )
header io!      callable io!        ( x port -- )

header negate   :noname ( n -- -n ) invert ;into cell+
header 1+       ( n -- n+1 )                \ the same word as cell+
header cell+    : cell+ ( a -- a+1 ) CELL + ;
header cells    : cells ( n -- n ) ;

header >c       : >c ( a -- c-addr ) (>c) ;     \ a cell's byte address

header key?
: key? ( -- f ) \ is a character waiting on the console?
    h# e1 io@
;

header uart-emit : uart-emit ( c -- ) IO-OUT io! ;

header space
: space ( -- )
    d# 32
;into emit
header emit
: emit ( c -- ) 'emit @ >r ;      \ as execute: return into the vector

header cr
: cr ( -- )
    d# 13 emit
    d# 10 emit
;

header bl
: bl ( -- c )
    d# 32
;

\ hex8 prints eight digits: DOUBLE calls what follows with the
\ upper half, which then runs again with the lower.
: hex8 dup d# 16 rshift DOUBLE
: hex4 dup d# 8 rshift DOUBLE
: hex2 dup d# 4 rshift DOUBLE
header hex1 : hex1 ( u -- ) \ prints the lowest hex digit of u
    h# f and
;into .digit
: .digit ( u -- )
    dup d# 9 > d# 7 and +  [char] 0 + emit
;

header .x : .x ( u -- ) \ prints 8 hex digits and a space
    hex8 space ;
header .x2 : .x2 ( u -- ) \ prints 2 hex digits and a space
    hex2 space ;

header true     : ttrue ( -- -1 )   true     ;
header false    : tfalse ( -- 0 )   false    ;
header rot      : rot ( x1 x2 x3 -- x2 x3 x1 ) >r swap r> swap ;
header tuck     :noname ( x1 x2 -- x2 x1 x2 ) tuck ;
header ?dup     : ?dup ( x -- 0 | x x ) dup if dup then ;

header +!       :noname ( n a -- )  +!       ;
header or!      :noname ( x a -- )  or!      ;
header and!     :noname ( x a -- )  and!     ;
header xor!     :noname ( x a -- )  xor!     ;
header *!       :noname ( n a -- )  *!       ;
header not!     :noname ( a -- )    not!     ;
header 2swap    : 2swap ( x1 x2 x3 x4 -- x3 x4 x1 x2 ) rot >r rot r> ;
header 2over    : 2over ( x1 x2 x3 x4 -- x1 x2 x3 x4 x1 x2 )
    >r >r 2dup r> r> 2swap ;

header min      : min ( n1 n2 -- n3 ) 2dup < if drop exit then nip ;
header max      : max ( n1 n2 -- n3 ) 2dup < if nip exit then drop ;

header hi32  :noname ( dw -- w ) hi32 ;
header lo32  : lo32 ( dw -- w ) h# ffffffff and ;
header hi16  : hi16 ( dw -- s ) d# 16 rshift ;into lo16
header lo16  : lo16 ( dw -- s ) h# ffff and ;

header count
: count ( c-addr -- c-addr+1 c )
    dup 1+ swap
;into c@
header c@
: c@ ( c-addr -- c ) h# ff ;into field@
\ a field of the cell a byte is in, from that byte up
header field@ : field@ ( c-addr mask -- x )
    >r
    dup OFFSET-MASK and >r
    (c>) @
;into (field)
\ a cell from one of its bytes up, and masked
: (field) ( w -- x ) ( R: mask place -- )
    r> d# 3 lshift rshift
    r> and
;

header c!
: c! ( c c-addr -- ) h# ff ;into field!
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

\ An instruction is sixteen bits: a cell, or where a cell is
\ more, the two bytes at twice the instruction's number.
W32+? [if]
header i@
: i@ ( ia -- w ) 2* ;into uw@
[then]
header uw@
: uw@ ( c-addr -- w ) h# ffff field@ ;

W32+? [if]
header i!
: i! ( w ia -- ) 2* ;into w!
[then]
header w!
: w! ( w c-addr -- ) h# ffff field! ;

W32+? [if] [else]
header i@
: i@ ( ia -- w ) @ ;
header i!
: i! ( w ia -- ) ! ;
[then]

header type
: type ( c-addr u -- )
    bounds
    begin
        2dup xor
    while
        dup c@ emit
        1+
    repeat
    2drop
;

create "cold"
    'c' c, 'o' c, 'l' c, 'd' c,
create "boot"
    'b' c, 'o' c, 'o' c, 't' c,
create "error:"
    'e' c, 'r' c, 'r' c, 'o' c, 'r' c, ':' c, $20 c,

\ forth, the header a search starts at, is the first cell of the data (cross.fs)
create cp       0 ,         \ Code pointer, grows up,
create cpn      0 ,         \ and where it goes on from after page 1
create cpr      $e00 ,      \ and how far into a page a definition may begin
\ for each code page, the instruction its code has reached, or 0
\ for a page with none (pages-laid, cross.fs)
create pages   meta code-pages cell-bytes * target allot
create dp       0 ,         \ Data pointer, in bytes, grows up,
create dpl      0 ,         \ and where the data must stop
create bdp      0 ,         \ the same of what is not variables (bulk)
create bdl      0 ,
create hdp      0 ,         \ the cell the next header goes in,
create hdl      0 ,         \ and the cell that headers stop before
create lastword 0 ,         \ the header being defined, or 0 for :noname
create thisxt   0 ,         \ where the definition being compiled starts
create syncpt   0 ,         \ the last place a branch lands: the peephole joins no instruction before it
create state    0 ,         \ 3 while compiling, 0 while interpreting; tested only for 0
create delim    0 ,         \ the character parse stops at
header blk create blk 0 ,   \ the block being read as source, or 0
header 'console create 'console 0 , \ where output goes when the prompt starts again
\ a block of 1,024 bytes: ( buf blk# -- ), buf the address of a cell
header 'blk-read create 'blk-read  0 ,
header 'blk-write create 'blk-write 0 ,
create rO       0 ,         \ the limit of the DO loop that is running (see `do`)
create leaves   0 ,         \ the LEAVEs of the loop being compiled, as a chain
create tib $80 allot

header dp    :noname ( -- a ) dp ;
header cp    :noname ( -- a ) cp ;
header state :noname ( -- a ) state ;
header forth :noname ( -- a ) forth ;

: prev ( -- ia ) cp @ ;into 1-  \ where the last instruction is
header 1-       : 1- ( n -- n-1 )   d# -1 + ;

header adepth : adepth ( -- n ) IO-SP io@ 1- ;            \ less the port number itself
header depth    : depth ( -- n )    adepth sbase @ ;into -        \ from sbase
header -        : - ( n1 n2 -- n3 ) negate + ;
header s>d
: s>d ( n -- d ) dup 0< ;
header abs      : abs ( n -- u )    s>d if negate then ;
header 2/ : 2/ ( n -- n/2 ) \ the sign stays where it is
    d# 0 2dup < >r                          ( n 0 ) ( R: is n negative? )
    1+ over swap rshift                     ( n n>>1 )
    TOPBIT r> swap lshift                   ( n n>>1 sign )
    or nip
;
header here     : here ( -- a )     dp @ ;into c>
header c>       : c> ( c-addr -- a ) (c>) ;     \ the cell a byte is in
header align    : align ( -- )      dp @ CELLB 1- + ALIGN-MASK and dp ! ;

\ pause runs the next task.  The task that stops pushes rO and
\ the depth of its data stack on its return stack; its record
\ keeps only the depth of that return stack.
header pause
: pause ( -- )
    rO @ >r  adepth >r
    IO-RSP io@ up @ CELL + !
    up @ @ up !
    up @ CELL + @ IO-RSP io!
    r> IO-SP io!  r> rO !
;

header key
: ckey ( -- c )
    begin key? if key exit then pause again
;


header aligned
: aligned ( a -- a ) \ the address of a whole cell, at or after a
    (>c) CELLB 1- + c>
;

header d2*
: d2* ( d -- 2d )
    2dup
;into d+
header d+
: d+ ( d1 d2 -- d3 )
    rot + >r                      ( augend addend) 
    over +                        ( augend sum)
    swap over swap                ( sum sum augend)
    u<                            ( sum carry )
    negate r> +                   ( sum . )
;

header dnegate
: dnegate ( d -- -d )
    >r invert r> invert
    d# 1. d+
;

header source
: source ( -- c-addr u ) sourceC ;into 2@
header 2@
: 2@ ( a -- lo hi )
    dup cell+ @ swap @
;

header 2!
: 2! ( lo hi a -- )
    tuck !
    cell+ !
;

: unparsed ( -- c-addr u ) source >in @ ;into /string
header /string
: /string ( c-addr u n -- c-addr+n u-n )
    dup >r - swap r> + swap
;

\ One step for every bit of a cell.  DOUBLE calls the code that
\ follows it and then runs into it, so that code runs twice.
: mulstep ( ud u -- ud u ) \ ud: the product, under what is left of the multiplier
W64? [if]
    DOUBLE
[then]
W32+? [if]
    DOUBLE
[then]
    DOUBLE DOUBLE DOUBLE DOUBLE
    false >r                    ( lo hi u ) ( R: 0 )
    over r@ <                   ( lo hi u f )
    rdrop over >r               ( lo hi u f ) ( R: u )
    and >r                      ( lo hi ) ( R: u u&f )
    d2*
    r> d# 0 d+
    r>
;

header um*
: um*  ( u1 u2 -- ud )
    d# 0 -rot
    mulstep
    drop
;

header *        :noname ( n1 n2 -- n3 ) * ;

\ see Hacker's Delight (2nd ed) 9-4 "Unsigned Long Division"
: divstep  \ ( y x z )   a step for every bit of a cell
W64? [if]
    DOUBLE
[then]
W32+? [if]
    DOUBLE
: div16                  \ 16 steps, of a dividend at the top of y
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
: um/mod ( ud u -- rem quot )
    divstep
    drop swap 
; 

\ a dividend that fits in 16 bits takes 16 steps
header u/mod : u/mod ( u1 u2 -- rem quot )
W32+? [if]
    >r
    dup d# 16 rshift if
        d# 0 r> um/mod exit
    then
    TOP16 lshift d# 0 r> div16
    drop swap
[else]
    d# 0 swap um/mod
[then]
;

\ Unicode-friendly ACCEPT contibuted by Matthias Koch

: delchar ( addr len -- addr len )
    dup if d# 8 emit space d# 8 emit then

    begin
        dup
    while
        1- 2dup + c@
        h# C0 and h# 80 <>
    until then
;

header accept
: accept ( c-addr n1 -- n2 ) \ reads a line of at most n1 characters
    >r d# 0  ( addr len R: maxlen )

    begin
        ckey   ( addr len key R: maxlen )
        
        d# 9 over = if drop d# 32 then
        d# 127 over = if drop d# 8 then
        
        dup d# 31 u>
        if
            over r@ u<
            if
                dup emit
                >r 2dup + r@ swap c! 1+ r>
            then
        then
        
        d# 8 over = if >r delchar r> then
          
        d# 10 over = swap d# 13 = or
    until
    
    rdrop nip
    space
;

: 3rd   >r over r> swap ;
header 3dup : 3dup ( x1 x2 x3 -- x1 x2 x3 x1 x2 x3 )
    3rd 3rd 3rd ;

header cells-move
: cells-move ( from to n -- )
    >r over - r> swap >r bounds                     ( end from ) ( R: from here to there )
    begin
        2dup xor
    while
        dup @  over r@ + !
        1+
    repeat
;into cells-done
\ the end of a loop over cells that kept x on the return stack:
\ jumped to, and never called
: cells-done ( end a -- ) ( R: x -- )
    2drop rdrop
;

header cells-fill
: cells-fill ( x a n -- )
    bounds rot >r                                   ( end a ) ( R: x )
    begin
        2dup xor
    while
        r@ over ! 1+
    repeat
    cells-done
;

\ ---- the dictionary
\
\ A header is cells: a link to the header looked at after it, or
\ 0; the xt, with the top bit set if the word is immediate; and
\ the name.  A name is codes of five bits, the first in the lowest
\ bits of a cell, and the top bit of its last cell is set
\ (cross.fs lists the codes).  A name is looked up by packing it
\ the same way into skey and comparing cells.

\ fields packed into cells
create pto 0 ,              \ the cell in hand: the last with a field in it,
create psh 0 ,              \ the bit of it the next field goes at,
create pw 0 ,               \ how many bits a field is,
create pfull 0 ,            \ and how many bits of a cell fields have

: pack> ( a width full -- ) \ to pack fields from the cell at a
    pfull !  pw !
    false over !  pto !
    psh false swap !
;

\ when the cell is full, pto steps on, and 0 goes there and in psh
: pack, ( x -- ) \ the next field
    psh @ pfull @ = if
        d# 1 pto 2dup @ +                   ( 1 pto p )
        tuck swap !                         ( 1 p )
        meta N->IN IN^N ->[T] alu target    ( 1 p )     \ over dup xor over !
        drop dup xor                        ( 0 )
        psh !
    then
    psh @ lshift  pto @ or!
    pw @ psh +!
;

create skey #96 allot       \ 63 characters of two codes each, and a cell
create kcode codes-of-chars,    \ a character's code, plus 32 or 64 if it is of one of the two further lots

: kcode@ ( c -- what ) h# 7f and kcode >c + c@ ;

header >key
: >key ( c-addr u -- n ) \ pack a name into skey; n is its size in cells
    d# 63 min
    skey d# 5 NAME-BITS pack>
    bounds
    begin
        2dup xor
    while
        dup c@ kcode@                       ( end a what )
        dup d# 31 > if                      \ which lot of the rest, then which
            dup d# 5 rshift d# 29 + pack,  d# 31 and
        then
        pack,
        1+
    repeat
    2drop
    d# 1 TOPBIT lshift  pto @ or!
    pto @ skey - 1+
;

header samename?
: samename? ( wp -- f ) \ has the header that name?
    CELL2 + ;into same-cells?
: same-cells? ( name -- f ) \ are the cells from there the key's?
    skey                                    ( name key )
    begin
        over @ over @ =
    while
        dup @ 0< if 2drop true exit then
        1+ swap 1+ swap
    repeat
    2drop false
;

\ The search goes from forth's header along the links.  A header
\ made here is linked before the rest; the image's are linked in
\ order of use (cross.fs, headers-ordered).
header sfind
: sfind ( c-addr u -- c-addr u 0 | xt flag ) \ flag: 1 immediate, -1 not
    2dup >key drop
    skey @  forth @                         ( c-addr u key wp )
    begin
        dup
    while
        dup @ >r  CELL2 +                   ( c-addr u key name ) ( R: the header after )
        2dup @ = if
            dup same-cells? if
                \ x is the header's cell: the xt, and the flag
                true + nip                  ( c-addr u name-1 )
                @ rdrop >r drop             ( c-addr ) ( R: x )
                d# 1 r@ over lshift >r      ( c-addr 1 ) ( R: x x<<1 )
                r> swap rshift              ( c-addr xt ) ( R: x )
                nip r>                      ( xt x )
                0< invert d# 1 or
                exit
            then
        then
        drop r>
    repeat
    nip
;

create nbuf #64 allot           \ a name as text, until the next is asked for
create kpg 0 ,                  \ which lot of the rest the next code is of: 32, 64, or 0 for none
create kchar chars-of-codes,    \ the plain codes, and then the two lots of the rest

: code> ( code -- ) \ a character, or which lot the next one is of
    kpg @ ?dup 0= if
        dup d# 29 > if  d# 29 - d# 5 lshift kpg !  exit then
        d# 0
    then
    +  d# 0 kpg !
    kchar >c + c@ pack,
;

header name>
: name> ( a -- c-addr u ) \ the name whose cells start at a
    d# 0 kpg !  nbuf d# 8 CELLBITS pack>
    begin
        dup @ >r                                    ( a ) ( R: the cell )
        NAME-SPARE r@ over lshift >r
        r> swap rshift                              ( a codes )
        begin
            dup
        while
            dup d# 31 and code>
            d# 5 rshift
        repeat
        drop
        1+
        r> 0<
    until
    drop
    nbuf >c  pto @ nbuf - >c  psh @ d# 3 rshift +
;

header words : words ( -- ) \ prints every name, in search order
    forth @
    begin
        dup
    while
        dup CELL2 + name> type
        space
        @
    repeat
    drop
;

\ In kcode a letter is its place in the alphabet, a figure 33-42.
: digit? ( c -- u f )
    kcode@
    dup d# 27 < if
        d# 9 +
    else
        d# 33 -  dup d# 9 u> or
    then
    dup base @ u<
;

: ud* ( ud1 u -- ud2 )
    tuck * >r
    2dup or HALFBITS rshift if
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
    while
        >r 2swap base @ ud*
        r> s>d d+ 2swap
        d# 1 /string
    repeat
        drop
    then
;

header cmove
: cmove ( addr1 addr2 u -- )
    bounds rot >r
    begin
        2dup xor
    while
        r@ c@ over c!
        r> 1+ >r
        1+
    repeat
    cells-done
;

create 'compile 0 ,     \ what compile, does: pcompile, (join.fs)

header-imm recurse
:noname ( -- )
    thisxt @
;into compile,
header compile,
: compile, ( xt -- ) 'compile @ ;into execute
header execute
: execute ( xt -- )
    >r
;

: source! ( addr u -- )
    sourceC 2!
    d# 0 >in !
;

header source-id
: source-id ( -- n )
    sourceid @
;

\ From Forth200x - public domain

header isspace?
: isspace? ( c -- f )
    h# 21 u< ;


: xt-skip   ( addr1 n1 xt -- addr2 n2 ) \ gforth
    \ skip all characters satisfying xt ( c -- f )
    >r
    BEGIN
        over c@ r@ execute
        over and
    WHILE
        d# 1 /string
    REPEAT
    rdrop ;

header parse-name
: parse-name ( "name" -- c-addr u )
    unparsed
    ['] isspace? xt-skip  over -rot
    ['] isnotspace? xt-skip
;into parsed
\ >in goes past what was parsed and the character that ended it
: parsed ( start end rest -- c-addr u )
    2dup d# 1 min + source drop - >in !
    drop over -
;

header isnotdelim
: isnotdelim ( c -- f )
    delim @ <>
;

header parse
: parse ( c "ccc<c>" -- c-addr u )
    delim !
    unparsed  over -rot
    ['] isnotdelim xt-skip
    parsed
;

\ A line of a block ends at the next whole number of lines.
header-imm \
:noname ( -- )
    blk @ if
        >in @ LINE-ROUND + LINE-MASK and
    else
        source nip
    then
    >in !
;

header-imm (
:noname ( -- )
    [char] ) parse 2drop
;


\ A word whose name cross.fs has a macro of is named otherwise
\ here (tallot, t, tliteral, tif), under a header of the name.
header ,
: t, ( x -- )
    align
    here !
    CELLB
;into allot
header allot
: tallot ( n -- )
    dp +!
;

header w,
: tw, ( w -- )
    dp @ w!
    d# 2 tallot
;

header c,
: tc, ( c -- )
    dp @ c!
    d# 1 tallot
;

: (.) ( u -- )
    base @ u/mod
    ?dup if (.) then
    .digit
;

header .
: . ( n -- )
    s>d if [char] - emit negate then
    (.) space
;

\ catch pushes the data stack's depth, the previous handler and
\ rO on the return stack, and keeps the return stack's depth in
\ `handler`.  throw cuts the return stack back to that depth and
\ restores the three, leaving the thrown number where the xt
\ was.  With no catch, throw prints the number and the line as
\ far as it was read, which ends with the word the throw came
\ from, empties both stacks and restarts the prompt.

header-imm [
: t[ ( -- )
    d# 0 state !
;

header decimal
: decimal ( -- )
    d# 10
;into setbase
: setbase ( n -- )
    base !
;

header hex
: hex ( -- )
    d# 16 setbase
;

: afresh ( -- ) \ what the console and throw need
    main-task dup up !  dup !               \ the first task, in a ring of one
    ['] uart-emit dup main-task EMIT-CELL + !  'console !  decimal
    d# 0 handler !  d# 0 blk !
;into prompting
\ the prompt as it starts: interpreting, from the console
: prompting ( -- )
    t[
    d# 0 sourceid !
    d# 0 sbase !        \ only the first task has no catch round it
;

: isvoid ( caddr u -- ) \ any char remains, throw -13
    nip 0<> d# 13
;into -throw
\ a must be a flag, -1 or 0: it is used as a mask
header -throw : -throw ( a b -- ) \ if a is true, throw -b
    negate and
;into throw
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
        dup invert if                   \ ABORT, which is -1, prints nothing
            "error:" >c d# 7 type
            .
            source drop >in @ type cr
        then
        prompting
        'console @ 'emit !
        d# 0 IO-RSP io!
        d# 0 IO-SP io!
        d# 1 execute        \ word 1 of the boot vector: quit
    then
;

\ copy the instruction after the call, and return to the caller
\ of the word the call is in, which so ends with ;fallthru
: inline; ( -- )
    r> i@
;into again
inline-operand

header-imm again
: tagain ( dest -- ) ;into code,    \ a jump is its target's field
: code,
    cp @ 1+ h# fff and if           \ the last slot of a page stays empty
        cp @ i!
        d# 1 cp +!
        exit
    then
    d# -8 throw
;

header inline: : inline: \ copy the instruction after the call, and skip it
    r>
    dup i@ code,
    1+ >r
;
inline-operand

\ A definition is all on one page.  Before one starts: if the
\ code pointer is past cpr in its page, it moves to the next
\ page, which after page 1 is the page cpn names.
: newpage? ( -- )
    cp @ h# fff and cpr @ u> if
        cp @ h# fff or 1+
        dup CODE-BASE u< if drop cpn @ then
        cp !
    then
    cp @ CODE-BASE CODE-SIZE + u< 0= d# 8 -throw
;

header-imm begin
: tbegin ( -- dest ) ;into landing
: landing ( -- field ) \ of the code about to be compiled
    cp @ dup syncpt !       \ no instruction is joined across it
;into near
\ the target field of a branch or call: an address on page 0 as
\ it is, or the offset in the branch's own page with bit 12 set
: near ( ia -- field )
    dup d# 12 rshift if h# fff and h# 1000 or then
;

: attach
    lastword @ ?dup if
        forth !
    then
;

header s,
: s, ( c-addr u -- ) \ compiles the string, with its length first
    dup tc,
    bounds
    begin
        2dup xor
    while
        count tc,
    repeat
    2drop
;

: mkheader
    newpage?
    dp @ dpl @ u> d# 8 -throw
    parse-name >key                         ( n )
    hdp @ over + CELL2 +  hdl @ u> d# 8 -throw
    \ the link
    forth hdp @ swap >r dup                 ( n h h ) ( R: forth )
    lastword !                              ( n h )
    meta R->IN [IN] ->[T] d+1 alu target    ( n h h )       \ r@ @ over ! dup
    cp @ tuck rdrop                         ( n h xt h xt )
    thisxt !  1+ !                          ( n h )         \ xt
    CELL2 +  2dup + hdp !                   ( n name )
    skey swap rot cells-move
;into sync
: sync
    cp @ syncpt !
;

header immediate
: timmediate ( -- )
    lastword @ cell+
    dup @ d# 1 TOPBIT lshift or
    swap !
;

header :
:noname ( "name" -- )
    mkheader
;into ]
header ]
: t] ( -- )
    d# 3 state !
;

header :noname
:noname ( -- xt )
    newpage?
    cp @
    dup thisxt !
    d# 0 lastword !
    sync
    t]
;

: (loopdone)  ( 0 -- )
    drop
    r> r> rO ! >r
;

\ A literal is twelve bits: h# 8000 pushes them, h# c000 adds
\ them, and h# 1000 more is the next twelve up.
: segs, ( u36 insn -- )
    begin ( u insn )
        over h# fff and ?dup if
            over or code,
            h# 4000 or
        then
        h# 1000 + swap d# 12 rshift swap
        over
    while
    repeat
    nip h# 4000 and if exit then
    ['] tfalse compile,         \ the number was 0
;

header-imm 2literal
: 2literal ( x1 x2 -- )
    swap DOUBLE
\ A negative number is its complement and invert: any on 64
\ bits; on 32 only one whose complement is a single literal; none
\ on 16, where two literals make any cell.  -1 is `true`.
header-imm literal
: tliteral ( x -- )
W64? [if]
    s>d if
        invert ?dup if tliteral inline; invert then
        ['] ttrue compile, exit
    then
[then]
W32? [if]
    dup invert d# 12 rshift 0= if
        invert ?dup if tliteral inline; invert then
        ['] ttrue compile, exit
    then
[then]
W32+? [if] [else]
    dup invert if else drop ['] ttrue compile, exit then
[then]
W64? [if]
    dup d# 36 rshift ?dup if \ wider than 36 bits: high part, shift, add the rest
        tliteral
        d# 36 tliteral
        inline: lshift
        d# 28 2dup lshift >r    \ the low 36 bits
        nip r> swap rshift
        h# c000 segs,
        exit
    then
[then]
    h# 8000 segs,
;

create doesexit 0 ,     \ where the last created word's exit is

header create
: tcreate ( "name" -- )
    align here
;into constant
header constant
: tconstant ( x "name" -- )
    mkheader attach
    tliteral
    cp @ doesexit !
;into exit,
: exit, ( -- ) inline; exit ;fallthru

header variable
: variable ( "name" -- )
    tcreate d# 0 t,
;

\ the created word's exit becomes a jump to the code after does>
header does>
: (does>) ( -- )
    r>                                  ( xt of the code after does> )
    dup d# 12 rshift if
        dup doesexit @ xor d# 12 rshift if
            \ on another page: a jump by xt, and the created word
            \ is to be the last code compiled
            doesexit @ 1+ cp @ <> d# 22 -throw
            doesexit @ cp !
            tliteral
            inline: >r
            exit,  exit
        then
    then
    near doesexit @ i!
;

\ a call, and not of a word that takes its own return address
: jumpable ( op -- f )
    dup h# e000 and h# 4000 =
    swap h# 1fff and
    dup ['] (loopdone) <>
    swap ['] (does>) <> and and
;

header-imm exit
: texit ( -- )
    cp @ thisxt @ <> if                     \ a call before it is made a jump,
        prev dup i@ dup jumpable if
            h# 4000 xor swap i!
            syncpt @ cp @ <> if exit then   \ and no branch lands after it
        else
            2drop
        then
    then
    exit,
;

\ The word goes into the dictionary only when its exit has been
\ compiled.  `;` is join.fs's, which gives `exit` a newer header.

header-imm if
: tif ( -- orig )
    cp @
    h# 2000 code,
;

header-imm ahead
: tahead ( -- orig )
    cp @
    h# 0000 code,
;

header-imm else
:noname ( orig1 -- orig2 )
    tahead
    swap
;into then
header-imm then
: tthen ( orig -- )
    dup i@
    landing or
    swap i!
;

header-imm until
: tuntil ( dest -- )
    h# 2000 or code,
;

header-imm while
:noname ( dest -- orig dest )
    tif swap
;

header-imm repeat
:noname ( orig dest -- )
    tagain tthen
;

header folds-at create folds-at 0 ,     \ where folds.fs's table is

: (compile,) ( xt -- ) \ a call to a word, and nothing else
    dup d# 12 rshift if
        dup cp @ xor d# 12 rshift if    \ another page: its xt, and a call of execute
            tliteral
            ['] execute
        then
    then
    near h# 4000 or code,
;

\ DO...LOOP: the top of the return stack counts up from
\ start - limit to 0.  rO holds the limit, and I is that count
\ plus rO.  The loop before's rO is kept on the return stack.

: (do)  ( limit start -- start-limit )
    swap r@                                 ( start limit ra )
    rO tuck rdrop @ >r                      ( start limit rO ra ) ( R: old )
    >r                                      ( start limit rO ) ( R: old ra )
    over swap !                             ( start limit )
    -
;

: (?do)  ( limit start -- start-limit start!=limit )
    swap r@
    rO tuck rdrop @ >r
    >r
    over swap !
    -
    dup 0=
;

: (loopnext)
    d# 1 + dup
;

header-imm do
:noname ( -- do-sys )
    leaves @ d# 0 leaves !
    ['] (do) compile,
    tbegin
    inline; >r
;fallthru

header-imm loop
:noname ( do-sys -- )
    inline: r>
    ['] (loopnext) compile,
    tif
    swap tagain
    tthen
;into resolveleaves
: resolveleaves \ each LEAVE of the chain from leaves lands here
    leaves @
    begin
        dup
    while
        dup i@ dup if
            h# fff and cp @ h# fff invert and or
        then
        swap                ( next leafptr )
        landing swap i!
    repeat
    drop
    leaves !
    ['] (loopdone) compile,
;

\ finished when the count has gone round, up or down
: (+loopnext) ( inc R -- R finished )
    dup >r over +                           ( inc new ) ( R: old )
    swap 0<                                 ( new f )       \ is the step negative?
    over r> u<                              ( new f f2 )    \ is new below old?
    xor
;

header-imm +loop
:noname ( do-sys -- )
    inline: r>
    ['] (+loopnext) compile,
    tuntil
    resolveleaves
;

header-imm leave
: leave ( -- )
    inline: r>
;into leaf,
\ a LEAVE is a jump.  Until the loop ends its target is the page
\ offset of the LEAVE before, with h# 4000 set, or 0 for none.
: leaf,
    cp @
    leaves @ dup if h# fff and h# 4000 or then code,
    leaves !
;

header-imm ?do
:noname ( -- do-sys )
    leaves @ d# 0 leaves !
    ['] (?do) compile,
    tif
        leaf,
    tthen
    tbegin
    inline; >r
;fallthru

header i
: i ( -- n ) \ the index of the loop that is running
    r>
    r@ over >r                              ( ra count ) ( R: count ra )
    rO  meta [T]->IN IN+N ->T d-2 alu target    \ @ + nip
;

header j
: j ( -- n ) \ the index of the loop outside that one
    r> r>
    r> r> 2dup + -rot
    >r >r
    -rot
    >r >r
;

header-imm unloop
:noname ( -- )
    inline: r>
    ['] (loopdone) compile,
;

header !        :noname ( x a -- )          !       ;
header +        :noname ( n1 n2 -- n3 )     +       ;
header xor      :noname ( x1 x2 -- x3 )     xor     ;
header and      :noname ( x1 x2 -- x3 )     and     ;
header or       :noname ( x1 x2 -- x3 )     or      ;
header invert   :noname ( x -- x' )         invert  ;
header =        :noname ( x1 x2 -- f )      =       ;
header <        :noname ( n1 n2 -- f )      <       ;
header u<       :noname ( u1 u2 -- f )      u<      ;
header swap     :noname ( x1 x2 -- x2 x1 )  swap    ;
header dup      :noname ( x -- x x )        dup     ;
header drop     :noname ( x -- )            drop    ;
header over     :noname ( x1 x2 -- x1 x2 x1 ) over  ;
header nip      :noname ( x1 x2 -- x2 )     nip     ;
header @        :noname ( a -- x )          @       ;
header io@      :noname ( port -- x )       io@     ;
header rshift   :noname ( x u -- x' )       rshift  ;
header lshift   :noname ( x u -- x' )       lshift  ;
\ >r r> r@ rdrop are immediate words, defined in join.fs.


: consume1 ( caddr u ch -- caddr' u' f )
    >r over c@ r> =
    over 0<> and
    dup >r d# 1 and /string r>
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

: tick? ( f c-addr -- f' ) \ and is the character there a quote?
    c@ [char] ' = and
;
: is'c' ( caddr u -- f )
    d# 3 =
    over tick?
    swap 1+ 1+ tick?
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

: sought ( "name" -- xt flag ) \ the word of that name, or -13
    parse-name sfind
    dup 0= d# 13 -throw
;

header-imm postpone
:noname ( "name" -- )
    sought
    0< if
        tliteral
        ['] compile,
    then
    compile,
;

header interpret1
: interpret1 ( c-addr u -- ) \ a word of the source: done, or compiled
    sfind ?dup if
        0< state @ and if compile, exit then
        execute exit
    then
;into numbered
header numbered
: numbered ( c-addr u -- ) \ a number of the source: left, or compiled
    (doubleAlso)                                ( x literal )
    state @ if execute exit then
    drop
;

: interpret
    begin
        parse-name dup
    while
        interpret1
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
    up dup rdrop @                          ( up task )
    HANDLER-CELL +  r> rdrop over !         ( up handler )
    drop dup xor                            ( 0 )
;

\ an idle task's word is cells, which does nothing
: (task) ( -- ) \ its word again and again; idle once that throws
    begin
        up @ c# 2 + @ catch
        ?dup if [char] ? emit .  ['] cells up @ c# 2 + ! then
        pause
    again
;

\ the word to run, where its parts of the two stacks start, and
\ eleven cells for its record
header spawn
: spawn ( xt dbase rbase task -- )
    >r
    over swap                       ( xt dbase dbase rbase )
    IO-RSP io@ swap                 ( xt dbase dbase mine rbase )
    dup IO-RSP io!                  \ on its return stack,
    ['] (task) >r  d# 0 >r  rot >r  \ what pause leaves   ( xt dbase mine rbase )
    d# 3 + swap IO-RSP io!          ( xt dbase rdepth )
    \ cells 1, 3 and 2 of the record
    r@ CELL + tuck !                ( xt dbase a )      \ the depth
    tuck d# 2 + !                   ( xt a )    \ where its data stack starts
    1+ !                                        \ the word it runs
    d# 0 r@ c# 4 + d# 7 cells-fill
    d# 10 r@ c# 5 + !
    \ where its output goes; and then it is next after this one
    'emit r@ over >r dup            ( 'emit task task ) ( R: task 'emit )
    EMIT-CELL + r@ @ swap !         ( 'emit task )
    up @ rdrop @ swap !             ( 'emit ) ( R: task )
    up @ nip r> swap !
;

header '
: tick ( "name" -- xt )
    sought drop
;

\ with a display, ['] is placement.fs's
DISPLAY? [if] [else]
header-imm [']
:noname ( "name" -- )
    tick tliteral
;
[then]

header refill
: refill ( -- f )
    source-id 0= dup
    if
        tib >c dup d# 128 accept
        source!
    then
;

header evaluate
: tevaluate ( c-addr u -- )
    blk dup @ >r  false swap !
    >in @ >r  source >r >r
    source-id >r d# -1 sourceid !
    source!
    interpret
    r> sourceid !
    r> r> source!  r> >in !
    r> blk !
;

\ ---- starting from the ROM (doc/boot.md)
\
\ The machine starts at instruction 0, which calls main.  Memory
\ may hold only the ROM.  main reads block 0 of the disk, the
\ index (doc/disk.md), loads every binary entry to the address
\ the entry gives, and runs the word named boot.  With no disk
\ and no image it reads the image from the console (ipl, the
\ initial program load).

\ The disk's driver is the file ROM names when the image is
\ built.  It gives:
\   disk? ( -- read write true | false )
\ where read and write are each ( a blk -- ): a block of 1,024
\ bytes to or from the cells at a.
include-rom

\ Is the image in memory?  In an image main-task's first cell is
\ main-task (the build sets it); in a bare ROM's memory it is not.
: image? ( -- f ) main-task @ main-task = ;

: boot-buf ( -- a ) MEM-END BLKCELLS - ;    \ block 0, in the last block of memory
: entry ( i -- a ) ENTRY-BYTES BYTESHIFT rshift *  boot-buf + ;
header ifield
: ifield ( a offset -- u ) \ two bytes that far into the cells from a
    h# ffff >r
    dup OFFSET-MASK and >r
    BYTESHIFT rshift + @
    (field)
;

\ The image from the console: numbers in hex, each followed by
\ @ (where the next cell goes), a space (the next cell), # (the
\ sum of the address and the cells since) or ! (the end).
: hexit ( c -- u ) \ 16 or more if c is not a digit
    dup [char] a < if [char] 0 else [char] a d# 10 - then -
;
: hex-in ( -- u c ) \ a number from the console, and what follows it
    d# 0
    begin
        key dup hexit dup d# 16 u<
    while
        nip swap d# 4 lshift or
    repeat
    drop
;
: deposit ( a sum u -- a' sum' ) dup >r +  swap r> over !  1+ swap ;
: record ( a sum u c -- a sum )
    dup bl = if drop deposit exit then
    dup [char] @ = if drop nip nip dup exit then
    [char] # = if over xor if [char] ? emit over . cr then exit then
    drop
;
: ipl ( -- )
    d# 0 d# 0                               ( a sum )
    begin
        hex-in dup [char] ! xor
    while
        record
    repeat
    2drop 2drop
;

: blocks-to ( read a first n -- read ) \ n blocks from first, to a
    bounds 2swap swap >r -rot               ( a end first ) ( R: read )
    begin
        2dup xor
    while
        rot 2dup swap r@ execute            ( end first a )
        BLKCELLS + -rot  1+
    repeat
    2drop drop r>
;

\ every entry is taken from block 0 before a block is read
: binaries ( read i -- read )
    dup if
        dup entry  dup ENTRY-TYPE ifield TYPE-BINARY = if   ( read i e )
            dup ENTRY-WHERE ifield  BLKCELLS *
            over ENTRY-FIRST ifield  rot ENTRY-BLOCKS ifield    ( read i a first n )
            >r >r >r  1- binaries  r> r> r>  blocks-to
            exit
        then
        drop 1- binaries exit
    then
    drop
;

: from-index ( read -- read f ) \ was block 0 this machine's index?
    dup >r  boot-buf d# 0 r> execute
    d# 0 entry  dup ENTRY-TYPE ifield TYPE-INDEX =
    swap ENTRY-WHERE ifield CELLBITS =  and
    dup if >r  ENTRIES 1- binaries  r> then
;

: by-name ( c-addr u -- ) \ run, if there is such a word
    sfind if execute exit then 2drop
;

meta
\ say" text" emits it from numbers in the code
: say-packed ( c-addr k -- x ) \ k characters, the first lowest
    0 swap 0 ?do  over i + c@  i 8 * lshift or  loop  nip ;
:: say" ( "text<quote>" -- )
    [char] " parse
    begin dup while
        2dup cell-bytes min say-packed literal  s" emits" evaluate
        dup cell-bytes min /string
    repeat
    2drop ;
target

: emits ( x -- ) \ the characters in x, the lowest first
    begin
        dup
    while
        dup h# ff and emit  d# 8 rshift
    repeat
    drop
;

: no-disk ( -- ) say" no disk, ready to receive" cr ;

: main
    image? afresh                           ( f )
    disk? if                                ( f read write )
        rot drop  >r  from-index            ( read f ) ( R: write )
        swap 'blk-read !  r> 'blk-write !
        if "boot" >c d# 4 by-name then
    else
        0= if no-disk ipl then
    then
    "cold" >c d# 4 by-name
;into quit
header quit
: quit ( -- ) \ the prompt: reads a line and interprets it, for ever
    begin
        refill drop
        interpret
        say"  ok"
        cr
    again
;

\ ---- the nucleus's words that are not in its ROM: nothing in
\ the ROM runs one before the image is read (kernel_test, typed)
meta rom-ends upper-half target

header .s
: .s ( -- ) \ prints the data stack, the top last, and keeps it
    depth dup                       ( x1 .. xn n n )
    begin dup while  rot >r 1-  repeat
    drop                            ( n ) ( R: xn .. x1 )
    begin dup while  r> dup . swap 1-  repeat
    drop
;

header dabs
: dabs ( d -- ud )
    dup 0< if dnegate then
;

header 2rot
: 2rot ( x1 x2 x3 x4 x5 x6 -- x3 x4 x5 x6 x1 x2 )
    >r >r 2swap r> r> 2swap
;

header d>s
: d>s ( d -- n ) drop ;

header d=
: d= ( a b c d -- f )
    >r                      \ a b c
    rot =                   \ b a=c
    swap r> =               \ a=c b=d
    and
;

header d<
: d< ( d1 d2 -- f )
    rot         \ al bl bh ah
    2dup xor if > nip nip exit then
    2drop u<
;

header du<
: du< ( ud1 ud2 -- f )
    rot         \ al bl bh ah
    2dup xor if u> nip nip exit then
    2drop u<
;

header d-
: d- ( d1 d2 -- d3 )
    dnegate d+
;

header d0<
: d0< ( d -- f )
    nip 0<
;

header d0=
: d0= ( d -- f )
    or 0=
;

header d2/
: d2/ ( d -- d' )
    >r d# 1 rshift r@
    TOPBIT lshift
    or r> 2/
;

header fill
: fill ( c-addr u char -- ) ( 6.1.1540 )
    >r  bounds
    begin
        2dup xor
    while
        r@ over c! 1+
    repeat
    rdrop 2drop
;

header cmove>
: cmove> ( addr1 addr2 u -- )
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

meta lower-half target

\ ---- code page 1 and after
meta
    $2000 org
target

header bye : bye ( -- ) halt ;

include join.fs
meta t' pcompile, t' 'compile tvar! target  \ placement.fs puts a word before it
DISPLAY? [if]
include-system
\ what is given out at run time starts above the display's data
meta lofty [if]  thi @ byte-shift rshift 1+ -2 and  t' himem tvar!  [then] target
[else]
include folds.fs
[then]

meta
    t' main-task t' main-task tvar!     \ the first task, in a ring of one
    t' main-task t' up tvar!
    t' uart-emit t' main-task 6 + tvar!
    t' uart-emit t' 'console tvar!
    there  t' dp tvar!                  \ data carries on after the data,
    data-limit  t' dpl tvar!            \ as far as the bulk, or the headers;
    bulky [if]                          \ and the bulk after the bulk
        tbp @ t' bdp tvar!  hdr-base t' bdl tvar!
    [then]
    thp @ cell-bytes 1- + byte-shift rshift  t' hdp tvar!
    code-base 2* byte-shift rshift  t' hdl tvar!
    \ cpn: where the last page's code ends.  cp: where page 1's
    \ code ends.
    code-top 2/ code-base max
    dup code-base code-size + < 0=  plain @ 0= and [if]
        cr .( no code room is left for what is defined at run time) abort
    [then]
    t' cpn tvar!
    page-at cell+ @ 2/  t' cp tvar!
    t' pages pages-laid
target
