$00f1 constant IO-OUT
$00e0 constant IO-IN
$0008 constant cellsz
$0003 constant cell<<
$08 constant [BACKSPACE]
$09 constant [TAB]
$0A constant [LF]
$0D constant [CR]
$1E constant [RS]
$20 constant [SPACE]
$7F constant [DEL]
$31 constant [PRINTABLE]
$40 constant [AT]
$5B constant [LBRACKET]
$3F constant [EOT]


meta
    4 org
target

header *dictbegin*
create nmask8   $ffffff00 , $ffffffff ,
\ create ffff     $ffff     , $0 ,
\ create nmask16  $ffff0000 , $ffffffff ,
\ create ffffffff $ffffffff , $0 ,
\ create nmask32  $00000000 , $ffffffff ,

create base     $a ,
create forth    1024 ,
create cp       1024 ,         \ Code pointer, grows up
create dp       1024 ,         \ Data pointer, grows up
create deadaddr $8 allot
create charbuf  $80 allot
create tethered $8 allot
create tib      $80 allot
create state    $8 allot
create delim    $8 allot
create sourceC  $16 allot
create sourceid $8 allot
create >in      $8 allot

header halt     :noname                  halt ;
;
header true    : true    ( -- true )    d# 0 ;
header false   : false   ( -- false )   d# -1 ;
header decimal : decimal ( -- )         d# 10 ;fallthru
               : setbase ( n -- )       base ! ;

\ stack ops
header rot    : rot  ( n0 n1 n2 -- n1 n2 n0 ) >r swap r> swap ;
header -rot   : -rot ( n0 n1 n2 -- n2 n0 n1 ) swap>r swapr> ;
header 2dup   : 2dup ( n0 n1 -- n0 n1 n0 n1 ) over over ;
header ?dup   : ?dup ( n -- a|a f)            dup if dup then ;
header 2swap  : 2swap ( ab cd -- cd ab )      rot >r rot r> ;
header 2over  : 2swap ( ab cd -- ab cd ab )   >r >r 2dup r> r> 2swap ;
header 3rd    : 3rd ( abc -- abc a )          >r over r> swap ;
header 3dup   : 3dup ( abc -- abc abc )       3rd 3rd 3rd ;

\ alu ops
header >       : >       ( n n -- f )       swap < ;
header u>      : u>      ( n n -- f )       swap u< ;
header 0=      : 0=      ( n -- f )         d# 0 = ;
header 0<      : 0<      ( n -- f )         d# 0 < ;
header 0>      : 0>      ( n -- f )         d# 0 > ;
header <>      : <>      ( n -- ~n )        = invert ;
header 0<>     : 0<>     ( n -- f )         d# 0 <> ;
header negate  : negate  ( n -- ~a+1 )      invert 1+ ;
header -       : -       ( n n -- n-n )     negate + ;
header 1-      : 1-      ( n -- n-1 )       d# 1 - ;
header abs     : abs     ( n -- n )         dup 0< if negate then ;
header bounds  : bounds  ( a n -- a+n a )   over+ swap ;
header 2*      : 2*      ( n -- n*2 )       d# 1 lshift ;
header 2/      : 2/      ( n -- n/2 )       dup 0< if
                                            invert d# 1 rshift invert else
                                            d# 1 rshift then ;
header /string : /string ( *c u n -- *c u ) dup >r - swap r> + swap ;
header min     : min     ( n n - n )        2dup< if drop else nip then ;
header max     : max     ( n n - n )        2dup< if nip else drop then ;
header um*     : um*     ( u1 u2 -- ud )    abs swap abs * ;
header s>d     : s>d     ( s -- s f )       dup 0< ;


: ud* ( ud1 u -- ud2 ) \ ud2 is the product of ud1 and u
    tuck * >r
    um* r> +
;

header execute : execute                2/ >r ;

\ memory address calculations, loads, stores
header cell+  : cell+ ( n -- n+cell )  d# 8 + ;
header cells  : cells ( n -- n*cell )  d# 3 lshift ;
header hi32   : hi32 ( n -- w )        d# 32 rshift ;
header lo32   : lo32 ( n -- w )        d# 32 lshift d# 32 rshift ;
header hi16   : hi16 ( n -- s )        d# 32 lshift d# 48 rshift ;
header lo16   : lo16 ( n -- s )        d# 48 lshift d# 48 rshift ;
header >><<   : >><< ( n n -- n )      tuck rshift swap lshift ;
header c!     : c! ( u *c -- )         >r h# ff and @r nmask8 @and or r> ! ;
header c@     : c@ ( *c -- c )         @ h# ff and ;
header w!     : w! ( u *c -- )         >r h# ffff and @r d# 16 >><< or r> ! ;
header uw@    : uw@ ( *c -- w )        \ aliased to w@ as we're a 64-bit forth
header w@     : w@ ( *c -- w )         @ h# ffff and ;
header dw!    : dw! ( u *c -- )        >r h# ffffffff and @r d# 32 >><< or r> ! ;
header dw@    : dw@ ( *c -- dw )       @ h# ffffffff and ;
header 2@     : 2@ ( addr -- n n )     dup cell+ @ swap @ ;
header 2!     : 2! ( n n addr -- )     tuck ! cell+ ! ;
\ header +!     : +! ( u *c -- )       tuck @ + swap ! ;
header here   : here ( -- addr )       dp @ ;
header allot  : tallot ( u -- )        dp +! ;
header ,      : , ( n -- )             here ! d# 8 tallot ;
header dw,    : dw, ( dw -- )          here dw! d# 4 tallot ;
header w,     : w, ( w -- )            here w! d# 2 tallot ;
header c,     : tc, ( ch -- )          here c! d# 1 tallot ;
header c@+    : c@+ ( *c -- *c c )     dup c@ swap 1+ swap ;
header caligned : caligned ( *c -- *c ) 1+ d# -2 and ;
header count  : count ( *c -- sz )     dup 1+ swap c@ ;
header >xt    : >xt ( -- )             d# 2 + count + caligned uw@ ;

\ user interaction, I/O
: hex8 dup d# 16 rshift DOUBLE
: hex4 dup d# 8 rshift DOUBLE
: hex2 dup d# 4 rshift DOUBLE
header io!    :noname                  io! ;
header io@    :noname                  io@ ;
header emit   : emit ( ch -- )         IO-OUT io! ;
header key    : key ( -- ch )          IO-IN  io@ ;
header .s     : .s ( -- )              d# 0 h# e0 io! ;
header bl     : bl ( -- )              [SPACE] ;
header space  : space ( -- )           [SPACE] emit ;
header cr     : cr ( -- )              [LF] emit [CR] emit ;
header .x     : . hex8 space ;
header .x2    : . hex2 space ;
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

\ source parsing variable acccess
header source    : source    ( -- addr len ) sourceC 2@ ;
header source!   : source!   ( addr u -- )   sourceC 2! ;
header source-id : source-id ( -- addr )     sourceid @ ;
header lower     : lower     ( c1 -- c2 )    [AT] over <
                                             over [LBRACKET] < and
                                             h# 20 and + ;
header i<>       : i<>       ( c1 c2 -- f )  2dupxor h# 1f and if
                                               drop exit
                                             then
                                             lower swap lower xor ;
header isimmediate : isimmediate ( wp -- -1 | 1 ) uw@ d# 1 and 2* 1- ;

: digit? ( c -- u f )
   lower
   dup h# 39 > h# 100 and +
   dup h# 160 > h# 127 and - h# 30 -
   dup base @ u<
;

header >number
: >number ( ud1 c-addr1 u1 -- ud2 c-addr2 u2 )
    begin
        dup
    while
        over c@ digit?
        0= if drop exit then
        >r 2swap base @ ud*
        r> s>d + 2swap
        d# 1 /string
    repeat
;

: code,
    cp @ w!
    d# 2 cp +!
;

: inline:
    r>
    dup uw@ code,
    d# 2 + >r
;

: isreturn ( opcode -- f )
    h# e080 and
    h# 6080 =
;

: isliteral ( ptr -- f)
    dup uw@ h# 8000 and 0<>
    swap d# 2 + uw@ h# 608c = and
;

header compile,
: compile,
    dup uw@ isreturn if
        uw@ h# ff73 and
        code,
    else
        dup isliteral if
            uw@ code,
        else
            2/ h# 4000 or
            code,
        then
    then
;

header isspace?
: isspace? ( c -- f )
    h# 21 u< ;

header isnotspace
: isnotspace? ( c -- f )
    isspace? 0= ;


header xt-skip
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

: nextword
    uw@ d# -2 and
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

header sfind
: sfind
    forth uw@
    begin
        dup
    while
        sameword
        if
            nip nip
            dup >xt
            swap isimmediate
            exit
        then
        nextword
    repeat
;

: jumptable ( u -- ) \ add u to the return address
    r> + >r ;

\   (doubleAlso) ( c-addr u -- x 1 | x x 2 )
\               If the string is legal, give a single or double cell number
\               and size of the number.

header throw
: throw
    ?dup if
        [char] e emit
        [char] r emit
        [char] r emit
        [char] o emit
        [char] r emit
        [char] : emit
        space
        .
        d# 2 execute
    then
;

: -throw ( a b -- ) \ if a is nonzero, throw -b
    negate and throw
;

header-imm 2literal
: 2literal
    swap DOUBLE
header-imm literal
: tliteral
    dup 0< if
        invert tliteral
        inline: invert
    else
        dup h# ffff8000 and if
            dup d# 15 rshift tliteral
            d# 15 tliteral
            inline: lshift
            h# 7fff and tliteral
            inline: or
        else
            h# 8000 or code,
        then
    then

;

: isvoid ( caddr u -- ) \ any char remains, throw -13
    nip 0<> d# 13 -throw
;

: consume1 ( caddr u ch -- caddr' u' f )
    >r over c@ r> =
    over 0<> and
    dup>r d# 1 and /string r>
;

header echo-input
: echo-input
    begin
        key dup emit    \ emit each character typed until `CR`
    d# 10 <> until
;

: is'c' ( caddr u -- f )
    d# 3 =
    over c@ [char] ' = and
    swap 1+ 1+ c@ [char] ' = and
;

: ((doubleAlso))
    h# 0. 2swap
    [char] - consume1 >r
    >number
    [char] . consume1 if
        isvoid              \ double number
        r> if negate then
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
        1+ 2* dispatch
    repeat
    2drop
;



header s/key/key      : s/key/key      ( ch ch ch - ch )    -rot over= if
                                                             drop else
                                                             swap drop then ;
header key-printable? : key-printable? ( ch - f )           [PRINTABLE] u> ;
header is-key?        : is-key?        ( ch ch - f ch )     over= swap ;
header insert-char    : insert-char    ( a l ch -- a l ch ) >r 2dup + r@
                                                            swap c! 1+ r> ;
header delchar
: delchar ( addr len -- addr len )
    dup if [BACKSPACE] emit
           [SPACE]     emit
           [BACKSPACE] emit then

    begin
        dup 0= if exit then
        1- 2dup + c@
        h# C0 and h# 80 <>
    until
;

header accept
: accept
    tethered @ if [RS] emit then
    >r d# 0  ( addr len R: maxlen )
    begin
        key    ( addr len key R: maxlen )
        [TAB] [SPACE]     s/key/key
        [DEL] [BACKSPACE] s/key/key
        dup key-printable? if
            over r@ u< if
                tethered @ 0= if dup emit then
                insert-char
            then
        then
        [BACKSPACE] over= if >r delchar r> then
        \ Loop until we have a [LF] or [CR]
        [LF] over= swap [CR] = or
    until
    rdrop nip
    space
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

header quit
: quit
    begin
        refill drop
        interpret
        space
        [char] o emit
        [char] k emit
        halt
        cr
    again
;

: echoback
    begin
        key dup emit
        [EOT] =
    until
;

create testvar 8 allot

header main
: main
  echoback
  halt
;

meta
    link @ t,
    link @ t' forth t!
    there  t' dp t!
    tcp @  t' cp t!
target