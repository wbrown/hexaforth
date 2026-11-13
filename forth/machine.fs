\ machine.fs - access to the machine underneath the VM

\ Device registers have a fixed width, but ! always moves a cell and c! and
\ w! read the whole cell back first.  The VM's device ports give exact-width
\ access at a latched machine address (see vm.c).

: membase ( -- pa ) h# c0 io@ ;         \ machine address of VM byte 0
header >phys : >phys ( a -- pa ) >c membase + ;      \ what a device must be told for a

: reg8@  ( pa -- x ) h# c0 io!  h# c1 io@ ;
header reg32@ : reg32@ ( pa -- x ) h# c0 io!  h# c4 io@ ;
header reg64@ : reg64@ ( pa -- x ) h# c0 io!  h# c8 io@ ;
header reg16! : reg16! ( x pa -- ) h# c0 io!  h# c2 io! ;
header reg32! : reg32! ( x pa -- ) h# c0 io!  h# c4 io! ;
header reg64! : reg64! ( x pa -- ) h# c0 io!  h# c8 io! ;

\ Memory above the dictionary, for buffers too big for it.  Never freed.

create himem $30000 ,       \ in bytes: cell $6000, after the display

header halloc
: halloc ( u -- a ) \ u bytes, 16-byte aligned; a is the address of a cell
    himem @ d# 15 + d# -16 and
    dup >r + himem !
    r> c>
;

\ 32-bit fields in ordinary memory, as parts of the cells they are in.  Like
\ c@ and uw@, these and the big-endian words take the address of a byte.

header l@ : l@ ( a -- x ) h# ffffffff field@ ;
header l! : l! ( x a -- ) h# ffffffff field! ;

\ Big-endian fields, for firmware structures and big-endian TIFF.

header be16@ : be16@ ( a -- x ) dup c@ d# 8 lshift swap 1+ c@ or ;
header be32@ : be32@ ( a -- x ) dup be16@ d# 16 lshift swap d# 2 + be16@ or ;

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
