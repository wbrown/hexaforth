\ virtio-disk.fs - a virtio block device as a disk, for the ROM
\ of a machine that starts from one

\ Version 2's registers, of 32 bits; polled.

lacks? reg32! [if]
include reg.fs
[then]

\ What the device and this share, the last bytes of memory
\ (top-kept); fields have their lowest byte first:
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
\   128  where the device's registers are: a cell

meta
#144 cell-bytes / top-kept !
1 addr-bits lshift top-kept @ - cell-bytes * constant vq-at
: vq-cell ( bytes -- cell ) vq-at + cell-bytes / ;
target
meta vq-at target constant VQB          \ the byte they start at
meta 0 vq-cell target constant VQ       \ and the cell
meta #16 vq-cell target constant VQ-BLOCK
meta #32 vq-cell target constant VQ-STATUS
meta #56 vq-cell target constant VQ-FIRST
meta #128 vq-cell target constant VQ-DEV
meta #136 cell-bytes / target constant VQ-CELLS

: vstatus ( x -- ) h# 70 ;into v!  \ how far the driver has got
: v! ( x register -- ) VQ-DEV @ + reg32! ;
: vq! ( x n -- ) VQB + w! ;    \ 16 bits, n bytes into them
: vq@ ( n -- x ) VQB + uw@ ;

: also-2 ( f pa -- f' ) reg32@ d# 2 xor or ;  \ and is the register there 2?
: vdisk? ( pa -- f ) \ is a block device at those registers?
    dup reg32@ h# 74726976 xor          \ "virt"
    over d# 4 + also-2                  \ version 2
    swap d# 8 + also-2                  \ a block device
    0=
;

: vstart ( pa -- ) \ the device there, started again
    d# 0 VQ VQ-CELLS cells-fill  VQ-DEV !
    d# 0 vstatus                    \ reset
    d# 3 vstatus                    \ seen, and driven
    d# 1 h# 24 v!  d# 1 h# 20 v!    \ of what it can do, bit 32
    d# 11 vstatus
;into vqueue

: vqueue ( -- )
    d# 4 h# 38 v!                   \ four descriptors
    VQ >phys
    dup h# 80 v!  dup d# 64 + h# 90 v!  dup d# 80 + h# a0 v!
    dup d# 48 + VQ !                \ 0: the header, 16 bytes
    d# 16 d# 8 vq!  d# 1 d# 12 vq!  d# 1 d# 14 vq!
    d# 1024 d# 24 vq!  d# 2 d# 30 vq!   \ 1: the block
    d# 120 + VQ-STATUS !            \ 2: the status, a byte,
    d# 1 d# 40 vq!  d# 2 d# 44 vq!  \ which the device writes
    d# 1 h# 44 v!                   \ the queue is ready,
    d# 15 vstatus                   \ and so is the driver
;

: vwrite ( a blk -- ) d# 1 ;into vio
\ type: 0 to read the block to the cells from a, 1 to write it
: vio ( a blk type -- )
    dup d# 48 vq!
    2* d# 3 xor d# 28 vq!           \ a block read is written
    2* VQ-FIRST !
    >phys VQ-BLOCK !
    d# 82 vq@ dup 1+ d# 66 vq!      \ one more than are done
    d# 0 h# 50 v!                   \ the device is told,
    begin                           \ and waited for
        d# 82 vq@ over xor
    until
    drop
    d# 120 vq@ 0<> d# 38 -throw
;

: vread  ( a blk -- ) d# 0 vio ;

: vdisk ( pa -- read write true )
    vstart  ['] vread ['] vwrite true
;
