\ machine.fs - access to the machine underneath the VM

lacks? reg32! [if]
include reg.fs
[then]

: reg8@  ( pa -- x ) h# c0 io!  h# c1 io@ ;
header reg64@ : reg64@ ( pa -- x ) h# c0 io!  h# c8 io@ ;
header reg16! : reg16! ( x pa -- ) h# c0 io!  h# c2 io! ;
header reg64! : reg64! ( x pa -- ) h# c0 io!  h# c8 io! ;

\ ---- memory above the display, used a cell at a time: on a
\ machine of 64K cells no byte there has an address

header himem \ the next cell to give out: an even cell
    create himem  meta display-end target ,

\ room for u bytes, from an even cell.  It is always new.
nearby
header halloc
: halloc ( u -- a )
    CELLB 1- + c>                       ( cells )
    himem @                             ( cells a )
    2dup MEM-END swap - u> d# 8 -throw
    tuck + 1+ d# -2 and himem !
;
nearby-end

\ what was given back: one run, the biggest there has been
create hole 0 , 0 ,  \ where it starts, and its cells

nearby
header hfree
: hfree ( a cells -- ) \ from a, an even cell, they are nobody's
    2dup + hole @ = if hole CELL + +!  hole !  exit then
    dup hole CELL + @ u> if
        hole CELL + !  hole !
    else
        2drop
    then
;

header hgive
: hgive ( a cells -- ) \ done with: himem comes back, or hfree
    2dup + 1+ d# -2 and himem @ = if drop himem ! exit then
    hfree
;

header hreuse
: hreuse ( u -- a ) \ of what was given back, or new
    dup CELLB 1- + c>  1+ d# -2 and                 ( u cells )
    dup hole CELL + @ u> if drop halloc exit then
    nip  hole @ swap                                ( a cells )
    dup hole +!  negate hole CELL + +!
;
nearby-end

: (with-room) ( x u xt -- ) swap halloc swap execute ;

\ xt ( x a -- ) run with u bytes at a, which are then given
\ back; false if there was no room or it threw
header with-room
: with-room ( x u xt -- ok? )
    himem @ >r
    ['] (with-room) catch if 2drop drop false else true then
    r> himem !
;

\ ---- fields, at the address of a byte

header l@ : l@ ( a -- x ) h# ffffffff field@ ;
header l! : l! ( x a -- ) h# ffffffff field! ;

header be16@ : be16@ ( a -- x )
    dup c@ d# 8 lshift swap 1+ c@ or ;
header be32@ : be32@ ( a -- x )
    dup be16@ d# 16 lshift swap d# 2 + be16@ or ;

header be32! : be32! ( x a -- )
    >r
    dup d# 24 rshift r@ c!
    dup d# 16 rshift r@ 1+ c!
    dup d# 8 rshift r@ d# 2 + c!
    r> d# 3 + c!
;

header be64! : be64! ( x a -- )
    over d# 32 rshift over be32!
    d# 4 + be32!
;
