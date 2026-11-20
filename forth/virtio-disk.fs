\ virtio-disk.fs - a virtio block device as a disk, for the ROM
\ of a machine that starts from one

\ The device's registers are of 32 bits, at an address of the
\ machine underneath that whoever calls this says, and have the
\ layout of version 2.  It is asked whether it is done, and
\ takes no interrupt.  Nothing here is a number of bits a cell
\ must be: what is a cell here is the address of a machine,
\ which is a cell (MODEL.md, the ports), and a block's number.

lacks? reg32! [if]
include reg.fs
[then]

\ What the device and this share is memory it is told the place
\ of, whose fields are of 16, 32 and 64 bits, the lowest byte
\ first.  A field of 16 bits is stored by the address of its
\ byte (w!).  One of 64 that holds an address or a block's
\ number is at a whole cell, and that is stored as a cell: the
\ rest of the field is nothing and stays so.  The bytes:
\
\   0    four descriptors of 16: where (64 bits), how long (32),
\        what is so of it (16; 1: another follows, 2: the device
\        writes it) and which follows (16).  0 is the request's
\        header; 1 the block, which follows it; 2 the status,
\        which follows that.  3 is not used as one:
\   48   in its place is the header: 0 to read or 1 to write
\        (32), nothing (32), and which 512 bytes are first (64)
\   64   what is offered: nothing (16), how many requests there
\        have been (16), and where each of the last four starts,
\        which is always descriptor 0
\   80   what is done: nothing (16), how many the device has
\        finished (16), and what it says of the last four
\   120  the status: a byte, 0 if all is well
\   128  where the device's registers are: a cell
\
\ They are the last 1,024 bytes of the kernel's page, where no
\ code is.  The image has nothing there, so no block of it is
\ read over them while they are what reads it; cross.fs looks.

meta
$2000 #1024 - constant vq-at
vq-at dup #1024 + kept-nothing
: vq-cell ( bytes -- cell ) vq-at + cell-bytes / ;
target
meta vq-at target constant VQB          \ the byte they start at
meta 0 vq-cell target constant VQ       \ and the cell
meta #16 vq-cell target constant VQ-BLOCK
meta #32 vq-cell target constant VQ-STATUS
meta #56 vq-cell target constant VQ-FIRST
meta #128 vq-cell target constant VQ-DEV
meta #136 cell-bytes / target constant VQ-CELLS

: v! ( x register -- ) VQ-DEV @ + reg32! ;
: vstatus ( x -- ) h# 70 v! ;  \ how far the driver has got
: vq! ( x n -- ) VQB + w! ;    \ 16 bits, n bytes into them
: vq@ ( n -- x ) VQB + uw@ ;

\ is a block device of this layout at those registers?
: vdisk? ( pa -- f )
    dup reg32@ h# 74726976 xor          \ "virt"
    over d# 4 + reg32@ d# 2 xor or      \ version 2
    swap d# 8 + reg32@ d# 2 xor or      \ a block device
    0=
;

\ The device there is started again and told what of it is
\ used: of what it can do, bit 32 alone, which is version 1's
\ layout.  Then its one queue, which is what follows.
: vstart ( pa -- )
    d# 0 VQ VQ-CELLS cells-fill  VQ-DEV !
    d# 0 vstatus                    \ as it was at first
    d# 3 vstatus                    \ seen, and driven
    d# 1 h# 24 v!  d# 1 h# 20 v!
    d# 11 vstatus                   \ which it is to agree to
;fallthru
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

\ type: 0 to read the block to the cells from a, 1 to write it
\ from them.  One request is made at a time, so it is done when
\ the device has finished one more than it had: nothing here
\ reads what the device says of it but the status, whose field
\ is read with the byte after it, which is always nothing.  A
\ status that is not nothing is thrown: a block the disk has not
\ is one.  (It was handed to -throw as it was, 1 for an error,
\ and -throw, which wants a flag, threw nothing: a block that
\ was not read was taken for read.)
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
    d# 120 vq@ 0<> d# 38 -throw     \ -throw is given a flag
;

: vread  ( a blk -- ) d# 0 vio ;
: vwrite ( a blk -- ) d# 1 vio ;

\ the device at those registers as the disk: its words to read
\ and to write a block (nuc.fs)
: vdisk ( pa -- read write true )
    vstart  ['] vread ['] vwrite true
;
