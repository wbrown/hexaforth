\ reg.fs - a device's cells, through ports $c0-$c3 (MODEL.md)

\ $c0 latches the number of a device cell, and $c1 reads or
\ writes that cell: a whole cell of this machine.

: latch ( n -- ) h# c0 io! ;
header dev@ : dev@ ( n -- x ) latch  h# c1 io@ ;
header dev! : dev! ( x n -- ) latch  h# c1 io! ;
\ device cell n is given the address of cell a
header dev-addr! : dev-addr! ( a n -- ) latch  h# c2 io! ;
\ memory cell d is given the address of cell a, as a device
\ takes it
header cell-addr! : cell-addr! ( a d -- ) tuck !  h# c3 io! ;
