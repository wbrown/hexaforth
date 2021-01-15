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

header "hello"
create "hello"  'h' c, 'e' c, 'l' c, 'l' c,
                'o' c, $32 c, 'w' c, 'o' c,
                'r' c, 'l' c, 'd' c, '!' c,


header tuck  : tuck ( a b -- b a b ) swap over ;
header hi32  : hi32 ( dw -- w ) d# 32 rshift ;
header lo32  : lo32 ( dw -- w ) d# 32 lshift d# 32 rshift ;
header hi16  : hi16 ( dw -- s ) d# 32 lshift d# 48 rshift ;
header lo16  : lo16 ( dw -- s ) d# 48 lshift d# 48 rshift ;
header c!    : c! ( u c-addr -- ) dup>r @ d# 8 rshift d# 8 lshift or r> ! ;
header c@    : c@ ( c-addr -- c ) @ h# ff and ;
header w!    : w! ( u c-addr -- ) dup>r @ d# 16 rshift d# 16 lshift or r> ! ;
header w@    : w@ ( c-addr -- w ) @ h# ffff and ;
header +!    : +! ( u c-addr -- ) tuck @ + swap ! ;
header here  : here ( -- dp )     dp @ ;
header allot : tallot             dp +! ;
header c,    : tc,                here c! d# 1 tallot ;

header io!      :noname     io!        ;
header io@      :noname     io@        ;
header <>       : <>        = invert   ;
header halt     :noname     halt       ;
header quit     : quit      halt       ;
header emit     : emit      IO-OUT io! ;
header key      : key       IO-IN  io@ ;

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

header main
: main
  forth w@
  "hello" c@+ emit c@+ emit c@+ emit c@+ emit
;


meta
    link @ t,
    link @ t' forth t!
    there  t' dp t!
    tcp @  t' cp t!
target