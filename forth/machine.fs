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

\ It starts after the display, which is the last thing the machine itself
\ lays out.
\
\ What is given out is counted in cells and is the address of a cell.  The
\ address of a byte is twice a cell's address or more, and where a cell is
\ 16 bits and memory is 65,536 of them, only the lower half of memory has
\ bytes that an address can say: so nothing up here is reached by the
\ address of a byte unless it is known to be low, and nothing up here is
\ counted in bytes.
\
\ On the machine of 64K cells the image itself has data up there, after
\ the display: the fonts opened out, and tables.  What is given out starts
\ after that, and nuc.fs says where when the system has all been laid down.
header himem create himem  meta display-end target ,     \ the next cell to give out

\ himem is always an even cell, so what is given is the cell it says, and
\ whoever gives its memory back puts himem where it was to the cell: the
\ tables of names are given back only if himem is where they ended, and a
\ scroll takes and gives back memory while they are in.  (himem was once
\ left where an allocation ended and made even by the next: a scroll after
\ names of an odd number of cells then left it a cell on, and the names
\ were never given back.)
header halloc
: halloc ( u -- a ) \ room for u bytes, in whole cells from an even cell; a is the address of a cell
    CELLB 1- + c>                                   ( cells )
    himem @                                         ( cells a )
    2dup MEM-END swap - u> d# 8 -throw              \ there are not that many left
    tuck + 1+ d# -2 and himem !
;

\ Memory that has been given back: one run of it, the biggest there has
\ been.  Whoever is done for good with memory that others were given
\ memory after says so (hfree), and whoever does not mind where its memory
\ is asks for that first (hreuse).  What halloc gives is always new: those
\ that take memory and put himem back to give it up rely on that.
\
\ What is had from the run is had from its start, so whoever gives back
\ what it had from it, before anyone has had more, gives back cells that
\ end where the run now starts, and the run is whole again (hfree).  The
\ cells given back are as many as were asked for, made even.
create hole 0 , 0 ,     \ where it starts, and how many cells of it are left

header hfree
: hfree ( a cells -- ) \ that many cells from a, which is an even cell, are nobody's
    2dup + hole @ = if hole CELL + +!  hole !  exit then    \ they end where the run starts: it is one run
    dup hole CELL + @ u> if hole CELL + !  hole ! else 2drop then
;

header hgive
: hgive ( a cells -- ) \ memory that is done with: if it was the last given out it is not given out, or else it is nobody's
    2dup + 1+ d# -2 and himem @ = if drop himem ! exit then
    hfree
;

header hreuse
: hreuse ( u -- a ) \ room for u bytes: of what was given back, if that is enough, or new
    dup CELLB 1- + c>  1+ d# -2 and                 ( u cells )
    dup hole CELL + @ u> if drop halloc exit then
    nip  hole @ swap                                ( a cells )
    dup hole +!  negate hole CELL + +!
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
