\ virt-disk.fs - the disks of the machine QEMU calls virt

\ Its disks are the block devices in its places for a virtio
\ device, in order: the first is disk 0.

\ in device cells, of which a place is 64
0 constant VPLACES              \ the first place,
$800 constant VPLACES-ALL       \ all 32,
$40 constant VPLACE             \ and from one to the next

include virtio-disk.fs

\ ---- in the ROM: disk 0

: disk? ( -- read write true | false )
    VPLACES VPLACES-ALL bounds
    begin
        2dup xor
    while
        dup vdisk? if nip vdisk exit then
        VPLACE +
    repeat
    xor
;

\ ---- not in the ROM: the disks after the first

\ Each disk is a device, and the driver has one queue: it is
\ pointed at the device a block is of.
meta upper-half target

create vnow 0 ,     \ the disk the driver is pointed at

\ the first device cell of the d'th block device
: vdrive-at ( d -- n true | false )
    VPLACES VPLACES-ALL bounds              ( d end n )
    begin
        2dup xor
    while
        dup vdisk? if
            rot ?dup 0= if nip true exit then
            1- -rot
        then
        VPLACE +
    repeat
    2drop drop false
;

\ pointed at disk d, the device it was at stopped first
: vdrive ( d -- f )
    dup vnow @ = if drop true exit then
    dup vdrive-at 0= if drop false exit then
    d# 0 vstatus  vstart  vnow !  true
;

: vdrives-read ( a blk -- ) \ nothing, if the disk is not there
    dup DISK-SHIFT rshift vdrive if
        d# 1 DISK-SHIFT lshift 1- and  vread exit
    then
    drop  d# 0 swap BLKCELLS cells-fill
;

: vdrives-write ( a blk -- )
    dup DISK-SHIFT rshift vdrive if
        d# 1 DISK-SHIFT lshift 1- and  vwrite exit
    then
    2drop
;

header disks \ what files.fs runs
: vdrives ( -- )
    ['] vdrives-read 'blk-read !
    ['] vdrives-write 'blk-write !
;

meta lower-half target
