\ tiff.fs - read an uncompressed TIFF from the block device into memory
\
\ Handles either byte order, 8-bit grayscale or RGB, and any number of
\ strips.  The strips are read into memory at `raw` as they are stored, and
\ `pixel` gives the image out of them a pixel at a time.

create tiff-base 0 ,    \ device byte offset of the file
create tiff-be   0 ,    \ true for a big-endian file
header tw create tw      0 ,      \ width, height
header th create th      0 ,
create tspp    0 ,      \ samples per pixel
create tphoto  0 ,      \ 0: white is zero  1: black is zero  2: RGB
create tcomp   0 ,      \ compression
create tstrips 0 ,      \ file offsets of the StripOffsets and
create tcounts 0 ,      \ StripByteCounts directory entries
create tscratch 0 ,
create raw     0 ,      \ the strips as stored: the address of a cell

: tbytes ( off n -- a ) \ n bytes of the file, in tscratch
    tscratch >c swap rot tiff-base @ + disk-read
    tscratch >c
;

: t16@ ( off -- x ) d# 2 tbytes  tiff-be @ if be16@ else uw@ then ;
: t32@ ( off -- x ) d# 4 tbytes  tiff-be @ if be32@ else l@ then ;

\ A directory entry is tag, type, count, value: 2, 2, 4 and 4 bytes.  A
\ SHORT (type 3) value sits in the first two bytes of the value field.

: tval ( entry -- x ) \ a single SHORT or LONG value
    dup d# 2 + t16@ d# 3 = if
        d# 8 + t16@
    else
        d# 8 + t32@
    then
;

: tfield ( entry tag n var -- entry tag ) \ if the tag is n, keep its value
    >r over = if over tval r@ ! then rdrop
;

: tentry ( entry tag n var -- entry tag ) \ if the tag is n, keep the entry
    >r over = if over r@ ! then rdrop
;

header tiff-open
: tiff-open ( blk# -- ) \ read the directory of the TIFF starting at a block
    d# 10 lshift tiff-base !
    d# 1 tspp !  d# 1 tcomp !  d# 1 tphoto !
    d# 0 tiff-be !
    d# 0 t16@
    dup h# 4d4d = tiff-be !                         \ "MM"
    dup h# 4949 <> swap h# 4d4d <> and d# 38 -throw \ or "II"
    d# 2 t16@ d# 42 <> d# 38 -throw
    d# 4 t32@                                       ( ifd )
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
        d# 273 tstrips tentry
        d# 279 tcounts tentry
        drop d# 12 +
    repeat
    2drop
    tcomp @ d# 1 <> d# 38 -throw                    \ uncompressed only
;

: selem ( i entry -- x ) \ value i of a StripOffsets or StripByteCounts entry
    dup d# 4 + t32@ d# 1 = if nip tval exit then    \ one value: held inline
    dup d# 2 + t16@ d# 3 = if
        d# 8 + t32@ swap 2* + t16@                  \ array of SHORT
    else
        d# 8 + t32@ swap d# 2 lshift + t32@         \ array of LONG
    then
;

\ The strips are read a piece at a time, so that whoever reads them can do
\ something else between the pieces: a piece is what is left of a strip, or
\ of the block of the disk it is in if that is less.  Where each strip is
\ and how long is noted before the first piece, in a table of two cells a
\ strip, so that the directory is not gone back to while the image is read.

create tdest 0 ,        \ where the next piece goes: the address of a byte
create tstab 0 ,        \ the table: each strip's place in the file, and its length
create tsend 0 ,        \ the end of the table,
create tsp   0 ,        \ and the next strip in it
create toff  0 ,        \ where in the file the strip being read goes on,
create tleft 0 ,        \ and how many bytes of it are still to come
create tgot  0 ,        \ bytes of the image read
create twant 0 ,        \ and how many the rows asked for come to

header tiff-start
: tiff-start ( -- ) \ make room for the image, and note where its strips are
    tw @ th @ * tspp @ * halloc raw !
    raw @ >c tdest !
    tstrips @ d# 4 + t32@                           ( strips )
    dup 2* >c halloc dup tstab !  dup tsp !  over 2* + tsend !
    d# 0
    begin
        2dupxor
    while
        dup tstrips @ selem  over 2* tstab @ + !
        dup tcounts @ selem  over 2* tstab @ + CELL + !
        1+
    repeat
    2drop
    d# 0 tleft !  d# 0 tgot !  d# 0 twant !
;

: tiff-more ( -- more? ) \ read the next piece; false if there is no more to read
    tleft @ 0= if
        tsp @ tsend @ = if false exit then
        tsp @ @ toff !  tsp @ CELL + @ tleft !
        CELL2 tsp +!
    then
    d# 1024  toff @ tiff-base @ + h# 3ff and -  tleft @ min     ( n )
    dup >r  tdest @ swap  toff @ tiff-base @ +  disk-read
    r@ tdest +!  r@ toff +!  r@ tgot +!
    r> negate tleft +!
    true
;

header tiff-row
: tiff-row ( -- ) \ read on until another row of the image is all there
    tw @ tspp @ * twant +!
    begin
        tgot @ twant @ <
    while
        tiff-more 0= if exit then
    repeat
;

header tiff-decode
: tiff-decode ( -- ) \ load every strip
    tiff-start
    begin tiff-more 0= until
;

\ An image that is in memory already, as its strips would be if they had
\ been read: for whoever has one from somewhere other than a file.
header image-is
: image-is ( cell w h samples -- ) \ gray, black being nothing, if it has one sample to a pixel
    tspp !  th !  tw !  raw !
    d# 1 tphoto !
;

\ The image is a run of bits, and memory is cells.  A cell is fetched once
\ and what is wanted is shifted out of it: tbuf holds the bits fetched and
\ not yet taken, lowest first, and tbn how many there are.  `take` gives
\ the next n, for n up to a cell, with whatever follows them above; when
\ there are fewer than n it fetches the next cell, and the result is made
\ of both.

create tbuf 0 ,
create tbn  0 ,
create tbp  0 ,         \ the next cell

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

\ A pixel's gray: its one sample, or 77 R + 150 G + 29 B over 256.  A pixel
\ of three samples is taken whole if a cell holds it, and its fields
\ shifted out on the stack; in a 16-bit cell it is taken a sample at a time.

: gray1 ( -- g ) d# 8 take h# ff and  tphoto @ 0= if d# 255 xor then ;

: grayn ( -- g ) \ the first of several samples
    gray1  tspp @ d# 1 begin 2dupxor while d# 8 take drop 1+ repeat 2drop
;

W32+? [if]
: rgb ( -- g )
    d# 24 take
    dup h# ff and d# 77 *
    over d# 8 rshift h# ff and d# 150 * +
    swap d# 16 rshift h# ff and d# 29 * +
    d# 8 rshift
;
[else]
: rgb ( -- g )
    d# 8 take h# ff and d# 77 *
    d# 8 take h# ff and d# 150 * +
    d# 8 take h# ff and d# 29 * +
    d# 8 rshift
;
[then]

header 'pixel create 'pixel 0 ,     \ ( -- g )  the next pixel's gray

\ A row of an image of three samples, where the row lies on cells, is not
\ taken a pixel at a time.  Eight pixels are three cells.  Two pixels are
\ six bytes of a cell, and their grays are made together: the two reds are
\ masked out where they are and multiplied by 77, the greens and the blues
\ shifted down to the same two places and multiplied by theirs, and the
\ three added, which is each pixel's gray times 256 with nothing carried
\ from one to the other.  Four pixels' grays are then a cell of four
\ lanes, sixteen bits each, and that is how whoever dithers keeps what each
\ pixel is owed: so the grays are added to those a cell at a time, and
\ nothing is fetched for a pixel.

W64? [if]

create tsrc 0 ,         \ the cell the next row starts in

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
: quad-a ( p -- y ) \ the first four
    dup @  swap dup @ d# 48 rshift swap 1+ @ d# 16 lshift or  quad
;
: quad-b ( p -- y ) \ and the next four
    dup 1+ @ d# 32 rshift over CELL2 + @ d# 32 lshift or
    swap CELL2 + @ d# 16 rshift  quad
;

: gray-lanes ( lanes -- ) \ the next row's grays, each added to its pixel's lane
    tsrc @ >r                                       ( a ) ( R: the three cells )
    tw @ d# 2 rshift over + swap                    ( end a )
    begin
        2dupxor
    while
        r@ quad-a over +!
        r@ quad-b over 1+ +!
        CELL2 +  r> c# 3 + >r
    repeat
    2drop
    r> tsrc !
;

[then]

\ ( lanes -- ) adds a row's grays to a row of lanes, or is 0 if the image
\ is one whose pixels have to be asked for one at a time.
header 'grays create 'grays 0 ,

header pixels
: pixels ( -- ) \ start again from the first pixel
    raw @ tbp !  d# 0 tbn !  d# 0 tbuf !
    tspp @ d# 3 = if ['] rgb else tspp @ d# 1 = if ['] gray1 else ['] grayn then then
    'pixel !
    d# 0 'grays !
W64? [if]
    raw @ tsrc !
    tspp @ d# 3 =  tw @ d# 7 and 0=  and if ['] gray-lanes 'grays ! then
[then]
;
