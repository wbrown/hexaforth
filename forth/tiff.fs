\ tiff.fs - read an uncompressed TIFF from the block device into memory
\
\ Handles either byte order, 8-bit grayscale or RGB, and any number of
\ strips.  The image ends up in `gray`, one byte per pixel.

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
create raw     0 ,      \ the strips as stored
header gray create gray    0 ,      \ the decoded image

: tbytes ( off n -- a ) \ n bytes of the file, in tscratch
    tscratch swap rot tiff-base @ + disk-read
    tscratch
;

: t16@ ( off -- x ) d# 2 tbytes  tiff-be @ if be16@ else uw@ then ;
: t32@ ( off -- x ) d# 4 tbytes  tiff-be @ if be32@ else @ lo32 then ;

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
create gsrc  0 ,

: >gray ( -- ) \ reduce raw to one byte per pixel
    raw @ gsrc !
    gray @ tw @ th @ * bounds                       ( end dst )
    begin
        2dupxor
    while
        tspp @ d# 3 = if
            gsrc @ c@ d# 77 *
            gsrc @ 1+ c@ d# 150 * +
            gsrc @ d# 2 + c@ d# 29 * +
            d# 8 rshift
        else
            gsrc @ c@
            tphoto @ 0= if d# 255 xor then
        then
        over c!
        tspp @ gsrc +!
        1+
    repeat
    2drop
;

header tiff-decode
: tiff-decode ( -- ) \ load every strip, then decode into gray
    tw @ th @ *
    dup halloc gray !
    tspp @ * halloc raw !
    raw @ tdest !
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
    >gray
;
