\ virtio-blk.fs - block driver for a virtio block device

create vb-dev   0 ,     \ machine address of the device's registers
create vq-desc  0 ,     \ descriptor table, available ring, used ring
create vq-avail 0 ,
create vq-used  0 ,
create vq-seen  0 ,     \ used-ring index consumed so far
create vreq     0 , 0 , \ request header: type, then sector
create vstat    0 ,     \ status byte written by the device

: vdesc! ( a len flags next i -- ) \ fill descriptor i
    d# 4 lshift vq-desc @ + >r
    d# 48 lshift swap d# 32 lshift or or
    r@ d# 8 + !
    >phys r> !
;

: vblk-io ( buf blk# type -- ) \ type 0 reads, 1 writes
    vb-dev @ vdev !
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
    d# 2 d# 0 vprobe 0= d# 37 -throw drop   \ device id 2: block
    vdev @ vb-dev !
    vhello
    d# 8 vqueue vq-used ! vq-avail ! vq-desc !
    vgo
    d# 0 vq-seen !

    ['] vblk-read  'blk-read !
    ['] vblk-write 'blk-write !
    d# -1 blk-cur !
;
