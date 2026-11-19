\ vmdisk.fs - block driver for the reference VM's disk
\
\ The reference VM can be given a file to be its disk.  The kernel
\ has the words for its ports (nuc.fs), since its ROM reads the
\ disk when the machine starts.  This makes them the driver.

header vmdisk-init
: vmdisk-init ( -- )
    disk-blocks 0= d# 37 -throw
    ['] vd-read  'blk-read !
    ['] vd-write 'blk-write !
    d# -1 blk-cur !
;

\ What a machine started from its ROM goes on with, once the ROM
\ has read the rest of the system from this VM's disk (nuc.fs):
\ the disk is this one, and the block of what is loaded is loaded
\ (doc/boot.md).
header boot
: boot ( -- ) vmdisk-init  SOURCE-BLK load ;
