\ port-disk.fs - a disk whose controller is ports $b0-$b2, for
\ the ROM of a machine that starts from one

\ The controller (MODEL.md): port $b0 holds the number of a
\ block, and reads as how many blocks there are, which is none
\ where there is no disk.  The address of a cell written to $b1
\ has the block read into the 1,024 bytes from there, and
\ written to $b2 has it written from them.  The reference VM
\ has one.

: pd-read  ( a blk -- ) h# b0 io!  h# b1 io! ;
: pd-write ( a blk -- ) h# b0 io!  h# b2 io! ;

\ the disk, if there is one: its words to read and to write a
\ block (nuc.fs)
: disk? ( -- read write true | false )
    h# b0 io@ dup if drop ['] pd-read ['] pd-write true then
;
