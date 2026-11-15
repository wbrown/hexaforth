\ vmdisk.fs - block driver for the reference VM's disk
\
\ The reference VM can be given a file to be its disk.  Port $b0 holds the
\ number of a block, and reads as how many blocks the disk has, which is
\ none on a machine that has no such disk.  The address of a cell written
\ to port $b1 has that block read into the 1,024 bytes from the cell, and
\ written to port $b2 has it written from them.

: vd-read  ( buf blk# -- ) h# b0 io!  h# b1 io! ;
: vd-write ( buf blk# -- ) h# b0 io!  h# b2 io! ;

header vmdisk-init
: vmdisk-init ( -- )
    h# b0 io@ 0= d# 37 -throw
    ['] vd-read  'blk-read !
    ['] vd-write 'blk-write !
    d# -1 blk-cur !
;
