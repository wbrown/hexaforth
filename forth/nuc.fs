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

#4 constant HANDLER-CELL                    \ where a record has its handler

header sbase    : sbase    up @ c# 3 + ;    \ where its data stack starts
: handler       up @ HANDLER-CELL + ;       \ the catch that a throw goes to
header base     : base     up @ c# 5 + ;
header 'emit    : 'emit    up @ c# 6 + ;    \ where its output goes
header sourceid : sourceid up @ c# 7 + ;
header >in      : >in      up @ c# 8 + ;
: sourceC                  up @ c# 9 + ;    \ two cells: the text being interpreted

header 1-       : 1-        d# -1 + ;

\ A baseword is a macro of cross.fs: one or two instructions,
\ compiled inline.  To be typed at the prompt it needs a header
\ and a body here.  `header X :noname X ;` gives it both.
\ `header X callable X` does too, and also makes uses of X in the
\ kernel a call instead of inline, which keeps the kernel short.
\ A word defined here with a baseword's name (1-, rot, -) takes
\ the name from there on: a call, where the baseword was inline.
header 0=       callable 0=
header <>       callable <>
header >        callable >
header 0<       callable 0<
header u>       callable u>
header 2*       callable 2*
header 2dup     callable 2dup
header bounds   callable bounds
header -rot     callable -rot

\ negate is not callable: `-` is negate and +, so a call here
\ would make every subtraction a call that makes a call.
header negate   :noname     negate   ;

header cell+    : cell+     CELL + ;
header cells    : cells ;

header >c       : >c        (>c) ;          \ a cell's byte address
header c>       : c>        (c>) ;          \ the cell a byte is in

header 0>       : 0>        d# 0 > ;
header 0<>      : 0<>       d# 0 <> ;

header key?
: key? ( -- f ) \ is a character waiting on the console?
    h# e1 io@
;

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

\ hex8 prints eight digits: DOUBLE calls what follows with the
\ upper half, which then runs again with the lower.
: hex8 dup d# 16 rshift DOUBLE
: hex4 dup d# 8 rshift DOUBLE
: hex2 dup d# 4 rshift DOUBLE
header hex1 : hex1
    h# f and
;fallthru
: .digit ( u -- )
    dup d# 9 > d# 7 and +  [char] 0 + emit
;

header .x       : .x hex8 space ;
header .x2      : .x2 hex2 space ;

header true     : true  d# -1 ;
header false    : false d# 0 ;
header rot      : rot   >r swap r> swap ;
header tuck     :noname     tuck     ;
header 2drop    :noname     2drop    ;
header ?dup     : ?dup  dup if dup then ;

header +!       :noname     +!       ;
header or!      :noname     or!      ;
header and!     :noname     and!     ;
header xor!     :noname     xor!     ;
header *!       :noname     *!       ;
header not!     :noname     not!     ;
header 2swap    : 2swap rot >r rot r> ;
header 2over    : 2over >r >r 2dup r> r> 2swap ;

header min      : min   2dup < if drop exit then nip ;
header max      : max   2dup < if nip exit then drop ;

header hi32  :noname hi32 ;
header lo32  : lo32 ( dw -- w ) h# ffffffff and ;
header lo16  : lo16 ( dw -- s ) h# ffff and ;
header hi16  : hi16 ( dw -- s ) d# 16 rshift lo16 ;

\ a field of the cell a byte is in, from that byte up.  The byte's
\ place in its cell is worked out first and kept on the return
\ stack, so the address is used up by the fetch
header field@ : field@ ( c-addr mask -- x )
    >r
    dup OFFSET-MASK and >r
    (c>) @
;fallthru
\ a cell from one of its bytes up, and masked: ifield ends here too
: (field) ( w -- x ) ( R: mask place -- )
    r> d# 3 lshift rshift
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

\ c@ is field@ with a mask.  Not done: c@ written out in full,
\ which is a few instructions quicker and repeats most of field@.
header c@
: c@ ( c-addr -- c ) h# ff field@ ;

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

header type
: type
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
    'e' c, 'r' c, 'r' c, 'o' c, 'r' c, ':' c,

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

header dp    :noname dp ;
header cp    :noname cp ;
header state :noname state ;
header forth :noname forth ;

header -        : -         negate + ;
header abs      : abs       dup 0< if negate then ;
\ The sign stays where it is.  Whether it is set goes to the return
\ stack, and the 0 it was compared with is made the count of the
\ shift (doc/superoptimizations.md).
header 2/ : 2/ ( n -- n/2 )
    d# 0 2dup < >r                          ( n 0 ) ( R: is n negative? )
    1+ over swap rshift                     ( n n>>1 )
    TOPBIT r> swap lshift                   ( n n>>1 sign )
    or nip
;
header here     : here      dp @ (c>) ;
header align    : align     dp @ CELLB 1- + ALIGN-MASK and dp ! ;
header adepth : adepth ( -- n ) IO-SP io@ 1- ;            \ less the port number itself
header depth    : depth     adepth sbase @ - ;            \ from sbase

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
    over +                        ( augend sum)
    swap over swap                ( sum sum augend)
    u<                            ( sum carry )
    negate r> +                   ( sum . )
;

header dnegate
: dnegate
    >r invert r> invert
    d# 1. d+
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

header d2*
: d2*
    2dup d+
; 

\ One step for every bit of a cell.  DOUBLE calls the code that
\ follows it and then runs into it, so that code runs twice.
\ Not done: a step that adds only for a set bit, which is
\ quicker and longer.
: mulstep ( ud u -- ud u ) \ ud: the product, under what is left of the multiplier
W64? [if]
    DOUBLE
[then]
W32+? [if]
    DOUBLE
[then]
    DOUBLE DOUBLE DOUBLE DOUBLE
    >r
    dup 0< r@ and >r
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

header *        :noname     *        ;

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
: um/mod \ ( ud u1 -- u2 u3 ) ( 6.1.2370 ) 
    divstep
    drop swap 
; 

\ A dividend that fits in 16 bits takes 16 steps; printing a
\ number divides by this.  Not done: a second short path for a
\ dividend of 32 bits, which would be as long again.
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
        dup 0= if exit then
        1- 2dup + c@
        h# C0 and h# 80 <>
      until
;

header accept
: accept
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
header 3dup : 3dup  3rd 3rd 3rd ;

header cells-move
: cells-move ( from to n -- )
    >r over - r> swap >r bounds                     ( end from ) ( R: from here to there )
    begin
        2dup xor
    while
        dup @  over r@ + !
        1+
    repeat
;fallthru
\ the end of a loop over cells that kept something on the return
\ stack.  cells-fill and cmove end here too, by a jump: it is
\ never called, since what it drops is the caller's
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

\ The 0 is pushed once: the store that swaps keeps it for psh.
: pack> ( a width full -- ) \ to pack fields from the cell at a
    pfull !  pw !
    d# 0 tuck over !                        ( 0 a )
    pto !  psh !
;

\ When the cell is full, pto steps to the next and 0 goes there
\ and in psh: the address is kept from the step, and each 0 is
\ the 1 with itself (doc/superoptimizations.md).
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

header samename?
: samename? ( wp -- f ) \ has the header that name?
    CELL2 + same-cells? ;

\ The search goes from forth's header along the links.  A header
\ made here is linked before the rest, so of a name the newest is
\ found.  The image's are linked in the order of how often source
\ says each (cross.fs, headers-ordered): source says the kernel's
\ words most, and they are the oldest.
\ Not done, because each changes the header's form for everything
\ that reads one: a last header whose name is the key, which
\ would remove the test for the end; and the link pointing at
\ the name.  Either would make a pass of the loop shorter.
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
                rdrop nip nip nip           ( name )
                1- @                        ( x )
                \ the xt, less the flag: x shifted up onto the return
                \ stack, and down from there by the same 1
                d# 1 2dup lshift >r         ( x 1 )
                r@ swap rshift              ( x xt )
                swap rdrop                  ( xt x )
                0< invert d# 1 or
                exit
            then
        then
        drop r>
    repeat
    nip
;

\ Every code is looked up in kchar.  Not done: a test for a
\ letter before the look-up, which is quicker and longer.
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
        \ the cell is fetched onto the return stack, and its codes
        \ come from there: shifted up onto it and down by one count
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

header words : words
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
        0= if drop exit then
        >r 2swap base @ ud*
        r> s>d d+ 2swap
        d# 1 /string
    repeat
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

header execute
: execute
    >r
;

header source
: source
    sourceC 2@
;

: source! ( addr u -- )
    sourceC 2!
    d# 0 >in !
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
        over and
    WHILE
        d# 1 /string
    REPEAT
    rdrop ;

\ >in goes past what was parsed and the character that ended it
: parsed ( start end rest -- c-addr u )
    2dup d# 1 min + source drop - >in !
    drop over -
;

: unparsed ( -- c-addr u ) source >in @ /string ;

header parse-name
: parse-name ( "name" -- c-addr u )
    unparsed
    ['] isspace? xt-skip  over -rot
    ['] isnotspace? xt-skip
    parsed
;

header isnotdelim
: isnotdelim
    delim @ <>
;

header parse
: parse ( "ccc<char" -- c-addr u )
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
    .digit
;

header .
: . ( n -- )
    s>d if [char] - emit negate then        \ s>d is dup 0<, and a call
    (.) space
;

\ catch pushes the data stack's depth, the previous handler and
\ rO on the return stack, and keeps the return stack's depth in
\ `handler`.  throw cuts the return stack back to that depth and
\ restores the three, leaving the thrown number where the xt
\ was.  With no catch, throw prints the number and restarts the
\ prompt.

\ the prompt as it starts, here and when the ROM does (afresh):
\ interpreting, from the console, and no catch round it
: prompting ( -- )
    d# 0 state !
    d# 0 sourceid !
    d# 0 sbase !        \ only the first task has no catch round it
;

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
        dup d# -1 = if                  \ ABORT prints nothing
            drop
        else
            "error:" >c d# 6 type
            space
            .
        then
        prompting
        'console @ 'emit !
        d# 0 IO-RSP io!
        d# 0 IO-SP io!
        d# 1 execute        \ word 1 of the boot vector: quit
    then
;

\ a must be a flag, -1 or 0: it is used as a mask
header -throw : -throw ( a b -- ) \ if a is true, throw -b
    negate and throw
;

header i@
: i@ ( ia -- w ) (i@) ;
header i!
: i! ( w ia -- ) (i!) ;


: code,
    cp @ 1+ h# fff and if           \ the last slot of a page stays empty
        cp @ i!
        d# 1 cp +!
        exit
    then
    d# -8 throw
;

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

\ the target field of a branch or call: an address on page 0 as
\ it is, or the offset in the branch's own page with bit 12 set
: near ( ia -- field )
    dup d# 12 rshift if h# fff and h# 1000 or then
;

: landing ( -- field ) \ of the code about to be compiled
    cp @ dup syncpt !       \ no instruction is joined across it
    near
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
    \ The link: forth's address goes to the return stack, and what
    \ forth holds goes from there to the header, a cell to a cell.
    forth hdp @ swap >r dup                 ( n h h ) ( R: forth )
    lastword !                              ( n h )
    meta R->IN [IN] ->[T] d+1 alu target    ( n h h )       \ r@ @ over ! dup
    cp @ tuck rdrop                         ( n h xt h xt )
    thisxt !  1+ !                          ( n h )         \ xt
    CELL2 +  2dup + hdp !                   ( n name )
    skey swap rot cells-move
    sync
;

header immediate
: timmediate
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
    cp @
    dup thisxt !
    d# 0 lastword !
    sync
    t]
;

header inline: : inline: \ copy the instruction after the call, and skip it
    r>
    dup i@ code,
    1+ >r
;
inline-operand

\ the same, and it returns to the caller of the word it is in,
\ which so needs no exit and ends with ;fallthru
: inline; ( -- )
    r> i@ code,
;
inline-operand

: prev
    cp @ 1-
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
    h# 8000 code,               \ the number was 0: push 0
;

header-imm 2literal
: 2literal
    swap DOUBLE
\ A negative number is its complement and invert: any on 64
\ bits; on 32 only one whose complement is a single literal; none
\ on 16, where two literals make any cell.
header-imm literal
: tliteral
W64? [if]
    s>d if
        invert tliteral
        inline; invert
    then
[then]
W32? [if]
    dup invert d# 12 rshift 0= if
        invert tliteral
        inline; invert
    then
[then]
W64? [if]
    dup d# 36 rshift ?dup if \ wider than 36 bits: high part, shift, add the rest
        tliteral
        d# 36 tliteral
        inline: lshift
        \ the low 36 bits: shifted up onto the return stack, and down
        \ from there by the same 28
        d# 28 2dup lshift >r
        nip r> swap rshift
        h# c000 segs,
        exit
    then
[then]
    h# 8000 segs,
;

create doesexit 0 ,     \ where the last created word's exit is

: exit, ( -- ) inline; exit ;fallthru

\ the created word's exit becomes a jump to the code after does>
header does>
: (does>)
    r>                                  ( xt of the code after does> )
    dup d# 12 rshift if
        dup doesexit @ xor d# 12 rshift if
            \ on another page: a jump by xt, which is longer, so
            \ the created word is the last code compiled
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
: texit
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
\ compiled.  join.fs gives `exit` and `;` newer headers.
header-imm ;
:noname
    texit
    attach
    t[
;

header-imm if
: tif
    cp @
    h# 2000 code,
;

header-imm ahead
: tahead
    cp @
    h# 0000 code,
;

header-imm then
: tthen
    dup i@
    landing or
    swap i!
;

header-imm begin
: tbegin
    landing
;

header-imm again
: tagain
    code,
;

header-imm until
: tuntil
    h# 2000 or code,
;

header-imm else
:noname
    tahead
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

create 'compile 0 ,     \ what compile, does: pcompile, (join.fs)

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

header compile,
: compile, ( xt -- ) 'compile @ execute ;

header-imm recurse
:noname
    thisxt @ compile,
;

\ DO...LOOP: the top of the return stack counts up from
\ start - limit to 0.  rO holds the limit, and I is that count
\ plus rO.  The loop before's rO is kept on the return stack.

\ rO's address is pushed once: the limit before is fetched through
\ it over the top of the return stack, which is copied first as
\ where this returns to, and the limit is stored through it
\ (doc/superoptimizations.md).
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
:noname
    leaves @ d# 0 leaves !
    ['] (do) compile,
    tbegin
    inline; >r
;fallthru

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

header-imm loop
:noname
    inline: r>
    ['] (loopnext) compile,
    tif
    swap tagain
    tthen
    resolveleaves
;

\ finished when the count has gone round, up or down
: (+loopnext) ( inc R -- R finished )
    dup >r over +                           ( inc new ) ( R: old )
    swap 0<                                 ( new f )       \ is the step negative?
    over r> u<                              ( new f f2 )    \ is new below old?
    xor
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
\ a LEAVE is a jump.  Until the loop ends its target is the page
\ offset of the LEAVE before, with h# 4000 set, or 0 for none.
: leaf,
    cp @
    leaves @ dup if h# fff and h# 4000 or then code,
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
    inline; >r
;fallthru

\ Where this returns to is put back before the add, which drops it
\ and the count under what it makes, and has the return in it.
\ The add is said by its fields, and rO's address pushed for it:
\ of `rO @ + nip` cross.fs makes the add and then swap >r, whose
\ return is an instruction of its own.
header i
: i
    r>
    r@ over >r                              ( ra count ) ( R: count ra )
    rO  meta [T]->IN IN+N ->T d-2 alu target    \ @ + nip
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
\ >r r> r@ rdrop cannot be called: they are immediate words,
\ defined in join.fs.

header constant
: tconstant ( x "name" -- )
    mkheader attach
    tliteral
    cp @ doesexit !
    exit,
;

header create
: tcreate
    align here tconstant
;

header variable
: variable
    tcreate d# 0 t,
;


: isvoid ( caddr u -- ) \ any char remains, throw -13
    nip 0<> d# 13 -throw
;

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

\ Not done: a table of jumps indexed by sfind's flag plus the
\ state, which does the same and is longer.
header interpret1
: interpret1 ( c-addr u -- ) \ a word of the source: done, or compiled
    sfind ?dup if
        0< state @ and if compile, exit then
        execute exit
    then
;fallthru
\ What says it has a number (d#, literals.fs) comes here with it:
\ a number is no word, and to find that out sfind looks at every
\ header there is.
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
    \ rO as it was is dropped in the fetch of up; the handler is put
    \ back and the depth dropped in one store; and the 0 is up's
    \ address with itself, in the instruction that returns.
    up dup rdrop @                          ( up task )
    HANDLER-CELL +  r> rdrop over !         ( up handler )
    drop dup xor                            ( 0 )
;

: idle ;
: (task) ( -- ) \ its word again and again; idle once that throws
    begin
        up @ c# 2 + @ catch
        ?dup if [char] ? emit .  ['] idle up @ c# 2 + ! then
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
    \ cells 1, 3 and 2 of the record, by one address stepped along
    r@ CELL + tuck !                ( xt dbase a )      \ the depth
    tuck d# 2 + !                   ( xt a )    \ where its data stack starts
    1+ !                                        \ the word it runs
    d# 0 r@ c# 4 + d# 7 cells-fill
    d# 10 r@ c# 5 + !
    \ where its output goes, a cell to a cell through the return
    \ stack; and then it is next after this one
    'emit r@ over >r dup            ( 'emit task task ) ( R: task 'emit )
    c# 6 + r@ @ swap !              ( 'emit task )
    up @ rdrop @ swap !             ( 'emit ) ( R: task )
    up @ nip r> swap !
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
    then
;

header evaluate
: tevaluate
    blk @ >r  d# 0 blk !
    >in @ >r  source >r >r
    source-id >r d# -1 sourceid !
    source!
    interpret
    r> sourceid !
    r> r> source!  r> >in !
    r> blk !
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

: afresh ( -- ) \ what the console and throw need
    main-task dup up !  dup !               \ the first task, in a ring of one
    ['] uart-emit dup 'emit !  'console !  decimal
    d# 0 handler !  d# 0 blk !
    prompting
;

: boot-buf ( -- a ) MEM-END BLKCELLS - ;    \ block 0, in the last block of memory
: entry ( i -- a ) ENTRY-BYTES BYTESHIFT rshift *  boot-buf + ;
: ifield ( a offset -- u ) \ two bytes that far into the cells from a
    h# ffff >r
    dup OFFSET-MASK and >r
    BYTESHIFT rshift + @
    (field)                                 \ a jump: it takes the two off the return stack
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
\ what is stored goes to the return stack, and the store takes it
\ from there as it swaps the address up
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

\ read is on the stack: the image is read over the vectors
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

\ Every entry is taken from block 0 before a block is read: the
\ image may be read over it.
: binaries ( read i -- read )
    dup 0= if drop exit then
    dup entry  dup ENTRY-TYPE ifield TYPE-BINARY = if   ( read i e )
        dup ENTRY-WHERE ifield  BLKCELLS *
        over ENTRY-FIRST ifield  rot ENTRY-BLOCKS ifield    ( read i a first n )
        >r >r >r  1- binaries  r> r> r>  blocks-to
        exit
    then
    drop 1- binaries
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
\ say" text" emits it from numbers in the code: the ROM has no data
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
    quit
;

\ ---- the nucleus's words that are not in its ROM: nothing in
\ the ROM runs one before the image is read (kernel_test, typed)
meta rom-ends upper-half target

header dabs
: dabs ( d -- ud )
    dup 0< if dnegate then
;

header 2rot
: 2rot
    >r >r 2swap r> r> 2swap
;

header d>s
: d>s   drop ;

header d=
: d= ( a b c d -- f )
    >r                      \ a b c
    rot =                   \ b a=c
    swap r> =               \ a=c b=d
    and
;

header d<
: d<            \ ( al ah bl bh -- flag )
    rot         \ al bl bh ah
    2dup xor if > nip nip exit then
    2drop u<
;

header du<
: du<           \ ( al ah bl bh -- flag )
    rot         \ al bl bh ah
    2dup xor if u> nip nip exit then
    2drop u<
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

header d2/
: d2/
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

meta lower-half target

\ ---- code page 1 and after
meta
    $2000 org
target

header bye : bye halt ;

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
    \ No code follows a table on its page (tabled, cross.fs).
    \ cpn: where the last page's code ends, or the page after if a
    \ table is there.  cp: where page 1's code ends, or cpn if
    \ folds.fs's table is on page 1.
    code-top 2/ code-base max
    dup $1000 / tabled @ = [if] $fff + $fff invert and [then]
    dup code-base code-size + < 0=  plain @ 0= and [if]
        cr .( no code room is left for what is defined at run time) abort
    [then]
    dup t' cpn tvar!
    tabled @ 1 = [if] [else] drop  page-at cell+ @ 2/ [then]
    t' cp tvar!
    t' pages pages-laid
target
