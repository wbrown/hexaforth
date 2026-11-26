# The words

Every word that has a header in the first machine's system, by the file that defines it and in the order the file defines them. The stack comment and the description are the ones in the source. `util/glossary.c` writes this file, and the target `words` runs it.

An address `a` is the number of a cell, and `c-addr` is the address of a byte. A word marked immediate runs while a definition is being compiled, and its stack comment is what it does then.

## nuc.fs

| Word | Stack | Description |
|---|---|---|
| `up` | `( -- a )` | the task that is running |
| `sbase` | `( -- a )` |  |
| `base` | `( -- a )` |  |
| `'emit` | `( -- a )` |  |
| `sourceid` | `( -- a )` |  |
| `>in` | `( -- a )` |  |
| `0=` | `( x -- f )` |  |
| `0<>` | `( x -- f )` |  |
| `<>` | `( x1 x2 -- f )` |  |
| `0>` | `( n -- f )` |  |
| `>` | `( n1 n2 -- f )` |  |
| `0<` | `( n -- f )` |  |
| `isnotspace?` | `( c -- f )` |  |
| `u>` | `( u1 u2 -- f )` |  |
| `2*` | `( x -- x' )` |  |
| `2dup` | `( x1 x2 -- x1 x2 x1 x2 )` |  |
| `bounds` | `( a n -- a+n a )` |  |
| `-rot` | `( x1 x2 x3 -- x3 x1 x2 )` |  |
| `2drop` | `( x1 x2 -- )` |  |
| `io!` | `( x port -- )` |  |
| `negate` | `( n -- -n )` |  |
| `1+` | `( n -- n+1 )` | the same word as cell+ |
| `cell+` | `( a -- a+1 )` |  |
| `cells` | `( n -- n )` |  |
| `>c` | `( a -- c-addr )` |  |
| `key?` | `( -- f )` | is a character waiting on the console? |
| `uart-emit` | `( c -- )` |  |
| `space` | `( -- )` |  |
| `emit` | `( c -- )` |  |
| `cr` | `( -- )` |  |
| `bl` | `( -- c )` |  |
| `hex1` | `( u -- )` | prints the lowest hex digit of u |
| `.x` | `( u -- )` | prints 8 hex digits and a space |
| `.x2` | `( u -- )` | prints 2 hex digits and a space |
| `true` | `( -- -1 )` |  |
| `false` | `( -- 0 )` |  |
| `rot` | `( x1 x2 x3 -- x2 x3 x1 )` |  |
| `tuck` | `( x1 x2 -- x2 x1 x2 )` |  |
| `?dup` | `( x -- 0 \| x x )` |  |
| `+!` | `( n a -- )` |  |
| `or!` | `( x a -- )` |  |
| `and!` | `( x a -- )` |  |
| `xor!` | `( x a -- )` |  |
| `*!` | `( n a -- )` |  |
| `not!` | `( a -- )` |  |
| `2swap` | `( x1 x2 x3 x4 -- x3 x4 x1 x2 )` |  |
| `2over` | `( x1 x2 x3 x4 -- x1 x2 x3 x4 x1 x2 )` |  |
| `min` | `( n1 n2 -- n3 )` |  |
| `max` | `( n1 n2 -- n3 )` |  |
| `hi32` | `( dw -- w )` |  |
| `lo32` | `( dw -- w )` |  |
| `hi16` | `( dw -- s )` |  |
| `lo16` | `( dw -- s )` |  |
| `count` | `( c-addr -- c-addr+1 c )` |  |
| `c@` | `( c-addr -- c )` |  |
| `field@` | `( c-addr mask -- x )` |  |
| `c!` | `( c c-addr -- )` |  |
| `field!` | `( x c-addr mask -- )` |  |
| `i@` | `( ia -- w )` |  |
| `uw@` | `( c-addr -- w )` |  |
| `i!` | `( w ia -- )` |  |
| `w!` | `( w c-addr -- )` |  |
| `type` | `( c-addr u -- )` |  |
| `blk` | `( -- a )` | the block being read as source, or 0 |
| `'console` | `( -- a )` | where output goes when the prompt starts again |
| `'blk-read` | `( -- a )` |  |
| `'blk-write` | `( -- a )` |  |
| `dp` | `( -- a )` |  |
| `cp` | `( -- a )` |  |
| `state` | `( -- a )` |  |
| `forth` | `( -- a )` |  |
| `1-` | `( n -- n-1 )` |  |
| `adepth` | `( -- n )` |  |
| `depth` | `( -- n )` |  |
| `-` | `( n1 n2 -- n3 )` |  |
| `s>d` | `( n -- d )` |  |
| `abs` | `( n -- u )` |  |
| `2/` | `( n -- n/2 )` | the sign stays where it is |
| `here` | `( -- a )` |  |
| `c>` | `( c-addr -- a )` |  |
| `align` | `( -- )` |  |
| `pause` | `( -- )` |  |
| `key` | `( -- c )` |  |
| `aligned` | `( a -- a )` | the address of a whole cell, at or after a |
| `d2*` | `( d -- 2d )` |  |
| `d+` | `( d1 d2 -- d3 )` |  |
| `dnegate` | `( d -- -d )` |  |
| `source` | `( -- c-addr u )` |  |
| `2@` | `( a -- lo hi )` |  |
| `2!` | `( lo hi a -- )` |  |
| `/string` | `( c-addr u n -- c-addr+n u-n )` |  |
| `um*` | `( u1 u2 -- ud )` |  |
| `*` | `( n1 n2 -- n3 )` |  |
| `um/mod` | `( ud u -- rem quot )` |  |
| `u/mod` | `( u1 u2 -- rem quot )` |  |
| `accept` | `( c-addr n1 -- n2 )` | reads a line of at most n1 characters |
| `3dup` | `( x1 x2 x3 -- x1 x2 x3 x1 x2 x3 )` |  |
| `cells-move` | `( from to n -- )` |  |
| `cells-fill` | `( x a n -- )` |  |
| `>key` | `( c-addr u -- n )` | pack a name into skey; n is its size in cells |
| `samename?` | `( wp -- f )` | has the header that name? |
| `sfind` | `( c-addr u -- c-addr u 0 \| xt flag )` | flag: 1 immediate, -1 not |
| `name>` | `( a -- c-addr u )` | the name whose cells start at a |
| `words` | `( -- )` | prints every name, in search order |
| `>number` | `( ud1 c-addr1 u1 -- ud2 c-addr2 u2 )` |  |
| `cmove` | `( addr1 addr2 u -- )` |  |
| `recurse` | `( -- )` | immediate |
| `compile,` | `( xt -- )` |  |
| `execute` | `( xt -- )` |  |
| `source-id` | `( -- n )` |  |
| `isspace?` | `( c -- f )` |  |
| `parse-name` | `( "name" -- c-addr u )` |  |
| `isnotdelim` | `( c -- f )` |  |
| `parse` | `( c "ccc<c>" -- c-addr u )` |  |
| `\` | `( -- )` | immediate |
| `(` | `( -- )` | immediate |
| `,` | `( x -- )` |  |
| `allot` | `( n -- )` |  |
| `w,` | `( w -- )` |  |
| `c,` | `( c -- )` |  |
| `.` | `( n -- )` |  |
| `[` | `( -- )` | immediate |
| `decimal` | `( -- )` |  |
| `hex` | `( -- )` |  |
| `-throw` | `( a b -- )` | if a is true, throw -b |
| `throw` | `( n -- )` |  |
| `again` | `( dest -- )` | immediate |
| `inline:` |  |  |
| `begin` | `( -- dest )` | immediate |
| `s,` | `( c-addr u -- )` | compiles the string, with its length first |
| `immediate` | `( -- )` |  |
| `:` | `( "name" -- )` |  |
| `]` | `( -- )` |  |
| `:noname` | `( -- xt )` |  |
| `2literal` | `( x1 x2 -- )` | immediate |
| `literal` | `( x -- )` | immediate |
| `create` | `( "name" -- )` |  |
| `constant` | `( x "name" -- )` |  |
| `variable` | `( "name" -- )` |  |
| `does>` | `( -- )` |  |
| `exit` | `( -- )` | immediate |
| `if` | `( -- orig )` | immediate |
| `ahead` | `( -- orig )` | immediate |
| `else` | `( orig1 -- orig2 )` | immediate |
| `then` | `( orig -- )` | immediate |
| `until` | `( dest -- )` | immediate |
| `while` | `( dest -- orig dest )` | immediate |
| `repeat` | `( orig dest -- )` | immediate |
| `folds-at` | `( -- a )` | where folds.fs's table is |
| `do` | `( -- do-sys )` | immediate |
| `loop` | `( do-sys -- )` | immediate |
| `+loop` | `( do-sys -- )` | immediate |
| `leave` | `( -- )` | immediate |
| `?do` | `( -- do-sys )` | immediate |
| `i` | `( -- n )` | the index of the loop that is running |
| `j` | `( -- n )` | the index of the loop outside that one |
| `unloop` | `( -- )` | immediate |
| `!` | `( x a -- )` |  |
| `+` | `( n1 n2 -- n3 )` |  |
| `xor` | `( x1 x2 -- x3 )` |  |
| `and` | `( x1 x2 -- x3 )` |  |
| `or` | `( x1 x2 -- x3 )` |  |
| `invert` | `( x -- x' )` |  |
| `=` | `( x1 x2 -- f )` |  |
| `<` | `( n1 n2 -- f )` |  |
| `u<` | `( u1 u2 -- f )` |  |
| `swap` | `( x1 x2 -- x2 x1 )` |  |
| `dup` | `( x -- x x )` |  |
| `drop` | `( x -- )` |  |
| `over` | `( x1 x2 -- x1 x2 x1 )` |  |
| `nip` | `( x1 x2 -- x2 )` |  |
| `@` | `( a -- x )` |  |
| `io@` | `( port -- x )` |  |
| `rshift` | `( x u -- x' )` |  |
| `lshift` | `( x u -- x' )` |  |
| `postpone` | `( "name" -- )` | immediate |
| `interpret1` | `( c-addr u -- )` | a word of the source: done, or compiled |
| `numbered` | `( c-addr u -- )` | a number of the source: left, or compiled |
| `catch` | `( xt -- 0 \| n )` |  |
| `spawn` | `( xt dbase rbase task -- )` |  |
| `'` | `( "name" -- xt )` |  |
| `refill` | `( -- f )` |  |
| `evaluate` | `( c-addr u -- )` |  |
| `ifield` | `( a offset -- u )` | two bytes that far into the cells from a |
| `quit` | `( -- )` | the prompt: reads a line and interprets it, for ever |
| `.s` | `( -- )` | prints the data stack, the top last, and keeps it |
| `dabs` | `( d -- ud )` |  |
| `2rot` | `( x1 x2 x3 x4 x5 x6 -- x3 x4 x5 x6 x1 x2 )` |  |
| `d>s` | `( d -- n )` |  |
| `d=` | `( a b c d -- f )` |  |
| `d<` | `( d1 d2 -- f )` |  |
| `du<` | `( ud1 ud2 -- f )` |  |
| `d-` | `( d1 d2 -- d3 )` |  |
| `d0<` | `( d -- f )` |  |
| `d0=` | `( d -- f )` |  |
| `d2/` | `( d -- d' )` |  |
| `fill` | `( c-addr u char -- )` |  |
| `cmove>` | `( addr1 addr2 u -- )` |  |
| `bye` | `( -- )` |  |

## join.fs

| Word | Stack | Description |
|---|---|---|
| `>r` |  | immediate |
| `r>` |  | immediate |
| `r@` |  | immediate |
| `rdrop` |  | immediate |
| `exit` |  | immediate |
| `;` |  | immediate |
| `in-place` |  | immediate |

## machine.fs

| Word | Stack | Description |
|---|---|---|
| `himem` | `( -- a )` |  |
| `halloc` | `( u -- a )` |  |
| `hfree` | `( a cells -- )` | from a, an even cell, they are nobody's |
| `hgive` | `( a cells -- )` | done with: himem comes back, or hfree |
| `hreuse` | `( u -- a )` | of what was given back, or new |
| `with-room` | `( x u xt -- ok? )` |  |
| `l@` | `( a -- x )` |  |
| `l!` | `( x a -- )` |  |
| `be16@` | `( a -- x )` |  |
| `be32@` | `( a -- x )` |  |
| `be32!` | `( x a -- )` |  |
| `be64!` | `( x a -- )` |  |

## reg.fs

| Word | Stack | Description |
|---|---|---|
| `dev@` | `( n -- x )` |  |
| `dev!` | `( x n -- )` |  |
| `dev-addr!` | `( a n -- )` |  |
| `cell-addr!` | `( a d -- )` |  |

## placement.fs

| Word | Stack | Description |
|---|---|---|
| `bulk` | `( -- )` |  |
| `unbulk` | `( -- )` |  |
| `high` | `( -- )` |  |
| `unhigh` | `( -- )` |  |
| `uncoded` | `( end from -- )` | the code between is made zero |
| `scratch` | `( -- a )` |  |
| `cp-kept` | `( -- a )` |  |
| `meta` | `( -- )` | immediate: -8 if code has reached it |
| `target` | `( -- )` | immediate |
| `scratch-back` | `( x -- )` | the scratch from x on is given back |
| `[']` | `( "name" -- )` | immediate |
| `code-on` | `( -- )` |  |
| `pages-kept` | `( -- )` |  |
| `code-room` | `( -- )` |  |
| `code-off` | `( ia -- )` |  |
| `code-page` | `( n -- )` | on from where page n's code had got to |
| `sys-page` | `( n -- )` |  |
| `big-page` | `( n -- )` |  |
| `small-page` | `( n -- )` |  |
| `small-first` | `( -- )` |  |
| `low-page` | `( -- )` |  |
| `low-page-end` | `( -- )` |  |

## blocks.fs

| Word | Stack | Description |
|---|---|---|
| `blk-moved` | `( -- a )` | blocks read and written |
| `blk-wrote` | `( -- a )` | true if the last was |
| `blk-disk` | `( -- a )` | a write; and its disk |
| `disks-there` | `( -- a )` |  |
| `blk-read` | `( buf blk# -- )` |  |
| `blk-write` | `( buf blk# -- )` |  |
| `blk-cur` | `( -- a )` | the block it holds |
| `flush` | `( -- )` | write the buffer back if it was changed |
| `blk-need` | `( -- )` | see that there is a buffer |
| `cblock` | `( blk# -- a )` | the block's buffer, as a cell's address |
| `block` | `( blk# -- a )` | and as a byte's |
| `update` | `( -- )` |  |
| `stay-xt` | `( -- a )` |  |
| `load` | `( blk# -- )` | a throw is printed, and thrown on as -1 |
| `thru` | `( first last -- )` |  |
| `.(` | `( "text<paren>" -- )` | immediate |

## literals.fs

| Word | Stack | Description |
|---|---|---|
| `d#` | `( "n" -- )` | immediate |
| `h#` | `( "n" -- )` | immediate |
| `c#` | `( "n" -- )` | immediate |
| `,"` | `( "ccc<quote>" -- )` | a counted string, here |
| `char` | `( "c" -- c )` |  |
| `[char]` | `( "c" -- )` | immediate: compiled |

## conditional.fs

| Word | Stack | Description |
|---|---|---|
| `[if]` | `( f -- )` | immediate |
| `[else]` | `( -- )` | immediate |
| `[then]` | `( -- )` | immediate |

## divide.fs

| Word | Stack | Description |
|---|---|---|
| `/` | `( n1 n2 -- n3 )` | the quotient, towards zero |
| `mod` | `( n1 n2 -- n3 )` |  |

## macros.fs

| Word | Stack | Description |
|---|---|---|
| `::` | `( "name" -- )` |  |
| `literal` | `( x -- )` | immediate |
| `s"` | `( "ccc<quote>" -- c-addr u )` | immediate |

## pixels.fs

| Word | Stack | Description |
|---|---|---|
| `picture[` | `( -- )` |  |
| `beside` | `( -- )` |  |
| `row\|` | `( "pixels" -- )` |  |
| `]picture` | `( -- )` |  |
| `p,` | `( byte -- )` |  |
| `pcell,` | `( byte -- )` |  |
| `prow,` | `( byte -- )` |  |
| `px-cells,` | `( n -- )` |  |
| `lefts,` | `( -- )` |  |

## names.fs

| Word | Stack | Description |
|---|---|---|
| `tevaluate` | `( c-addr u -- )` |  |
| `tcalign` | `( -- )` |  |
| `tcell,` | `( x -- )` |  |
| `tc,` | `( c -- )` |  |
| `2variable` | `( "name" -- )` |  |
| `cell-bytes` |  |  |
| `byte-shift` |  |  |
| `cell-mask` |  |  |
| `data-base` |  |  |
| `halt` |  |  |
| `noop` |  |  |
| `2+` |  |  |
| `u2/` |  |  |
| `8emit` |  |  |
| `3rd` |  |  |
| `>><<` |  |  |
| `nmask8` |  |  |
| `nmask16` |  |  |
| `trace-on` |  |  |
| `trace-off` |  |  |
| `cell-bits` |  |  |
| `addr-bits` |  |  |
| `stack-cells` |  |  |
| `var-base` |  |  |
| `disp-w` |  |  |
| `disp-h` |  |  |
| `disp-base` |  |  |
| `disp-left-low` |  |  |
| `disp-ink-black` |  |  |
| `disp-stride` |  |  |
| `disp-banks` |  |  |
| `disp-bank-step` |  |  |
| `krom-bytes` |  |  |
| `disk-block` |  |  |
| `disk-line` |  |  |
| `disk-source-block` |  |  |
| `disk-block-bits` |  |  |
| `disk-drives` |  |  |
| `disk-names-block` |  |  |
| `disk-canvas-block` |  |  |
| `disk-masks-block` |  |  |
| `disk-mask32-block` |  |  |
| `disk-bayer-block` |  |  |
| `disk-mask-width` |  |  |
| `disk-mask-planes` |  |  |
| `disk-view-block` |  |  |
| `disk-view-blocks` |  |  |
| `disk-gray-block` |  |  |
| `disk-gray-blocks` |  |  |
| `disk-own-blocks` |  |  |
| `disk-entry` |  |  |
| `disk-entry-type` |  |  |
| `disk-entry-first` |  |  |
| `disk-entry-blocks` |  |  |
| `disk-entry-where` |  |  |
| `disk-binary` |  |  |
| `disk-source` |  |  |
| `disk-index` |  |  |
| `f-in` |  |  |
| `f-op` |  |  |
| `f-out` |  |  |
| `CELL` |  |  |
| `CELL2` |  |  |
| `CELLB` |  |  |
| `BYTESHIFT` |  |  |
| `ALIGN-MASK` |  |  |
| `OFFSET-MASK` |  |  |
| `TOPBIT` |  |  |
| `HALFBITS` |  |  |
| `NAME-PER` |  |  |
| `NAME-BITS` |  |  |
| `NAME-SPARE` |  |  |
| `TOP16` |  |  |
| `PXCELL` |  |  |
| `CELLBITS` |  |  |
| `LANE1` |  |  |
| `LANES` |  |  |
| `LANESHIFT` |  |  |
| `PXMASK` |  |  |
| `PXSHIFT` |  |  |
| `VARS` |  |  |
| `HDR-BASE` |  |  |
| `HDR-LIMIT` |  |  |
| `DATA-LIMIT` |  |  |
| `MEM-END` |  |  |
| `CODE-BASE` |  |  |
| `CODE-SIZE` |  |  |
| `LINE-CHARS` |  |  |
| `BLOCK-LINES` |  |  |
| `LINE-SHIFT` |  |  |
| `LAST-COLUMN` |  |  |
| `LAST-LINE` |  |  |
| `BLOCK-CHARS` |  |  |
| `BLKCELLS` |  |  |
| `LINE-ROUND` |  |  |
| `LINE-MASK` |  |  |
| `NAMES-BLK` |  |  |
| `CANVAS-BLK` |  |  |
| `MASK64-BLK` |  |  |
| `MASK32-BLK` |  |  |
| `BAYER-BLK` |  |  |
| `VIEW-BLK` |  |  |
| `VIEW-BLKS` |  |  |
| `GRAY-BLK` |  |  |
| `GRAY-BLKS` |  |  |
| `SOURCE-BLK` |  |  |
| `ENTRY-BYTES` |  |  |
| `ENTRIES` |  |  |
| `ENTRY-TYPE` |  |  |
| `ENTRY-FIRST` |  |  |
| `ENTRY-BLOCKS` |  |  |
| `ENTRY-WHERE` |  |  |
| `TYPE-BINARY` |  |  |
| `TYPE-SOURCE` |  |  |
| `TYPE-INDEX` |  |  |
| `MASK-WIDTH` |  |  |
| `MASK-CELLS` |  |  |
| `MASK-ROW` |  |  |
| `MASK-ROWS-SHIFT` |  |  |
| `(>c)` |  |  |
| `(c>)` |  |  |
| `RAW>SRC` |  |  |
| `PIXR` |  |  |
| `PIXL` |  |  |
| `EDGE` |  |  |
| `LEFT8` |  |  |
| `LEFT1` |  |  |
| `M8` |  |  |
| `BANKED?` |  |  |
| `INK-BLACK?` |  |  |
| `LEFT-LOW?` |  |  |
| `DISPLAY?` |  |  |
| `W64?` |  |  |
| `W32?` |  |  |
| `W32+?` |  |  |
| `SMALL?` |  |  |
| `BOOTING?` |  |  |

## image.fs

| Word | Stack | Description |
|---|---|---|
| `forth0` | `( -- a )` | the image's first header |
| `tstamp` | `( -- a )` | the stamp of its names |
| `fused-at` | `( -- a )` | where fused.fs's table is |

## mono.fs

| Word | Stack | Description |
|---|---|---|
| `mono` | `( -- a )` |  |

## sans.fs

| Word | Stack | Description |
|---|---|---|
| `sans` | `( -- a )` |  |

## spin.fs

| Word | Stack | Description |
|---|---|---|
| `spin` | `( -- a )` |  |

## display.fs

| Word | Stack | Description |
|---|---|---|
| `fb-w` | `( -- a )` |  |
| `fb-h` | `( -- a )` |  |
| `ox` |  |  |
| `oy` |  |  |
| `clip` |  |  |
| `pattern` |  |  |
| `'mode` |  |  |
| `inverse` |  |  |
| `bold` |  |  |
| `italic` |  |  |
| `font` |  |  |
| `clip-rgn` |  |  |
| `text-or` |  |  |
| `origin!` | `( x y -- )` |  |
| `row` | `( y -- a )` | the first cell of row y |
| `ink` | `( mask a -- )` |  |
| `paper` | `( mask a -- )` |  |
| `'plot` | `( -- a )` | ( on? x y -- )  where plot goes |
| `plot` | `( on? x y -- )` |  |
| `copy-mode` | `( -- )` |  |
| `or-mode` | `( -- )` |  |
| `xor-mode` | `( -- )` |  |
| `bic-mode` | `( -- )` |  |
| `p-white` | `( -- a )` |  |
| `p-black` | `( -- a )` |  |
| `p-gray` | `( -- a )` |  |
| `p-ltgray` | `( -- a )` |  |
| `p-dkgray` | `( -- a )` |  |
| `rgn-clip!` | `( rgn\|0 -- )` |  |
| `slab` | `( x y n -- )` |  |
| `solid` | `( on? -- )` |  |
| `clip!` | `( x y w h -- )` |  |
| `clip-screen!` | `( x0 y0 x1 y1 -- )` |  |
| `noclip` | `( -- )` | the whole screen |
| `clip-to` | `( x y w h -- )` | this and the clip |
| `clip-rows` | `( -- top bottom )` | in the port |
| `pat-hline` | `( x y n -- )` |  |
| `pat-rect` | `( x y w h -- )` |  |
| `screen` | `( -- a )` | the display |
| `copy-busy` | `( -- a )` |  |
| `copy-state` | `( -- a )` | a copy, up to copy-end |
| `copy-end` | `( -- a )` |  |
| `copy-bits` | `( bitmap sx sy dx dy w h -- )` |  |
| `keep-bits` | `( bitmap x y w h -- )` |  |
| `put-bits` | `( bitmap x y w h -- )` | and back |
| `xor8` | `( bits x y -- )` | invert the pixels where bits are set |
| `xor-column` | `( x y n -- )` |  |
| `char@` | `( a bit -- c )` |  |
| `ctext` | `( cstr -- a i u )` | of a counted string |
| `>chars` | `( addr -- a i )` | of a byte's address |
| `chars-width` | `( a i u -- n )` | in pixels |
| `text-width` | `( addr u -- n )` |  |
| `text-busy` | `( -- a )` |  |
| `text-state` | `( -- a )` | a string, up to text-end |
| `text-end` | `( -- a )` |  |
| `draw-chars` | `( a i u x y -- )` |  |
| `draw-text` | `( addr u x y -- )` |  |
| `draw-char` | `( c x y -- )` |  |
| `port` | `( -- a )` |  |
| `screen-port` | `( -- a )` |  |
| `port!` | `( port -- )` |  |
| `new-port` | `( -- port )` |  |
| `display-init` | `( -- )` |  |

## region.fs

| Word | Stack | Description |
|---|---|---|
| `rgn-none` | `( rgn -- )` | nothing is in it |
| `new-rgn` | `( -- rgn )` | with nothing in it |
| `rgn-empty?` | `( rgn -- f )` |  |
| `rgn-copy` | `( from to -- )` |  |
| `rect-rgn` | `( x y w h rgn -- )` |  |
| `rgn-and` | `( a b to -- )` | what is in both |
| `rgn-or` | `( a b to -- )` | what is in either |
| `rgn-diff` | `( a b to -- )` | in a and not in b |
| `rgn-offset` | `( dx dy rgn -- )` | move it |
| `rgn-box` | `( rgn -- x y w h )` |  |
| `scroll-update` | `( -- a )` |  |
| `scroll-rect` | `( x y w h dx dy -- )` |  |

## gfx.fs

| Word | Stack | Description |
|---|---|---|
| `hline` | `( on? x y n -- )` |  |
| `fill-rect` | `( on? x y w h -- )` |  |
| `frame-rect` | `( on? x y w h -- )` |  |
| `sb` | `( -- a )` |  |
| `sbn` | `( -- a )` |  |
| `sb-begin` | `( -- )` |  |
| `sb-end` | `( -- )` |  |
| `sb-draw` | `( x y -- )` |  |
| `char-xy` | `( col row -- x y )` |  |

## keymap.fs

| Word | Stack | Description |
|---|---|---|
| `keymap` | `( -- a )` |  |

## wm.fs

| Word | Stack | Description |
|---|---|---|
| `nwin` | `( -- a )` |  |
| `nawin` | `( -- a )` | how many were made |
| `no-windows` | `( -- )` |  |
| `top` | `( -- win\|0 )` | the front window |
| `win-data` | `( win -- a )` |  |
| `win-menu` | `( win -- a )` |  |
| `win-front` | `( win -- a )` |  |
| `win-back` | `( win -- a )` |  |
| `window` | `( x y w h title draw key click -- win )` |  |
| `content` | `( win -- x y )` | inside the frame |
| `cursor-off` | `( -- )` |  |
| `cursor-show` | `( -- )` |  |
| `'moved` | `( -- a )` |  |
| `menu-open` | `( -- a )` |  |
| `wm-regions` | `( -- )` |  |
| `inval-rect` | `( x y w h -- )` | on the screen |
| `inval-rgn` | `( rgn -- )` | on the screen |
| `inval-window` | `( win -- )` | frame and all |
| `'overlay` | `( -- a )` |  |
| `overlays-off` | `( -- )` |  |
| `overlays-on` | `( -- )` |  |
| `win-port` | `( win -- )` |  |
| `win-inval` | `( x y w h win -- )` | in its content |
| `refresh` | `( -- )` | paint what is out of date |
| `held` | `( -- a )` | true while the button is down |
| `'track` | `( -- a )` |  |
| `on-event` | `( type code value -- )` |  |
| `'poll` | `( -- a )` |  |
| `uart-poll` | `( -- )` |  |
| `event-loop` | `( -- )` |  |

## files.fs

| Word | Stack | Description |
|---|---|---|
| `find-disks` | `( -- )` |  |
| `labelled` | `( c-addr u -- first blocks )` |  |
| `own-kept` | `( -- a )` |  |
| `own-ix` | `( -- a )` |  |
| `pushes` | `( xt -- n true \| false )` |  |
| `header` | `( "name" -- )` |  |
| `constant` | `( x "name" -- )` | immediate |
| `create` | `( "name" -- )` | immediate |
| `variable` | `( "name" -- )` | immediate |
| `include` | `( "label" -- )` |  |

## console.fs

| Word | Stack | Description |
|---|---|---|
| `con-emit` | `( c -- )` | to the serial line, and on the screen |
| `console` | `( -- )` | the screen, white, is the console too |
| `boot` | `( -- )` |  |

## tiff.fs

| Word | Stack | Description |
|---|---|---|
| `tw` | `( -- a )` |  |
| `th` | `( -- a )` |  |
| `'pixel` | `( -- a )` |  |
| `'grays` | `( -- a )` |  |
| `'pairs` | `( -- a )` |  |
| `HALFMASK` |  |  |
| `tiff-open` | `( blk# -- img )` | the TIFF that starts at a block |
| `tiff-close` | `( img -- )` | its window, then its record |
| `tiff-row` | `( img -- )` | in hand, its next row there to be taken |
| `image-is` | `( cell w h samples -- img )` |  |
| `gray-mask` | `( -- a )` |  |
| `pair-mask` | `( -- a )` |  |
| `fold-pairs` | `( y -- z )` |  |
| `image!` | `( img -- )` |  |
| `pixels` | `( -- )` | the image in hand, from its first pixel |

## atkinson.fs

| Word | Stack | Description |
|---|---|---|
| `dither-part` | `( bitmap img x y w h -- dither )` |  |
| `dither-half` | `( bitmap img x y w h -- dither )` |  |
| `dither-twice` | `( bitmap img x y w h -- dither )` |  |
| `dither-start` | `( bitmap img -- dither )` |  |
| `dither-row` | `( dither -- )` | the next row |
| `atkinson` | `( bitmap img -- )` | the whole image |
| `dither-levels` | `( planes img shift -- dither )` |  |
| `level-row` | `( dither -- )` | the next row of samples |

## text.fs

| Word | Stack | Description |
|---|---|---|
| `INDENT` |  |  |
| `INDENT-COLS` |  |  |
| `FACE` |  |  |
| `F-BOLD` |  |  |
| `F-LEAN` |  |  |
| `F-UNDER` |  |  |
| `tb.len` | `( tb -- a )` |  |
| `tb.lines` | `( tb -- a )` |  |
| `text-clear` | `( tb -- )` | nothing in it but an empty line |
| `text-new` | `( a n roll? tb -- )` | a text in the n cells from a |
| `text-line` | `( n tb -- a )` | where its nth line starts |
| `text-add` | `( a i u tb -- )` | onto the end of its last line |
| `text-code` | `( c tb -- )` | a code, the same |
| `text-nl` | `( tb -- )` | an empty line after its last |
| `text-back` | `( tb -- )` | the last character of its last line out |
| `draw-line` | `( a x y -- )` | the line that starts at a |
| `mark-up` | `( x y -- )` |  |
| `mark-down` | `( x y -- )` |  |
| `draw-lines` | `( a n x y -- )` | n lines from the one at a |

## term.fs

| Word | Stack | Description |
|---|---|---|
| `tclear` | `( -- )` |  |
| `term-draw` | `( win -- )` |  |
| `term-ok` | `( -- )` |  |
| `repl-start` | `( -- )` | once |
| `repl-key` | `( c win -- )` |  |

## edit.fs

| Word | Stack | Description |
|---|---|---|
| `edit-draw` | `( win -- )` |  |
| `edit-key` | `( c win -- )` |  |

## see.fs

| Word | Stack | Description |
|---|---|---|
| `names?` | `( -- f )` | are they in? |
| `names-load` | `( -- )` |  |
| `names-unload` | `( -- )` |  |
| `see` | `( "name" -- )` | a word, a label, or a label's data |
| `t-words` | `( -- a )` |  |
| `words-draw` | `( win -- )` |  |
| `words-key` | `( c win -- )` |  |
| `words-click` | `( x y win -- )` |  |

## gray.fs

| Word | Stack | Description |
|---|---|---|
| `gray-cols` | `( gray -- n )` |  |
| `gray-rows` | `( gray -- n )` |  |
| `gray-dither` | `( gray -- dither )` |  |
| `gray-record` | `( w h shift -- gray )` |  |
| `gray-planes` | `( gray -- )` | memory for all of its planes |
| `gray-new` | `( w h shift -- gray )` |  |
| `gray-open` | `( img gray -- )` |  |
| `gray-row` | `( gray -- )` | its next row of samples |
| `gray-keep` | `( img shift -- gray )` | the whole gray, at once |
| `mask-from` | `( a n bright -- mask )` |  |
| `mask-load` | `( blk n bright -- mask )` |  |
| `mask-bayer` | `( bright -- mask )` |  |
| `mask-blue64` | `( bright -- mask )` |  |
| `mask-blue32` | `( bright -- mask )` |  |
| `gray-draw-start` | `( bitmap mask gray dshift x0 y0 -- d )` |  |
| `gray-draw-row` | `( y d -- )` | row y of the bitmap |
| `gray-draw-done` | `( d -- )` |  |
| `gray-bitmap` | `( bitmap gray dshift -- )` |  |
| `gray-draw` | `( bitmap mask gray dshift -- )` |  |

## view.fs

| Word | Stack | Description |
|---|---|---|
| `view-load` | `( blk# w h -- viewer )` | the TIFF at a block |
| `view-draw` | `( win -- )` |  |
| `ignore-key` | `( c win -- )` |  |
| `view-menu` | `( win -- )` |  |
| `picture-blk` | `( -- blk\|0 )` |  |

## calc.fs

| Word | Stack | Description |
|---|---|---|
| `calc-key` | `( c win -- )` |  |
| `calc-draw` | `( win -- )` |  |
| `calc-click` | `( x y win -- )` |  |

## paint.fs

| Word | Stack | Description |
|---|---|---|
| `paint-click` | `( x y win -- )` |  |
| `paint-draw` | `( win -- )` |  |

## icons.fs

| Word | Stack | Description |
|---|---|---|
| `icon-bits` | `( -- a )` |  |

## virtio.fs

| Word | Stack | Description |
|---|---|---|
| `vdev` | `( -- a )` |  |
| `vcell@` | `( n -- x )` |  |
| `vcell!` | `( x n -- )` |  |
| `vprobe` | `( id slot -- slot true \| false )` |  |
| `vhello` | `( -- )` | reset; VERSION_1 and no more |
| `vqueue` | `( n -- desc avail used )` |  |
| `vgo` | `( -- )` |  |

## virtio-input.fs

| Word | Stack | Description |
|---|---|---|
| `'event` | `( -- a )` | ( type code value -- ) |
| `nidev` | `( -- a )` | how many devices there are |
| `input-init` | `( -- )` | every input device there is, up to four |
| `input-poll` | `( -- )` |  |

## mandrill.fs

| Word | Stack | Description |
|---|---|---|
| `devices` | `( -- )` |  |
| `show` | `( blk# -- )` | the TIFF at a block, dithered and centred |
| `mandrill` | `( -- )` |  |

## desk.fs

| Word | Stack | Description |
|---|---|---|
| `windows` | `( -- )` | open them; ^Q with none open ends it |
| `desktop` | `( -- )` | runs until ^Q with no window open |
| `cold` | `( -- )` |  |

## Forth-2012 core words that this system lacks

`#` `#>` `#s` `*/` `*/mod` `."` `/mod` `<#` `>body` `abort` `abort"` `char+` `chars` `environment?` `find` `fm/mod` `hold` `m*` `move` `sign` `sm/rem` `spaces` `u.` `word`
