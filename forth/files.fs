\ files.fs - a source of the disk's by its label, and the names
\ that are a file's own

\ Source of the disk's, by its label in block 0, the disk's
\ index (doc/disk.md): its blocks are loaded in turn.  Only
\ block 0 is looked in yet, not the index blocks a disk may go
\ on to.
: entry@ ( i -- e ) \ entry i, as a byte's address
    ENTRY-BYTES *  d# 0 block + ;

: same-chars? ( a1 a2 u -- f )
    \ are the u characters from each the same?
    >r over - r> swap >r bounds  ( end a1 ) ( R: a2 less a1 )
    begin
        2dupxor
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

: find-source ( c-addr u i -- e|0 )
    \ the source of that label, from entry i on
    dup ENTRIES = if drop 2drop d# 0 exit then
    >r  2dup r@ entry@ label?
    r@ entry@ ENTRY-TYPE + uw@ TYPE-SOURCE =  and
    if 2drop r> entry@ exit then
    r> 1+ find-source
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
        2dupxor
    while
        [char] . emit  true line-said !
        dup >r swap >r  load  r> r> 1+
    repeat
    2drop
;

\ The label is read from the block's buffer, which block 0 is
\ then read into: so it is copied first, to here, where nothing
\ goes while it is looked for.  It is said from where it was
\ read, before it is copied: the screen's console measures each
\ character it draws by putting it at here, so a label said
\ from here was gone after its first letter.  Whatever the file
\ throws, its own names are taken out before it is thrown on.
header include
: include-label ( "label" -- )
    line-ends
    parse-name  2dup type space  true line-said !
    here >c swap  dup >r  cmove
    here >c r>  d# 1 find-source
    dup 0= d# 38 -throw  \ as a file that is not there
    dup ENTRY-FIRST + uw@  swap ENTRY-BLOCKS + uw@
    dup .  over + 1-
    own-names >r >r
    ['] file-blocks catch  dup if nip nip then
    r> r> own-done
    line-ends  throw
;
