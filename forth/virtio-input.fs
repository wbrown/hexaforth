\ virtio-input.fs - keyboards and pointers as virtio input devices
\
\ Every device delivers the same 8-byte events: type, code, value (16, 16
\ and 32 bits), using Linux input codes.  This driver hands each one to the
\ 'event vector and does not care which device it came from.

header 'event create 'event 0 ,       \ ( type code value -- )
create idevs  0 , 0 , 0 , 0 ,
header nidev create nidev  0 ,     \ how many devices there are

\ A device record: registers, descriptors, available ring, used ring, used
\ index consumed so far, and 64 8-byte event buffers.  A key press is four
\ events, and nothing is polled during a repaint, so a small queue drops
\ input.

: idev-init ( -- ) \ vdev is at an input device
    vhello
    d# 64 vqueue                            ( desc avail used )
    d# 48 halloc >r
    r@ c# 3 + !  r@ CELL2 + !  r@ CELL + !
    vdev @ r@ !
    d# 0 r@ c# 4 + !
    d# 512 halloc r@ c# 5 + !
    d# 0
    begin
        dup d# 64 <
    while
        dup cells r@ c# 5 + @ + >phys               ( i buffer )
        over 2* cells r@ CELL + @ +                 ( i buffer desc )
        tuck !
        d# 2 d# 32 lshift d# 8 or swap CELL + !     \ 8 bytes, device writes
        dup dup 2* r@ CELL2 + @ >c + d# 4 + w!      \ offer it
        1+
    repeat
    drop
    d# 64 r@ CELL2 + @ >c d# 2 + w!
    vgo
    d# 0 h# 50 vreg!
    r> nidev @ cells idevs + !
    d# 1 nidev +!
;

header input-init
: input-init ( -- ) \ take every input device there is, up to four
    d# 0 nidev !
    d# 0
    begin
        d# 18 swap vprobe                   \ device id 18: input
    while
        idev-init
        1+
        nidev @ d# 4 = if drop exit then
    repeat
;

: idev-poll ( dev -- )
    >r
    begin
        r@ c# 3 + @ >c d# 2 + uw@  r@ c# 4 + @ <>
    while
        r@ c# 4 + @ d# 63 and d# 3 lshift
        r@ c# 3 + @ >c + d# 4 + l@                  ( id )
        dup cells r@ c# 5 + @ + @                   ( id event )
        dup h# ffff and
        over d# 16 rshift h# ffff and
        rot d# 32 rshift                            ( id type code value )
        'event @ execute
        r@ CELL2 + @ >c d# 2 + uw@ d# 63 and 2*     \ give the buffer back
        r@ CELL2 + @ >c + d# 4 + w!
        r@ CELL2 + @ >c d# 2 + dup uw@ 1+ swap w!
        r@ c# 4 + @ 1+ h# ffff and r@ c# 4 + !
        r@ @ vdev !
        d# 0 h# 50 vreg!
    repeat
    rdrop
;

header input-poll
: input-poll ( -- )
    d# 0
    begin
        dup nidev @ <
    while
        dup cells idevs + @ idev-poll
        1+
    repeat
    drop
;
