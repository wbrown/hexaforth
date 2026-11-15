\ blocks.fs - the block device interface and classic block words

\ A block is 1024 bytes.  A driver stores its routines here; the buffer it
\ is handed is the address of a cell.

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
: block ( blk# -- a ) \ address of the first byte of a buffer holding the block
    blk-buf @ 0= if d# 1024 halloc blk-buf ! then
    dup blk-cur @ = if drop blk-buf @ >c exit then
    flush
    d# -1 blk-cur !
    blk-buf @ over blk-read
    blk-cur !
    blk-buf @ >c
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

create dr-dest 0 ,
create dr-len  0 ,
create dr-off  0 ,

: lined? ( -- f ) \ is the next of it a whole block, to go where a cell starts?
    dr-off @ h# 3ff and  dr-dest @ CELLB 1- and  or 0=
    dr-len @ d# 1023 > and
    dr-off @ d# 10 rshift blk-cur @ <> and
;

header disk-read
: disk-read ( dest u offset -- ) \ dest is the address of a byte
    dr-off ! dr-len ! dr-dest !
    begin
        dr-len @
    while
        lined? if
            dr-dest @ c>  dr-off @ d# 10 rshift  blk-read
            d# 1024
        else
            dr-off @ d# 10 rshift block
            dr-off @ h# 3ff and +                   ( src )
            d# 1024 dr-off @ h# 3ff and -
            dr-len @ min                            ( src n )
            dup >r dr-dest @ swap cmove r>
        then                                        ( n )
        dup dr-dest +!
        dup dr-off +!
        negate dr-len +!
    repeat
;
