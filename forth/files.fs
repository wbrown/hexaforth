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
\ name): what has none is the file's own.  While a file is read
\ the headers the kernel makes go in the upper half of the room
\ there is left for headers, and one that `header` makes goes
\ below them, and stays.  When the file is done its own are
\ taken out of the dictionary, and the room they had is free
\ again.  A file read from within another has its own after
\ that one's, and they go when it is done.  With no file being
\ read, at the prompt or in a block that is loaded, every
\ header stays.

\ (The kernel is as it was: its mkheader lays a header where hdp
\ says, and that is all that is changed here.  The names that
\ go are in no table for the decompiler either: it shows a
\ file's own words, compiled on the machine, as numbers.)

create kept 0 , 0 ,  \ while a file is read: where the next
                     \ header that stays goes, and where the
                     \ file's own start; nothing if none is

\ A file is begun: where its own names start, if it is read from
\ within another, and the newest header there is.
: own-names ( -- mark base )
    kept @ if hdp @ else
        hdp @ dup kept !
        HDR-LIMIT over - u2/ +  dup kept CELL + !  hdp !
        d# 0
    then
    forth @
;

\ The file is done: each header newer than base that is not one
\ that stays is taken out, and the room is as it was.
: own-done ( mark base -- )
    kept @ c> >r  forth                     ( mark base a )
    begin 2dup @ xor while
        dup @ r@ u< if @ else dup @ @ over ! then
    repeat
    2drop rdrop  d# 0 lastword !
    ?dup if hdp ! exit then  \ read from within another
    kept @ hdp !  d# 0 kept !
;

\ The definition after this is found by this name, and the
\ header stays.  It names the code that is compiled next, as
\ cross.fs has it, whatever the definition is called.
header header
: name-stays ( "name" -- )
    kept @ 0= if parse-name 2drop exit then
    kept @ d# 256 + kept CELL + @ u> d# 8 -throw
    hdp @ >r  kept @ hdp !
    mkheader attach
    hdp @ kept !  r> hdp !
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
        [char] . emit  true line-said !
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
    own-names >r >r
    ['] file-blocks catch  dup if nip nip then
    r> r> own-done
    line-ends  throw
;
