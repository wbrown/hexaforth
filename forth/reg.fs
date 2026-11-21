\ reg.fs - a device's registers and its memory, through the
\ ports

\ An address written to $c0 is where $c4 reads or writes 32
\ bits, and $c0 reads as where cell 0 is (MODEL.md).

\ what a device is told for the cell a
header >phys  : >phys ( a -- pa ) >c  h# c0 io@ + ;
header reg32@ : reg32@ ( pa -- x ) h# c0 io!  h# c4 io@ ;
header reg32! : reg32! ( x pa -- ) h# c0 io!  h# c4 io! ;
