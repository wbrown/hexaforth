\ virtio.fs - what the virtio-mmio drivers share
\
\ QEMU `virt` has 32 virtio-mmio slots from 0x0a000000, 0x200 apart.  These
\ drivers use the modern (version 2) register layout, one queue each, and
\ poll instead of taking interrupts.

$0a000000 constant VIRTIO0

header vdev create vdev  0 ,        \ machine address of the registers being worked on
create vkind 0 ,

header vreg@ : vreg@ ( off -- x ) vdev @ + reg32@ ;
header vreg! : vreg! ( x off -- ) vdev @ + reg32! ;

header vprobe : vprobe ( id slot -- slot true | false ) \ next device of this kind, from slot
    swap vkind !
    begin
        dup d# 32 <
    while
        dup d# 9 lshift VIRTIO0 + vdev !
        d# 0 vreg@ h# 74726976 =            \ magic "virt"
        d# 4 vreg@ d# 2 = and               \ version
        d# 8 vreg@ vkind @ = and            \ device id
        if true exit then
        1+
    repeat
    drop false
;

: vstatus+ ( bit -- ) h# 70 vreg@ or h# 70 vreg! ;

header vhello : vhello ( -- ) \ reset, then negotiate VERSION_1 and nothing else
    d# 0 h# 70 vreg!
    d# 1 vstatus+  d# 2 vstatus+            \ acknowledge, driver
    d# 1 h# 24 vreg!  d# 1 h# 20 vreg!      \ features 32-63: VERSION_1
    d# 0 h# 24 vreg!  d# 0 h# 20 vreg!      \ features 0-31: none
    d# 8 vstatus+                           \ features ok
    h# 70 vreg@ d# 8 and 0= d# 37 -throw
;

header vqueue : vqueue ( n -- desc avail used ) \ set up queue 0 with n descriptors
    d# 0 h# 30 vreg!
    dup h# 38 vreg!
    dup d# 4 lshift halloc
    over 2* d# 6 + halloc
    rot d# 3 lshift d# 6 + halloc
    >r >r
    dup    >phys dup h# 80 vreg!  d# 32 rshift h# 84 vreg!
    r> dup >phys dup h# 90 vreg!  d# 32 rshift h# 94 vreg!
    r> dup >phys dup h# a0 vreg!  d# 32 rshift h# a4 vreg!
    d# 1 h# 44 vreg!                        \ queue ready
;

header vgo : vgo ( -- ) d# 4 vstatus+ ;                \ driver ok
