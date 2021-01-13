$00f1 constant IO-OUT
$00e0 constant IO-IN
create forth    0 ,
create cp       0 ,         \ Code pointer, grows up
create dp       0 ,         \ Data pointer, grows up

header io!      :noname     io!        ;
header io@      :noname     io@        ;
header <>       : <>        = invert   ;
header halt     :noname     halt       ;
header quit     : quit      halt       ;
header emit     : emit      IO-OUT io! ;
header key      : key       IO-IN  io@ ;

header main
: main
    begin
        key dup emit    \ emit each character typed until `CR`
    d# 10 <> until
;

meta
    link @ t,
    link @ t' forth t!
    there  t' dp t!
    tcp @  t' cp t!
target