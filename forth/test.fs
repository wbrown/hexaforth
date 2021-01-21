$00f1 constant IO-OUT
$00e0 constant IO-IN
$0008 constant cellsz
$0003 constant cell<<

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
create sourceC  $16 allot
create sourceid $8 allot
create >in      $8 allot

header halt   :noname                  halt ;
\ stack ops
header rot    : rot  ( n0 n1 n2 -- n1 n2 n0 ) >r swap r> swap ;
header -rot   : -rot ( n0 n1 n2 -- n2 n0 n1 ) swap>r swapr> ;
header 2dup   : 2dup ( n0 n1 -- n0 n1 n0 n1 ) over over ;
header ?dup   : ?dup ( n -- a|a f)            dup if dup then ;
header 2swap  : 2swap ( ab cd -- cd ab )      rot >r rot r> ;
header 2over  : 2swap ( ab cd -- ab cd ab )   >r >r 2dup r> r> 2swap ;

\ alu ops
header >      : >      ( n n -- f )     swap < ;
header u>     : u>     ( n n -- f )     swap u< ;
header 1-     : 1-     ( n -- n-1 )     d# 1 - ;
header 0=     : 0=     ( n -- f )       d# 0 = ;
header 0<     : 0<     ( n -- f )       d# 0 < ;
header 0>     : 0>     ( n -- f )       d# 0 > ;
header <>     : <>     ( n -- ~n )      = invert ;
header 0<>    : 0<>    ( n -- f )       d# 0 <> ;
header negate : negate ( n -- ~a+1 )    invert 1+ ;
header -      : -      ( n n -- n-n )   negate + ;
header abs    : abs    ( n -- n )       dup 0< if negate then ;
header bounds : bounds ( a n -- a+n a ) over+ swap ;
header 2*     : 2*     ( n -- n*2 )     d# 1 lshift ;
header 2/     : 2/     ( n -- n/2 )     dup 0< if
                                          invert d# 1 rshift invert
                                        else
                                          d# 1 rshift
                                        then ;

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

\ user interaction, I/O
header io!    :noname                  io! ;
header io@    :noname                  io@ ;
header emit   : emit ( ch -- )         IO-OUT io! drop ;
header key    : key ( -- ch )          IO-IN  io@ ;
header .s     : .s ( -- )              d# 0 h# e0 io! drop ;

header space  : space ( --  )          d# 32 emit ;
header cr     : cr                     d# 13 emit d# 10 emit ;

\ source parsing variable acccess
header source    : source    ( -- addr len ) sourceC 2@ ;
header source!   : source!   ( addr u -- )   sourceC 2! ;
header source-id : source-id ( -- addr )     sourceid @ ;

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

header echo-input
: echo-input
    begin
        key dup emit    \ emit each character typed until `CR`
    d# 10 <> until
;


$08 constant [BACKSPACE]
$09 constant [TAB]
$0A constant [LF]
$0D constant [CR]
$1E constant [RS]
$20 constant [SPACE]
$7F constant [DEL]
$31 constant [PRINTABLE]

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
        \ interpret
        space
        [char] o emit
        [char] k emit
        cr
    again
;

: echoback
    begin
        key emit
    again
;

header main
: main
  deadaddr
  charbuf
  tethered
  tib
  \ d# -1 d# 16 lshift
  .s
  \ h# ab tib c!
  \ tib @
  \ d# 8 >><< dup 0< IF negate THEN
  halt
;


meta
    link @ t,
    link @ t' forth t!
    there  t' dp t!
    tcp @  t' cp t!
target