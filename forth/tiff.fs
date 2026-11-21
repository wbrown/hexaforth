\ tiff.fs - read an uncompressed TIFF from the block device, a
\ row at a time

\ Either byte order, 8-bit gray or RGB, any number of strips.
\ The image is never all in memory: what is kept is a window, a
\ row and a block long.  A place in the file is a block and a
\ byte in it, never a count of bytes.

\ An image is a record: `img` is the one in hand, and the words
\ below are its cells.

create img 0 ,          \ the image in hand: its record

$18 constant IMG-CELLS  \ even, since hreuse gives even numbers
                        \ of cells

header tw : tw     ( -- a ) \ width, height
    img @ ;
header th : th     ( -- a ) img @ 1+ ;
: tspp      ( -- a ) \ samples per pixel
    img @ CELL2 + ;
: tphoto    ( -- a )
    \ 0: white is zero  1: black is zero  2: RGB
    img @ c# 3 + ;
: tcomp     ( -- a ) \ compression
    img @ c# 4 + ;
: tiff-base ( -- a )
    \ the block of the device the file starts at
    img @ c# 5 + ;
: tiff-be   ( -- a ) \ true for a big-endian file
    img @ c# 6 + ;

: tfblk     ( -- a )
    \ the block of the file that bytes are counted from
    img @ c# 7 + ;
: tdblk     ( -- a ) \ the block the directory is in,
    img @ c# 8 + ;
: tstrips   ( -- a )
    \ and the byte from there of its StripOffsets entry, or 0
    img @ c# 9 + ;
: trps      ( -- a ) \ rows in a strip
    img @ c# 10 + ;
: raw       ( -- a )
    \ the window; or the whole image, if it is in memory
    img @ c# 11 + ;
: wend      ( -- a )
    \ the cell after the last that has been read into it
    img @ c# 12 + ;

: tblk      ( -- a ) \ the block of the device that comes next
    img @ c# 13 + ;
: tstrip    ( -- a ) \ the strip that comes next,
    img @ c# 14 + ;
: tsleft    ( -- a )
    \ and the rows still to come of the one being read
    img @ c# 15 + ;
: tbuf      ( -- a ) \ bits fetched and not yet taken,
    img @ c# 16 + ;
: tbn       ( -- a ) img @ c# 17 + ;                \ how many,
: tbp       ( -- a ) \ and the next cell
    img @ c# 18 + ;
header 'pixel : 'pixel ( -- a )
    \ ( -- g )  the next pixel's gray
    img @ c# 19 + ;

header 'grays : 'grays ( -- a )
    \ ( lanes -- )  a row's grays, or 0: see rows-how
    img @ c# 20 + ;
header 'pairs : 'pairs ( -- a )
    \ ( lanes -- )  the same, two pixels to a lane, or 0
    img @ c# 21 + ;
: traw      ( -- a )
    \ the window's size in cells, when reading a file
    img @ c# 22 + ;

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

\ the dither says it too, and a macro is its file's own: so a
\ word with a header says it (text.fs has the same for its own)
header HALFMASK : half-mask HALFMASK ;

: new-image ( -- ) \ a record with nothing in it, in hand
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

\ a place in the file: bytes are counted from its block, and it
\ is this one
: tseek ( lo hi -- byte )
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

: tfield ( entry tag n var -- entry tag )
    \ if the tag is n, keep its value
    >r over = if over tval r@ ! then rdrop
;

: tentry ( entry tag n var -- entry tag )
    \ if the tag is n, keep the entry
    >r over = if over r@ ! then rdrop
;

: tiff-header ( -- ifd ) \ its byte order, and its directory
    d# 0 t16@
    dup h# 4d4d = tiff-be !                         \ "MM"
    dup h# 4949 <> swap h# 4d4d <> and d# 38 -throw \ or "II"
    d# 2 t16@ d# 42 <> d# 38 -throw
    d# 4 t32@ tseek  tfblk @ tdblk !
;

: tiff-tag ( entry -- ) \ what an entry of the directory says
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

: tiff-tags ( ifd -- ) \ what the directory's entries say
    dup t16@ swap d# 2 +                      ( n entry )
    swap d# 12 * over + swap                  ( end entry )
    begin 2dup xor while dup tiff-tag d# 12 + repeat
    2drop
;

\ Only an uncompressed file is read.  The window is a row, a
\ block and a cell, made an even number of cells.
: tiff-window ( -- )
    tcomp @ d# 1 <> d# 38 -throw
    tstrips @ 0= d# 38 -throw
    trps @ 1- th @ u< 0= if th @ trps ! then
    tw @ tspp @ *  BLOCK-CHARS +  CELLB +
    CELLB 1- + c> 1+ d# -2 and  dup traw !
    >c hreuse raw !
;

header tiff-open
: tiff-open ( blk# -- img )
    \ read the directory of the TIFF starting at a block
    IMG-CELLS >c hreuse  dup img !
    d# 0 swap IMG-CELLS cells-fill
    tiff-base !
    d# 1 tspp !  d# 1 tcomp !  d# 1 tphoto !
    tiff-header tiff-tags tiff-window
    img @
;

\ free what reading the file took: its window, then its record
header tiff-close
: tiff-close ( img -- )
    img !  raw @ traw @ hgive  img @ IMG-CELLS hgive
;

\ where the strips' starts are: in the entry, if they fit in it,
\ and if not, where it says
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

\ tbuf holds the bits fetched and not yet taken, lowest first,
\ and tbn how many.  `take` gives the next n, up to a cell.
: take ( n -- x )
    dup tbn @ > if
        tbp @ @  d# 1 tbp +!            ( n next )
        dup tbn @ lshift tbuf @ or >r   \ the bits, with
                                        \ rubbish above
        over tbn @ - rshift tbuf !      \ the rest of the cell
        CELLBITS over - tbn +!
        drop r>
    else
        tbuf @ 2dup swap rshift tbuf !  ( n x )
        swap negate tbn +!
    then
;

: win-block ( -- ) \ the next block, after what the window has
    tblk @ blk-cur @ = if flush then  \ as it now is, if it is
                                      \ the one being edited
    wend @ tblk @ blk-read
    BLKCELLS wend +!  d# 1 tblk +!
;

: next-strip ( -- ) \ go to where the next strip starts
    tstrip @ selem  d# 1 tstrip +!                  ( lo hi )
    tseek  tfblk @ tiff-base @ + tblk !             ( byte )
    raw @ wend !  win-block
    dup BYTESHIFT rshift raw @ + tbp !  d# 0 tbn !  d# 0 tbuf !
    \ it does not start where a cell does:
    CELLB 1- and d# 3 lshift ?dup if
        \ then its rows are not cells
        take drop  d# 0 'grays !  d# 0 'pairs !
    then
    trps @ tsleft !
;

\ What has not been taken is moved to the front, and blocks are
\ read after it: nothing is read past the end of the image.
: row-there ( -- )
    \ read on until the next row is all in the window
    tw @ tspp @ * d# 3 lshift  tbn @ -  \ the bits of it that
                                        \ are not fetched yet,
    CELLBITS 1- + BITSHIFT rshift       ( n ) \ as cells
    dup tbp @ + wend @ u> if
        tbp @ raw @  wend @ tbp @ -  dup >r cells-move
        raw @ dup tbp !  r> + wend !
        dup raw @ +                     ( n need )
        begin dup wend @ u> while win-block repeat
        drop
    then
    drop
;

\ take the image in hand, with its next row all there to
\ be taken
header tiff-row
: tiff-row ( img -- )
    img !
    tsleft @ 0= if next-strip then
    d# -1 tsleft +!
    row-there
;

\ An image that is in memory already: one strip, and a window
\ that has everything in it.
header image-is
: image-is ( cell w h samples -- img )
    \ gray, black being nothing, if it has one sample to a pixel
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

\ the gray of a pixel in the low three bytes of a cell, red
\ lowest: a macro, compiled in line where rows are converted
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

\ ---- a row's grays, a cell at a time

\ Whoever dithers keeps each pixel in a lane, sixteen bits of a
\ cell, and a gray is the low eight of its lane.
header gray-mask
\ a gray in each lane
create gray-mask  meta $00ff00ff00ff00ff cell-mask and target ,
W64? [if]
\ the lower lane of each two
header pair-mask
create pair-mask  meta $0000ffff0000ffff target ,
[then]

\ Three cells of a row of three samples are as many pixels as a
\ cell has bytes, and two cells of lanes: lanes-a and lanes-b.
\ At half size each two grays are added as they are made: sums.

\ grays of two pixels, first lowest, at bits 0 and 24, with junk
\ above each: a macro, since it runs for every two pixels
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

\ the eight pixels in the three cells at p: pairs of grays
\ summed, in four lanes
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

\ the eight pixels in the three cells at p, four and four
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

\ With cells of 32 bits a cell is a pixel and a byte of the
\ next: four pixels are three cells, two and two.
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

\ the four pixels in the three cells at p: pairs of grays
\ summed, in two lanes
W32? [if]
: sums ( p -- y )
    dup CELL2 + @ >r  dup 1+ @ >r  @  ( c0 ) ( R: c2 c1 )
    \ pixels 0 and 1,
    dup WEIGH24  swap d# 24 rshift r@ d# 8 lshift or WEIGH24  +
    \ 2 and 3
    r> d# 16 rshift r@ d# 16 lshift or WEIGH24
    r> d# 8 rshift WEIGH24  +
    d# 16 lshift or
;
[then]

\ With cells of 16 bits two pixels are three cells: red and
\ green, blue and red, green and blue.  A pixel's gray is a cell
\ of lanes by itself.  Macros: the first pixel's, and the
\ second's.
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

: gray-lanes ( lanes -- )
    \ the next row's grays, each added to its pixel's lane
    tbp @ >r  ( a ) ( R: the three cells )
    \ two cells of lanes to three of the row
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

\ adjacent lanes added in pairs, the sums packed in the lower
\ half of the cell: for the dither to sixteen levels too
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

: gray-pairs ( lanes -- )
    \ the next row's grays, each two pixels' added into a lane
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

\ A cell of a row of one sample is twice as many grays as it has
\ lanes: each half of it is a cell of lanes, its grays moved
\ apart.

\ the grays in half a cell, nothing above them: each in a lane,
\ the first lowest
: spread ( x -- y )
W64? [if]
    dup d# 16 lshift or  pair-mask @ and
[then]
W32+? [if]
    dup d# 8 lshift or  gray-mask @ and
[then]
;

\ the next row's grays, if white is nothing: each made what
\ black being nothing has it, where it is
: white-row ( -- )
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

: gray1-lanes ( lanes -- )
    \ the next row's grays, each added to its pixel's lane
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
    \ the next row's grays, each two pixels' added into a lane
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

header image! : image! ( img -- ) \ take an image in hand
    img ! ;

: pixel-how ( -- ) \ how a pixel of the image is asked for
    tspp @ d# 3 = if
        ['] rgb
    else
        tspp @ d# 1 = if ['] gray1 else ['] grayn then
    then
    'pixel !
;

\ 'grays and 'pairs are 0 where a row is not whole cells, or
\ does not start where a cell does: then it is asked a pixel at
\ a time.
: rows-how ( -- ) \ and a row, if a row is whole cells
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
: pixels ( -- )
    \ the image in hand starts again from its first pixel
    raw @ tbp !  d# 0 tbn !  d# 0 tbuf !
    d# 0 tstrip !
    \ a file starts at its first strip
    tstrips @ if d# 0 else th @ then tsleft !
    pixel-how rows-how
;
