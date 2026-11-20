\ blocks.fs - the block device interface and classic block words

\ A block is 1024 bytes.  A driver stores its routines in
\ 'blk-read and 'blk-write, ( buf blk# -- ); the buffer it is
\ handed is the address of a cell.  The two are the kernel's,
\ and the ROM sets them to the disk it finds (nuc.fs).

\ Counted for the disk lights in the menu bar (wm.fs), which are
\ one for each disk there is.  A block's number says which disk
\ it is of (doc/disk.md): the light that shows a read or a write
\ is that disk's.
header blk-moved create blk-moved 0 ,  \ blocks read and written
                                       \ so far
header blk-wrote create blk-wrote 0 ,  \ true if the last was
                                       \ a write
header blk-disk  create blk-disk 0 ,   \ the disk the last
                                       \ was of
header disks-there                     \ the disks there are
    create disks-there 0 ,             \ known to be, bit d for
                                       \ disk d: whoever finds
                                       \ one says so here
                                       \ (files.fs)

: moved ( blk# write? -- ) \ a block was read or written
    blk-wrote !  DISK-SHIFT rshift blk-disk !
    d# 1 blk-moved +! ;

header blk-read
: blk-read  ( buf blk# -- )
    dup >r  'blk-read  @ dup 0= d# 37 -throw execute
    r> false moved ;
header blk-write
: blk-write ( buf blk# -- )
    dup >r  'blk-write @ dup 0= d# 37 -throw execute
    r> true moved ;

\ One block buffer.  `block` gives the address of its first
\ byte, so it has to be where the address of a byte can say:
\ with cells of 16 bits that is the lower half of memory, and
\ the buffer is in the data and not in the memory that halloc
\ gives out, which is all above the display.

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

\ see that there is a buffer: for whoever must have it made
\ before memory of its own
header blk-need
: blk-need ( -- )
    blk-buf @ 0= if blk-room blk-buf ! then
;

header cblock
: cblock ( blk# -- a )
    \ the cell that a buffer holding the block starts at
    blk-need
    dup blk-cur @ = if drop blk-buf @ exit then
    flush
    d# -1 blk-cur !
    blk-buf @ over blk-read
    blk-cur !
    blk-buf @
;

header block
: block ( blk# -- a )
    \ address of the first byte of a buffer holding the block
    cblock >c
;

header update
: update ( -- ) true blk-dirty ! ;

\ A block of source is read as one thing, all its characters, a
\ word at a time, and blk says which block it is: so a comment
\ in brackets may end lines after it began, and \ is what knows
\ that a block has lines.  The block is asked for again before
\ each word, since the word before may have put another in the
\ buffer: it may have read the disk, or loaded a block itself.
\ What was being read is what is read again when the block is
\ done, and so it is when something in the block throws, which
\ is then thrown on.  Nothing of this is on the data stack while
\ the block is read, which may leave there whatever it likes.

\ What was thrown is said where load catches it, before it puts
\ blk back: what it was, the block, the line and the word that
\ was being read, and that line of the block.  It is then thrown
\ on as -1, which says nothing more (throw, nuc.fs), and each
\ load it passes back through throws it on as it is.

\ From the end of the n characters from a, back over those xt
\ says yes to.
: back-over ( a n xt -- a n' )
    >r
    begin
        dup
    while
        2dup + 1- c@ r@ execute 0= if rdrop exit then
        1-
    repeat
    rdrop
;

: the-word ( a n -- c-addr u )
    \ the last word of the n characters from a
    ['] isspace? back-over  dup >r
    ['] isnotspace? back-over
    tuck +  swap r> swap -
;

: said-where ( n -- )
    \ what was thrown in the block being read, and where
    base @ >r decimal
    say" error " .  say" in block " blk @ .
    blk @ block dup >in @ the-word          ( a c-addr u )
    rot >r  over r@ - LINE-SHIFT rshift
    ( c-addr u line ) ( R: base a )
    say" line " dup .  >r
    say" at " type cr
    r> LINE-SHIFT lshift r> + LINE-CHARS type cr
    r> base !
;

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
    \ anything but nothing or -1
    dup 1+ d# 1 u> if said-where d# -1 then
    r> sourceid !  r> r> source!  r> >in !  r> blk !
    throw
;

header thru
: thru ( first last -- )
    \ the blocks from first to last, loaded in turn
    1+ swap                                 ( end blk )
    begin
        2dup xor
    while
        dup >r swap >r  load  r> r> 1+
    repeat
    2drop
;

\ What a block says as it is read: the text up to the
\ next bracket.
header-imm .(
:noname ( "text<paren>" -- ) [char] ) parse type ;

\ A source of the disk's by its label (include), and the names
\ that are a file's own (header), are a file of their own:
\ files.fs, which is built on load.

\ Whoever reads more of the disk than a block at a time reads
\ whole blocks, by their numbers, to where they are wanted
\ (blk-read) or through the buffer (cblock): the picture, the
\ tables of names and HexaPaint's canvas do.  There was a word
\ here that copied any run of bytes of the device to the address
\ of a byte; nothing called it once they did, and it is gone.
