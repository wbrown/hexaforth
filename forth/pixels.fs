\ pixels.fs - pixels written in source: a picture as rows of #
\ and ., and rows of eight pixels as a byte

\ The words lay cells as they are read.  cross.fs runs them
\ too: gforth compiles this file for it (host-included), so a
\ cell is stepped over with cell+ and counted with cells.

\ ---- pictures

\ a cell a row that says how many pixels it has, then the rows
create pc-buf  0 ,
create pc-rows 0 ,     \ how many the first piece had, or -1
create pc-row  0 ,     \ the row of this piece that is next

: pc-cells ( -- n ) d# 8192 BYTESHIFT rshift d# 64 + ;
: pc-n ( row -- a ) cells pc-buf @ + ;
: pc-cell ( x row -- a ) \ the cell with pixel x of a row
    d# 7 lshift BYTESHIFT rshift
    swap BYTESHIFT d# 3 + rshift +
    cells  pc-buf @ d# 64 cells + + ;
: pc-bit ( x -- mask ) \ pixel x in its cell
    CELLB d# 3 lshift 1- and
    LEFT-LOW? [if] d# 1 swap lshift
    [else] d# 1 TOPBIT lshift swap rshift [then] ;

: pc-done ( -- ) pc-buf @ pc-cells hgive  d# 0 pc-buf ! ;

\ What is not a picture is given up, and throws -24: a word of
\ these with no picture[ before it or a second picture[, a
\ piece with no rows or whose rows differ, a pixel that is not
\ # or ., and a row that is not whole 64s of pixels.
: ?pc ( f -- )
    if  pc-buf @ if pc-done then  d# -24 throw  then ;

header picture[ : cw-picture[ ( -- )
    pc-buf @ 0<> ?pc
    pc-cells >c halloc pc-buf !
    d# 0 pc-buf @ pc-cells cells-fill
    d# -1 pc-rows !  d# 0 pc-row !
;

: pc-piece ( -- ) \ the rows of the piece done
    pc-buf @ 0=  pc-row @ 0=  or ?pc
    pc-row @ cells pc-buf @ +  pc-buf @
    begin 2dup xor while
        dup @ d# 0 pc-n @ <> ?pc  cell+
    repeat
    2drop
    pc-rows @ 0< if pc-row @ pc-rows ! then
    pc-row @ pc-rows @ <> ?pc
;

header beside : cw-beside ( -- ) pc-piece  d# 0 pc-row ! ;

: pc-line ( -- c-addr u ) \ the rest of the line being read
    source  >in @ LINE-SHIFT rshift 1+ LINE-SHIFT lshift min
    >in @ - swap >in @ + swap  dup >in +!
;

: pc-pixel ( c -- ) \ the next pixel of the row, if c is one
    dup bl = if drop exit then
    dup [char] # <>  over [char] . <>  and ?pc
    pc-row @ pc-n @  dup d# 1023 u> ?pc
    swap [char] # = if
        dup pc-bit  swap pc-row @ pc-cell  or!
    else drop then
    d# 1 pc-row @ pc-n +!
;

header row| : cw-row| ( "pixels" -- )
    pc-buf @ 0=  pc-row @ d# 63 u>  or ?pc
    pc-line bounds
    begin 2dup xor while  dup c@ pc-pixel  1+  repeat
    2drop  d# 1 pc-row +!
;

: pc-laid ( -- n ) \ the cells of a row: whole 64s of pixels
    d# 0 pc-n @ d# 63 + d# 6 rshift
    d# 3 lshift BYTESHIFT rshift
;

header ]picture : cw-]picture ( -- )
    pc-piece  align
    d# 0 pc-n @ d# 63 and 0<> ?pc
    pc-rows @ pc-laid * >c  dpl @ dp @ - u> if
        pc-done  true d# 8 -throw
    then
    d# 0
    begin dup pc-rows @ < while
        d# 0 over pc-cell  pc-laid cells bounds
        begin 2dup xor while  dup @ t,  cell+  repeat
        2drop  1+
    repeat
    drop  pc-done
;

\ ---- rows of pixels

: pc-flip ( b -- b' ) \ the bits of a byte the other way round
    d# 0 swap d# -8
    begin dup while
        >r  dup d# 1 and  rot 2* or  swap u2/  r> 1+
    repeat
    2drop
;

: pc-pixels ( byte -- byte' ) LEFT-LOW? [if] pc-flip [then] ;

header p, : cw-p, ( byte -- ) pc-pixels tc, ;
header pcell, : cw-pcell, ( byte -- ) align pc-pixels t, ;
header prow, : cw-prow, ( byte -- )
    align pc-pixels  CELLB negate
    begin dup while  over tc,  1+  repeat
    2drop
;
header px-cells, : cw-px-cells, ( n -- )
    d# 8 BYTESHIFT rshift *  align t, ;

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
