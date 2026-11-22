\ files.fs - what the disks have, by its label: a source that is
\ read, and the names that are a file's own

create indexed 0 ,  \ bit d: disk d's first block is an index

: index? ( d -- f )
    DISK-SHIFT lshift ['] block catch if drop false exit then
    ENTRY-TYPE + uw@ TYPE-INDEX =
;

\ the word a machine has if its other disks are reached another
\ way than its ROM's driver's: it is run first
create "disks"
    'd' c, 'i' c, 's' c, 'k' c, 's' c,

header find-disks
: find-disks ( -- )
    "disks" >c d# 5 by-name
    d# 0 indexed !
    d# 0 begin
        dup DRIVES xor
    while
        dup index? if
            d# 1 over lshift indexed @ or indexed !
        then
        1+
    repeat
    drop
    indexed @ disks-there !
;

\ An entry's number counts through the disks, ENTRIES to a disk.
: entry@ ( n -- e ) \ as a byte's address
    ENTRIES u/mod DISK-SHIFT lshift block
    swap ENTRY-BYTES * + ;

: indexed? ( n -- f ) \ has the disk entry n is of an index?
    ENTRIES u/mod nip  indexed @ swap rshift d# 1 and ;

: same-chars? ( a1 a2 u -- f )
    >r over - r> swap >r bounds  ( end a1 ) ( R: a2 less a1 )
    begin
        2dup xor
    while
        dup c@ over r@ + c@ <> if 2drop rdrop false exit then
        1+
    repeat
    2drop rdrop true
;

: label? ( c-addr u e -- f ) \ has the entry at e that label?
    2dup c@ = if 1+ swap same-chars? exit then
    drop 2drop false
;

\ from entry n on; a disk's first entry is the disk's own
: find-label ( c-addr u n -- n'|0 )
    dup ENTRIES DRIVES * = if drop 2drop d# 0 exit then
    dup ENTRIES 1- and 0= if
        dup indexed? if 1+ else ENTRIES + then
        find-label exit
    then
    >r  2dup r@ entry@ label? if 2drop r> exit then
    r> 1+ find-label
;

: its-blocks ( n -- first blocks ) \ of entry n
    dup entry@ >r
    ENTRIES u/mod nip DISK-SHIFT lshift  r@ ENTRY-FIRST + uw@ +
    r> ENTRY-BLOCKS + uw@
;

\ On the first disk that has the label; no blocks if none has.
\ The label is not to be in the block's buffer.
header labelled
: labelled ( c-addr u -- first blocks )
    d# 0 find-label dup if its-blocks exit then d# 0
;

\ ---- a file's own names

\ While a file is read, headers are laid in pieces of memory
\ above the image; when it is done they are out of
\ the dictionary.

$400 constant OWN-CELLS  \ a piece of memory for names,
$100 constant OWN-LEAST  \ and the least a block is begun with

\ the newest piece, the dictionary's own hdp and hdl, and where
\ the scratch was (placement.fs)
create own 0 , 0 , 0 , 0 ,

\ the newest of the headers that stay: a chain of their own
\ until the file is done
create stays 0 ,

\ A piece: the piece before it, where its names end, the names.
: own-piece ( -- )
    own @ ?dup if hdp @ swap 1+ ! then
    OWN-CELLS >c halloc
    own @ over !  dup own !
    dup CELL2 + hdp !  OWN-CELLS + hdl !
;

: own-room ( -- ) \ before a block: room for the names it makes
    own @ 0= if exit then
    hdl @ hdp @ - OWN-LEAST u< if own-piece then
;

\ a file is begun: dpl, forth's header, the stays of the file
\ this is read from, and where its own names start
: own-names ( -- limit head stays first? base )
    dpl @  forth @  stays @  d# 0 stays !
    own @ 0=  dup if
        hdp @ own 1+ !  hdl @ own CELL2 + !  own-piece
        scratch @ own c# 3 + !
    then
    hdp @
;

: stays-in ( -- ) \ the headers that stay, into the dictionary
    stays @ ?dup 0= if exit then            ( newest )
    dup begin dup @ ?dup while nip repeat   ( newest oldest )
    forth @ swap !  forth !
;

\ ---- a file's own names, kept for the decompiler

\ In the blocks the disk's entry own-names has.  A record is a
\ cell with its top bit set, 1 more if the word is immediate;
\ the word's place; and its name.  A block is how many records
\ it has and then those.
\ A word of the scratch that only pushes a number has 2 more in
\ that cell, and the number where the place would be.  Other
\ code of the scratch has no record.

\ the first block, or 0; how many have records; how many in all
header own-kept create own-kept 0 , 0 , 0 ,
create own-fill 0 ,     \ the cells that are used of the last
high
\ the least place and the most of each block
header own-ix create own-ix
    meta disk-own-blocks 2* cell-bytes * target allot
unhigh
create "own-names"
    'o' c, 'w' c, 'n' c, '-' c, 'n' c, 'a' c, 'm' c, 'e' c,
    's' c,

: own-where ( -- f ) \ is there anywhere to keep them?
    own-kept @ if true exit then
    "own-names" >c d# 9 labelled  dup 0= if nip exit then
    OWN-BLKS min own-kept CELL2 + !  own-kept !
    BLKCELLS own-fill !  true
;

: own-buf ( -- a ) \ the block that records are going in
    own-kept @ own-kept 1+ @ + 1- cblock ;
: own-ix-at ( -- a ) own-kept 1+ @ 1- 2* own-ix + ;

: own-more ( -- f ) \ another block, if there is one
    own-kept 1+ @ own-kept CELL2 + @ u< 0= if false exit then
    d# 1 own-kept 1+ +!
    d# 0 own-buf BLKCELLS cells-fill  update
    d# 1 own-fill !
    d# -1 d# 1 rshift own-ix-at !  d# 0 own-ix-at 1+ !
    true
;

: name-cells ( hdr -- n ) \ the cells of its name
    CELL2 +  d# 0
    begin 1+ swap dup @ 0< >r 1+ swap r> until
    nip
;

\ the number, if the code at xt only pushes one and returns
header pushes
: pushes ( xt -- n true | false )
    dup i@ dup h# 8000 lit? 0= if 2drop false exit then
    lval swap                               ( n xt )
    begin
        1+ dup i@ dup h# c000 lit?          ( n xt w f )
    while
        lval rot + swap
    repeat
    nip h# 7403 = dup 0= if nip then
;

\ what a header's record has where the place goes, and 2 if
\ that is a number
: rec-key ( hdr -- key kind true | false )
    1+ @ d# 1 lshift d# 1 rshift
    dup SCRATCH0 u< if d# 0 true exit then
    pushes if d# 2 true exit then  false
;

: own-rec ( hdr -- ) \ kept as a record, if there is room
    dup rec-key 0= if drop exit then  >r >r ( R: kind key )
    dup name-cells CELL2 +                  ( hdr n )
    dup own-fill @ + BLKCELLS u> if
        own-more 0= if 2drop rdrop rdrop exit then
    then
    >r  dup own-buf own-fill @ +            ( hdr hdr to )
    tuck r@ cells-move  r> own-fill +!      ( hdr to )
    swap 1+ @ TOPBIT rshift  d# 1 TOPBIT lshift or
    r> swap r@ or                           ( to key first )
    3rd !  dup rot 1+ !                     ( key ) ( R: kind )
    r> if drop else
        dup own-ix-at @ min own-ix-at !
        own-ix-at 1+ @ max own-ix-at 1+ !
    then
    d# 1 own-buf +!  update
;

: keep-run ( a e -- ) \ the headers from a up to e
    begin 2dup u< while
        over own-rec  swap dup name-cells + CELL2 + swap
    repeat
    2drop
;

: pieces ( -- n )
    d# 0 own @ begin dup while @ swap 1+ swap repeat drop ;
: piece# ( n -- piece ) \ the piece n before the newest
    own @ swap negate begin dup while swap @ swap 1+ repeat
    drop ;

: own-keep ( -- ) \ every piece's headers, the oldest first
    hdp @ own @ 1+ !
    pieces negate
    begin dup while
        dup invert piece#  dup CELL2 + swap 1+ @ keep-run
        1+
    repeat
    drop
;

: own-free ( -- ) \ the names kept, and the pieces given back
    own-where if own-keep then
    own 1+ @ hdp !  own CELL2 + @ hdl !
    begin own @ ?dup while
        dup @ own !  OWN-CELLS hgive
    repeat
;

\ the file is done: its own headers, those laid at base or
\ after, are out of the dictionary; and when the first file is,
\ the scratch is given back
: own-done ( limit head stays first? base -- )
    >r rot forth  ( limit stays first? head a ) ( R: base )
    begin 2dup @ xor while
        dup @ r@ u< if @ else dup @ @ over ! then
    repeat
    2drop rdrop  d# 0 lastword !
    stays-in
    if own-free  own c# 3 + @ scratch-back then
    stays !  dpl !
;

\ The code compiled next is found by this name once the file is
\ done.
header header
: name-stays ( "name" -- )
    own @ 0= if parse-name 2drop exit then
    hdp @ hdl @  own 1+ @ hdp !  own CELL2 + @ hdl !
    mkheader  lastword @ 1+ stay-xt !
    stays @ lastword @ !  lastword @ stays !  d# 0 lastword !
    hdp @ own 1+ !  hdl !  hdp !
;

\ ---- a file's own words that only push a number

\ Such a word is the number in place wherever it is said
\ (join.fs), so its code goes to the scratch: not that of a name
\ that stays, nor of a number join.fs does not lay in place.

: staying? ( -- f ) \ does a name that stays have the next code?
    stay-xt @ dup if @ cp @ = then ;

: aside? ( x -- f ) \ may the word that pushes x go there?
    own @ 0=  cp-kept @ or  staying? or
    if drop false exit then
    cw-meta  cp @ swap  tliteral exit,      ( xt )
    dup dup small dup 0< if 2drop false else number? then
    swap cp !  cw-target ;

: aside ( x xt -- ) \ xt defines the word that pushes x
    swap aside? dup >r if cw-meta then
    execute  r> if cw-target then ;

\ In a definition each compiles the kernel's word.
header-imm constant
:noname ( x "name" -- )
    state @ if ['] tconstant compile, exit then
    dup ['] tconstant aside ;
header-imm create
:noname ( "name" -- )
    state @ if ['] tcreate compile, exit then
    align here ['] tcreate aside ;
header-imm variable
:noname ( "name" -- )
    state @ if ['] variable compile, exit then
    align here ['] variable aside ;

\ ---- a file, read

create line-said 0 ,    \ true while anything is on the line

: line-ends ( -- ) line-said @ if cr then  d# 0 line-said ! ;

: file-blocks ( first last -- ) \ loaded, a dot as each starts
    1+ swap                                 ( end blk )
    begin
        2dup xor
    while
        [char] . emit  true line-said !  own-room
        dup >r swap >r  load  r> r> 1+
    repeat
    2drop
;

header include
: include-label ( "label" -- )
    line-ends  align
    parse-name  2dup type space  true line-said !
    here >c swap  dup >r  cmove   \ out of the block's buffer
    here >c r>  d# 0 find-label
    dup 0= d# 38 -throw
    dup entry@ ENTRY-TYPE + uw@ TYPE-SOURCE <> d# 38 -throw
    its-blocks  dup .  over + 1-
    own-names >r >r >r >r >r
    ['] file-blocks catch  dup if nip nip then
    r> r> r> r> r> own-done
    cp @ SCRATCH0 u< 0= if drop d# -8 then  \ in the scratch
    line-ends  throw
;
