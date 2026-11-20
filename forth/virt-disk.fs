\ virt-disk.fs - the disks of the machine QEMU calls virt

\ That machine has 32 places for a virtio device, from
\ $0a000000 of its addresses and $200 apart.  Its disks are the
\ block devices in them, in the order of their places: the first
\ is disk 0, which the machine starts from, the next disk 1, and
\ so on.  The AArch64 kernels are the machine on it
\ (baremetal/).

$0a000000 constant VPLACES      \ the first place,
$4000 constant VPLACES-ALL      \ how far past it the last ends,
$200 constant VPLACE            \ and from one to the next

include virtio-disk.fs

\ ---- in the ROM: disk 0

\ the disk the machine starts from, if there is one: its words
\ to read and to write a block (nuc.fs)
: disk? ( -- read write true | false )
    VPLACES VPLACES-ALL bounds
    begin
        2dupxor
    while
        dup vdisk? if nip vdisk exit then
        VPLACE +
    repeat
    xor
;

\ ---- not in the ROM: the disks after the first

\ Each disk is a device of its own, and the ROM's driver is of
\ one device, which it has one queue for.  A block of another
\ disk is read by pointing that driver at the other device: the
\ device is started as the first was (vstart), with the same
\ queue.  A block's number says which disk (doc/disk.md); a disk
\ that is not there reads as nothing, and writing it does
\ nothing.  This is in the upper half of the kernel's page, and
\ comes with the image: the ROM needs disk 0 alone.  When the
\ system asks which disks there are it runs the word `disks`, if
\ a machine has one (files.fs), and this machine's puts these in
\ the place of the ROM's two.
meta upper-half target

create vnow 0 ,     \ the disk the driver is pointed at

\ the registers of the d'th block device there is, or nothing
: vdrive-at ( d -- pa|0 )
    VPLACES VPLACES-ALL bounds              ( d end pa )
    begin
        2dupxor
    while
        dup vdisk? if
            rot ?dup 0= if nip exit then
            1- -rot
        then
        VPLACE +
    repeat
    2drop drop d# 0
;

\ The driver pointed at disk d: false if there is no such disk.
\ The device it was pointed at is stopped first, as it was when
\ it started: left as it was, it goes on watching the queue,
\ which is the other's now, and takes what is offered there for
\ its own.
: vdrive ( d -- f )
    dup vnow @ = if drop true exit then
    dup vdrive-at ?dup 0= if drop false exit then
    d# 0 vstatus  vstart  vnow !  true
;

: vdrives-read ( a blk -- )
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

header disks
: vdrives ( -- )
    ['] vdrives-read 'blk-read !  ['] vdrives-write 'blk-write !
;

meta lower-half target
