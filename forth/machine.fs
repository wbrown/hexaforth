\ machine.fs - access to the machine underneath the VM

\ Device registers have a fixed width, but ! always moves 64 bits and c! and
\ w! read the whole cell back first.  The VM's device ports give exact-width
\ access at a latched machine address (see vm.c).

: membase ( -- pa ) h# c0 io@ ;         \ machine address of VM byte 0
: >phys ( a -- pa ) membase + ;         \ what a device must be told for a

: reg8@  ( pa -- x ) h# c0 io!  h# c1 io@ ;
: reg32@ ( pa -- x ) h# c0 io!  h# c4 io@ ;
: reg64@ ( pa -- x ) h# c0 io!  h# c8 io@ ;
: reg16! ( x pa -- ) h# c0 io!  h# c2 io! ;
: reg32! ( x pa -- ) h# c0 io!  h# c4 io! ;
: reg64! ( x pa -- ) h# c0 io!  h# c8 io! ;

\ Memory above the dictionary, for buffers too big for it.  Never freed.

create himem $100000 ,

header halloc
: halloc ( u -- a ) \ u bytes, 16-byte aligned
    himem @ d# 15 + d# -16 and
    dup >r + himem !
    r>
;

\ 32-bit fields in ordinary memory, as parts of the cells they are in.

: l@ ( a -- x ) h# ffffffff field@ ;
: l! ( x a -- ) h# ffffffff field! ;

\ Big-endian fields, for firmware structures and big-endian TIFF.

: be16@ ( a -- x ) dup c@ d# 8 lshift swap 1+ c@ or ;
: be32@ ( a -- x ) dup be16@ d# 16 lshift swap d# 2 + be16@ or ;

: be32! ( x a -- )
    >r
    dup d# 24 rshift r@ c!
    dup d# 16 rshift r@ 1+ c!
    dup d# 8 rshift r@ d# 2 + c!
    r> d# 3 + c!
;

: be64! ( x a -- )
    over d# 32 rshift over be32!
    d# 4 + be32!
;
