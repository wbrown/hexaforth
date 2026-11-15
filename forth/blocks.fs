\ blocks.fs - the block device interface and classic block words

\ A block is 1024 bytes.  A driver stores its routines here; the buffer it
\ is handed is the address of a cell.

header 'blk-read create 'blk-read  0 ,   \ ( buf blk# -- )
header 'blk-write create 'blk-write 0 ,   \ ( buf blk# -- )

: blk-read  ( buf blk# -- ) 'blk-read  @ dup 0= d# 37 -throw execute ;
: blk-write ( buf blk# -- ) 'blk-write @ dup 0= d# 37 -throw execute ;

\ One block buffer.  `block` gives the address of its first byte, so it has
\ to be where the address of a byte can say: with cells of 16 bits that is
\ the lower half of memory, and the buffer is in the data and not in the
\ memory that halloc gives out, which is all above the display.

W32+? [if]
: blk-room ( -- a ) d# 1024 halloc ;
[else]
bulk
create blk-cells #1024 allot
unbulk
: blk-room ( -- a ) blk-cells ;
[then]

create blk-buf   0 ,    \ its address, once allocated
header blk-cur create blk-cur   -1 ,   \ the block it holds
create blk-dirty 0 ,

header flush
: flush ( -- ) \ write the buffer back if it was changed
    blk-dirty @ if
        blk-buf @ blk-cur @ blk-write
        d# 0 blk-dirty !
    then
;

header blk-need
: blk-need ( -- ) \ see that there is a buffer: for whoever must have it made before memory of its own
    blk-buf @ 0= if blk-room blk-buf ! then
;

header cblock
: cblock ( blk# -- a ) \ the cell that a buffer holding the block starts at
    blk-need
    dup blk-cur @ = if drop blk-buf @ exit then
    flush
    d# -1 blk-cur !
    blk-buf @ over blk-read
    blk-cur !
    blk-buf @
;

header block
: block ( blk# -- a ) \ address of the first byte of a buffer holding the block
    cblock >c
;

header update
: update ( -- ) true blk-dirty ! ;

header load
: load ( blk# -- ) \ interpret a block of source
    block d# 1024 tevaluate
;

\ Copy any byte range of the device into memory, across block boundaries.
\
\ A whole block that is wanted where a cell starts is not copied: the
\ driver is handed the place it is wanted, and the buffer is left holding
\ whatever block it held.  So something read a great deal at a time, as a
\ picture is, does not turn out the block that is being edited, or write
\ it back before it was asked to.  Only if the buffer holds that very
\ block, which may have been changed since it was read, is it taken from
\ there.

\
\ What is read goes up through the device from offset to where it ends,
\ and where it goes in memory is that plus how far apart the two are, which
\ is on the return stack: nothing is kept anywhere else, so it does not
\ matter who else is reading.

: room ( end offset -- end offset n ) \ how much of what is left is in the block the offset is in
    2dup -  over h# 3ff and d# 1024 swap -  min
;

: piece ( offset n dest -- ) \ n bytes of one block, to dest
    over d# 1024 =  over CELLB 1- and 0=  and       \ a whole block, to go where a cell starts,
    >r rot r>                                       ( n dest offset f )
    over d# 10 rshift blk-cur @ <> and if           \ and not the one in the buffer
        d# 10 rshift  swap c> swap  blk-read  drop
    else
        dup d# 10 rshift block  swap h# 3ff and +   ( n dest src )
        swap rot cmove
    then
;

header disk-read
: disk-read ( dest u offset -- ) \ dest is the address of a byte
    rot over - >r                                   ( u offset ) ( R: from an offset to where it goes )
    tuck + swap                                     ( end offset )
    begin
        2dupxor
    while
        room                                        ( end offset n )
        2dup over r@ + piece
        +
    repeat
    2drop rdrop
;
