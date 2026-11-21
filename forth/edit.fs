\ edit.fs - a block editor: a block as BLOCK-LINES lines

create ei 0 ,           \ a loop counter

create eblk  meta disk-source-block target ,
create ecx 0 ,
create ecy 0 ,
create ewin 0 ,         \ the window
create t-keys ," ^S save  ^L load  PgUp PgDn"

: ecell ( -- ) \ the cursor's character is out of date
    ecx @ ecy @ char-xy  d# 8 d# 16  ewin @ win-inval ;

: eaddr ( -- a )
    eblk @ block ecy @ LINE-SHIFT lshift + ecx @ + ;

: elines ( -- ) \ the block's lines
    d# 0 ei !
    begin
        ei @ BLOCK-LINES <
    while
        eblk @ cblock  ei @ LINE-SHIFT lshift  LINE-CHARS
        d# 0 ei @ char-xy  draw-chars
        d# 1 ei +!
    repeat
;

: ebar ( -- ) \ under the block: its number, and the keys
    false d# 0 d# 260 d# 516 hline
    sb-begin
    [char] b emit  [char] l emit  [char] o emit
    [char] c emit  [char] k emit
    space eblk @ . space space t-keys >c count type
    sb-end
    d# 2 d# 263 sb-draw
;

header edit-draw : edit-draw ( win -- )
    drop  elines
    true inverse !  eaddr c@  ecx @ ecy @ char-xy  draw-char
    d# 0 inverse !
    ebar
;

: ecol ( n -- ) ecx @ + d# 0 max LAST-COLUMN min ecx ! ;
: eline ( n -- ) ecy @ + d# 0 max LAST-LINE min ecy ! ;

: ekey ( c -- ) \ the cursor keys, ^S, and a character typed
    dup d# 128 = if drop d# -1 eline exit then
    dup d# 129 = if drop d# 1 eline exit then
    dup d# 130 = if drop d# -1 ecol exit then
    dup d# 131 = if drop d# 1 ecol exit then
    dup d# 19 = if drop flush exit then
    dup d# 13 = if drop d# 0 ecx !  d# 1 eline exit then
    dup d# 8 = if
        drop
        ecx @ if d# -1 ecx +!  d# 32 eaddr c!  update then
        exit
    then
    dup d# 32 < over d# 126 > or if drop exit then
    eaddr c! update  d# 1 ecol
;

\ PgUp and PgDn change block; ^L writes it and loads it
header edit-key : edit-key ( c win -- )
    ewin !
    dup d# 132 = if
        drop flush eblk @ 1- d# 0 max eblk ! exit
    then
    dup d# 133 = if drop flush d# 1 eblk +! exit then
    dup d# 12 = if
        drop flush
        eblk @ ['] load catch if drop then
        exit
    then
    ecell ekey ecell
;
