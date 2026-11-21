\ port-disk.fs - a disk whose controller is ports $b0-$b2, for
\ the ROM of a machine that starts from one

\ $b0 holds a block's number, and reads as how many blocks there
\ are; a cell's address written to $b1 reads the block to there,
\ and to $b2 writes it from there (MODEL.md).

: pd-read  ( a blk -- ) h# b0 io!  h# b1 io! ;
: pd-write ( a blk -- ) h# b0 io!  h# b2 io! ;

: disk? ( -- read write true | false )
    h# b0 io@ dup if drop ['] pd-read ['] pd-write true then
;
