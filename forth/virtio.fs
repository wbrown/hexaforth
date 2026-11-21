\ virtio.fs - what the virtio-mmio drivers share

\ Version 2's registers, a queue a device, polled.

$0a000000 constant VIRTIO0  \ the first of 32 slots, $200 apart

header vdev \ machine address of the registers being worked on
    create vdev  0 ,
create vkind 0 ,

header vreg@ : vreg@ ( off -- x ) vdev @ + reg32@ ;
header vreg! : vreg! ( x off -- ) vdev @ + reg32! ;

\ the next device of this kind, from slot
header vprobe : vprobe ( id slot -- slot true | false )
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

header vhello : vhello ( -- ) \ reset; VERSION_1 and no more
    d# 0 h# 70 vreg!
    d# 1 vstatus+  d# 2 vstatus+        \ acknowledge, driver
    d# 1 h# 24 vreg!  d# 1 h# 20 vreg!  \ features 32-63
    d# 0 h# 24 vreg!  d# 0 h# 20 vreg!  \ features 0-31: none
    d# 8 vstatus+                       \ features ok
    h# 70 vreg@ d# 8 and 0= d# 37 -throw
;

: vroom ( u -- a ) \ u bytes, all 0
    dup halloc tuck swap CELLB 1- + c>  d# 0 -rot cells-fill ;

\ queue 0, with n descriptors
header vqueue : vqueue ( n -- desc avail used )
    d# 0 h# 30 vreg!
    dup h# 38 vreg!
    dup d# 4 lshift vroom
    over 2* d# 6 + vroom
    rot d# 3 lshift d# 6 + vroom
    >r >r
    dup    >phys dup h# 80 vreg!  d# 32 rshift h# 84 vreg!
    r> dup >phys dup h# 90 vreg!  d# 32 rshift h# 94 vreg!
    r> dup >phys dup h# a0 vreg!  d# 32 rshift h# a4 vreg!
    d# 1 h# 44 vreg!                        \ queue ready
;

header vgo : vgo ( -- ) d# 4 vstatus+ ;  \ driver ok
