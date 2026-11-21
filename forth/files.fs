\ files.fs - what the disks have, by its label: a source that is
\ read, and the names that are a file's own

\ A machine may have several disks, and each has an index in its
\ first block (doc/disk.md).  When the machine starts each disk
\ is asked for that block (find-disks, which boot runs), and
\ indexed keeps which of them had an index there: bit d for disk
\ d.  A disk that is not there reads as nothing (MODEL.md), and
\ one whose driver throws is taken not to be there.  Only a
\ disk's first block is looked in yet, not the index blocks it
\ may go on to.
create indexed 0 ,

: index? ( d -- f ) \ is disk d's first block an index?
    DISK-SHIFT lshift ['] block catch if drop false exit then
    ENTRY-TYPE + uw@ TYPE-INDEX =
;

\ A machine whose disks after the first are reached another way
\ than its ROM's driver has says so in a word called disks,
\ which is run first if there is one (virt-disk.fs).
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
    indexed @ disks-there !     \ which the bar has a light for
;

\ An entry is said by a number that counts through the disks,
\ ENTRIES to a disk, disk 0's first.  So going through the
\ numbers in order is looking on disk 0 before disk 1, and that
\ before disk 2: a label that an earlier disk has is found
\ there, whatever a later disk has under it.
: entry@ ( n -- e ) \ entry n, as a byte's address
    ENTRIES u/mod DISK-SHIFT lshift block
    swap ENTRY-BYTES * + ;

: indexed? ( n -- f ) \ has the disk entry n is of an index?
    ENTRIES u/mod nip  indexed @ swap rshift d# 1 and ;

: same-chars? ( a1 a2 u -- f )
    \ are the u characters from each the same?
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

\ the entry that has that label, from entry n on, or nothing: a
\ disk's first entry is the disk's own, and a disk with no index
\ is passed over
: find-label ( c-addr u n -- n'|0 )
    dup ENTRIES DRIVES * = if drop 2drop d# 0 exit then
    dup ENTRIES 1- and 0= if
        dup indexed? if 1+ else ENTRIES + then
        find-label exit
    then
    >r  2dup r@ entry@ label? if 2drop r> exit then
    r> 1+ find-label
;

\ of entry n: its first block, whose number says its disk too,
\ and how many blocks it is
: its-blocks ( n -- first blocks )
    dup entry@ >r
    ENTRIES u/mod nip DISK-SHIFT lshift  r@ ENTRY-FIRST + uw@ +
    r> ENTRY-BLOCKS + uw@
;

\ The blocks a label has, on the first disk that has it: its
\ first block and how many, or no blocks if no disk has it.  The
\ label is not to be in the block's buffer, which an index is
\ read into.
header labelled
: labelled ( c-addr u -- first blocks )
    d# 0 find-label dup if its-blocks exit then d# 0
;

\ ---- a file's own names

\ A word of a file is found by its name until the file has been
\ read, and after that only if a header came before it (header
\ name): what has none is the file's own.  A file's own names
\ are not the dictionary's.  While a file is read the kernel
\ lays headers in memory had for them from above the image, a
\ piece at a time (hdp and hdl, nuc.fs), and one that `header`
\ makes is laid in the dictionary, and stays.  When the file is
\ done its own are taken out of the chain and the memory is
\ given back.  With no file being read, at the prompt or in a
\ block that is loaded, every header is the dictionary's.

$400 constant OWN-CELLS  \ a piece of memory for names,
$100 constant OWN-LEAST  \ and the least a block is begun with

\ while a file is read: the newest piece, and the dictionary's
\ own hdp and hdl
create own 0 , 0 , 0 ,

\ The headers that stay are not found while their file is read:
\ a name is then what it is in the file, as it is to cross.fs,
\ where a header is a name for the prompt and no word.  (One was
\ found as soon as it was laid, and `header FACE : code-face
\ FACE ;` compiled a jump to itself where FACE is a macro of
\ the file.)  They are a chain of their own, the newest here,
\ and are put into the dictionary when the file is done.
create stays 0 ,

\ another piece: its first cell is the piece before it, and its
\ second, once it is left for another, where its names end; the
\ names go after them
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

\ A file is begun: where the data must stop, which the text of
\ the file's macros brings down (cross-words.fs); the newest
\ header there is; the headers that stay of the file this one
\ is read from; whether it is the first being read; and the
\ cell its own names start at.
: own-names ( -- limit newest stays first? base )
    dpl @  forth @  stays @  d# 0 stays !
    own @ 0=  dup if
        hdp @ own 1+ !  hdl @ own CELL2 + !  own-piece
    then
    hdp @
;

\ the file's headers that stay, put before the newest header
\ there is, the oldest of them the nearest to it
: stays-in ( -- )
    stays @ ?dup 0= if exit then            ( newest )
    dup begin dup @ ?dup while nip repeat   ( newest oldest )
    forth @ swap !  forth !
;

\ ---- a file's own names, kept for the decompiler

\ They are gone from the dictionary when the file is done, and
\ the decompiler shows a word by its name.  So where a disk has
\ blocks kept for them, which an entry of its index says
\ (own-names: a disk a machine boots from that is its desktop's
\ too, mkdisk), each is written there when its file is done, as
\ a label is in the tables of names (see.fs): a cell with its
\ top bit set, which is 1 more if the word is immediate, the
\ word's place, and its name.  A block of them
\ is how many it has and then those, and none is across two
\ blocks.  For each block the least place and the most are kept
\ (own-ix), so that one block is read to find most.  They are
\ written through the block's buffer: what was being loaded is
\ asked for again when a word of it is next read (blocks.fs).

\ the first block kept for them, or nothing; how many have
\ records; and how many there are
header own-kept create own-kept 0 , 0 , 0 ,
create own-fill 0 ,     \ the cells that are used of the last
high
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

\ a header, kept as a record if there is room for it
: own-rec ( hdr -- )
    dup name-cells CELL2 +                  ( hdr n )
    dup own-fill @ + BLKCELLS u> if
        own-more 0= if 2drop exit then
    then
    >r  dup own-buf own-fill @ +            ( hdr hdr to )
    tuck r@ cells-move                      ( hdr to )
    over 1+ @ TOPBIT rshift  d# 1 TOPBIT lshift or  over !
    swap 1+ @ d# 1 lshift d# 1 rshift       ( to xt )
    tuck swap 1+ !                          ( xt )
    dup own-ix-at @ min own-ix-at !
    own-ix-at 1+ @ max own-ix-at 1+ !
    r> own-fill +!  d# 1 own-buf +!  update
;

: keep-run ( a e -- ) \ the headers from a up to e
    begin 2dup u< while
        over own-rec  swap dup name-cells + CELL2 + swap
    repeat
    2drop
;

: pieces ( -- n ) \ how many there are
    d# 0 own @ begin dup while @ swap 1+ swap repeat drop ;
: piece# ( n -- piece ) \ the piece n before the newest
    own @ swap negate begin dup while swap @ swap 1+ repeat
    drop ;

\ the headers of every piece, the oldest piece's first, so that
\ of two of a name the newer is the later
: own-keep ( -- )
    hdp @ own @ 1+ !
    pieces negate
    begin dup while
        dup invert piece#  dup CELL2 + swap 1+ @ keep-run
        1+
    repeat
    drop
;

\ The names of the file that is done, and of the files it read,
\ are kept; then the dictionary's hdp and hdl again, and the
\ pieces given back, the newest first.
: own-free ( -- )
    own-where if own-keep then
    own 1+ @ hdp !  own CELL2 + @ hdl !
    begin own @ ?dup while
        dup @ own !  OWN-CELLS hgive
    repeat
;

\ The file is done: each header newer than `newest` that is its
\ own, which is one laid at base or after, is taken out; those
\ of its headers that stay are put in; and the data may go as
\ far as it might before.
: own-done ( limit newest stays first? base -- )
    >r rot forth  ( limit stays first? newest a ) ( R: base )
    begin 2dup @ xor while
        dup @ r@ u< if @ else dup @ @ over ! then
    repeat
    2drop rdrop  d# 0 lastword !
    stays-in
    if own-free then
    stays !  dpl !
;

\ The definition after this is found by this name once the file
\ is done, and the header stays.  It names the code that is
\ compiled next, as cross.fs has it, whatever the definition is
\ called.
header header
: name-stays ( "name" -- )
    own @ 0= if parse-name 2drop exit then
    hdp @ hdl @  own 1+ @ hdp !  own CELL2 + @ hdl !
    mkheader  lastword @ 1+ stay-xt !
    stays @ lastword @ !  lastword @ stays !  d# 0 lastword !
    hdp @ own 1+ !  hdl !  hdp !
;

\ ---- a file, read

\ A machine that compiles its system from its disk says what it
\ is at, and the screen shows it (console.fs).  A file has a
\ line: its label, how many blocks it is, and a dot as each of
\ them starts.  A file read from within another starts its line
\ where that one's had got to: so that one's line is ended
\ first, and the dots of its later blocks are on lines of their
\ own.  A line is ended only if there is anything on it, which
\ line-said says: so a file that ends where another starts
\ leaves no line of nothing between them.
create line-said 0 ,

: line-ends ( -- ) line-said @ if cr then  d# 0 line-said ! ;

\ a file's blocks, loaded in turn, a dot as each starts
: file-blocks ( first last -- )
    1+ swap                                 ( end blk )
    begin
        2dup xor
    while
        [char] . emit  true line-said !  own-room
        dup >r swap >r  load  r> r> 1+
    repeat
    2drop
;

\ The label is read from the block's buffer, which block 0 is
\ then read into: so it is copied first, to here, where nothing
\ goes while it is looked for.  The data is brought to a whole
\ cell first: here is the cell the data has got to, and if text
\ was the last thing laid that cell is part of it.  (The
\ viewer's label for its picture lost its end to the name of the
\ file included next.)  Whatever the file throws, its own names
\ are taken out before it is thrown on.

header include
: include-label ( "label" -- )
    line-ends  align
    parse-name  2dup type space  true line-said !
    here >c swap  dup >r  cmove
    here >c r>  d# 0 find-label
    dup 0= d# 38 -throw  \ as a file that is not there
    dup entry@ ENTRY-TYPE + uw@ TYPE-SOURCE <> d# 38 -throw
    its-blocks  dup .  over + 1-
    own-names >r >r >r >r >r
    ['] file-blocks catch  dup if nip nip then
    r> r> r> r> r> own-done
    line-ends  throw
;
