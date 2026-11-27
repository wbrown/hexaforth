\ virtio-input.fs - keyboards and pointers as virtio
\ input devices

\ An event is 8 bytes: type and code of 16 bits and a value of
\ 32, in Linux's input codes.

header 'event create 'event 0 ,       \ ( type code value -- )
create idevs  0 , 0 , 0 , 0 ,
header nidev create nidev  0 ,     \ how many devices there are

\ A device's record: its first device cell, descriptors,
\ available ring, used ring, how far the used ring is read, and
\ 64 buffers.

: ring ( n a -- ia ) \ 16-bit field n of the ring at cell a
    CELLB 2/ * + ;

: idev-buffer ( i rec -- i ) \ buffer i: described, and offered
    >r
    dup cells r@ c# 5 + @ +                 ( i buffer )
    over 2* cells r@ CELL + @ +             ( i buffer desc )
    tuck cell-addr!
    d# 2 d# 32 lshift d# 8 or swap CELL + ! \ 8 bytes, written
    dup dup d# 2 + r@ CELL2 + @ ring i!
    rdrop
;

: idev-init ( -- ) \ vdev is at an input device
    vhello
    d# 64 vqueue                            ( desc avail used )
    d# 48 halloc >r
    r@ c# 3 + !  r@ CELL2 + !  r@ CELL + !
    vdev @ r@ !
    d# 0 r@ c# 4 + !
    d# 512 halloc r@ c# 5 + !
    d# 0 begin dup d# 64 < while r@ idev-buffer 1+ repeat drop
    d# 64 d# 1 r@ CELL2 + @ ring i!
    vgo
    d# 0 d# 10 vcell!
    r> nidev @ cells idevs + !
    d# 1 nidev +!
;

header input-init
: input-init ( -- ) \ every input device there is, up to four
    d# 0 nidev !
    d# 0
    begin
        d# 18 swap vprobe  \ device id 18: input
    while
        idev-init
        1+
        nidev @ d# 4 = if drop exit then
    repeat
;

: idev-take ( dev -- id type code value ) \ the event used next
    >r
    r@ c# 4 + @ d# 63 and
    r@ c# 3 + @ + @ d# 32 rshift             ( id )
    dup cells r> c# 5 + @ + @                ( id event )
    dup h# ffff and
    over d# 16 rshift h# ffff and
    rot d# 32 rshift
;

: idev-give ( id dev -- ) \ its buffer given back
    >r
    d# 1 r@ CELL2 + @ ring i@ d# 63 and
    d# 2 + r@ CELL2 + @ ring i!
    d# 1 r@ CELL2 + @ ring dup i@ 1+ swap i!
    r@ c# 4 + @ 1+ h# ffff and r@ c# 4 + !
    r> @ vdev !
    d# 0 d# 10 vcell!
;

: idev-poll ( dev -- )
    >r
    begin
        d# 1 r@ c# 3 + @ ring i@  r@ c# 4 + @ <>
    while
        r@ idev-take 'event @ execute
        r@ idev-give
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
