\ blocks.fs - the block device interface and classic block words

\ A block's number says which disk it is of (doc/disk.md).

header blk-moved create blk-moved 0 ,  \ blocks read and written
header blk-wrote create blk-wrote 0 ,  \ true if the last was
header blk-disk  create blk-disk 0 ,   \ a write; and its disk
header disks-there                     \ bit d: disk d is there
    create disks-there 0 ,

nearby
: moved ( blk# write? -- )
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

\ One block buffer, where a byte has an address.

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
: blk-need ( -- ) \ see that there is a buffer
    blk-buf @ 0= if blk-room blk-buf ! then
;

header cblock
: cblock ( blk# -- a ) \ the block's buffer, as a cell's address
    blk-need
    dup blk-cur @ = if drop blk-buf @ exit then
    flush
    d# -1 blk-cur !
    blk-buf @ over blk-read
    blk-cur !
    blk-buf @
;

header block
: block ( blk# -- a ) \ and as a byte's
    cblock >c
;

header update
: update ( -- ) true blk-dirty ! ;
nearby-end

\ ---- a block read as source

\ from the end, back over the characters xt says yes to
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

: the-word ( a n -- c-addr u ) \ the last word of them
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

\ what was thrown, and where; blk is 0 if text was evaluated
: said-where ( n blk# -- )
    base @ >r decimal
    swap say" error " .  say" in block " dup .
    blk @ if said-line else
        drop  source >in @ min the-word  say" at " type
        say"  of " source type cr
    then
    r> base !
;

\ A definition of a block that runs off its page of code is
\ compiled again on the next, from where it began.

\ where one may begin: >in, hdp, dp, the stack's depth and base
create began 0 , 0 , 0 , 0 , 0 ,
\ the xt's cell of the header that stays for it (files.fs), or 0
header stay-xt create stay-xt 0 ,

: ran-off? ( n -- f ) \ and did not begin where its page does
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

\ begun again if it ran off its page and the stack is no
\ shallower than it was; if not, thrown on
: thrown ( x x n blk# -- )
    >r >r 2drop r>
    dup ran-off? 0= if throw then
    depth 1- began c# 3 + @ < if throw then
    drop  r> begin-again
;

nearby
: begins? ( -- f ) \ is nothing being defined?
    state @ if false exit then
    lastword @ dup 0= swap forth @ = or
;
: begins ( -- ) \ a definition may begin here
    >in @ began !  hdp @ began 1+ !  dp @ began CELL2 + !
    depth began c# 3 + !  base @ began c# 4 + !
;

: reading ( -- ) \ the block that blk says
    blk @ >r
    begin
        blk @ block drop  \ the last word may have read another
        begins? if begins then
        parse-name dup
    while
        ['] interpret1 catch ?dup if r@ thrown then
    repeat
    2drop rdrop
;

header load
: load ( blk# -- ) \ a throw is printed, and thrown on as -1
    blk @ >r  >in @ >r  source >r >r  source-id >r  cpr @ >r
    dup >r  dup blk !  d# -1 sourceid !  h# ff0 cpr !
    block BLOCK-CHARS source!
    ['] reading catch
    dup 1+ d# 1 u> if r@ said-where d# -1 then  rdrop
    r> cpr !
    r> sourceid !  r> r> source!  r> >in !  r> blk !
    throw
;

header thru
: thru ( first last -- )
    1+ swap                                 ( end blk )
    begin
        2dup xor
    while
        dup >r swap >r  load  r> r> 1+
    repeat
    2drop
;

header-imm .(
:noname ( "text<paren>" -- ) [char] ) parse type ;
nearby-end
