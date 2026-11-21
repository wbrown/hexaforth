\ cross-words.fs - the words of cross.fs that the system's
\ source says, for the nucleus to compile that source too

\ Each is a word of the nucleus named as cross.fs's own is.
\ cross.fs does not let a word of the program take the name of
\ one of its own, so the header is given that name and the word
\ itself another: a header names the definition after it,
\ whatever that is called here.

\ ---- numbers and text

\ The kernel's evaluate has a header and no name here (it is
\ :noname), so it is found by its header, as main finds boot.
create t-evaluate ," evaluate"

: base-number ( base "n" -- ) \ compiled, or left if not
    base @ >r  base !
    parse-name t-evaluate >c count by-name
    r> base !
;

\ and under the name the system's source has for it, since
\ evaluate is cross.fs's own there (term.fs)
header tevaluate : cw-tevaluate ( c-addr u -- ) tevaluate ;

header-imm d# : (d#) ( "n" -- ) d# 10 base-number ;
header-imm h# : (h#) ( "n" -- ) d# 16 base-number ;
\ the distance from a cell's address to the nth cell after it
header-imm c# : (c#) ( "n" -- ) d# 10 base-number ;

header ," : (,") ( "ccc<quote>" -- ) \ a counted string, here
    d# 34 parse s, ;

\ The kernel's literal, found as evaluate is.
create t-literal ," literal"

header char : cw-char ( "c" -- c ) parse-name drop c@ ;
header-imm [char] : cw-[char] ( "c" -- ) \ compiled
    parse-name drop c@  t-literal >c count by-name ;

\ ---- how a file is laid down

\ (header is in blocks.fs, with include: it names the definition
\ after it, and is what stays of a file's names.)

\ What is not variables is laid in a place of its own, where the
\ machine has one, as cross.fs lays it (bulk): the data pointer
\ and its limit change places with the bulk's (bdp and bdl,
\ nuc.fs).  What cross.fs lays above the display (high) is
\ bulk here: the data pointer counts bytes.  What was laid in
\ the place that is left is to have stopped where that place
\ does: past it are the block buffer, or the oldest headers.
: bulk-swap ( -- )
    bdp @ 0= if exit then
    dp @ dpl @ u> d# 8 -throw
    dp @ bdp @ dp ! bdp !  dpl @ bdl @ dpl ! bdl !
;
header bulk : cw-bulk ( -- ) bulk-swap ;
header unbulk : cw-unbulk ( -- ) bulk-swap ;
header high : cw-high ( -- ) bulk-swap ;
header unhigh : cw-unhigh ( -- ) bulk-swap ;

\ u characters at two places, the address of a byte each
: chars= ( a a' u -- f )
    negate
    begin dup while
        >r  over c@ over c@ <> if 2drop rdrop false exit then
        1+ swap 1+ swap  r> 1+
    repeat
    drop 2drop true
;

: is? ( a u cstr -- f ) \ the text the text of a counted string?
    >c count  rot over <> if 2drop drop false exit then  chars=
;

create t-if   ," [if]"
create t-else ," [else]"
create t-then ," [then]"

\ The words skipped by a conditional, up to its [else] or its
\ [then], as many deep as conditionals are in it.  A conditional
\ is in one block, so the end of what is read is its end too.
: skip-cond ( -- )
    d# 1
    begin
        parse-name dup
    while
        2dup t-if is? if 2drop 1+ else
        2dup t-else is? if 2drop dup d# 1 = if drop exit then
        else
        t-then is? if 1- ?dup 0= if exit then then
        then then
    repeat
    2drop drop
;

header-imm [if] : cond-if ( f -- ) 0= if skip-cond then ;
header-imm [else] : cond-else ( -- ) skip-cond ;
header-imm [then] : cond-then ( -- ) ;

\ ---- what cross.fs runs as it compiles

\ What is between meta and target cross.fs runs as it reads it,
\ and here it is run as it is read too: the two do nothing.
header meta : cw-meta ( -- ) ;
header target : cw-target ( -- ) ;
\ and the pages of code it lays what follows on: the nucleus
\ has its own (cp)
header code-page : cw-code-page ( n -- ) drop ;
header sys-page : cw-sys-page ( n -- ) drop ;
header big-page : cw-big-page ( n -- ) drop ;
header small-page : cw-small-page ( n -- ) drop ;
header small-first : cw-small-first ( -- ) ;
header low-page : cw-low-page ( -- ) ;
header low-page-end : cw-low-page-end ( -- ) ;

\ A :: word runs where it is compiled, and what it compiles is
\ what it says with s" ... " evaluate and literal.  So in the
\ body of one literal is compiled as a call to the kernel's, and
\ anywhere else it is that.  Which word is a :: word is told by
\ where its code starts, which no other word's does.  (It was
\ told by its header: a file's own names are in memory that the
\ next file's are in after it, so a word of a later file could
\ have its header where a macro's had been, and `[char] b` in it
\ compiled a call of literal.)
create macro-xt 0 ,     \ where the :: word being made starts
create t-immediate ," immediate"

header :: : cw-:: ( "name" -- )
    mkheader t]  thisxt @ macro-xt !
    t-immediate >c count by-name
;
header-imm literal : cw-literal ( x -- )
    thisxt @ macro-xt @ = if
        ['] tliteral 'compile @ execute exit
    then
    tliteral
;

\ The text of a :: word is wanted only while the word is: so it
\ is not laid in the data, but copied to under where the data
\ must stop, which is brought down to it.  A file that is read
\ puts that back when it is done (files.fs), and the text of its
\ macros is gone with their names.
: text-kept ( c-addr u -- c-addr' u )
    dpl @ over -  CELLB negate and          ( c-addr u to )
    dup dp @ u< d# 8 -throw  dup dpl !
    swap >r  tuck r@ cmove  r>
;

\ text in a definition, or as it is in what is being read
header-imm s" : cw-s" ( "ccc<quote>" -- c-addr u )
    d# 34 parse  state @ 0= if exit then
    thisxt @ macro-xt @ = if
        text-kept swap tliteral tliteral exit
    then
    here over >r >r  s, align  r> >c 1+ tliteral  r> tliteral
;

header / : cw-/ ( n1 n2 -- n3 ) \ the quotient, towards nothing
    2dup xor >r  abs swap abs swap  u/mod nip
    r> 0< if negate then
;
header mod : cw-mod ( n1 n2 -- n3 ) \ and what is left over
    2dup cw-/ * -
;

\ The kernel's create, found as evaluate is: create here is
\ cross.fs's own, which lays a header as it compiles.
create t-create ," create"
header 2variable : cw-2variable ( "name" -- )
    t-create >c count by-name  d# 0 t,  d# 0 t,
;

\ What cross.fs lays data down with as it runs, which here is
\ laid down as it is read.
header tcalign : cw-tcalign ( -- ) align ;
header tcell, : cw-tcell, ( x -- ) t, ;
header tc, : cw-tc, ( c -- ) tc, ;

\ What cross.fs works out from the machine's sizes, under its
\ names (the sizes themselves are words of the nucleus already:
\ baseword-words, from basewords.fs).
header cell-bytes : cw-cell-bytes HOST cell-bytes ;
header byte-shift : cw-byte-shift HOST byte-shift ;
header cell-mask : cw-cell-mask HOST cell-mask ;
header data-base : cw-data-base HOST data-base ;

\ ---- pictures

\ As cross.fs lays them (picture[ ... ]picture): a row of a
\ picture is the rest of a line after row|, # a pixel of ink and
\ . one of none; a picture wider than a line is pieces side by
\ side, with beside between.  Here the rows are kept as the
\ cells the display would hold them in, a row of up to 1,024
\ pixels after a cell each that says how many it has, in memory
\ that is given back at ]picture.  It is memory above the
\ display, which is reached a cell at a time and on a small
\ machine no other way: a pixel is a bit of a cell, and no byte
\ of a row is given an address.

create pc-buf  0 ,     \ the cells that say, then the rows
create pc-rows 0 ,     \ how many the first piece had, or -1
create pc-row  0 ,     \ the row of this piece that is next

: pc-cells ( -- n ) d# 8192 BYTESHIFT rshift d# 64 + ;
: pc-n ( row -- a ) pc-buf @ + ;
: pc-cell ( x row -- a ) \ the cell with pixel x of a row
    d# 7 lshift BYTESHIFT rshift
    swap BYTESHIFT d# 3 + rshift +  pc-buf @ d# 64 + + ;
\ pixel x in its cell: the leftmost is the lowest bit or the
\ highest, as the display has them
: pc-bit ( x -- mask )
    CELLB d# 3 lshift 1- and
    LEFT-LOW? [if] d# 1 swap lshift
    [else] d# 1 TOPBIT lshift swap rshift [then] ;

header picture[ : cw-picture[ ( -- )
    pc-cells >c halloc pc-buf !
    d# 0 pc-buf @ pc-cells cells-fill
    d# -1 pc-rows !  d# 0 pc-row !
;

: pc-piece ( -- ) \ the rows of the piece done
    pc-rows @ 0< if pc-row @ pc-rows ! then
    pc-row @ pc-rows @ <> d# -24 and throw
;

header beside : cw-beside ( -- ) pc-piece  d# 0 pc-row ! ;

: pc-line ( -- c-addr u ) \ the rest of the line being read
    source  >in @ LINE-SHIFT rshift 1+ LINE-SHIFT lshift min
    >in @ - swap >in @ + swap  dup >in +!
;

\ the next pixel of the row, if c is one: a row has room for
\ 1,024
: pc-pixel ( c -- )
    dup bl = if drop exit then
    pc-row @ pc-n @  dup d# 1023 u> d# -24 and throw
    swap [char] # = if
        dup pc-bit  swap pc-row @ pc-cell  or!
    else drop then
    d# 1 pc-row @ pc-n +!
;

header row| : cw-row| ( "pixels" -- ) \ and there is room for 64
    pc-row @ d# 63 u> d# -24 and throw
    pc-line bounds
    begin 2dup xor while  dup c@ pc-pixel  1+  repeat
    2drop  d# 1 pc-row +!
;

\ the cells of a row that are laid: whole 64s of pixels, as
\ cross.fs lays them (px-lay)
: pc-laid ( -- n )
    d# 0 pc-n @ d# 63 + d# 6 rshift
    d# 3 lshift BYTESHIFT rshift
;

: pc-done ( -- ) pc-buf @ pc-cells hgive  d# 0 pc-buf ! ;

\ It is laid only if there is room for all of it.
header ]picture : cw-]picture ( -- )
    pc-piece  align
    pc-rows @ pc-laid * >c  dpl @ dp @ - u> if
        pc-done  true d# 8 -throw
    then
    d# 0
    begin dup pc-rows @ < while
        d# 0 over pc-cell  pc-laid bounds
        begin 2dup xor while  dup @ t,  1+  repeat
        2drop  1+
    repeat
    drop  pc-done
;

\ ---- rows of pixels, laid as cross.fs lays them

: pc-flip ( b -- b' ) \ the bits of a byte the other way round
    d# 0 swap d# -8
    begin dup while
        >r  dup d# 1 and  rot 2* or  swap u2/  r> 1+
    repeat
    2drop
;

\ eight pixels written with the leftmost highest, as the display
\ has them in a byte
: pc-pixels ( byte -- byte' ) LEFT-LOW? [if] pc-flip [then] ;

header p, : cw-p, ( byte -- ) pc-pixels tc, ;
\ the same eight as a cell of their own
header pcell, : cw-pcell, ( byte -- ) align pc-pixels t, ;
\ a row of a pattern: eight pixels across a cell
header prow, : cw-prow, ( byte -- )
    align pc-pixels  CELLB negate
    begin dup while  over tc,  1+  repeat
    2drop
;
\ how many cells n runs of 64 pixels are, as a cell
header px-cells, : cw-px-cells, ( n -- )
    d# 8 BYTESHIFT rshift *  align t, ;

\ a cell for each number of pixels from none to a whole cell:
\ that many pixels at the left of a cell
header lefts, : cw-lefts, ( -- )
    align  d# 0
    begin dup CELLBITS 1+ < while
        dup CELLBITS = if d# -1 else d# 1 over lshift 1- then
        LEFT-LOW? [if] [else]
            over CELLBITS swap - lshift
        [then]
        t,  1+
    repeat
    drop
;

\ The basewords are words of the nucleus too, each with a
\ header, which the machine's file has from vm_opcodes.h
\ (basewords.fs); fused.fs names them by those headers.
baseword-words

\ And the names of an instruction's fields, from the same file,
\ which the decompiler shows an instruction by (see.fs): f-in,
\ f-op and f-out, each with a header.
field-names

\ What else the image says of itself to the decompiler, which
\ may be compiled after the image is made: the newest word at
\ the build and the stamp the tables of names must have
\ (name-tables, cross.fs), and where the table of instructions
\ as words is.  That table is made from an image and laid after
\ all the image's other data (system.fs), so that how long it is
\ moves nothing; it has no header, so that the newest word of
\ the dictionary is the system's, and the tools that read an
\ image find it by this cell too.
header forth0 create forth0 0 ,
header tstamp create tstamp 0 ,
header fused-at create fused-at 0 ,

\ ---- the machine, as cross.fs says it

\ Each is the macro of cross.fs it is named for, so that what it
\ is is said there and only there: a number, an instruction or
\ two, or a flag.  The nucleus compiles a word this small in
\ place, as cross.fs does the macro.

header CELL : cw-cell CELL ;
header CELL2 : cw-cell2 CELL2 ;
header CELLB : cw-cellb CELLB ;
header BYTESHIFT : cw-byteshift BYTESHIFT ;
header ALIGN-MASK : cw-align-mask ALIGN-MASK ;
header OFFSET-MASK : cw-offset-mask OFFSET-MASK ;
header TOPBIT : cw-topbit TOPBIT ;
header HALFBITS : cw-halfbits HALFBITS ;
header NAME-PER : cw-name-per NAME-PER ;
header NAME-BITS : cw-name-bits NAME-BITS ;
header NAME-SPARE : cw-name-spare NAME-SPARE ;
header TOP16 : cw-top16 TOP16 ;
header PXCELL : cw-pxcell PXCELL ;
header CELLBITS : cw-cellbits CELLBITS ;
header LANE1 : cw-lane1 LANE1 ;
header LANES : cw-lanes LANES ;

header LANESHIFT : cw-laneshift LANESHIFT ;
header PXMASK : cw-pxmask PXMASK ;

header PXSHIFT : cw-pxshift PXSHIFT ;

\ (DISP-BASE, DISP-STRIDE, DISP-BANKS and DISP-BANK-STEP are the
\ sizes disp-base and the rest: a name has one case.)
header VARS : cw-vars VARS ;
header HDR-BASE : cw-hdr-base HDR-BASE ;
header HDR-LIMIT : cw-hdr-limit HDR-LIMIT ;
header DATA-LIMIT : cw-data-limit DATA-LIMIT ;
header MEM-END : cw-mem-end MEM-END ;
header CODE-BASE : cw-code-base CODE-BASE ;
header CODE-SIZE : cw-code-size CODE-SIZE ;

header LINE-CHARS : cw-line-chars LINE-CHARS ;
header BLOCK-LINES : cw-block-lines BLOCK-LINES ;
header LINE-SHIFT : cw-line-shift LINE-SHIFT ;
header LAST-COLUMN : cw-last-column LAST-COLUMN ;
header LAST-LINE : cw-last-line LAST-LINE ;
header BLOCK-CHARS : cw-block-chars BLOCK-CHARS ;
header BLKCELLS : cw-blkcells BLKCELLS ;
header LINE-ROUND : cw-line-round LINE-ROUND ;
header LINE-MASK : cw-line-mask LINE-MASK ;

header NAMES-BLK : cw-names-blk NAMES-BLK ;
header CANVAS-BLK : cw-canvas-blk CANVAS-BLK ;
header MASK64-BLK : cw-mask64-blk MASK64-BLK ;
header MASK32-BLK : cw-mask32-blk MASK32-BLK ;
header BAYER-BLK : cw-bayer-blk BAYER-BLK ;
header VIEW-BLK : cw-view-blk VIEW-BLK ;
header VIEW-BLKS : cw-view-blks VIEW-BLKS ;
header GRAY-BLK : cw-gray-blk GRAY-BLK ;
header GRAY-BLKS : cw-gray-blks GRAY-BLKS ;
header SOURCE-BLK : cw-source-blk SOURCE-BLK ;

header ENTRY-BYTES : cw-entry-bytes ENTRY-BYTES ;
header ENTRIES : cw-entries ENTRIES ;
header ENTRY-TYPE : cw-entry-type ENTRY-TYPE ;
header ENTRY-FIRST : cw-entry-first ENTRY-FIRST ;
header ENTRY-BLOCKS : cw-entry-blocks ENTRY-BLOCKS ;
header ENTRY-WHERE : cw-entry-where ENTRY-WHERE ;
header TYPE-BINARY : cw-type-binary TYPE-BINARY ;
header TYPE-SOURCE : cw-type-source TYPE-SOURCE ;
header TYPE-INDEX : cw-type-index TYPE-INDEX ;
header MASK-WIDTH : cw-mask-width MASK-WIDTH ;
header MASK-CELLS : cw-mask-cells MASK-CELLS ;
header MASK-ROW : cw-mask-row MASK-ROW ;
header MASK-ROWS-SHIFT : cw-mask-rows-shift MASK-ROWS-SHIFT ;

\ the instructions that differ with the machine
header (i@) : cw-i@ (i@) ;
header (i!) : cw-i! (i!) ;
header (>c) : cw->c (>c) ;
header (c>) : cw-c> (c>) ;
header RAW>SRC : cw-raw>src RAW>SRC ;
header PIXR : cw-pixr PIXR ;
header PIXL : cw-pixl PIXL ;
header EDGE : cw-edge EDGE ;
header LEFT8 : cw-left8 LEFT8 ;
header LEFT1 : cw-left1 LEFT1 ;
header M8 : cw-m8 M8 ;

\ and the flags a conditional is said with, which are run where
\ they are read, in a definition too, as cross.fs runs them
header-imm BANKED? : cw-banked?
    BANKED? [if] true [else] false [then] ;
header-imm INK-BLACK? : cw-ink-black?
    INK-BLACK? [if] true [else] false [then] ;
header-imm LEFT-LOW? : cw-left-low?
    LEFT-LOW? [if] true [else] false [then] ;
header-imm DISPLAY? : cw-display?
    DISPLAY? [if] true [else] false [then] ;
header-imm W64? : cw-w64? W64? [if] true [else] false [then] ;
header-imm W32? : cw-w32? W32? [if] true [else] false [then] ;
header-imm W32+? : cw-w32+?
    W32+? [if] true [else] false [then] ;
header-imm BOOTING? : cw-booting?
    BOOTING? [if] true [else] false [then] ;
