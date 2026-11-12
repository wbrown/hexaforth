\ virtio-blk.fs - block driver for a virtio block device on virtio-mmio
\
\ QEMU `virt` has 32 virtio-mmio slots from 0x0a000000, 0x200 apart.  This
\ uses the modern (version 2) register layout, one queue of 8 descriptors,
\ and polls for completion instead of taking interrupts.

$0a000000 constant VIRTIO0

create vdev     0 ,     \ machine address of the device's registers
create vq-desc  0 ,     \ descriptor table, available ring, used ring
create vq-avail 0 ,
create vq-used  0 ,
create vq-seen  0 ,     \ used-ring index consumed so far
create vreq     0 , 0 , \ request header: type, then sector
create vstat    0 ,     \ status byte written by the device

: vreg@ ( off -- x ) vdev @ + reg32@ ;
: vreg! ( x off -- ) vdev @ + reg32! ;

: vblk-probe ( -- found? ) \ leaves vdev at a version 2 block device
    d# 0
    begin
        dup d# 32 <
    while
        dup d# 9 lshift VIRTIO0 + vdev !
        d# 0 vreg@ h# 74726976 =            \ magic "virt"
        d# 4 vreg@ d# 2 = and               \ version
        d# 8 vreg@ d# 2 = and               \ device id: block
        if drop true exit then
        1+
    repeat
    drop false
;

: vstatus+ ( bit -- ) h# 70 vreg@ or h# 70 vreg! ;

: vdesc! ( a len flags next i -- ) \ fill descriptor i
    d# 4 lshift vq-desc @ + >r
    d# 48 lshift swap d# 32 lshift or or
    r@ d# 8 + !
    >phys r> !
;

: vblk-io ( buf blk# type -- ) \ type 0 reads, 1 writes
    vreq !
    2* vreq d# 8 + !                        \ sectors are 512 bytes
    vreq    d# 16   d# 1  d# 1 d# 0 vdesc!  \ header, then
    d# 1024  vreq @ if d# 1 else d# 3 then  \ data: device writes it on a read
            d# 2 d# 1 vdesc!
    vstat   d# 1    d# 2  d# 0 d# 2 vdesc!  \ status, written by the device

    d# 0                                    \ offer the chain starting at 0
    vq-avail @ d# 2 + uw@ d# 7 and 2*
    vq-avail @ + d# 4 + w!
    vq-avail @ d# 2 + dup uw@ 1+ swap w!
    d# 0 h# 50 vreg!                        \ notify queue 0

    vq-seen @ 1+ h# ffff and vq-seen !
    begin
        vq-used @ d# 2 + uw@ vq-seen @ =
    until
    h# 60 vreg@ h# 64 vreg!                 \ acknowledge the interrupt
    vstat c@ d# 38 -throw
;

: vblk-read  ( buf blk# -- ) d# 0 vblk-io ;
: vblk-write ( buf blk# -- ) d# 1 vblk-io ;

header vblk-init
: vblk-init ( -- )
    vblk-probe 0= d# 37 -throw
    d# 0 h# 70 vreg!                        \ reset
    d# 1 vstatus+  d# 2 vstatus+            \ acknowledge, driver
    d# 1 h# 24 vreg!  d# 1 h# 20 vreg!      \ features 32-63: VERSION_1 only
    d# 0 h# 24 vreg!  d# 0 h# 20 vreg!      \ features 0-31: none
    d# 8 vstatus+                           \ features ok
    h# 70 vreg@ d# 8 and 0= d# 37 -throw

    d# 0 h# 30 vreg!                        \ queue 0
    d# 8 h# 38 vreg!                        \ of 8 descriptors
    d# 128 halloc vq-desc !
    d# 32  halloc vq-avail !
    d# 80  halloc vq-used !
    vq-desc  @ >phys dup h# 80 vreg!  d# 32 rshift h# 84 vreg!
    vq-avail @ >phys dup h# 90 vreg!  d# 32 rshift h# 94 vreg!
    vq-used  @ >phys dup h# a0 vreg!  d# 32 rshift h# a4 vreg!
    d# 1 h# 44 vreg!                        \ queue ready
    d# 4 vstatus+                           \ driver ok
    d# 0 vq-seen !

    ['] vblk-read  'blk-read !
    ['] vblk-write 'blk-write !
    d# -1 blk-cur !
;
