\ reg.fs - a device's registers and its memory, through the
\ ports

\ A device has registers of a fixed width at an address of the
\ machine underneath, and may be told where in that machine's
\ memory to read and write.  The ports reach both (MODEL.md):
\ an address written to $c0 is where $c4 reads or writes 32
\ bits, and $c0 reads as where the machine's cell 0 is.

\ These are what a driver is made of.  A driver that is in a
\ machine's ROM has them there with it; where none is, they are
\ with the rest of the port's words (machine.fs).  Whichever of
\ the two comes first includes this.

\ what a device is told for the cell a
header >phys  : >phys ( a -- pa ) >c  h# c0 io@ + ;
header reg32@ : reg32@ ( pa -- x ) h# c0 io!  h# c4 io@ ;
header reg32! : reg32! ( x pa -- ) h# c0 io!  h# c4 io! ;
