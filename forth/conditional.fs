\ conditional.fs - [if] [else] [then]: source that is skipped

\ cross.fs runs these too: gforth compiles this file for it
\ (host-included).

: chars= ( a a' u -- f ) \ at two addresses of bytes
    negate
    begin dup while
        >r  over c@ over c@ <> if 2drop rdrop false exit then
        1+ swap 1+ swap  r> 1+
    repeat
    drop 2drop true
;

: is? ( a u cstr -- f ) \ is the text a counted string's?
    >c count  rot over <> if 2drop drop false exit then  chars=
;

create t-if   ," [if]"
create t-else ," [else]"
create t-then ," [then]"

\ a word that is skipped: true at the [else] or [then] that
\ ends the skipping
: skipped? ( depth c-addr u -- depth' f )
    2dup t-if is? if 2drop 1+ false exit then
    2dup t-else is? if 2drop dup d# 1 = exit then
    t-then is? if 1- dup 0= exit then
    false
;

\ to its [else] or [then].  At the end of a line the next is
\ read, where there is one; a block is all one line.
: skip-cond ( -- )
    d# 1
    begin
        parse-name dup if
            skipped? if drop exit then
        else
            2drop  refill 0= if drop exit then
        then
    again
;

header-imm [if] : cond-if ( f -- ) 0= if skip-cond then ;
header-imm [else] : cond-else ( -- ) skip-cond ;
header-imm [then] : cond-then ( -- ) ;
