$00f1 constant IO-OUT
$00e0 constant IO-IN
$00d0 constant IO-SP    \ data stack depth
$00d1 constant IO-RSP   \ return stack depth

meta
    4 org 
target

\ ---- tasks
\
\ A task is a thread of Forth with its own part of each stack.  The stacks
\ are shared by depth (MODEL.md): a task owns a range of depths on each,
\ and changing task is saving two depths and setting two others.  Tasks
\ take turns: one runs until it says pause.
\
\ A task is a record of thirteen cells.  The first four are the next task
\ round the ring, the two depths it stopped at, and the word it runs.  The
\ rest are the variables each task has its own of, which are words that
\ find the cell in the record of the task that is running, `up`.

create main-task 0 , 0 , 0 , 0 ,   0 , 0 , $a , 0 ,   0 , 0 , 0 , 0 ,   0 ,
header up create up 0 ,

header sbase    : sbase    up @ c# 4 + ;    \ where its data stack starts
: handler                  up @ c# 5 + ;    \ the catch that a throw goes to
header base     : base     up @ c# 6 + ;
header 'emit    : 'emit    up @ c# 7 + ;    \ where its output goes
header sourceid : sourceid up @ c# 8 + ;
header >in      : >in      up @ c# 9 + ;
: sourceC                  up @ c# 10 + ;   \ two cells: the text being interpreted

header 1-       : 1-        d# -1 + ;

\ A baseword is a name for an instruction or two, which the cross-compiler
\ lays down wherever the name is written.  The nucleus has to have each in
\ its dictionary as well, and it is given one in either of two ways.
\
\ `:noname` gives it a header and a body and leaves the name what it was:
\ its instructions go in line wherever it is written, where the peephole
\ can join them to what is beside them.  A baseword of one instruction is
\ always given so, a call being no shorter than it is.
\
\ `callable` (cross.fs) makes it a word of the kernel's too, with the
\ baseword's own instructions for its body.  In the kernel, which has to
\ fit its ROM, a use is then one call where it was two instructions,
\ unless the peephole joins them to what is beside them; on every other
\ page it is in line, as before.  These are callable: with them calls in
\ the kernel, drawing and the dither take no longer than they did
\ (bench/bench.sh).
header 0=       callable 0=
header <>       callable <>
header >        callable >
header 0<       callable 0<
header u>       callable u>
header 2*       callable 2*
header 2dup     callable 2dup
header bounds   callable bounds
header -rot     callable -rot

\ negate is two instructions as well, and it is NOT callable.  It stays in
\ line everywhere, the kernel included, because `-` is made of it, and the
\ rest of the system subtracts all the time.  A word with a call in it
\ takes longer, and it is no longer small enough to be laid in line where
\ another page uses it, so it is called there as well: with negate a call,
\ a subtraction is a call that makes a call, where it was three
\ instructions in line, and drawing takes longer (bench/bench.sh draw).
\ Of all of these it is the one that was found to cost drawing anything.
header negate   :noname     negate   ;

\ CELL, CELLB, BYTESHIFT, TOPBIT, HALFBITS and the W64? W32? W32+? tests
\ come from cross.fs, which takes the machine's sizes from basewords.fs.
\
\ An address is the number of a cell, and @ and ! take one.  cell+ is the
\ next cell and cells turns a count of cells into a difference of addresses.
header cell+    : cell+     CELL + ;
header cells    : cells ;

\ Text and code are reached by the address of a byte, which is not an
\ address @ can take: it is the cell's address times the bytes in a cell,
\ plus the byte's place in the cell.  >c gives the byte address of the
\ start of a cell, and c> the cell a byte is in.
header >c       : >c        (>c) ;
header c>       : c>        (c>) ;

header 0>       : 0>        d# 0 > ;
header 0<>      : 0<>       d# 0 <> ;

header key?
: key? ( -- f ) \ is a character waiting on the console?
    h# e1 io@
;

\ Output goes through a vector, so it can be sent to a window.

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
;fallthru
: .digit ( u -- ) \ a digit, sent out as its character: the letters are seven on from the figures
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

header min      : min   2dup< if drop exit then nip ;
header max      : max   2dup< if nip exit then drop ;

header hi32  :noname hi32 ;
header lo32  : lo32 ( dw -- w ) h# ffffffff and ;
header lo16  : lo16 ( dw -- s ) h# ffff and ;
header hi16  : hi16 ( dw -- s ) d# 16 rshift lo16 ;

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

\ c@ is field@ with its mask written in, and not a call of it: every
\ character that is read is read with this.
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
create "error:"
    'e' c, 'r' c, 'r' c, 'o' c, 'r' c, ':' c,

\ forth, the newest word of the dictionary, is the first cell of the data in
\ every image, and cross.fs says so: it is not laid down here.
create labels   0 ,         \ names of definitions that have no header, while they are read in (see.fs)
create cp       0 ,         \ Code pointer, grows up
create dp       0 ,         \ Data pointer, grows up
create hdp      0 ,         \ where the next dictionary header goes
create lastword 0 ,
create thisxt   0 ,
create syncpt   0 ,
create state    0 ,
create delim    0 ,
header blk create blk 0 ,   \ the block that is being read as source, or nothing
create rO       0 ,
create leaves   0 ,
create tib $80 allot

header dp    :noname dp ;
header cp    :noname cp ;
header state :noname state ;
header forth :noname forth ;
header labels :noname labels ;

header -        : -         negate + ;
header abs      : abs       dup 0< if negate then ;
header 2/       : 2/        dup 0< TOPBIT lshift  swap d# 1 rshift  or ;    \ the sign stays where it is
\ The data pointer counts bytes, so that bytes can be laid down one at a
\ time.  here is the address of the cell it is in, which is the next free
\ cell once the pointer has been aligned.
header here     : here      dp @ (c>) ;
header align    : align     dp @ CELLB 1- + ALIGN-MASK and dp ! ;
\ The data stack depth.  adepth is the true depth; depth is what a program
\ sees, measured from sbase, where the task's part of the stack starts.
header adepth : adepth ( -- n ) IO-SP io@ 1- ;            \ less the port number itself
header depth    : depth     adepth sbase @ - ;

\ pause gives the other tasks a turn.  It comes back when they have each
\ had one.  The loop offset is the one thing kept outside the record,
\ since loops use it all the time.
header pause
: pause ( -- )
    rO @ up @ c# 12 + !
    adepth up @ CELL + !
    IO-RSP io@ up @ CELL2 + !
    up @ @ up !
    up @ c# 12 + @ rO !
    up @ CELL2 + @ IO-RSP io!
    up @ CELL + @ IO-SP io!
;

\ Waiting for a key gives the other tasks their turns.
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

\ The high cells say, unless they are the same: then the low ones do.
header d<
: d<            \ ( al ah bl bh -- flag )
    rot         \ al bl bh ah
    2dupxor if > nip nip exit then
    2drop u<
;

header du<
: du<           \ ( al ah bl bh -- flag )
    rot         \ al bl bh ah
    2dupxor if u> nip nip exit then
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
    dup d# 16 rshift if
W64? [if]
        dup d# 32 rshift if
            d# 0 r> divstep
        else
            d# 32 lshift d# 0 r> div32
        then
[else]
        d# 0 r> divstep
[then]
    else
        TOP16 lshift d# 0 r> div16
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
        ckey   ( addr len key R: maxlen )
        
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
\ immediate word, then the name, five bits a character and as many to a
\ cell as a cell holds, with the top bit of its last cell set (cross.fs
\ says what the codes are).  To look a name up it is packed the same way
\ into skey, and then compared with each header a cell at a time; the first
\ cell holds the first characters and whether they are all of it, which
\ settles nearly every entry with one aligned fetch.

\ Cells from one place to another.  There is no count kept: the place the
\ cells come from goes up to where they end, and where each goes is that
\ far on from where it came.
header cells-move
: cells-move ( from to n -- )
    >r over - r> swap >r bounds                     ( end from ) ( R: from here to there )
    begin
        2dupxor
    while
        dup @  over r@ + !
        1+
    repeat
    2drop rdrop
;

\ The same thing into every cell of a run.  It is kept on the return stack,
\ where it is an input of the ALU: a turn is the store, the step, and the
\ test.
header cells-fill
: cells-fill ( x a n -- )
    bounds rot >r                                   ( end a ) ( R: x )
    begin
        2dupxor
    while
        r@ over ! 1+
    repeat
    2drop rdrop
;

\ Fields are packed into cells in two places, and it is one thing: codes
\ of five bits into the key when a name is looked up, and characters of
\ eight into text when a name is shown.  A field goes into the cell in
\ hand where the last one ended, with or!; when the cell is full the next
\ is in hand, and is made empty.
create pto 0 ,              \ the cell in hand,
create psh 0 ,              \ how far along it the next field goes,
create pw 0 ,               \ how many bits a field is,
create pfull 0 ,            \ and how many bits of a cell fields have

: pack> ( a width full -- ) \ to pack fields from the cell at a
    pfull !  pw !
    d# 0 over !  pto !  d# 0 psh !
;

: pack, ( x -- ) \ the next field
    psh @ lshift  pto @ or!
    psh @ pw @ +
    dup pfull @ = if                        \ the cell is full: on to the next, which is empty
        drop  d# 1 pto +!  d# 0 pto @ !  d# 0
    then
    psh !
;

create skey #96 allot       \ room for 63 characters that are two codes each, and a cell
create kcode codes-of-chars,    \ what each of the 128 characters is in a name: its code, or 32 or 64
                                \ and where it is among the rest; a character each, and the same
                                \ for a capital as for its letter

: kcode@ ( c -- what ) h# 7f and kcode >c + c@ ;

: >key ( c-addr u -- n ) \ pack a name into skey; n is its size in cells
    d# 63 min
    skey d# 5 NAME-BITS pack>
    bounds
    begin
        2dupxor
    while
        dup c@ kcode@                       ( end a what )
        dup d# 31 > if                      \ one of the rest: which lot of them, and then which
            dup d# 5 rshift d# 29 + pack,  d# 31 and
        then
        pack,
        1+
    repeat
    2drop
    psh @ 0=  pto @ skey <> and  pto +!     \ the last code filled its cell: that cell is the last
    d# 1 TOPBIT lshift  pto @ or!
    pto @ skey - 1+
;

: samename? ( wp -- f ) \ are the name's cells the key's, as far as the key's last?
    CELL2 + skey                            ( name key )
    begin
        over @ over @ =
    while
        dup @ 0< if 2drop true exit then    \ that was the last: its top bit is set
        1+ swap 1+ swap
    repeat
    2drop false
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
                dup 2* u2/                          \ the xt, less the flag
                swap 0< invert d# 1 or              \ which is 1 if it was set, and -1 if not
                exit
            then
        then
        @
    repeat
    rdrop
;

\ A name as text.  Whoever shows a name (words, the decompiler) has its
\ characters put in a buffer, and that is the text until the next name is
\ asked for.  The name's cells may be anywhere in memory: they are fetched
\ as cells.
\
\ The characters are packed into the buffer's cells as the codes were into
\ the key's (pack,).  A cell of the name is gone through for as long as
\ there is anything left of it: no code is nothing, so what is left after
\ its last is the padding.  Nine characters in ten are letters, and a
\ letter is its code and 96.
create nbuf #64 allot           \ the characters, packed into cells as text is
create kpg 0 ,                  \ which lot of the rest the next code is one of: 32 or 64, or nothing
create kchar chars-of-codes,    \ what each code is: the plain ones, and then the two lots of the rest

: code> ( code -- ) \ a code that is not simply a letter: a character, or which lot the next one is of
    kpg @ ?dup 0= if
        dup d# 29 > if  d# 29 - d# 5 lshift kpg !  exit then
        d# 0                                \ a plain one: it is not one of a lot
    then
    +  d# 0 kpg !
    kchar >c + c@ pack,
;

header name>
: name> ( a -- c-addr u ) \ the name whose cells start at a
    d# 0 kpg !  nbuf d# 8 CELLBITS pack>
    begin
        dup @ dup >r                                ( a cell ) ( R: the cell )
        NAME-SPARE lshift NAME-SPARE rshift         \ its codes, and not the bits over
        begin
            dup
        while
            dup d# 31 and
            dup d# 27 <  kpg @ 0= and if d# 96 + pack, else code> then
            d# 5 rshift
        repeat
        drop
        1+
        r> 0<                                       \ the top bit says this was its last cell
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

\ What a character is as a digit, and whether it is one in this base.  The
\ table that says what a character is in a name says it: a letter, capital
\ or not, is its place in the alphabet, and the figures are 33 to 42.
: digit? ( c -- u f )
    kcode@
    dup d# 27 < if
        d# 9 +                              \ a is ten
    else
        d# 33 -  dup d# 9 u> or             \ a figure, or what no base has
    then
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
  rdrop 2drop
;

header cmove
: cmove ( addr1 addr2 u -- ) \ a character at a time; what is whole cells is moved with cells-move
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

: source! ( addr u -- ) \ set the source, to be read from its start
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
        overand
    WHILE
        d# 1 /string
    REPEAT
    rdrop ;

\ What both ways of parsing end with: >in is put past what was parsed and
\ the character that ended it, and what was parsed is where it started
\ and how long it is.
: parsed ( start end rest -- c-addr u )
    2dup d# 1 min + source drop - >in !
    drop over -
;

: unparsed ( -- c-addr u ) source >in @ /string ;       \ the rest of the source

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

\ Comments.  What is between ( and the next ) is not read: in a block that
\ may be lines on, and what was typed ends where it does.  Nor is what is
\ after \ on its line.  A line is what was typed, or a line of the block
\ that is being read (blk), which has nothing to end it: there >in is two
\ past the \, which are the \ and the blank after it, and the line ends at
\ the next whole number of lines after the \ itself.
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
    dup 0< if [char] - emit negate then
    (.) space
;

\ catch saves the data stack depth, the previous handler and the DO loop
\ offset on the return stack, and records the return stack depth at that
\ point in `handler`.  throw cuts the return stack back to that depth, which
\ leaves those cells on top, then restores the data stack depth with the
\ xt's cell replaced by the throw code.


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
        "error:" >c d# 6 type
        space
        .
        d# 0 state !
        d# 0 sourceid !
        ['] uart-emit 'emit !
        d# 0 sbase !        \ only the first task has no catch round it
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

\ Code is addressed by instruction: the code pointer, an execution token and
\ a branch origin are all the number of an instruction.  i@ and i! fetch and
\ store one.  Run-time code goes in CODE-SIZE instructions from CODE-BASE,
\ both from cross.fs.

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
    d# -8 throw                     \ ran off the end of the page
;

: newpage? ( -- ) \ before a definition: move on if under 1KB is left here
    cp @ h# fff and h# e00 u> if
        cp @ h# fff or 1+ cp !
    then
    cp @ CODE-BASE - CODE-SIZE u< 0= d# 8 -throw
;

: target ( -- field ) \ branch target field for the code about to be compiled
    cp @ dup syncpt !       \ a branch lands here: nothing may be joined across it
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

\ Headers are kept apart from data, above it, so that variables stay
\ within reach of the instructions that fetch and store one.  A header is
\ laid down with the ordinary words, with the two pointers changed over.
: hswap ( -- ) dp @ hdp @ dp ! hdp ! ;

: mkheader
    newpage?
    dp @ DATA-LIMIT u> d# 8 -throw          \ the data has run into what is after it
    hswap
    align
    here lastword !
    forth @ t,                              \ link
    cp @ dup thisxt ! t,                    \ xt, on any page
    parse-name >key >r                      \ name: the cells of the key
    skey here r@ cells-move  r> (>c) tallot
    hswap
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
    calign cp @
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

: prev
    cp @ 1-
;

: (loopdone)  ( 0 -- )
    drop
    r> r> rO ! >r
;

\ Literals: 12-bit immediates with a shift selector (0/12/24 bits) and an
\ add flag.  The first non-zero segment pushes, later ones add.

: segs, ( u36 addflag -- ) \ addflag: h# 4000 to add to T, 0 to push
    >r d# 0
    begin ( u shift )
        over h# fff and ?dup if
            over d# 12 lshift or
            r> or h# 8000 or code,
            h# 4000 >r
        then
        1+ swap d# 12 rshift swap
        over
    while
    repeat
    2drop
    r> if exit then
    h# 8000 code,               \ nothing emitted: push zero
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
    dup d# 36 rshift ?dup if \ wider than 36 bits: high part, shift, add the rest
        tliteral
        d# 36 tliteral
        inline: lshift
        d# 28 lshift d# 28 rshift
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
: exit, ( -- ) inline: exit ;            \ compile an exit

header does>
: (does>)
    r>                                  ( xt of the code after does> )
    dup d# 12 rshift if                 \ not in the kernel, which a jump reaches as it is
        dup doesexit @ xor d# 12 rshift if
            \ On another page than the created word: replace the exit
            \ with a jump by xt.  That is longer, so the created word has
            \ to be the last code compiled.
            doesexit @ 1+ cp @ <> d# 22 -throw
            doesexit @ cp !
            tliteral
            inline: >r
            exit,  exit
        then
        h# fff and h# 1000 or           \ on the created word's page
    then
    doesexit @ i!
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
    cp @ thisxt @ <> if                     \ a call just before it is made a jump,
        prev dup i@ dup jumpable if
            h# 4000 xor swap i!
        else
            2drop
        then
    then
    exit,                                   \ and there is an exit all the same: a branch may land on it
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
: tahead
    cp @
    h# 0000 code,
;

header-imm then
: tthen
    dup i@
    target or
    swap i!
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

: isreturn ( opcode -- f ) \ ALU instruction with RET set
    h# f000 and
    h# 7000 =
;

: isliteral ( xt -- f) \ a literal followed by exit
    dup i@ h# 8000 and 0<>
    swap 1+ i@ h# 7403 = and
;

\ @ or ! straight after a literal that is the address of one of the 4,096
\ cells from VARS: the machine has one instruction for the two together.
\ The literal is one instruction, or a push and an add.  Nothing is joined
\ across a place a branch lands on.
: lval ( insn -- n ) dup h# fff and swap d# 12 rshift d# 3 and d# 12 * lshift ;

\ Is it a literal of this kind: h# 8000 for a push, h# c000 for an add?
: lit? ( insn kind -- f ) over h# c000 and =  swap h# 3000 and h# 3000 <>  and ;

: var, ( insn -- )
    >r
    prev i@ dup h# 8000 lit? if
        lval d# 1
    else
        dup h# c000 lit?  prev 1- i@ h# 8000 lit?  and if
            lval prev 1- i@ lval + d# 2
        else
            drop r> code, exit
        then
    then                                        ( address instructions )
    cp @ over - syncpt @ <
    rot VARS - dup d# 4095 u> rot or if         ( instructions place )
        2drop r> code, exit
    then
    swap negate cp +!
    r> h# 6308 = if h# f000 else h# b000 then or code,
;

\ compile, goes through a vector: the build sets it to pcompile, in join.fs,
\ which joins instructions as they are compiled and comes here for the rest.
create 'compile 0 ,

\ Where its rules are, and how many: they are instructions laid down after
\ its code, and the tools that read an image must not take them for code.
\ The third cell is where what it is told of numbers is (folds.fs), and
\ the fourth where the count of each rule's uses is.
header join-rules create join-rules 0 , 0 , 0 , 0 ,

: (compile,) ( xt -- )
    dup i@ isreturn if
        i@ h# effc and      \ single-instruction word: inline it without RET r-1
        dup h# 6800 = over h# 6308 = or if var, exit then
        code, exit
    then
    dup isliteral if
        i@ code, exit
    then
    dup i@ h# 8000 and 0<>  over 1+ isliteral  and if
        dup i@ code,  1+ i@ code,       \ a literal of two instructions: an address
        exit                            \ too far up for one, which @ or ! may join
    then
    dup d# 12 rshift if
        dup cp @ xor d# 12 rshift if    \ another page: go by xt
            tliteral
            ['] execute h# 4000 or code, exit
        then
        h# fff and h# 5000 or code, exit    \ this page
    then
    h# 4000 or code,                    \ kernel: reachable from anywhere
;

header compile,
: compile, ( xt -- ) 'compile @ execute ;

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
        dup i@ dup if      \ link: page offset of the previous leave, marked
            h# fff and cp @ h# fff invert and or
        then
        swap                ( next leafptr )
        target swap i!
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
\ Words the image's fused instructions are made of, which the decompiler
\ shows by their headers (fused.fs).
header dup@     :noname     dup@     ;
header over@    :noname     over@    ;
header-imm rdrop  :noname   inline: rdrop ;
header-imm @r     :noname   inline: @r noop ;
header-imm swap>r :noname   inline: swap>r ;
header-imm swapr> :noname   inline: swapr> ;

: jumptable ( u -- ) \ add u to the return address
    r> + >r ;

\   (doubleAlso) ( c-addr u -- x 1 | x x 2 )
\               If the string is legal, give a single or double cell number
\               and size of the number.

\ A created word is the literal address of its data field, then exit.

\ A constant is a number and an exit, and a created word is the constant
\ that its data's address is.  Where the exit is is kept for does>, which
\ makes a jump of a created word's.

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

header interpret1
: interpret1 ( c-addr u -- ) \ a word of the source: it is done, or compiled
    sfind
    state @ +
    1+ dispatch             \ table index in words, one jmp each
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
    rdrop
    r> handler !
    rdrop
    d# 0
;

\ What a task does: its word, again and again, with a turn for the others
\ in between.  If the word throws, the task says so and from then on does
\ nothing but take its turn.
: idle ;
: (task) ( -- )
    begin
        up @ c# 3 + @ catch
        ?dup if [char] ? emit .  ['] idle up @ c# 3 + ! then
        pause
    again
;

\ spawn starts a task.  It is given the word to run, the depths at which
\ its parts of the two stacks start, and thirteen cells for its record.
\ Its return stack is given one cell, the task loop, for its first turn to
\ return to.  Its output goes where the output of the task that started
\ it goes.  Nothing stops a task that runs past its part of a stack.
header spawn
: spawn ( xt dbase rbase task -- )
    >r
    IO-RSP io@ swap                 ( xt dbase mine rbase )
    dup IO-RSP io!  ['] (task) >r   \ on its return stack, not this one
    1+ swap IO-RSP io!              ( xt dbase rdepth )
    r@ CELL2 + !
    dup r@ CELL + !  r@ c# 4 + !
    r@ c# 3 + !
    d# 0 r@ c# 5 + d# 8 cells-fill          \ nothing in the rest of it, but for
    d# 10 r@ c# 6 + !                       \ its base
    'emit @ r@ c# 7 + !                     \ and where its output goes
    up @ @ r@ !  r> up @ !          \ next after this one
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
    blk @ >r  d# 0 blk !                    \ a string is not a block, whoever has it read
    >in @ >r  source >r >r                  \ what was being read, and how far it had been
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

\ Everything below is compiled into code page 1 and after.  The kernel
\ never calls it directly: these words are reached by name from the
\ prompt, or through an execution token or a vector.
meta
    $2000 org
target

\ The nucleus's peephole (join.fs) is in every size of machine.  A machine
\ with a display, whatever its cell, has all of system.fs, the desktop,
\ with the peephole after everything else.  One with no display has the
\ peephole alone.
DISPLAY? [if]
include system.fs
[else]
include join.fs
[then]

meta
    t' pcompile, t' 'compile tvar!
    t' main-task t' main-task tvar!     \ the first task, in a ring of one
    t' main-task t' up tvar!
    there  t' dp tvar!                  \ data carries on after the data,
    thp @  t' hdp tvar!                 \ and headers after the headers
    tcp @ 2/ code-base max $fff + $fff invert and
    t' cp tvar!             \ run-time definitions start on the next free code page
target
