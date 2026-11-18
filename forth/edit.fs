\ edit.fs - a block editor: one 1024-byte block as 16 lines of
\ 64, which is what \ takes a line of a block to be (LINE-CHARS
\ and BLOCK-LINES: vm_constants.h says how long a line is)

\ Typing overwrites.  The cursor keys move, PgUp and PgDn change
\ block, ^S writes the block to disk, and ^L writes it and then
\ loads it as Forth.

\ A key that types or moves changes only the character the
\ cursor was on and the one it comes to, so only those are put
\ out of date.

create ei 0 ,           \ a loop counter

create eblk  meta disk-source-block target ,
create ecx 0 ,
create ecy 0 ,
create ewin 0 ,         \ the window
create t-keys ," ^S save  ^L load  PgUp PgDn"

: ecell ( -- ) \ the character the cursor is on is out of date
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

: ecol ( n -- ) \ the cursor n columns on, within the line
    ecx @ + d# 0 max LAST-COLUMN min ecx ! ;

: eline ( n -- ) \ the cursor n lines on, within the block
    ecy @ + d# 0 max LAST-LINE min ecy ! ;

: ekey ( c -- )
    \ a key that changes no more than where the cursor is
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
