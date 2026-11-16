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

\ A block of source is read as one thing, all its characters, a word at a
\ time, and blk says which block it is: so a comment in brackets may end
\ lines after it began, and \ is what knows that a block has lines.  The
\ block is asked for again before each word, since the word before may
\ have put another in the buffer: it may have read the disk, or loaded a
\ block itself.  What was being read is what is read again when the block
\ is done, and so it is when something in the block throws, which is then
\ thrown on.  Nothing of this is on the data stack while the block is
\ read, which may leave there whatever it likes.
: reading ( -- ) \ interpret the block that blk says
    begin
        blk @ block drop
        parse-name dup
    while
        interpret1
    repeat
    2drop
;

header load
: load ( blk# -- ) \ interpret a block of source
    blk @ >r  >in @ >r  source >r >r  source-id >r
    dup blk !  d# -1 sourceid !
    block BLOCK-CHARS source!
    ['] reading catch
    r> sourceid !  r> r> source!  r> >in !  r> blk !
    throw
;

\ Whoever reads more of the disk than a block at a time reads whole blocks,
\ by their numbers, to where they are wanted (blk-read) or through the
\ buffer (cblock): the picture, the tables of names and HexaPaint's canvas
\ do.  There was a word here that copied any run of bytes of the device
\ to the address of a byte; nothing called it once they did, and it is
\ gone.
