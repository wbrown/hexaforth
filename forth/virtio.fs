\ virtio.fs - what the virtio-mmio drivers share, on a machine
\ with 64-bit cells

\ virtio-mmio version 2, one queue per device, polled.  A
\ device is 64 of this machine's device cells, read and written
\ whole (reg.fs); virtio-disk.fs says what is in each.

header vdev \ the first device cell of the device worked on
    create vdev  0 ,
create vkind 0 ,    \ the kind looked for, in a cell's high half

header vcell@ : vcell@ ( n -- x ) vdev @ + dev@ ;
header vcell! : vcell! ( x n -- ) vdev @ + dev! ;
: vaddr! ( a n -- ) vdev @ + dev-addr! ;

\ the next device of this kind, from slot
header vprobe : vprobe ( id slot -- slot true | false )
    swap d# 32 lshift vkind !
    begin
        dup d# 32 <
    while
        dup d# 6 lshift vdev !
        \ "virt" and version 2, and of that kind
        d# 0 vcell@ h# 274726976 =
        d# 1 vcell@ d# 32 lshift vkind @ = and
        if true exit then
        1+
    repeat
    drop false
;

: vstatus+ ( bit -- ) d# 14 vcell@ or d# 14 vcell! ;

header vhello : vhello ( -- ) \ reset; VERSION_1 and no more
    d# 0 d# 14 vcell!
    d# 1 vstatus+  d# 2 vstatus+        \ acknowledge, driver
    h# 100000000 dup d# 4 vcell!        \ features 32-63:
    1+ d# 4 vcell!                      \ bit 32
    d# 8 vstatus+                       \ features ok
    d# 14 vcell@ d# 8 and 0= d# 37 -throw
;

: vroom ( u -- a ) \ u bytes, all 0
    dup halloc tuck swap CELLB 1- + c>  d# 0 -rot cells-fill ;

\ queue 0, with n descriptors
header vqueue : vqueue ( n -- desc avail used )
    d# 0 d# 6 vcell!
    dup d# 7 vcell!
    dup d# 4 lshift vroom
    over 2* d# 6 + vroom
    rot d# 3 lshift d# 6 + vroom
    >r >r
    dup    d# 16 vaddr!
    r> dup d# 18 vaddr!
    r> dup d# 20 vaddr!
    h# 100000000 d# 8 vcell!                \ queue ready
;

header vgo : vgo ( -- ) d# 4 vstatus+ ;  \ driver ok
