\ edit.fs - a block editor: one 1024-byte block as 16 lines of 64
\
\ Typing overwrites.  The cursor keys move, PgUp and PgDn change block,
\ ^S writes the block to disk, and ^L writes it and then loads it as Forth.

create eblk 1 ,
create ecx 0 ,
create ecy 0 ,
create t-keys ," ^S save  ^L load  PgUp PgDn"

: eaddr ( -- a ) eblk @ block ecy @ d# 6 lshift + ecx @ + ;

: edit-draw ( win -- )
    content ty0 ! tx0 !
    d# 0 ti !
    begin
        ti @ d# 16 <
    while
        eblk @ block ti @ d# 6 lshift + d# 64
        tx0 @ d# 2 +  ty0 @ d# 2 + ti @ d# 4 lshift +  draw-text
        d# 1 ti +!
    repeat
    true inverse !
    eaddr c@
    tx0 @ d# 2 + ecx @ d# 3 lshift +
    ty0 @ d# 2 + ecy @ d# 4 lshift +
    draw-char
    d# 0 inverse !
    false tx0 @ ty0 @ d# 260 + d# 516 hline
    sb-begin
    [char] b emit [char] l emit [char] o emit [char] c emit [char] k emit
    space eblk @ . space space t-keys count type
    sb-end
    tx0 @ d# 2 + ty0 @ d# 263 + sb-draw
;

: edit-key ( c win -- )
    drop
    dup d# 128 = if drop ecy @ 1- d# 0 max ecy ! exit then
    dup d# 129 = if drop ecy @ 1+ d# 15 min ecy ! exit then
    dup d# 130 = if drop ecx @ 1- d# 0 max ecx ! exit then
    dup d# 131 = if drop ecx @ 1+ d# 63 min ecx ! exit then
    dup d# 132 = if drop flush eblk @ 1- d# 0 max eblk ! exit then
    dup d# 133 = if drop flush d# 1 eblk +! exit then
    dup d# 19 = if drop flush exit then
    dup d# 12 = if
        drop flush
        eblk @ ['] load catch if drop then
        exit
    then
    dup d# 13 = if
        drop
        d# 0 ecx !
        ecy @ 1+ d# 15 min ecy !
        exit
    then
    dup d# 8 = if
        drop
        ecx @ if d# -1 ecx +!  d# 32 eaddr c!  update then
        exit
    then
    dup d# 32 < over d# 126 > or if drop exit then
    eaddr c! update
    ecx @ 1+ d# 63 min ecx !
;
