\ tiff.fs - read an uncompressed TIFF from the block device, a row at a time
\
\ Handles either byte order, 8-bit grayscale or RGB, and any number of
\ strips.  The image is never all in memory: it may be bigger than memory
\ is.  What is kept of it is a window, a row and a block long.  Blocks are
\ read into the window as they are stored, just before the row that is in
\ them is wanted, and `pixel` gives the row out of the window a pixel at a
\ time; what has been taken is let go when the next row needs the room.
\
\ Nothing here is reached by the address of a byte, and no place in the
\ file is kept as a count of bytes: a file may be longer than a cell can
\ count, and the window may be where the address of a byte cannot say.  A
\ place in the file is a block and a byte in it.

\
\ An image is a record, so that there may be more than one: what its file
\ says of it, where its window is and what is in it, and where its pixels
\ have been taken up to.  `img` is the image in hand, and the words below
\ are its cells.  Nothing here gives another task a turn, so an image is in
\ hand for as long as the word that took it up runs; but no longer, and
\ every word that others use takes the image it is to work on and takes it
\ in hand.

create img 0 ,          \ the image in hand: its record

$16 constant IMG-CELLS

header tw : tw     ( -- a ) img @ ;                 \ width, height
header th : th     ( -- a ) img @ 1+ ;
: tspp      ( -- a ) img @ CELL2 + ;                \ samples per pixel
: tphoto    ( -- a ) img @ c# 3 + ;                 \ 0: white is zero  1: black is zero  2: RGB
: tcomp     ( -- a ) img @ c# 4 + ;                 \ compression
: tiff-base ( -- a ) img @ c# 5 + ;                 \ the block of the device the file starts at
: tiff-be   ( -- a ) img @ c# 6 + ;                 \ true for a big-endian file
: tfblk     ( -- a ) img @ c# 7 + ;                 \ the block of the file that bytes are counted from
: tdblk     ( -- a ) img @ c# 8 + ;                 \ the block the directory is in,
: tstrips   ( -- a ) img @ c# 9 + ;                 \ and the byte from there of its StripOffsets entry, or 0
: trps      ( -- a ) img @ c# 10 + ;                \ rows in a strip
: raw       ( -- a ) img @ c# 11 + ;                \ the window; or the whole image, if it is in memory
: wend      ( -- a ) img @ c# 12 + ;                \ the cell after the last that has been read into it
: tblk      ( -- a ) img @ c# 13 + ;                \ the block of the device that comes next
: tstrip    ( -- a ) img @ c# 14 + ;                \ the strip that comes next,
: tsleft    ( -- a ) img @ c# 15 + ;                \ and the rows still to come of the one being read
: tbuf      ( -- a ) img @ c# 16 + ;                \ bits fetched and not yet taken,
: tbn       ( -- a ) img @ c# 17 + ;                \ how many,
: tbp       ( -- a ) img @ c# 18 + ;                \ and the next cell
header 'pixel : 'pixel ( -- a ) img @ c# 19 + ;     \ ( -- g )  the next pixel's gray
header 'grays : 'grays ( -- a ) img @ c# 20 + ;     \ ( lanes -- )  a row's grays, or 0: see below
header 'pairs : 'pairs ( -- a ) img @ c# 21 + ;     \ ( lanes -- )  the same, two pixels to a lane, or 0

meta
:: BLKCELLS  #1024 cell-bytes / literal ;           \ cells in a block
:: BLKSHIFT  #10 byte-shift - literal ;             \ from a count of cells to a count of blocks
:: BITSHIFT  byte-shift 3 + literal ;               \ from a count of bits to a count of cells
:: HALFMASK  1 cell-bits 2/ lshift 1- literal ;     \ the lower half of a cell
target

: new-image ( -- ) \ a record with nothing in it, in hand
    IMG-CELLS >c halloc  dup img !
    d# 0 swap IMG-CELLS cells-fill
;

\ The directory is read through the block buffer, a byte at a time: a
\ byte is shifted out of the cell it is in.  A byte's place is counted from
\ the start of a block of the file (tfblk), so that it is a small number
\ wherever in the file it is, and a field may lie across two blocks.  A
\ number of 32 bits is its two halves, which a cell of any size holds.

: byte@ ( a n -- b ) \ byte n of the cells from a; a cell's lowest byte is its first
    dup BYTESHIFT rshift rot + @
    swap CELLB 1- and d# 3 lshift rshift
    h# ff and
;

: t8@ ( byte -- b )
    dup d# 10 rshift tfblk @ + tiff-base @ + cblock
    swap h# 3ff and byte@
;

: t16@ ( byte -- x ) dup t8@ swap 1+ t8@  tiff-be @ if swap then  d# 8 lshift or ;
: t32@ ( byte -- lo hi ) dup t16@ swap d# 2 + t16@  tiff-be @ if swap then ;

: tseek ( lo hi -- byte ) \ a place in the file: bytes are counted from its block, and it is this one
    d# 6 lshift over d# 10 rshift or tfblk !
    h# 3ff and
;

\ A directory entry is tag, type, count, value: 2, 2, 4 and 4 bytes.  A
\ SHORT (type 3) value sits in the first two bytes of the value field.

: tval2 ( entry -- lo hi ) \ a single SHORT or LONG value
    dup d# 2 + t16@ d# 3 = if
        d# 8 + t16@ d# 0
    else
        d# 8 + t32@
    then
;

: tval ( entry -- x ) tval2 d# 16 lshift or ;       \ as much of it as a cell holds

: tfield ( entry tag n var -- entry tag ) \ if the tag is n, keep its value
    >r over = if over tval r@ ! then rdrop
;

: tentry ( entry tag n var -- entry tag ) \ if the tag is n, keep the entry
    >r over = if over r@ ! then rdrop
;

header tiff-open
: tiff-open ( blk# -- img ) \ read the directory of the TIFF starting at a block
    new-image
    tiff-base !
    d# 1 tspp !  d# 1 tcomp !  d# 1 tphoto !
    d# 0 t16@
    dup h# 4d4d = tiff-be !                         \ "MM"
    dup h# 4949 <> swap h# 4d4d <> and d# 38 -throw \ or "II"
    d# 2 t16@ d# 42 <> d# 38 -throw
    d# 4 t32@ tseek  tfblk @ tdblk !                ( ifd )
    dup t16@ swap d# 2 +                            ( n entry )
    swap d# 12 * over + swap                        ( end entry )
    begin
        2dupxor
    while
        dup t16@                                    ( n entry tag )
        d# 256 tw      tfield
        d# 257 th      tfield
        d# 259 tcomp   tfield
        d# 262 tphoto  tfield
        d# 277 tspp    tfield
        d# 278 trps    tfield
        d# 273 tstrips tentry
        drop d# 12 +
    repeat
    2drop
    tcomp @ d# 1 <> d# 38 -throw                    \ uncompressed only
    tstrips @ 0= d# 38 -throw
    trps @ 1- th @ u< 0= if th @ trps ! then        \ a file that does not say has one strip
    tw @ tspp @ *  d# 1024 +  CELLB + halloc raw !  \ the window: a row, a block, and a cell
    img @
;

: selem ( i -- lo hi ) \ where strip i starts in the file
    tdblk @ tfblk !
    tstrips @  dup d# 2 + t16@ d# 3 = >r            ( i entry ) ( R: whether they are SHORTs )
    dup d# 4 + t32@ drop  r@ if d# 3 else d# 2 then u< if
        d# 8 +                                      \ values that fit in the entry are in it
    else
        d# 8 + t32@ tseek                           \ and more are where it says
    then                                            ( i byte )
    r> if
        swap 2* + t16@ d# 0
    else
        swap d# 2 lshift + t32@
    then
;

\ The image is a run of bits, and memory is cells.  A cell is fetched once
\ and what is wanted is shifted out of it: tbuf holds the bits fetched and
\ not yet taken, lowest first, and tbn how many there are.  `take` gives
\ the next n, for n up to a cell, with whatever follows them above; when
\ there are fewer than n it fetches the next cell, and the result is made
\ of both.

: take ( n -- x )
    dup tbn @ > if
        tbp @ @  d# 1 tbp +!                        ( n next )
        dup tbn @ lshift tbuf @ or >r               \ the bits, with rubbish above
        over tbn @ - rshift tbuf !                  \ the rest of the cell
        CELLBITS over - tbn +!
        drop r>
    else
        tbuf @ 2dup swap rshift tbuf !              ( n x )
        swap negate tbn +!
    then
;

\ The window.  A block is read where the last one ended, so the bytes of
\ the file are next to each other in it as they are in the file, and a row
\ is a run of cells.  When the next row is not all there, the cells that
\ have not been taken are moved to the front and blocks are read after
\ them until it is: so a block is read only if a row that is wanted has
\ bytes in it, and nothing is read past the end of the image.
\
\ A strip is whole rows, and may be anywhere in the file.  At the first
\ row of one the window starts again, from the block the strip starts in.

: win-block ( -- ) \ the next block, after what the window has
    tblk @ blk-cur @ = if flush then                \ as it now is, if it is the one being edited
    wend @ tblk @ blk-read
    BLKCELLS wend +!  d# 1 tblk +!
;

: next-strip ( -- ) \ go to where the next strip starts
    tstrip @ selem  d# 1 tstrip +!                  ( lo hi )
    tseek  tfblk @ tiff-base @ + tblk !             ( byte )
    raw @ wend !  win-block
    dup BYTESHIFT rshift raw @ + tbp !  d# 0 tbn !  d# 0 tbuf !
    CELLB 1- and d# 3 lshift ?dup if                \ it does not start where a cell does:
        take drop  d# 0 'grays !  d# 0 'pairs !     \ then its rows are not cells
    then
    trps @ tsleft !
;

: row-there ( -- ) \ read on until the next row is all in the window
    tw @ tspp @ * d# 3 lshift  tbn @ -              \ the bits of it that are not fetched yet,
    CELLBITS 1- + BITSHIFT rshift                   ( n ) \ as cells
    dup tbp @ + wend @ u> if
        tbp @ raw @  wend @ tbp @ -  dup >r cells-move
        raw @ dup tbp !  r> + wend !
        dup raw @ +                                 ( n need )
        begin dup wend @ u> while win-block repeat
        drop
    then
    drop
;

header tiff-row
: tiff-row ( img -- ) \ take the image in hand, with its next row all there to be taken
    img !
    tsleft @ 0= if next-strip then
    d# -1 tsleft +!
    row-there
;

\ An image that is in memory already, as its strips would be if they had
\ been read: for whoever has one from somewhere other than a file.  It is
\ one strip, and a window that has everything in it.
header image-is
: image-is ( cell w h samples -- img ) \ gray, black being nothing, if it has one sample to a pixel
    new-image
    tspp !  th !  tw !  raw !
    d# 1 tphoto !
    d# -1 wend !
    img @
;

\ A pixel's gray: its one sample, or 77 R + 150 G + 29 B over 256.  A pixel
\ of three samples is taken whole if a cell holds it, and its fields
\ shifted out on the stack; in a 16-bit cell it is taken a sample at a time.

: gray1 ( -- g ) d# 8 take h# ff and  tphoto @ 0= if d# 255 xor then ;

: grayn ( -- g ) \ the first of several samples
    gray1  tspp @ d# 1 begin 2dupxor while d# 8 take drop 1+ repeat 2drop
;

W32+? [if]
: gray24 ( x -- g ) \ a pixel in the low three bytes of a cell, its red the lowest, whatever is above: its gray
    dup h# ff and d# 77 *
    over d# 8 rshift h# ff and d# 150 * +
    swap d# 16 rshift h# ff and d# 29 * +
    d# 8 rshift
;
: rgb ( -- g ) d# 24 take gray24 ;
[else]
: rgb ( -- g )
    d# 8 take h# ff and d# 77 *
    d# 8 take h# ff and d# 150 * +
    d# 8 take h# ff and d# 29 * +
    d# 8 rshift
;
[then]

\ Whoever dithers keeps what each pixel is owed in a lane, sixteen bits of
\ a cell, as many to a cell as fit, and a gray is the low eight of its
\ lane.  Where a row lies on cells its grays are added to those lanes a
\ cell at a time, and nothing is fetched for a pixel.
header gray-mask
create gray-mask  meta $00ff00ff00ff00ff cell-mask and target ,     \ a gray in each lane
W64? [if]
create pair-mask  meta $0000ffff0000ffff target ,   \ the lower lane of each two
[then]

\ A row of an image of three samples, where the row lies on cells, is not
\ taken a pixel at a time, with a cell of any size.  Three cells of the
\ row are as many pixels as a cell has bytes, and that is two cells of
\ lanes: the grays of the first half of those pixels (lanes-a) and of the
\ second (lanes-b), each made from the three cells where they are.  How
\ is the cell's size.

W64? [if]

\ Eight pixels are three cells.  Two pixels are six bytes of a cell, and
\ their grays are made together: the two reds are masked out where they
\ are and multiplied by 77, the greens and the blues shifted down to the
\ same two places and multiplied by theirs, and the three added, which is
\ each pixel's gray times 256 with nothing carried from one to the other.
\ Four pixels' grays are then a cell of four lanes.

: pair ( w -- y ) \ two pixels, the first lowest: their grays, in two lanes
    dup h# ff0000ff and d# 77 *
    over d# 8 rshift h# ff0000ff and d# 150 * +
    swap d# 16 rshift h# ff0000ff and d# 29 * +     \ each times 256, and twenty-four bits apart
    d# 8 rshift
    dup h# ff and  swap d# 8 rshift h# ff0000 and  or
;

: quad ( wa wb -- y ) \ four pixels' grays, in four lanes
    pair swap pair swap d# 32 lshift or
;

\ The eight pixels in the three cells at p: the first two are the first
\ cell's, and each two after start sixteen bits further up the cell before.
: lanes-a ( p -- y ) \ the first four
    dup @  swap dup @ d# 48 rshift swap 1+ @ d# 16 lshift or  quad
;
: lanes-b ( p -- y ) \ and the next four
    dup 1+ @ d# 32 rshift over CELL2 + @ d# 32 lshift or
    swap CELL2 + @ d# 16 rshift  quad
;

[then]

W32? [if]

\ A cell is a pixel and a byte of the next, so four pixels are three
\ cells.  Each is shifted down to the bottom of a cell, with whatever is
\ above it, and its gray made there; two grays are a cell of two lanes.

: lanes-a ( p -- y ) \ the first two: the first cell, and its top byte with two of the second
    dup @ tuck  d# 24 rshift swap 1+ @ d# 8 lshift or gray24
    d# 16 lshift  swap gray24 or
;
: lanes-b ( p -- y ) \ the next two: half the second cell with a byte of the third, and the rest of the third
    dup 1+ @ d# 16 rshift  swap CELL2 + @ tuck  d# 16 lshift or gray24
    swap d# 8 rshift gray24  d# 16 lshift or
;

[then]

W32+? [if] [else]

\ A cell is two samples, so two pixels are three cells: red and green,
\ blue and red, green and blue.  A pixel's gray is made of its three
\ samples where they are, and is a cell of lanes by itself.  The most the
\ three come to, before the eight bits are shifted off, is 65,280, which a
\ cell holds.

: lanes-a ( p -- y ) \ the first pixel: the first cell, and the low half of the second
    dup @  dup h# ff and d# 77 *  swap d# 8 rshift d# 150 * +
    swap 1+ @ h# ff and d# 29 * +
    d# 8 rshift
;
: lanes-b ( p -- y ) \ the second: the high half of the second cell, and the third
    dup 1+ @ d# 8 rshift d# 77 *
    swap CELL2 + @  dup h# ff and d# 150 *  swap d# 8 rshift d# 29 * +  +
    d# 8 rshift
;

[then]

: gray-lanes ( lanes -- ) \ the next row's grays, each added to its pixel's lane
    tbp @ >r                                        ( a ) ( R: the three cells )
    tw @ BYTESHIFT rshift 2*  over + swap           ( end a ) \ two cells of lanes to three of the row
    begin
        2dupxor
    while
        r@ lanes-a over +!
        r@ lanes-b over 1+ +!
        CELL2 +  r> c# 3 + >r
    repeat
    2drop
    r> tbp !
;

\ For a picture at half its size a lane is two pixels of a row, and their
\ grays are added.  The grays in a cell of lanes are folded: the cell is
\ added to itself a lane down, which leaves each two added in the lower
\ lane of the two, and those are brought next to each other in the lower
\ half of the cell.  So the three cells of the row are one cell of sums.
\ With cells of 16 bits a cell of lanes is one gray, and the two are added
\ as they are.
W64? [if]
: fold ( y -- z ) \ four grays in four lanes: each two added, in the lower two lanes
    dup d# 16 rshift +  pair-mask @ and
    dup d# 16 rshift or  HALFMASK and
;
[then]
W32? [if]
: fold ( y -- z ) \ two grays in two lanes: added, in the lower lane
    dup d# 16 rshift +  HALFMASK and
;
[then]

: gray-pairs ( lanes -- ) \ the next row's grays, each two pixels' added into a lane
    tbp @ >r                                        ( a ) ( R: the three cells )
    tw @ BYTESHIFT rshift over + swap               ( end a )
    begin
        2dupxor
    while
W32+? [if]
        r@ lanes-a fold  r@ lanes-b fold HALFBITS lshift or
[else]
        r@ lanes-a  r@ lanes-b +
[then]
        over +!
        1+  r> c# 3 + >r
    repeat
    2drop
    r> tbp !
;

\ Nor is a row of an image of one sample, with a cell of any size.  A cell
\ of the row is as many grays as it has bytes, which is twice as many as
\ it has lanes: so each half of it is a cell of lanes, when its grays have
\ been moved apart.  They are moved half of them at a time: the upper half
\ up by as far as it is long, and then the upper half of each of those,
\ until a gray is by itself at the bottom of a lane.

: spread ( x -- y ) \ the grays in half a cell, nothing above them: each in a lane, the first lowest
W64? [if]
    dup d# 16 lshift or  pair-mask @ and
[then]
W32+? [if]
    dup d# 8 lshift or  gray-mask @ and
[then]
;

\ Where white is nothing a gray is what it is short of white.  The row is
\ in the window to be taken once, so it is made so where it is: not!, an
\ instruction a cell.  (An image that image-is made has black for nothing,
\ and is left as it was given.)
: white-row ( -- ) \ the next row's grays, if white is nothing: each made what black being nothing has it
    tphoto @ if exit then
    tbp @  tw @ BYTESHIFT rshift  bounds
    begin
        2dupxor
    while
        dup not!
        1+
    repeat
    2drop
;

: gray1-lanes ( lanes -- ) \ the next row's grays, each added to its pixel's lane
    white-row
    >r                                              ( R: the two cells of lanes a cell of the row is )
    tbp @  tw @ BYTESHIFT rshift  bounds            ( end p )
    begin
        2dupxor
    while
        dup @ HALFMASK and spread  r@ +!
        dup @ HALFBITS rshift spread  r@ 1+ +!
        1+  r> CELL2 + >r
    repeat
    drop tbp !  rdrop
;

\ At half the size a lane is two pixels of the row, so a cell of the row
\ is one cell of lanes: every other gray where it is, and the others moved
\ down onto them and added.
: gray1-pairs ( lanes -- ) \ the next row's grays, each two pixels' added into a lane
    white-row
    tbp @ - >r                                      ( R: from a cell of the row to its cell of lanes )
    tbp @  tw @ BYTESHIFT rshift  bounds            ( end p )
    begin
        2dupxor
    while
        dup @  dup d# 8 rshift gray-mask @ and  swap gray-mask @ and  +
        over r@ + +!
        1+
    repeat
    drop tbp !  rdrop
;

\ 'grays ( lanes -- ) adds a row's grays to a row of lanes, or is 0 if the
\ image is one whose pixels have to be asked for one at a time: one whose
\ rows are not whole cells, or are not where cells start.  'pairs is the
\ same for a row of half as many lanes.

header image! : image! ( img -- ) img ! ;           \ take an image in hand

header pixels
: pixels ( -- ) \ the image in hand starts again from its first pixel
    raw @ tbp !  d# 0 tbn !  d# 0 tbuf !
    d# 0 tstrip !
    tstrips @ if d# 0 else th @ then tsleft !       \ a file starts at its first strip
    tspp @ d# 3 = if ['] rgb else tspp @ d# 1 = if ['] gray1 else ['] grayn then then
    'pixel !
    d# 0 'grays !  d# 0 'pairs !
    tw @ CELLB 1- and 0= if                         \ a row is whole cells
        tspp @ d# 3 = if ['] gray-lanes 'grays !  ['] gray-pairs 'pairs ! then
        tspp @ d# 1 = if ['] gray1-lanes 'grays !  ['] gray1-pairs 'pairs ! then
    then
;
