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

\ What reads and writes a block is in the kernel's page, where a
\ call from any page is one instruction (nearby, cross.fs):
\ everything that reads a disk comes here.  What says where an
\ error was, and what compiles a definition again, are run
\ seldom, and are on this file's own page.
nearby
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
nearby-end

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

: said-line ( blk# -- ) \ the line and the word, and that line
    block dup >in @ the-word                ( a c-addr u )
    rot >r  over r@ - LINE-SHIFT rshift
    ( c-addr u line ) ( R: a )
    say" line " dup .  >r
    say" at " type cr
    r> LINE-SHIFT lshift r> + LINE-CHARS type cr
;

\ What was thrown in the block being read, and where.  A word of
\ the block may have been reading text of its own when it was
\ thrown (evaluate, as a macro does): blk is then nothing, and
\ what is said is the word of that text, and the text.  (It was
\ said as block 0's, and a line of the disk's index shown.)
: said-where ( n blk# -- )
    base @ >r decimal
    swap say" error " .  say" in block " dup .
    blk @ if said-line else
        drop  source >in @ min the-word  say" at " type
        say"  of " source type cr
    then
    r> base !
;

\ A definition is all in one page of code, and the kernel begins
\ none where less than a kilobyte of its page is left, since
\ what is typed has no source to go back to (newpage?, nuc.fs):
\ the end of most pages was left empty for that.  A block has
\ its source to hand.  While one is read, a definition may begin
\ nearly at a page's end (cpr), and one that runs off its page
\ (code, throws) is compiled again on the next, from where it
\ began: its header and the data it laid are taken back, the
\ stack is as deep as it was, and a header that names it
\ (files.fs) is told where it starts now.

\ where a definition may begin: >in, the cell headers go in, the
\ data pointer, how deep the stack is, and the base numbers are
\ read in, which a word that compiles one changes while it does
create began 0 , 0 , 0 , 0 , 0 ,
\ the cell of a header that stays which says where the
\ definition after it starts (files.fs), or nothing
header stay-xt create stay-xt 0 ,

\ Was it a definition running off its page?  One that began
\ where its page does is longer than a page, and is not.
: ran-off? ( n -- f )
    d# -8 =  state @ 0<> and
    cp @ 1+ h# fff and 0= and
    thisxt @ h# fff and 0<> and
;

: begin-again ( blk# -- ) \ the definition, on the next page
    dup blk !  d# -1 sourceid !  block BLOCK-CHARS source!
    began @ >in !  began 1+ @ hdp !  began CELL2 + @ dp !
    begin depth began c# 3 + @ > while drop repeat
    began c# 4 + @ base !  d# 0 state !
    thisxt @  newpage?
    stay-xt @ ?dup if
        2dup @ = if cp @ swap ! else drop then
    then
    drop
;

\ What was thrown while a word of the block was read: a
\ definition that ran off its page is begun again, if what the
\ stack had when it began is still there, and anything else is
\ thrown on.
: thrown ( x x n blk# -- )
    >r >r 2drop r>
    dup ran-off? 0= if throw then
    depth 1- began c# 3 + @ < if throw then
    drop  r> begin-again
;

nearby
\ Is nothing being defined?  A definition's header is the newest
\ in the dictionary only once the definition is ended, and until
\ then it is not: so what is said in brackets in a definition
\ is not where one may begin.
: begins? ( -- f )
    state @ if false exit then
    lastword @ dup 0= swap forth @ = or
;
: begins ( -- ) \ a definition may begin here
    >in @ began !  hdp @ began 1+ !  dp @ began CELL2 + !
    depth began c# 3 + !  base @ began c# 4 + !
;

: reading ( -- ) \ interpret the block that blk says
    blk @ >r
    begin
        blk @ block drop
        begins? if begins then
        parse-name dup
    while
        ['] interpret1 catch ?dup if r@ thrown then
    repeat
    2drop rdrop
;

header load
: load ( blk# -- ) \ interpret a block of source
    blk @ >r  >in @ >r  source >r >r  source-id >r  cpr @ >r
    dup >r  dup blk !  d# -1 sourceid !  h# ff0 cpr !
    block BLOCK-CHARS source!
    ['] reading catch
    \ anything but nothing or -1
    dup 1+ d# 1 u> if r@ said-where d# -1 then  rdrop
    r> cpr !
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
nearby-end

\ A source of the disk's by its label (include), and the names
\ that are a file's own (header), are a file of their own:
\ files.fs, which is built on load.

\ Whoever reads more of the disk than a block at a time reads
\ whole blocks, by their numbers, to where they are wanted
\ (blk-read) or through the buffer (cblock): the picture, the
\ tables of names and HexaPaint's canvas do.  There was a word
\ here that copied any run of bytes of the device to the address
\ of a byte; nothing called it once they did, and it is gone.
