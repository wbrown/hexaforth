\ virtio-disk.fs - a virtio block device as a disk, for the ROM
\ of a machine with 64-bit cells that starts from one

\ virtio-mmio version 2, polled.  The device, as this
\ machine's 64-bit cells (reg.fs):
\   0    the magic number, and above it the version
\   1    low half: the kind of device (2 is a block device)
\   4    what the driver can do, 32 bits at a time: the low
\        half is the bits, and the high half says which 32
\   7    how many descriptors the queue has
\   8    high half: 1 when the queue is ready
\   10   written to say that the queue has a request
\   14   how far the driver has got
\   16, 18, 20  where the descriptors, what is offered and
\        what is done are

lacks? dev! [if]
include reg.fs
[then]

\ The memory shared with the device: the last cells of memory
\ (top-kept).  Offsets are in bytes and fields are
\ little-endian:
\   0    four descriptors of 16: where (64 bits), how long (32),
\        flags (16; 1: another follows, 2: the device writes it)
\        and which follows (16).  0 is the request's header, 1
\        the block, 2 the status
\   48   the header: 0 to read or 1 to write (32), unused (32),
\        and which 512 bytes are first (64)
\   64   offered: unused (16), how many requests (16), and
\        where each of the last four starts
\   80   done: unused (16), how many (16), and the last four
\   120  the status: a byte, 0 if all is well
\   128  the device's first cell: a cell
\ vq@ and vq! take a 16-bit field's number: its offset / 2.

meta
#144 cell-bytes / top-kept !
1 addr-bits lshift top-kept @ - cell-bytes * constant vq-at
: vq-cell ( bytes -- cell ) vq-at + cell-bytes / ;
target
meta vq-at 2/ target constant VQI   \ offset 0, in 16-bit units
meta 0 vq-cell target constant VQ   \ offset 0, in cells
meta #16 vq-cell target constant VQ-BLOCK
meta #32 vq-cell target constant VQ-STATUS
meta #48 vq-cell target constant VQ-HEADER
meta #56 vq-cell target constant VQ-FIRST
meta #64 vq-cell target constant VQ-AVAIL
meta #80 vq-cell target constant VQ-USED
meta #120 vq-cell target constant VQ-RESULT
meta #128 vq-cell target constant VQ-DEV
meta #136 cell-bytes / target constant VQ-CELLS

: vdev+ ( n -- n' ) VQ-DEV @ + ;    \ cell n of the device
: vstatus ( x -- ) d# 14 ;into v!   \ how far the driver has got
: v! ( x n -- ) vdev+ dev! ;
: va! ( a n -- ) vdev+ dev-addr! ;  \ its cell n: where cell a is
: vq! ( x n -- ) VQI + i! ;    \ 16-bit field n
: vq@ ( n -- x ) VQI + i@ ;

: vdisk? ( n -- f ) \ is a block device at those device cells?
    dup dev@ h# 274726976 xor           \ "virt", version 2
    swap 1+ dev@ d# 32 lshift           \ and of the kind that
    h# 200000000 xor or                 \ is a block device
    0=
;

: vstart ( n -- ) \ the device there, started again
    d# 0 VQ VQ-CELLS cells-fill  VQ-DEV !
    d# 0 vstatus                    \ reset
    d# 3 vstatus                    \ seen, and driven
    h# 100000000 dup d# 4 v!        \ of what it can do, the
    1+ d# 4 v!                      \ second 32: bit 32
    d# 11 vstatus
;into vqueue

: vqueue ( -- )
    d# 4 d# 7 v!                    \ four descriptors
    VQ d# 16 va!  VQ-AVAIL d# 18 va!  VQ-USED d# 20 va!
    VQ-HEADER VQ cell-addr!         \ 0: the header, 16 bytes
    d# 16 d# 4 vq!  d# 1 d# 6 vq!  d# 1 d# 7 vq!
    d# 1024 d# 12 vq!  d# 2 d# 15 vq!   \ 1: the block
    VQ-RESULT VQ-STATUS cell-addr!  \ 2: the status, a byte,
    d# 1 d# 20 vq!  d# 2 d# 22 vq!  \ which the device writes
    h# 100000000 d# 8 v!            \ the queue is ready,
    d# 15 vstatus                   \ and so is the driver
;

: vwrite ( a blk -- ) d# 1 ;into vio
\ type: 0 to read the block to the cells from a, 1 to write it
: vio ( a blk type -- )
    dup d# 24 vq!
    2* d# 3 xor d# 14 vq!           \ a block read is written
    2* VQ-FIRST !
    VQ-BLOCK cell-addr!
    d# 41 vq@ dup 1+ d# 33 vq!      \ one more than are done
    d# 0 d# 10 v!                   \ the device is told,
    begin                           \ and waited for
        d# 41 vq@ over xor
    until
    drop
    d# 60 vq@ 0<> d# 38 -throw
;

: vread  ( a blk -- ) d# 0 vio ;

: vdisk ( n -- read write true )
    vstart  ['] vread ['] vwrite true
;
