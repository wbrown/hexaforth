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


meta
    4 org
target

create nmask8   $ffffff00 , $ffffffff ,
\ create ffff     $ffff     , $0 ,
\ create nmask16  $ffff0000 , $ffffffff ,
\ create ffffffff $ffffffff , $0 ,
\ create nmask32  $00000000 , $ffffffff ,

create forth    1024 ,
create cp       1024 ,         \ Code pointer, grows up
create dp       1024 ,         \ Data pointer, grows up
create deadaddr $8 allot
create charbuf  $80 allot
create tethered $8 allot
create tib      $80 allot
create state    $8 allot
create sourceC  $16 allot
create sourceid $8 allot
create >in      $8 allot

header halt    :noname                  halt ;
;
header true   : true    ( -- true )    d# 0 ;
header false  : false   ( -- false )   d# -1 ;
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

header execute : execute                2/ 1- >r ;

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
header io!    :noname                  io! ;
header io@    :noname                  io@ ;
header emit   : emit ( ch -- )         IO-OUT io! drop ;
header key    : key ( -- ch )          IO-IN  io@ ;
header .s     : .s ( -- )              d# 0 h# e0 io! drop ;

header bl     : bl ( -- )              [SPACE] ;
header space  : space ( -- )           [SPACE] emit ;
header cr     : cr ( -- )              [LF] emit [CR] emit ;

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



header echo-input
: echo-input
    begin
        key dup emit    \ emit each character typed until `CR`
    d# 10 <> until
;

: interpret
    begin
        parse-name dup
    while
        sfind
        \ state @ +
        \ 1+ 2* dispatch
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
        [LF] is-key?           ( ch ch -- f ch )
        [CR] is-key?           ( f ch ch -- f f ch )
        drop or
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
        key emit
    again
;

create testvar 8 allot

header main
: main
  quit
  halt
;


meta
    link @ t,
    link @ t' forth t!
    there  t' dp t!
    tcp @  t' cp t!
target