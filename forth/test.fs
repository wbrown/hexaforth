$00f1 constant IO-OUT
$00e0 constant IO-IN

meta
    4 org
target

create forth    1024 ,
create cp       1024 ,         \ Code pointer, grows up
create dp       1024 ,         \ Data pointer, grows up
create deadaddr 8 allot 0 ,
create charbuf  80 allot 0 ,
create tethered 0 ,
create tib      $80 allot


header "hello"
create "hello"  12 c,
                'h' c, 'e' c, 'l' c, 'l' c,
                'o' c, $20 c, 'w' c, 'o' c,
                'r' c, 'l' c, 'd' c, '!' c,


\ stack ops
header tuck   : tuck ( a b -- b a b )   swap over ;
header 2drop  : 2drop ( a b -- )        drop drop ;
header -rot   : -rot  ( abc -- cab )    swap >r swap r> ;

header 1-     : 1- ( a -- a-1 )        1 - ;
header 2dup   : 2dup ( a b -- a b ab ) over over ;
header 0=     : 0=   ( a -- f )        0 = ;
header u>     : u>                     swap u< ;
header hi32   : hi32 ( dw -- w )       d# 32 rshift ;
header lo32   : lo32 ( dw -- w )       d# 32 lshift d# 32 rshift ;
header hi16   : hi16 ( dw -- s )       d# 32 lshift d# 48 rshift ;
header lo16   : lo16 ( dw -- s )       d# 48 lshift d# 48 rshift ;
header >><<   : >><< ( a b -- a[:b])   tuck rshift swap lshift ;
header c!     : c! ( u c-addr -- )     dup>r @ d# 8 >><< or r> ! ;
header c@     : c@ ( c-addr -- c )     @ h# ff and ;
header w!     : w! ( u c-addr -- )     dup>r @ d# 16 >><< or r> ! ;
header w@     : w@ ( c-addr -- w )     @ h# ffff and ;
header +!     : +! ( u c-addr -- )     tuck @ + swap ! ;
header here   : here ( -- dp )         dp @ ;
header allot  : tallot                 dp +! ;
header c,     : tc,                    here c! d# 1 tallot ;

header io!    :noname                  io! ;
header io@    :noname                  io@ ;
header <>     : <>                     = invert ;
header halt   :noname                  halt ;
header emit   : emit                   IO-OUT io! ;
header key    : key                    IO-IN  io@ ;

header space  : space ( --  )          d# 32 emit ;
header cr     : cr                     d# 13 emit d# 10 emit ;

header bounds : bounds ( a n -- a+n a ) over + swap ;

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

header deadbeef64
: deadbeef64
    h# dea d# 48 lshift
    h# dbe d# 36 lshift or
    h# efa d# 24 lshift or
    h# bcd d# 12 lshift or
    h# ef0              or
;

: test-hilo
    deadaddr @ >r
    r@ hi32
    r@ lo32
    r@ hi16
    r@ lo16
    rdrop
;

: c@+
  dup c@ swap 1+ swap
;

$08 constant [BACKSPACE]
$09 constant [TAB]
$10 constant [LF]
$13 constant [CR]
$1E constant [RS]
$20 constant [SPACE]
$7F constant [DEL]
$31 constant [PRINTABLE]

: s/key/key         ( ch ch - ch )    -rot over= if drop else swap drop then ;
: key-printable?    ( ch - f )        [PRINTABLE] u> ;
header insert-char-buff
: insert-char-buff  ( a l ch -- a l ch ) >r 2dup + r@ swap c! 1+ r> ;
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
            over r@ u<
            if
                tethered @ 0= if dup emit then
                insert-char-buff
            then
        then

        [BACKSPACE] over= if >r delchar r> then

        d# 10 over= swap d# 13 = or
    until

    rdrop nip
    space
;

header quit
: quit
    begin
        tib dup d# 128 accept
        space
        [char] o emit
        [char] k emit
        cr
    again
;

: typehello
  "hello" dup c@ swap 1+ swap type
;

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