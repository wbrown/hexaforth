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
    begin
        over
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
        swap 1- swap
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

create tdest 0 ,

header tiff-decode
: tiff-decode ( -- ) \ load every strip
    tw @ th @ * tspp @ * halloc raw !
    raw @ >c tdest !
    d# 0
    begin
        dup tstrips @ d# 4 + t32@ <
    while
        dup tcounts @ selem                         ( i count )
        over tstrips @ selem                        ( i count offset )
        over >r
        tdest @ -rot tiff-base @ + disk-read
        r> tdest +!
        1+
    repeat
    drop
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
    gray1  tspp @ 1- begin dup while d# 8 take drop 1- repeat drop
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

header pixels
: pixels ( -- ) \ start again from the first pixel
    raw @ tbp !  d# 0 tbn !  d# 0 tbuf !
    tspp @ d# 3 = if ['] rgb else tspp @ d# 1 = if ['] gray1 else ['] grayn then then
    'pixel !
;
