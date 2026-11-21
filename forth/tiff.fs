\ tiff.fs - read an uncompressed TIFF from the block device, a
\ row at a time

\ Either byte order, 8-bit gray or RGB, any number of strips.
\ What is in memory is a window, a row and a block long.  A
\ place in the file is a block and a byte in it.

create img 0 ,          \ the image in hand: its record

$18 constant IMG-CELLS  \ even, as hreuse gives

header tw : tw ( -- a ) img @ ;       \ width
header th : th ( -- a ) img @ 1+ ;    \ height
: tspp      ( -- a ) img @ CELL2 + ;  \ samples per pixel
: tphoto    ( -- a ) img @ c# 3 + ;   \ 0 white is zero, 1 black
: tcomp     ( -- a ) img @ c# 4 + ;   \ compression
: tiff-base ( -- a ) img @ c# 5 + ;   \ the file's first block
: tiff-be   ( -- a ) img @ c# 6 + ;   \ true if big-endian
: tfblk     ( -- a ) img @ c# 7 + ;   \ bytes count from here
: tdblk     ( -- a ) img @ c# 8 + ;   \ the directory's block,
: tstrips   ( -- a ) img @ c# 9 + ;   \ its strips' entry, or 0
: trps      ( -- a ) img @ c# 10 + ;  \ rows in a strip
: raw       ( -- a ) img @ c# 11 + ;  \ the window
: wend      ( -- a ) img @ c# 12 + ;  \ the end of what it has
: tblk      ( -- a ) img @ c# 13 + ;  \ the next block,

: tstrip    ( -- a ) img @ c# 14 + ;  \ the next strip,
: tsleft    ( -- a ) img @ c# 15 + ;  \ the rows left of this
: tbuf      ( -- a ) img @ c# 16 + ;  \ bits not yet taken,
: tbn       ( -- a ) img @ c# 17 + ;  \ how many,
: tbp       ( -- a ) img @ c# 18 + ;  \ and the next cell
\ ( -- g ) the next pixel's gray
header 'pixel : 'pixel ( -- a ) img @ c# 19 + ;
\ ( lanes -- ) a row's grays, and two pixels to a lane; or 0
header 'grays : 'grays ( -- a ) img @ c# 20 + ;
header 'pairs : 'pairs ( -- a ) img @ c# 21 + ;
: traw      ( -- a ) img @ c# 22 + ;  \ the window's cells

meta
:: BLKSHIFT \ from a count of cells to a count of blocks
    #10 byte-shift - literal ;
:: BITSHIFT \ from a count of bits to a count of cells
    byte-shift 3 + literal ;
:: HALFMASK \ the lower half of a cell
    1 cell-bits 2/ lshift 1- literal ;

\ the grays in the row's cell at p, added in pairs into lanes
:: GRAY-PAIR ( p -- p y )
    cell-bits #16 = if
        s" dup @ >r  d# 255 d# 8  r@ swap rshift" evaluate
        s" over r@ and  + nip rdrop" evaluate
    else
        s" dup @  dup d# 8 rshift gray-mask @ and" evaluate
        s" swap gray-mask @ and  +" evaluate
    then
;

target

header HALFMASK : half-mask HALFMASK ;

: new-image ( -- )
    IMG-CELLS >c halloc  dup img !
    d# 0 swap IMG-CELLS cells-fill
;

\ ---- the directory, read through the block buffer

\ byte n of the cells from a; a cell's lowest byte is its first
: byte@ ( a n -- b )
    dup BYTESHIFT rshift rot + @
    swap CELLB 1- and d# 3 lshift rshift
    h# ff and
;

\ a byte's place is counted from a block of the file (tfblk)
: t8@ ( byte -- b )
    dup d# 10 rshift tfblk @ + tiff-base @ + cblock
    swap h# 3ff and byte@
;

: t16@ ( byte -- x )
    dup t8@ swap 1+ t8@  tiff-be @ if swap then
    d# 8 lshift or ;
: t32@ ( byte -- lo hi )
    dup t16@ swap d# 2 + t16@  tiff-be @ if swap then ;

: tseek ( lo hi -- byte ) \ bytes are counted from its block
    d# 6 lshift over d# 10 rshift or tfblk !
    h# 3ff and
;

\ An entry is tag, type, count, value: 2, 2, 4 and 4 bytes.

: tval2 ( entry -- lo hi ) \ a single SHORT or LONG value
    dup d# 2 + t16@ d# 3 = if
        d# 8 + t16@ d# 0
    else
        d# 8 + t32@
    then
;

: tval ( entry -- x ) \ as much of it as a cell holds
    tval2 d# 16 lshift or ;

\ if the tag is n, its value is kept, or the entry
: tfield ( entry tag n var -- entry tag )
    >r over = if over tval r@ ! then rdrop
;

: tentry ( entry tag n var -- entry tag )
    >r over = if over r@ ! then rdrop
;

: tiff-header ( -- ifd ) \ its byte order, and its directory
    d# 0 t16@
    dup h# 4d4d = tiff-be !                         \ "MM"
    dup h# 4949 <> swap h# 4d4d <> and d# 38 -throw \ or "II"
    d# 2 t16@ d# 42 <> d# 38 -throw
    d# 4 t32@ tseek  tfblk @ tdblk !
;

: tiff-tag ( entry -- )
    dup t16@                                  ( entry tag )
    d# 256 tw      tfield
    d# 257 th      tfield
    d# 259 tcomp   tfield
    d# 262 tphoto  tfield
    d# 277 tspp    tfield
    d# 278 trps    tfield
    d# 273 tstrips tentry
    2drop
;

: tiff-tags ( ifd -- )
    dup t16@ swap d# 2 +                      ( n entry )
    swap d# 12 * over + swap                  ( end entry )
    begin 2dup xor while dup tiff-tag d# 12 + repeat
    2drop
;

: tiff-window ( -- ) \ a row, a block and a cell
    tcomp @ d# 1 <> d# 38 -throw
    tstrips @ 0= d# 38 -throw
    trps @ 1- th @ u< 0= if th @ trps ! then
    tw @ tspp @ *  BLOCK-CHARS +  CELLB +
    CELLB 1- + c> 1+ d# -2 and  dup traw !
    >c hreuse raw !
;

header tiff-open
: tiff-open ( blk# -- img ) \ the TIFF that starts at a block
    IMG-CELLS >c hreuse  dup img !
    d# 0 swap IMG-CELLS cells-fill
    tiff-base !
    d# 1 tspp !  d# 1 tcomp !  d# 1 tphoto !
    tiff-header tiff-tags tiff-window
    img @
;

header tiff-close
: tiff-close ( img -- ) \ its window, then its record
    img !  raw @ traw @ hgive  img @ IMG-CELLS hgive
;

\ where the strips' starts are: in the entry, or where it says
: starts-at ( entry shorts? -- byte )
    >r  dup d# 4 + t32@ drop  r> if d# 3 else d# 2 then u< if
        d# 8 +
    else
        d# 8 + t32@ tseek
    then
;

: selem ( i -- lo hi ) \ where strip i starts in the file
    tdblk @ tfblk !
    tstrips @  dup d# 2 + t16@ d# 3 = >r
    ( i entry ) ( R: whether they are SHORTs )
    r@ starts-at                          ( i byte )
    r> if
        swap 2* + t16@ d# 0
    else
        swap d# 2 lshift + t32@
    then
;

\ ---- the window

: take ( n -- x ) \ the next n bits, up to a cell
    dup tbn @ > if
        tbp @ @  d# 1 tbp +!            ( n next )
        dup tbn @ lshift tbuf @ or >r   \ with rubbish above
        over tbn @ - rshift tbuf !
        CELLBITS over - tbn +!
        drop r>
    else
        tbuf @ 2dup swap rshift tbuf !  ( n x )
        swap negate tbn +!
    then
;

: win-block ( -- ) \ the next block, after what the window has
    tblk @ blk-cur @ = if flush then
    wend @ tblk @ blk-read
    BLKCELLS wend +!  d# 1 tblk +!
;

: next-strip ( -- )
    tstrip @ selem  d# 1 tstrip +!                  ( lo hi )
    tseek  tfblk @ tiff-base @ + tblk !             ( byte )
    raw @ wend !  win-block
    dup BYTESHIFT rshift raw @ + tbp !  d# 0 tbn !  d# 0 tbuf !
    CELLB 1- and d# 3 lshift ?dup if    \ its rows are not cells
        take drop  d# 0 'grays !  d# 0 'pairs !
    then
    trps @ tsleft !
;

: row-there ( -- ) \ the next row is all in the window
    tw @ tspp @ * d# 3 lshift  tbn @ -
    CELLBITS 1- + BITSHIFT rshift       ( n ) \ cells to fetch
    dup tbp @ + wend @ u> if
        tbp @ raw @  wend @ tbp @ -  dup >r cells-move
        raw @ dup tbp !  r> + wend !
        dup raw @ +                     ( n need )
        begin dup wend @ u> while win-block repeat
        drop
    then
    drop
;

header tiff-row
: tiff-row ( img -- ) \ in hand, its next row there to be taken
    img !
    tsleft @ 0= if next-strip then
    d# -1 tsleft +!
    row-there
;

header image-is \ an image that is in memory already
: image-is ( cell w h samples -- img )
    new-image
    tspp !  th !  tw !  raw !
    d# 1 tphoto !
    d# -1 wend !
    img @
;

\ ---- a pixel's gray: 77 R + 150 G + 29 B over 256

: gray1 ( -- g )
    d# 8 take h# ff and  tphoto @ 0= if d# 255 xor then ;

: grayn ( -- g ) \ the first of several samples
    gray1
    tspp @ d# 1 begin 2dup xor while d# 8 take drop 1+ repeat
    2drop
;

\ of a pixel in the low three bytes of a cell, red lowest
W32+? [if]
meta
:: WEIGH24 ( x -- g )
    s" dup h# ff and d# 77 *" evaluate
    s" over d# 8 rshift h# ff and d# 150 * +" evaluate
    s" swap d# 16 rshift h# ff and d# 29 * +" evaluate
    s" d# 8 rshift" evaluate
;
target
[then]

W32+? [if]
: gray24 ( x -- g ) WEIGH24 ;
: rgb ( -- g ) d# 24 take gray24 ;
[else]
: rgb ( -- g )
    d# 8 take h# ff and d# 77 *
    d# 8 take h# ff and d# 150 * +
    d# 8 take h# ff and d# 29 * +
    d# 8 rshift
;
[then]

\ ---- a row's grays, a cell at a time: a gray is the low eight
\ bits of a lane of sixteen

header gray-mask  \ a gray in each lane
create gray-mask  meta $00ff00ff00ff00ff cell-mask and target ,
W64? [if]
header pair-mask  \ the lower lane of each two
create pair-mask  meta $0000ffff0000ffff target ,
[then]

\ Three cells of RGB are two cells of lanes: lanes-a, lanes-b.

\ two pixels' grays at bits 0 and 24, with junk above each:
\ h# ff0000ff is one colour of both pixels, 24 bits apart, so
\ one multiply weighs both: 77, 150, 29 of 256 for R, G, B
W64? [if]
meta
:: WEIGHED ( w -- x )
    s" dup h# ff0000ff and d# 77 *" evaluate
    s" over d# 8 rshift h# ff0000ff and d# 150 * +" evaluate
    s" swap d# 16 rshift h# ff0000ff and d# 29 * +" evaluate
    s" d# 8 rshift" evaluate
;
target
[then]

W64? [if]
: pair ( w -- y ) \ two pixels' grays, in two lanes
    WEIGHED  dup h# ff and  swap d# 8 rshift h# ff0000 and  or
;
: twain ( w -- s ) \ the sum of two pixels' grays
    WEIGHED  dup d# 24 rshift +  h# 1ff and
;
[then]

\ the pixels in the three cells at p, in pairs, a sum a lane
W64? [if]
: sums ( p -- y )
    dup CELL2 + @ d# 16 rshift twain d# 48 lshift >r  \ 6 and 7
    dup 1+ @ d# 32 rshift  over CELL2 + @ d# 32 lshift or
    twain d# 32 lshift r> or >r                       \ 4 and 5
    dup @ d# 48 rshift  over 1+ @ d# 16 lshift or
    twain d# 16 lshift r> or >r                       \ 2 and 3
    @ twain r> or                                     \ 0 and 1
;
[then]

W64? [if]
: quad ( wa wb -- y ) \ four pixels' grays, in four lanes
    pair swap pair swap d# 32 lshift or
;
: lanes-a ( p -- y )
    dup @  swap dup @ d# 48 rshift swap 1+ @ d# 16 lshift or
    quad
;
: lanes-b ( p -- y )
    dup 1+ @ d# 32 rshift over CELL2 + @ d# 32 lshift or
    swap CELL2 + @ d# 16 rshift  quad
;
[then]

W32? [if]
: lanes-a ( p -- y )
    dup @ tuck  d# 24 rshift swap 1+ @ d# 8 lshift or WEIGH24
    d# 16 lshift  swap WEIGH24 or
;
: lanes-b ( p -- y )
    dup 1+ @ d# 16 rshift  swap CELL2 + @ tuck
    d# 16 lshift or WEIGH24
    swap d# 8 rshift WEIGH24  d# 16 lshift or
;
[then]

W32? [if]
: sums ( p -- y )
    dup CELL2 + @ >r  dup 1+ @ >r  @  ( c0 ) ( R: c2 c1 )
    dup WEIGH24  swap d# 24 rshift r@ d# 8 lshift or WEIGH24  +
    r> d# 16 rshift r@ d# 16 lshift or WEIGH24
    r> d# 8 rshift WEIGH24  +
    d# 16 lshift or
;
[then]

\ two pixels in three cells of 16 bits: the first's gray, and
\ the second's
W32+? [if] [else]
meta
:: PIXEL-A ( p -- y )
    s" dup @  dup h# ff and d# 77 *" evaluate
    s" swap d# 8 rshift d# 150 * +" evaluate
    s" swap 1+ @ h# ff and d# 29 * +" evaluate
    s" d# 8 rshift" evaluate
;
target
[then]

W32+? [if] [else]
meta
:: PIXEL-B ( p -- y )
    s" dup 1+ @ d# 8 rshift d# 77 *" evaluate
    s" swap CELL2 + @  dup h# ff and d# 150 *" evaluate
    s" swap d# 8 rshift d# 29 * +  +" evaluate
    s" d# 8 rshift" evaluate
;
target
[then]

: gray-lanes ( lanes -- ) \ a row's grays, each to its lane
    tbp @ >r  ( a ) ( R: the three cells )
    tw @ BYTESHIFT rshift 2*  over + swap  ( end a )
    begin 2dup xor while
W32+? [if]
        r@ lanes-a over +!  r@ lanes-b over 1+ +!
[else]
        r@ PIXEL-A over +!  r@ PIXEL-B over 1+ +!
[then]
        CELL2 +  r> c# 3 + >r
    repeat
    2drop  r> tbp !
;

\ lanes added in pairs, the sums in the lower half of the cell
W64? [if]
header fold-pairs
: fold-pairs ( y -- z )
    dup d# 16 rshift +  pair-mask @ and
    dup d# 16 rshift or  HALFMASK and
;
[then]
W32? [if]
header fold-pairs
: fold-pairs ( y -- z )
    dup d# 16 rshift +  HALFMASK and
;
[then]

: gray-pairs ( lanes -- ) \ the same, two pixels to a lane
    tbp @ >r  ( a ) ( R: the three cells )
    tw @ BYTESHIFT rshift over + swap  ( end a )
    begin 2dup xor while
W32+? [if]
        r@ sums
[else]
        r@ PIXEL-A  r@ PIXEL-B +
[then]
        over +!  1+  r> c# 3 + >r
    repeat
    2drop  r> tbp !
;

\ the grays in half a cell, each in a lane, the first lowest
: spread ( x -- y )
W64? [if]
    dup d# 16 lshift or  pair-mask @ and
[then]
W32+? [if]
    dup d# 8 lshift or  gray-mask @ and
[then]
;

: white-row ( -- ) \ the next row inverted, if white is 0
    tphoto @ if exit then
    tbp @  tw @ BYTESHIFT rshift  bounds
    begin
        2dup xor
    while
        dup not!
        1+
    repeat
    2drop
;

: gray1-lanes ( lanes -- ) \ of a row of one sample
    white-row
    >r  ( R: the two cells of lanes a cell of the row is )
    tbp @  tw @ BYTESHIFT rshift  bounds  ( end p )
    begin
        2dup xor
    while
        dup @ HALFMASK and spread  r@ +!
        dup @ HALFBITS rshift spread  r@ 1+ +!
        1+  r> CELL2 + >r
    repeat
    drop tbp !  rdrop
;

: gray1-pairs ( lanes -- )
    white-row
    tbp @ - >r
    ( R: from a cell of the row to its cell of lanes )
    tbp @  tw @ BYTESHIFT rshift  bounds  ( end p )
    begin
        2dup xor
    while
        GRAY-PAIR
        over r@ + +!
        1+
    repeat
    drop tbp !  rdrop
;

\ ---- how an image's pixels are asked for

header image! : image! ( img -- ) img ! ;

: pixel-how ( -- )
    tspp @ d# 3 = if
        ['] rgb
    else
        tspp @ d# 1 = if ['] gray1 else ['] grayn then
    then
    'pixel !
;

: rows-how ( -- ) \ 'grays and 'pairs, if a row is whole cells
    d# 0 'grays !  d# 0 'pairs !
    tw @ CELLB 1- and if exit then
    tspp @ d# 3 = if
        ['] gray-lanes 'grays !  ['] gray-pairs 'pairs !
    then
    tspp @ d# 1 = if
        ['] gray1-lanes 'grays !  ['] gray1-pairs 'pairs !
    then
;

header pixels
: pixels ( -- ) \ the image in hand, from its first pixel
    raw @ tbp !  d# 0 tbn !  d# 0 tbuf !
    d# 0 tstrip !
    tstrips @ if d# 0 else th @ then tsleft !
    pixel-how rows-how
;
