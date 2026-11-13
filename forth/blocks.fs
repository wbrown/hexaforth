\ blocks.fs - the block device interface and classic block words

\ A block is 1024 bytes.  A driver stores its routines here.

header 'blk-read create 'blk-read  0 ,   \ ( buf blk# -- )
header 'blk-write create 'blk-write 0 ,   \ ( buf blk# -- )

: blk-read  ( buf blk# -- ) 'blk-read  @ dup 0= d# 37 -throw execute ;
: blk-write ( buf blk# -- ) 'blk-write @ dup 0= d# 37 -throw execute ;

\ One block buffer.

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

header block
: block ( blk# -- a ) \ address of a buffer holding the block
    blk-buf @ 0= if d# 1024 halloc blk-buf ! then
    dup blk-cur @ = if drop blk-buf @ exit then
    flush
    d# -1 blk-cur !
    blk-buf @ over blk-read
    blk-cur !
    blk-buf @
;

header update
: update ( -- ) true blk-dirty ! ;

header load
: load ( blk# -- ) \ interpret a block of source
    block d# 1024 tevaluate
;

\ Copy any byte range of the device into memory, across block boundaries.

create dr-dest 0 ,
create dr-len  0 ,
create dr-off  0 ,

header disk-read
: disk-read ( dest u offset -- )
    dr-off ! dr-len ! dr-dest !
    begin
        dr-len @
    while
        dr-off @ d# 10 rshift block
        dr-off @ h# 3ff and +                       ( src )
        d# 1024 dr-off @ h# 3ff and -
        dr-len @ min                                ( src n )
        >r dr-dest @ r@ cmove
        r@ dr-dest +!
        r@ dr-off +!
        r> negate dr-len +!
    repeat
;
