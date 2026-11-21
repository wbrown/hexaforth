\ display.fs - the display: a bitmap in memory, a bit a pixel
\ In the flags these words take, true is white.

header fb-w \ width and height in pixels
    create fb-w  meta disp-w target ,
header fb-h create fb-h  meta disp-h target ,

\ ---- the port in use; port! changes it for another's record

create gstate 0 , 0 ,  0 , 0 , 0 , 0 ,  0 , 0 ,  0 , 0 , 0 ,
              0 ,  0 ,  0 , 0 , 0 , 0 ,  0 ,
$12 constant PORT-CELLS

header ox      : ox      gstate ;          \ its origin, on the
header oy      : oy      gstate CELL + ;   \ screen
header clip    : clip    gstate CELL2 + ;  \ left top right end
header pattern : pattern gstate c# 6 + ;   \ what fills are of
header 'mode   : 'mode   gstate c# 7 + ;   \ ( src mask a -- )
header inverse : inverse gstate c# 8 + ;   \ how text looks
header bold    : bold    gstate c# 9 + ;
header italic  : italic  gstate c# 10 + ;
header font    : font    gstate c# 11 + ;
header clip-rgn : clip-rgn gstate c# 12 + ;  \ a region, or 0
header text-or : text-or gstate c# 17 + ;  \ text adds black

\ the same, compiled in line; clip0 is the rectangle asked for
$0 at: gstate ox        $1 at: gstate oy
    $2 at: gstate clip
$6 at: gstate pattern   $7 at: gstate 'mode
    $8 at: gstate inverse
$9 at: gstate bold      $a at: gstate italic
    $b at: gstate font
$c at: gstate clip-rgn  $d at: gstate clip0
    $11 at: gstate text-or

nearby
header origin! : origin! ( x y -- ) oy ! ox ! ;
nearby-end

nearby
header row : row ( y -- a ) \ the first cell of row y
BANKED? [if]
    DISP-BANKS u/mod DISP-STRIDE *
    swap DISP-BANK-STEP * + DISP-BASE +
[else]
    DISP-STRIDE * DISP-BASE +
[then]
;
nearby-end

INK-BLACK? [if]
header ink   : ink   ( mask a -- ) or! ;
header paper : paper ( mask a -- ) swap invert swap and! ;
[else]
header paper : paper ( mask a -- ) or! ;
header ink   : ink   ( mask a -- ) swap invert swap and! ;
[then]

nearby
: borrow ( cells -- a ) \ at here, while the asker runs
    align here swap >c dp +!
    dp @ dpl @ u> d# 8 -throw
;
nearby-end

: x!   ( bits a -- ) xor! ;  \ invert
: seg! ( bits mask a -- ) \ replace
    dup >r @ swap invert and or r> ! ;

nearby
: prow ( x y -- n a ) \ a pixel's cell, and how far into it
    row over PXSHIFT rshift + swap PXMASK and swap ;
nearby-end

: dplot ( on? x y -- )
    prow >r LEFT1 swap PIXR
    swap if r> paper else r> ink then
;

header 'plot create 'plot 0 ,  \ ( on? x y -- )  where plot goes
header plot
: plot ( on? x y -- ) oy @ + swap ox @ + swap 'plot @ execute ;

\ ---- modes and patterns

\ In a source a set bit is black, whatever the display's is.

INK-BLACK? [if]
: m-copy ( src mask a -- ) >r tuck and swap r> seg! ;
[else]
: m-copy ( src mask a -- )
    >r swap invert over and swap r> seg! ;
[then]
: m-or   ( src mask a -- ) >r and r> ink ;
: m-xor  ( src mask a -- ) >r and r> x! ;
: m-bic  ( src mask a -- ) >r and r> paper ;

nearby
header copy-mode : copy-mode ( -- ) ['] m-copy 'mode ! ;
nearby-end
header or-mode   : or-mode   ( -- ) ['] m-or   'mode ! ;
nearby
header xor-mode  : xor-mode  ( -- ) ['] m-xor  'mode ! ;
nearby-end
header bic-mode  : bic-mode  ( -- ) ['] m-bic  'mode ! ;

\ a pattern: eight cells, each a row of 8 pixels repeated
header p-white  create p-white   $00 prow, $00 prow, $00 prow,
    $00 prow, $00 prow, $00 prow, $00 prow, $00 prow,
header p-black  create p-black   $ff prow, $ff prow, $ff prow,
    $ff prow, $ff prow, $ff prow, $ff prow, $ff prow,
header p-gray   create p-gray    $aa prow, $55 prow, $aa prow,
    $55 prow, $aa prow, $55 prow, $aa prow, $55 prow,
header p-ltgray create p-ltgray  $88 prow, $22 prow, $88 prow,
    $22 prow, $88 prow, $22 prow, $88 prow, $22 prow,
header p-dkgray create p-dkgray  $77 prow, $dd prow, $77 prow,
    $dd prow, $77 prow, $dd prow, $77 prow, $dd prow,

create lefts  lefts,
: leftn ( n -- mask ) lefts + @ ; \ n pixels at a cell's left

\ ---- clipping to a region (region.fs), a row at a time

\ the region's pixels on a row, as a row of the screen
create mrow  meta disp-stride cell-bytes * target allot
create mk0 1 ,     \ for the rows from this
create mk1 0 ,     \ up to this
create mbase 0 ,   \ the first cell of the row on the screen
create mnone 0 ,   \ true if the region has none of the row
create mrgn 0 ,    \ the region masked, or 0 if none is
create mone 0 ,    \ true if it is one run of pixels there,
create mx0 0 ,     \ from this
create mx1 0 ,     \ up to this
create mmask 0 ,   \ true if the mask is to be used
create mforce 0 ,  \ true if whoever draws does not trim

: mspan ( x0 x1 -- ) \ set those pixels of mrow
    fb-w @ min swap d# 0 max tuck -                ( x n )
    swap dup PXSHIFT rshift mrow + >r  PXMASK and
    ( n b ) ( R: the cell )
    begin
        over 0>
    while
        2dup PXCELL swap - min                     ( n b k )
        tuck leftn swap PIXR  r@ or!               ( n k )
        -  d# 0
        r> 1+ >r
    repeat
    2drop rdrop
;

\ a band ends at its row plus AS-END, so that END-MIN < tells
\ a crossing from an end; the last band ends at NEVER
meta
1 cell-bits 3 - #32 min lshift
dup target constant AS-END
meta dup 2/ target constant END-MIN
meta 2* target constant NEVER

: mfind ( y -- band ) \ y's band; sets mk0 and mk1
    AS-END + >r           ( R: y plus AS-END, as an end is )
    END-MIN negate mk0 !
    mrgn @ 1+
    begin
        dup dup @ + 1+ @  ( band end )
        dup r@ > 0=       \ it ends at y, or above
    while
        AS-END - mk0 !
        dup @ + CELL2 +
    repeat
    AS-END - mk1 !  rdrop
;

: band-mask ( band -- ) \ its crossings, as pixels of mrow
    dup @ 0= mnone !
    dup @ d# 2 = if
        true mone !  dup CELL + @ mx0 !  dup CELL2 + @ mx1 !
    then
    dup 1+ swap @ bounds                            ( end p )
    begin
        2dup xor
    while
        dup @ over 1+ @ mspan
        CELL2 +
    repeat
    2drop
;

: rmask ( y -- ) \ y on the screen
    mrgn @ 0= if drop exit then
    dup row mbase !
    dup mk0 @ < 0=  over mk1 @ <  and if drop exit then
    d# 0 mrow DISP-STRIDE cells-fill
    d# 0 mone !
    mfind band-mask
    mforce @  mone @ 0=  or mmask !
;

: rm ( mask a -- mask' a )
    mmask @ if dup mbase @ - mrow + @ rot and swap then
;

: span-clip ( x n -- x' n' ) \ cut to the row's one run
    over mx0 @ < if + mx0 @ tuck - then
    over + mx1 @ min over -
;

: band-stale ( -- ) \ the band is to be found again
    d# 1 mk0 !  d# 0 mk1 ! ;

\ clip is clip0 cut to the region, if that is a rectangle
nearby
: clip-rect-rgn ( rgn -- ) \ 0 top, 2 x0 x1 bottom, 0 never
    >r
    r@ c# 4 + @ clip @ max clip !
    r@ CELL2 + @ AS-END - clip CELL + @ max clip CELL + !
    r@ c# 5 + @ clip CELL2 + @ min clip CELL2 + !
    r> c# 6 + @ AS-END - clip c# 3 + @ min clip c# 3 + !
;

: clip-by ( rgn -- ) \ the region, in clip or as mrgn
    dup @ d# 8 = if clip-rect-rgn exit then
    dup @ d# 2 = if drop  clip @ clip CELL2 + ! exit then
    mrgn !
;

: reclip ( -- )
    clip0 @ clip !  clip0 CELL + @ clip CELL + !
    clip0 CELL2 + @ clip CELL2 + !
    clip0 c# 3 + @ clip c# 3 + !
    d# 0 mrgn !  d# 1 mk0 !  d# 0 mk1 !  d# 0 mnone !
    d# 0 mone !  d# 0 mmask !  d# 0 mforce !
    clip-rgn @ ?dup if clip-by then
    clip CELL2 + @ clip @ >  clip c# 3 + @ clip CELL + @ >
    and 0= if
        clip @ clip CELL2 + !  clip CELL + @ clip c# 3 + !
    then
;

header rgn-clip! : rgn-clip! ( rgn|0 -- ) clip-rgn !  reclip ;
nearby-end

: under! ( a mask row -- a ) \ the row's pixels the mask says
    over and >r  invert over @ and  r> or  over !
;

\ n pixels from bit b of the cell at a, of a row of the pattern
: fast-slab ( b a row n -- )
    swap RAW>SRC >r  swap >r  swap      ( n b ) ( R: row a )
    2dup + PXCELL > 0= if               \ all in one cell
        swap leftn swap PIXR  r> swap r> under! drop  exit
    then
    ?dup if
        tuck PXCELL in-place - +        ( b n )
        swap d# -1 swap PIXR  r> swap r@ under! 1+ >r
    then
    dup PXSHIFT rshift  r> swap bounds  ( n end a ) ( R: row )
    begin 2dup xor while  r@ over ! 1+  repeat
    drop  swap PXMASK and ?dup if leftn r@ under! then
    drop rdrop
;

: cell-mode ( a mask row -- a ) \ through the mode and region
    >r over rm  r> -rot  'mode @ execute
;

: slow-slab ( b a row n -- ) \ the same, a cell through the mode
    swap >r  swap >r  swap              ( n b ) ( R: row a )
    2dup + PXCELL > 0= if
        swap leftn swap PIXR  r> swap r> cell-mode drop  exit
    then
    ?dup if
        tuck PXCELL - +
        swap d# -1 swap PIXR  r> swap r@ cell-mode 1+ >r
    then
    dup PXSHIFT rshift  r> swap bounds  ( n end a ) ( R: row )
    begin 2dup xor while  d# -1 r@ cell-mode 1+  repeat
    drop  swap PXMASK and ?dup if leftn r@ cell-mode then
    drop rdrop
;

header slab
: slab ( x y n -- )
    >r
    dup d# 7 and pattern @ + @ >r  ( x y ) ( R: n row )
    prow r> r>                     ( b a row n )
    mmask @ 0=  'mode @ ['] m-copy =
    and if fast-slab exit then
    slow-slab
;

nearby
header solid : solid ( on? -- )
    if p-white else p-black then pattern !  copy-mode ;
nearby-end

\ ---- clipping: from here on, coordinates are in the port

nearby
: >screen ( x y -- x' y' ) oy @ + swap ox @ + swap ;

header clip! : clip! ( x y w h -- )
    >r >r >screen
    over d# 0 max clip0 !
    dup d# 0 max clip0 CELL + !
    swap r> + fb-w @ min clip0 CELL2 + !
    r> + fb-h @ min clip0 c# 3 + !
    reclip
;

header clip-screen! : clip-screen! ( x0 y0 x1 y1 -- )
    clip0 c# 3 + !  clip0 CELL2 + !  clip0 CELL + !  clip0 !
    reclip
;

header noclip : noclip ( -- ) \ the whole screen
    d# 0 clip-rgn !
    d# 0 d# 0 fb-w @ fb-h @ clip-screen!
;

header clip-to : clip-to ( x y w h -- ) \ this and the clip
    >r >r >screen
    over clip0 @ max clip0 !
    dup clip0 CELL + @ max clip0 CELL + !
    swap r> + clip0 CELL2 + @ min clip0 CELL2 + !
    r> + clip0 c# 3 + @ min clip0 c# 3 + !
    reclip
;

header clip-rows : clip-rows ( -- top bottom ) \ in the port
    clip CELL + @ oy @ -  clip c# 3 + @ oy @ - ;

: clip-span ( x n -- x' n' ) \ n' is not positive if none
    over clip @ < if + clip @ tuck - then
    over + clip CELL2 + @ min over -
;
nearby-end

: rgn-slab ( x y n -- ) \ what the region leaves of it
    over rmask
    mnone @ if drop 2drop exit then
    swap >r clip-span  mone @ if span-clip then  r> swap
    dup 0> if slab else drop 2drop then
;

nearby
header pat-hline : pat-hline ( x y n -- )
    >r >screen r>
    over dup clip CELL + @ < swap clip c# 3 + @ < 0= or if
        drop 2drop exit
    then
    mrgn @ if rgn-slab exit then
    swap >r clip-span r> swap
    dup 0> if slab else drop 2drop then
;
nearby-end

\ plain white or black is filled a column of cells at a time
BANKED? [if]

: plain-rect ( x y n rows -- ) drop drop 2drop ;
: plain? ( -- f ) false ;

[else]

: col-and! ( x a n -- ) \ in a cell of each of n rows, from a
    DISP-STRIDE * over + >r             ( x a ) ( R: end )
    dup r@ xor if
        begin  2dup and!  DISP-STRIDE +  r@ over =  until
    then
    2drop rdrop
;

: col-or! ( x a n -- )
    DISP-STRIDE * over + >r
    dup r@ xor if
        begin  2dup or!  DISP-STRIDE +  r@ over =  until
    then
    2drop rdrop
;

: col! ( x a n -- )
    DISP-STRIDE * over + swap rot >r
    begin 2dup xor while  r@ over !  DISP-STRIDE +  repeat
    2drop rdrop
;

: column ( v mask a n -- ) \ v's pixels the mask says, as above
    >r >r  ( v mask ) ( R: n a )
    dup invert 0= if drop r> r> col! exit then
    dup invert r> r> 2dup >r >r col-and!
    and ?dup if r> r> col-or! else rdrop rdrop then
;

\ n pixels from bit b: the first cell's mask, the whole cells,
\ and the last cell's mask.  A mask of 0 is no cell.
: spans ( b n -- m1 w m0 )
    2dup + PXCELL > 0= if                      \ all in one cell
        leftn swap PIXR  >r d# 0 d# 0 r> exit
    then
    over if
        over PXCELL - +  swap d# -1 swap PIXR  ( n' m0 )
    else
        nip d# 0
    then
    >r  dup PXMASK and leftn  swap PXSHIFT rshift  r>
;

: strip ( a n mask -- a n ) \ a column of the pattern
    >r 2dup  pattern @ @ RAW>SRC  r> 2swap column
;

: plain-rect ( x y n rows -- ) \ on the screen and clipped
    >r >r prow r> swap >r  ( b n ) ( R: rows a )
    spans r> r> rot        ( m1 w a rows m0 )
    ?dup if strip swap 1+ swap then
    rot negate             ( m1 a rows -w )
    begin
        dup
    while
        >r  d# -1 strip swap 1+ swap  r> 1+
    repeat
    drop
    rot ?dup if strip then
    2drop
;

: plain? ( -- f )
    pattern @ dup p-white = swap p-black = or
    'mode @ ['] m-copy = and
    mmask @ 0= and
;

[then]

: rows-fill ( x y n rows -- ) \ on the screen, clipped, unmasked

    plain? if plain-rect exit then
    >r swap r> over + >r swap
    ( x y n ) ( R: past the last row )
    begin
        over r@ <
    while
        >r 2dup r@ slab
        1+ r>
    repeat
    drop 2drop rdrop
;

: run-fill ( y rows x n -- ) \ n pixels of a run from x, if any
    dup 0> if  2swap >r swap r>  rows-fill exit  then
    2drop 2drop
;

: runs ( y rows band -- ) \ what the band has of the rows
    -rot >r >r  ( band ) ( R: rows y )
    dup 1+ swap @ bounds
    begin
        2dup xor
    while
        dup @  over 1+ @ over -  clip-span     ( end p x n )
        r> r@ over >r  2swap run-fill
        CELL2 +
    repeat
    2drop rdrop rdrop
;

: bands-fill ( y yend -- ) \ a band of the region at a time
    swap  mrgn @ 1+ >r  ( yend y ) ( R: band )
    begin
        2dup >
    while
        over  r@ dup @ + 1+ @ AS-END -  min  over max
        ( yend y e )
        tuck over -  ( yend e y rows )
        dup 0> if r@ runs else 2drop then
        r> dup @ + CELL2 + >r
    repeat
    2drop rdrop
;

: masked-rect ( x y n yend -- ) \ a rectangle a run of a band
    d# 0 mmask !  band-stale
    clip @ >r  clip CELL2 + @ >r  ( R: clip's left and right )
    >r rot  dup clip !  + clip CELL2 + !  r>  ( y yend )
    bands-fill
    r> clip CELL2 + !  r> clip !
;

nearby
header pat-rect : pat-rect ( x y w h -- )
    >r >r >screen r> r>
    >r over r> +  clip c# 3 + @ min  >r
    ( x y w ) ( R: past the last row drawn )
    swap  clip CELL + @ max  swap
    >r swap r> clip-span rot swap        ( x' y n' )
    over r@ <  over 0> and  0= if drop 2drop rdrop exit then
    mrgn @ 0= if
        over r> swap - rows-fill exit
    then
    r> masked-rect
;
nearby-end

\ ---- copying

\ a bitmap: its first row, and the cells from a row to the next
header screen create screen 0 , 0 ,  \ the display

header copy-busy create copy-busy 0 ,

header copy-state create copy-state  \ a copy, up to copy-end
create sbm 0 ,      \ the source, and where in it
create ssx 0 ,
create ssy 0 ,
create bdx 0 ,      \ where on the screen
create bdy 0 ,
create bw 0 ,
create bh 0 ,
create boff 0 ,     \ pixels to shift the source left by
create bi0 0 ,      \ the cell of rowbuf the first comes from
create bstore 0 ,   \ true in copy mode
create bp0 0 ,      \ where in rowbuf a row's first cell is
create bdirect 0 ,  \ true if rows go straight across

create rowbuf  meta disp-stride 4 + cell-bytes * target allot
header copy-end create copy-end

: src-row ( y -- a )
    sbm @ @ ?dup if swap sbm @ CELL + @ * + else row then
;

: row-aside ( y -- ) \ a source row, into rowbuf from its second
    src-row ssx @ PXSHIFT rshift +
    rowbuf 1+
    ssx @ PXMASK and bw @ + PXMASK + PXSHIFT rshift 1+
    cells-move
;

: bsrc ( p -- raw ) \ from the cell of rowbuf at p and the next
    boff @ ?dup if
        >r  dup @ r@ PIXL  swap 1+ @ PXCELL r> - PIXR  or
    else
        @
    then
;

: copy-cell ( a mask dist -- a ) \ from rowbuf, dist from a
    >r over r> + bsrc RAW>SRC cell-mode
;

: whole? ( a -- f ) \ is the cell at a stored as it comes?
    bstore @ 0= if drop false exit then
    mmask @ if mbase @ - mrow + @ 1+ 0= else drop true then
;

\ n pixels from bit b of the cell at a, from rowbuf dist on

: row-out ( b n dist a -- )
    swap >r >r swap                     ( n b ) ( R: dist a )
    2dup + PXCELL > 0= if               \ all in one cell
        swap leftn swap PIXR  r> swap r> copy-cell drop  exit
    then
    ?dup if  tuck PXCELL - +  swap d# -1 swap PIXR
        r> swap r@ copy-cell 1+ >r
    then
    dup PXSHIFT rshift  r> swap bounds  ( n end a ) ( R: dist )
    begin 2dup xor while
        dup whole? if  dup r@ + bsrc over !
        else  d# -1 r@ copy-cell  then  1+
    repeat
    drop  swap PXMASK and ?dup if leftn r@ copy-cell then
    drop rdrop
;

: whole-row ( row -- ) \ all of the row that is copied
    bdx @ swap bdy @ + prow        ( b a )
    bw @ swap  bp0 @ over -  swap  ( b n dist a )
    row-out
;

: move! ( a mask dist -- a ) \ from the cell dist on from a
    >r over r> + @ under!
;

\ n pixels from bit b of the cell at a, from the cell dist on.
\ The two instructions written by their fields: a + dist to R,
\ then the cell there to the cell at a.

: move-row ( b n dist a -- )
    swap >r >r swap                     ( n b ) ( R: dist a )
    2dup + PXCELL > 0= if               \ all in one cell
        swap leftn swap PIXR  r> swap r> move! drop  exit
    then
    ?dup if  tuck PXCELL in-place - +  swap d# -1 swap PIXR
        r> swap r@ move! 1+ >r
    then
    dup PXSHIFT rshift  r> swap bounds  ( n end a ) ( R: dist )
    begin 2dup xor while
        over  meta R->IN IN+N ->R d-1 r+1 alu target
        meta R->IN [IN] ->[T] alu target  1+ rdrop
    repeat
    drop  swap PXMASK and ?dup if leftn r@ move! then
    drop rdrop
;

: row-direct ( row x n -- ) \ n pixels of a row, from x
    >r
    2dup swap bdy @ + prow          ( row x b a ) ( R: n )
    2swap
    ssx @ bdx @ - +  PXSHIFT rshift
    swap ssy @ + src-row +          ( b a from )
    over in-place -  swap  r> -rot  ( b n dist a )
    move-row
;

: copy-clip ( bitmap sx sy dx dy w h -- ) \ into the record
    bh ! bw ! bdy ! bdx ! ssy ! ssx ! sbm !
    ox @ bdx +!  oy @ bdy +!
    sbm @ @ 0= if ox @ ssx +!  oy @ ssy +! then
    clip @ bdx @ - d# 0 max  dup bdx +!  dup ssx +!
    negate bw +!
    clip CELL + @ bdy @ - d# 0 max  dup bdy +!  dup ssy +!
    negate bh +!
    clip CELL2 + @ bdx @ - bw @ min bw !
    clip c# 3 + @ bdy @ - bh @ min bh !
;

: copy-how ( -- ) \ how the source lies against the screen
    ssx @ PXMASK and  bdx @ PXMASK and -  PXCELL +
    dup PXMASK and boff !  PXSHIFT rshift bi0 !
    'mode @ ['] m-copy = bstore !
    bi0 @ rowbuf + bp0 !
    bstore @  boff @ 0=  and
    sbm @ @ 0<>  ssy @ bdy @ <>  or  and  bdirect !
;

: one-run ( row -- ) \ what the region's one run has of a row
    bdx @ bw @ span-clip           ( row x n )
    dup 0> 0= if 2drop drop exit then
    bdirect @ if row-direct exit then
    >r  ( row x ) ( R: n )
    over ssy @ + row-aside
    dup PXSHIFT rshift
    bdx @ PXSHIFT rshift -  bp0 @ +
    >r  swap bdy @ + prow
    ( b a ) ( R: n, where in rowbuf )
    r> over -  swap  r> -rot
    row-out
;

: masked-row ( row -- ) \ under a region that is not a rectangle
    dup bdy @ + rmask
    mnone @ if drop exit then
    mone @ if one-run exit then
    dup ssy @ + row-aside  whole-row
;

: copy-row ( row -- )
    mrgn @ if masked-row exit then
    bdirect @ if bdx @ bw @ row-direct exit then
    dup ssy @ + row-aside  whole-row
;

: (copy-bits) ( bitmap sx sy dx dy w h -- )
    copy-clip
    bw @ 0>  bh @ 0>  and 0= if exit then
    copy-how
    bh @ negate
    begin dup while
        dup bh @ +  \ from the bottom up if they move down
        ssy @ bdy @ < if bh @ 1- swap - then       ( count row )
        copy-row  1+
    repeat
    drop
;

\ a rectangle of a bitmap to the screen, in the mode
header copy-bits : copy-bits ( bitmap sx sy dx dy w h -- )
    copy-busy @ if
        dp @ >r
        copy-end copy-state - borrow >r
        ( R: where memory was, the copy )
        copy-state r@ copy-end copy-state - cells-move
        (copy-bits)
        r> copy-state copy-end copy-state - cells-move
        r> dp !
        exit
    then
    true copy-busy !  (copy-bits)  d# 0 copy-busy !
;

: screen-rect ( x y w h -- x0 y0 x1 y1 ) \ cut to the screen
    >r >r >screen  over r> +  over r> +  ( X0 Y0 X1 Y1 )
    fb-h @ min >r  fb-w @ min >r
    d# 0 max swap d# 0 max swap          ( x0 y0 ) ( R: Y1 X1 )
    over r> max  over r> max
;

: kept-cells ( x0 x1 -- c0 n ) \ the cells those pixels are in
    1- PXSHIFT rshift  swap PXSHIFT rshift  tuck - 1+
;

: keep-row ( c0 n a y -- ) \ n cells of row y from c0, to a
    row  >r rot r> +  -rot swap  cells-move
;

\ the screen's whole cells under the rectangle, into the bitmap
header keep-bits : keep-bits ( bitmap x y w h -- )
    screen-rect  ( bm x0 y0 x1 y1 )
    >r  rot swap kept-cells  ( bm y0 c0 n ) ( R: y1 )
    dup 0> 0= if 2drop 2drop rdrop exit then
    2swap swap  dup 1+ @  r> swap >r >r  @ swap
    ( c0 n a y0 ) ( R: stride y1 )
    begin
        dup r@ <
    while
        2over 2over keep-row
        swap  r> r@ swap >r  +  swap  1+
    repeat
    2drop 2drop  rdrop rdrop
;

header put-bits : put-bits ( bitmap x y w h -- ) \ and back
    screen-rect
    rot tuck - >r  ( bm x0 x1 y0 ) ( R: h )
    -rot over - >r  ( bm y0 x0 ) ( R: h w )
    swap oy @ -  swap dup PXMASK and  swap ox @ -
    ( bm dy sx dx )
    rot  >r >r d# 0 r> r>  ( bm sx 0 dx dy )
    r> r>  copy-bits
;

\ eight pixels b into a cell: what it has, and what the next has
nearby
: over8 ( bits b -- here next )
    >r dup r@ PIXR swap r>  ( here bits b )
    dup PXCELL d# 8 - > if
        PXCELL swap - PIXL
    else
        2drop d# 0
    then
;

header xor8
: xor8 ( bits x y -- ) \ invert the pixels where bits are set
    prow >r  ( bits b ) ( R: the cell )
    swap EDGE swap over8
    ?dup if r@ 1+ x! then
    r> x!
;
nearby-end

\ the column's pixel in row y, if the pattern has ink there
: (column-bits) ( x y -- bits a )
    tuck prow >r          ( y n ) ( R: the cell )
    d# 1 leftn swap PIXR  ( y pixel )
    swap d# 7 and pattern @ + @ and  r>
;

BANKED? [if]

\ invert column x of the screen for n rows from y, where the
\ pattern has ink
header xor-column : xor-column ( x y n -- )
    over + fb-h @ min >r  d# 0 max
    ( x y ) ( R: the row after the last )
    over fb-w @ u< 0= if 2drop rdrop exit then
    begin
        dup r@ <
    while
        2dup (column-bits) x!
        1+
    repeat
    2drop rdrop
;

[else]

\ invert bits in the cell at a and every 8 rows down, to end
: column-run ( bits end a -- )
    rot ?dup 0= if 2drop exit then  >r  ( end a ) ( R: bits )
    begin
        2dup u>
    while
        r@ over x!
        DISP-STRIDE d# 3 lshift +
    repeat
    2drop rdrop
;

: column-pass ( x k end -- ) \ rows k, k + 8, ... before end
    >r  over >r  (column-bits)   ( bits a ) ( R: end x )
    r> PXSHIFT rshift  r> row +  ( bits a end-cell )
    swap column-run
;

\ invert column x of the screen for n rows from y, where the
\ pattern has ink
header xor-column : xor-column ( x y n -- )
    over + fb-h @ min  swap d# 0 max swap           ( x y end )
    rot dup fb-w @ u< 0= if drop 2drop exit then  -rot
    d# -8 >r  ( R: minus the passes left )
    begin
        r@
    while
        3dup column-pass
        swap 1+ swap
        r> 1+ >r
    repeat
    rdrop drop 2drop
;

[then]

\ ---- text

\ A font is as ]font lays it (cross.fs).

nearby
: glyph# ( c -- i ) \ the font's first, if it has not c
    font @ CELL + @ in-place -
    dup font @ CELL2 + @ u< 0= if drop d# 0 then
;

\ a glyph's lane: its width in the low five bits, then its first
\ row with black, then the row after its last
: ginfo ( i -- x )
W32+? [if]
    dup LANESHIFT rshift font @ c# 4 + @ + 1+ @
    swap LANES 1- and d# 4 lshift rshift  h# ffff and
[else]
    font @ c# 4 + @ + 1+ @
[then]
;
nearby-end

\ Text is a cell and which character from there is its first.

nearby
header char@ : char@ ( a bit -- c )
    dup PXSHIFT rshift rot + @
    swap PXMASK and rshift
    h# ff and
;

header ctext : ctext ( cstr -- a i u ) \ of a counted string
    d# 1 over @ h# ff and
;

header >chars : >chars ( addr -- a i ) \ of a byte's address
    dup c> swap OFFSET-MASK and
;

header chars-width : chars-width ( a i u -- n ) \ in pixels
    rot >r                            ( i u ) ( R: a )
    d# 3 lshift swap d# 3 lshift swap
    d# 0 -rot bounds                  ( n end bit )
    begin
        2dup xor
    while
        r@ over char@ glyph# ginfo h# 1f and  ( n end bit w )
        >r rot r> + -rot
        d# 8 +
    repeat
    2drop
    r> drop
;
nearby-end

header text-width : text-width ( addr u -- n )
    >r >chars r> chars-width
;

$20 constant MAX-ROWS    \ the highest a font may be
$80 constant MAX-GLYPHS  \ the most that one call will draw

header text-busy create text-busy 0 ,

header text-state create text-state  \ a string, up to text-end
create ta 0 ,      \ the text: a cell,
create tf 0 ,      \ which character from there is its first,
create tu 0 ,      \ and how many there are
create tx 0 ,      \ where the first glyph drawn goes
create ty 0 ,
create pen 0 ,     \ where the next glyph goes
create tr 0 ,      \ the row being drawn
create styled 0 ,  \ true if the rows need more than copying
create da 0 ,      \ the cell being stored
create tn 0 ,      \ how many rows are drawn
create tend 0 ,    \ the end of those cells of tband
create gd 0 ,      \ from a cell of tband to the glyph's row
create tk 0 ,      \ pixels of a glyph's left that are left out
create tq 0 ,      \ where its rows that are drawn start
create tcm 0 ,     \ the pixels drawn, of the cells being stored

create tor 0 ,     \ true if the text only adds its black
create hi 0 ,      \ how far across the cell they reach
create cx 0 ,      \ where on the screen the cell starts
create vx0 0 ,     \ the pixels of the row drawn: from here
create vx1 0 ,     \ to here
create r0 0 ,      \ and the rows of the glyphs: from this
create r1 0 ,      \ to this
create m0x 0 ,     \ the cell the drawing starts in,
create m0 0 ,      \ and the pixels of it that are drawn
create m1x 0 ,     \ the cell it ends in,
create m1 0 ,      \ and its

create gn 0 ,      \ how many glyphs will be seen
create gw 0 ,      \ how wide the one in hand is,
create gx 0 ,      \ its lane, or which glyph it is (gspan),
create ga 0 ,      \ where its rows are,
create ra 0 ,      \ and which of them are drawn: from this
create rb 0 ,      \ to this

create frows 0 ,   \ where the rows of the font's glyphs start,
create gcpg 0 ,    \ and how many cells a glyph's are
create lsb 0 ,     \ a lane of the font's: its bits as a shift,
create lb 0 ,      \ its bits,
create lm 0 ,      \ the lowest lane of a cell,
create lone 0 ,    \ and 1 in each lane of a cell
create rsh 0 ,     \ the rows in a cell, as a shift,
create rless 0 ,   \ and one less than how many they are
create 'lanes 0 ,  \ what is done with a cell of a glyph's rows
create gm 0 ,      \ the glyph's width at the left of each lane,
create gm1 0 ,     \ and a pixel less
create tclip 0 ,   \ true if the font's last rows are not drawn

create rend 0 ,    \ where the glyph's rows that are drawn end
create ttrim 0 ,   \ true if the clip was trimmed to one run
create tcx0 0 ,    \ and what its sides were
create tcx1 0 ,
\ a cell of the screen for each row, a glyph's rows styled, and
\ a cell for each glyph that will be seen
create tband  meta $20 cell-bytes * target allot
create tsty   meta $20 cell-bytes * target allot
create gbuf   meta $80 cell-bytes * target allot
header text-end create text-end

\ a glyph noted, and gw set: its number << 16 | its lane; or
\ where a cell is 16 bits, its width << 8 | its number
: gnote ( c -- g )
    glyph# dup ginfo
W32+? [if]
    dup h# 1f and gw !  swap d# 16 lshift or
[else]
    h# 1f and dup gw !  d# 8 lshift or
[then]
;

: glyphs ( -- ) \ notes each glyph that will be seen
    d# 0 gn !  tx @ pen !  ta @ >r
    tf @ d# 3 lshift  tu @ d# 3 lshift  bounds  ( end bit )
    begin 2dup xor while
        dup PXSHIFT rshift r@ + @  over PXMASK and rshift
        h# ff and  gnote                    ( end bit g )
        pen @ gw @ + clip @ > 0=  pen @ 0<  or
        pen @ clip CELL2 + @ < 0= or  gn @ MAX-GLYPHS = or
        if drop else
            gn @ 0= if pen @ tx ! then
            gbuf gn @ + !  d# 1 gn +!  pen @ gw @ + vx1 !
        then
        gw @ pen +!  d# 8 +
    repeat
    2drop  r> drop
;

: font-rows ( -- ) \ what the port's font says of its rows
    font @ CELL2 + @ LANES 1- + LANESHIFT rshift
    font @ c# 4 + @ + 1+ frows !
    font @ c# 3 + @  dup h# ff and gcpg !  d# 8 rshift lsb !
W32+? [if]
    d# 1 lsb @ lshift  dup lb !  d# 1 swap lshift 1- lm !
    PXSHIFT lsb @ in-place - dup rsh !
    d# 1 swap lshift 1- rless !
    font @ c# 4 + @ @ lone !
[then]
;

: gset ( g -- ) \ the glyph in hand, from what was noted
W32+? [if]
    dup h# ffff and  dup gx !  h# 1f and gw !  d# 16 rshift
[else]
    dup d# 8 rshift gw !  h# ff and  dup gx !
[then]
    gcpg @ * frows @ + ga !
;

: gspan ( -- ) \ which of its rows with black are drawn
    gx @
W32+? [if] [else]
    ginfo
[then]
    d# 5 rshift  dup h# 1f and r0 @ max ra !
    d# 5 rshift r1 @ min rb !
;

: endmask ( x -- mask ) \ what is drawn of the cell from x
    vx1 @ over - d# 0 max PXCELL min leftn
    vx0 @ rot - d# 0 max PXCELL min leftn invert and
;

: colmask ( -- mask ) \ what is drawn of the cell at cx
    cx @ vx0 @ < 0=  cx @ PXCELL + vx1 @ > 0=
    and if d# -1 exit then
    cx @ m0x @ = if m0 @ exit then
    cx @ m1x @ = if m1 @ exit then
    cx @ endmask
;

: put-rows ( -- ) \ the cells gathered replace what is there
    d# 0
    begin dup tn @ < while
        dup r0 @ + ty @ +  dup rmask  mnone @ if drop else
            row cx @ PXSHIFT rshift + da !  tcm @ da @ rm drop
            dup 1+ if ?dup if
                over tband + @ over and swap da @ seg!
            then else
                drop dup tband + @ da @ !
            then
        then
        d# 0 over tband + !  1+
    repeat  drop
;

: or-rows ( -- ) \ their black is added to what is there
    d# 0
    begin
        dup tn @ <
    while
        dup r0 @ + ty @ +  dup rmask
        mnone @ if
            drop
        else
            row cx @ PXSHIFT rshift + da !
            tcm @ da @ rm drop  over tband + @ and  da @ or!
        then
        d# 0 over tband + !  1+
    repeat
    drop
;

: tflush ( -- ) \ store the cells gathered; start on the next
    colmask tcm !
    tor @ if or-rows else put-rows then
    PXCELL cx +!  d# 0 hi !
;

\ ---- a glyph's rows, into the cells being gathered

W32+? [if]

meta
\ the lowest lane of a cell, as a row at the left of a cell; and
\ n pixels at the left of every lane
:: LANE
    disp-left-low if s" lm @ and"
    else s" PXCELL lb @ - lshift" then evaluate ;
:: LANE-LEFT
    disp-left-low if s" leftn lone @ *"
    else s" leftn PXCELL lb @ - rshift lone @ *" then
    evaluate ;
target

\ the rows in c, the first lowest, hi pixels along, to t on
: +lanes ( c t -- )
    >r
    begin dup while
        dup LANE hi @ PIXR r@ or!
        r> 1+ >r  lb @ rshift
    repeat
    drop rdrop
;

: -lanes ( c t -- ) \ the same rows, less their first tk pixels
    >r
    begin dup while
        dup LANE tk @ PIXL r@ or!
        r> 1+ >r  lb @ rshift
    repeat
    drop rdrop
;

: style-cell ( p -- c ) \ a cell of the glyph's rows, styled
    dup @ swap ga @ in-place -              ( c k )
    d# 8 rsh @ rshift <  italic @ and
    if gm1 @ and d# 1 PIXR then
    bold @ if dup gm1 @ and d# 1 PIXR or then
    inverse @ if gm @ xor then
INK-BLACK? [if] [else]
    gm @ xor
[then]
;

: gcell ( p -- c ) styled @ if style-cell exit then @ ;

\ the glyph's rows from ra up to rb, a cell of them at a time,
\ to the word in 'lanes ( c t -- ).  Row r is lane r and rless
\ of cell r >> rsh: with lanes of 8 bits in a cell of 64, row
\ 11 is lane 3 of the second cell.  The first cell is shifted
\ down so that ra's lane is its lowest.
: glyph-band ( -- )
    ra @ rb @ < 0= if exit then
    ra @ rsh @ rshift ga @ +                        ( p )
    dup gcell  ra @ rless @ and lsb @ lshift rshift
    ra @ r0 @ in-place - tband +  'lanes @ execute
    ra @ rless @ or 1+                              ( p row )
;fallthru

: band-rest ( p row -- ) \ the cells after the first
    begin dup rb @ < while
        swap 1+ swap
        over gcell  over r0 @ in-place - tband +
        'lanes @ execute
        rless @ + 1+
    repeat
    2drop
;

: band+ ( -- ) \ the glyph's rows into the cells, hi along
    styled @ if
        r0 @ ra !  r1 @ rb !
        gw @ LANE-LEFT gm !  gw @ 1- d# 0 max LANE-LEFT gm1 !
    else
        gspan
    then
    ['] +lanes 'lanes !  glyph-band
;

\ the glyph's rows less their first n pixels start the cells
: band> ( n -- ) tk !  ['] -lanes 'lanes !  glyph-band ;

[else]

: style ( pixels n -- pixels' n ) \ a row of a glyph, styled
    >r
    italic @ if tr @ d# 8 < if d# 1 PIXR then then
    bold @ if dup d# 1 PIXR or then
    r@ leftn and
    inverse @ if r@ leftn xor then
INK-BLACK? [if] [else]
    r@ leftn xor
[then]
    r>
;

: +rows ( -- ) \ the glyph's rows into the cells, hi along
    tend @ tband
    begin
        2dup xor
    while
        dup gd @ + @ hi @ PIXR over or!
        1+
    repeat
    2drop
;

: srows ( rows -- rows' ) \ those drawn, styled, in tsty
    r0 @ + tq !
    d# 0
    begin
        dup tn @ <
    while
        dup r0 @ + tr !
        tq @ over + @ gw @ style drop  over tsty + !
        1+
    repeat
    drop
    tsty r0 @ -
;

: band+ ( -- )
    ga @  styled @ if srows then
    r0 @ + tband in-place - gd !  +rows
;

\ the glyph's rows less their first n pixels start the cells
: band> ( n -- )
    tk !
    tend @ tband
    begin
        2dup xor
    while
        dup gd @ + @ tk @ PIXL over !
        1+
    repeat
    2drop
;

[then]

\ ---- text that only adds its black, with nothing to mask:
\ straight to the screen

INK-BLACK? [if] BANKED? [if] [else]

W32+? [if]

\ the rows in c, the first lowest, hi pixels along, to the cell
\ at dst and those below it
: or-plain ( c dst -- )
    >r
    begin dup while
        dup LANE hi @ PIXR r@ or!
        r> DISP-STRIDE + >r  lb @ rshift
    repeat
    drop rdrop
;

: or-right ( c dst -- ) \ the same, only the pixels of tcm
    >r
    begin dup while
        dup LANE hi @ PIXR tcm @ and r@ or!
        r> DISP-STRIDE + >r  lb @ rshift
    repeat
    drop rdrop
;

: or-left ( c dst -- ) \ what runs over into the next cell
    >r
    begin dup while
        dup LANE tk @ PIXL tcm @ and r@ or!
        r> DISP-STRIDE + >r  lb @ rshift
    repeat
    drop rdrop
;

\ c, whose first row is `row`, without its rows from rb on
: lanes-cut ( c row -- c' )
    tclip @ 0= if drop exit then
    rb @ swap in-place -  dup rless @ > if drop exit then
    lsb @ lshift d# 1 swap lshift 1- and
;

\ the glyph's rows from ra up to rb, a cell of them at a time,
\ to the word in 'lanes ( c dst -- )
: glyph-screen ( dst -- )
    ra @ rsh @ rshift ga @ + swap          ( p dst )
    over @  ra @ rless @ and lsb @ lshift rshift
    ra @ lanes-cut  over 'lanes @ execute  ( p dst )
    rless @ 1+ ra @ rless @ and in-place -  DISP-STRIDE * +
    ra @ rless @ or 1+                     ( p dst row )
;fallthru

: screen-rest ( p dst row -- ) \ the cells after the first
    begin dup rb @ < while
        >r  swap 1+ swap                   ( p dst )
        over @ r@ lanes-cut  over 'lanes @ execute
        rless @ 1+ DISP-STRIDE * +
        r> rless @ + 1+
    repeat
    drop 2drop
;

: glyph-over ( dst -- ) \ what runs over into the next cell
    hi @ gw @ + PXCELL > if
        PXCELL hi @ - tk !
        PXCELL cx +!
        colmask ?dup if
            tcm !  ['] or-left 'lanes !  1+ glyph-screen exit
        then
    then
    drop
;

: glyph-or ( g -- ) \ a glyph's rows that are drawn, at pen
    gset gspan
    ra @ rb @ < 0= if exit then
    pen @ PXMASK invert and cx !  pen @ PXMASK and hi !
    ra @ ty @ + row  pen @ PXSHIFT rshift +         ( dst )
    colmask dup 1+ if
        ?dup if
            tcm !  ['] or-right 'lanes !  dup glyph-screen
        then
    else
        drop  ['] or-plain 'lanes !  dup glyph-screen
    then
    glyph-over
;

[else]

\ the rows of a glyph up to rend, hi pixels along
: or-plain ( rows dst -- )
    >r rend @ swap  ( end rows ) ( R: dst )
    begin
        2dup xor
    while
        dup @ hi @ PIXR  r@ or!
        r> DISP-STRIDE + >r
        1+
    repeat
    2drop rdrop
;

: or-right ( rows dst -- ) \ the same, only the pixels of tcm
    >r rend @ swap
    begin
        2dup xor
    while
        dup @ hi @ PIXR tcm @ and  r@ or!
        r> DISP-STRIDE + >r
        1+
    repeat
    2drop rdrop
;

: or-left ( rows dst -- ) \ what runs over into the next cell
    >r rend @ swap
    begin
        2dup xor
    while
        dup @ tk @ PIXL tcm @ and  r@ or!
        r> DISP-STRIDE + >r
        1+
    repeat
    2drop rdrop
;

: glyph-over ( rows dst -- ) \ what runs over into the next
    hi @ gw @ + PXCELL > if
        PXCELL hi @ - tk !
        PXCELL cx +!
        colmask ?dup if tcm ! 1+ or-left exit then
    then
    2drop
;

: glyph-or ( g -- ) \ a glyph's rows that are drawn, at pen
    gset gspan
    ra @ rb @ < 0= if exit then
    ga @ rb @ + rend !  ga @ ra @ +     ( rows )
    pen @ PXMASK invert and cx !  pen @ PXMASK and hi !
    ra @ ty @ + row  pen @ PXSHIFT rshift +  ( rows dst )
    colmask dup 1+ if
        ?dup if tcm ! 2dup or-right then
    else
        drop 2dup or-plain
    then
    glyph-over
;

[then]

: text-fast ( -- ) \ every glyph that will be seen
    tx @ pen !
    gbuf gn @ bounds
    begin
        2dup xor
    while
        dup @ glyph-or
        gw @ pen +!
        1+
    repeat
    2drop
;

[then] [then]

: text-span ( -- ) \ the pixels drawn, and the cells they end in
    tx @ clip @ max vx0 !
    vx1 @ clip CELL2 + @ min vx1 !
    vx0 @ PXMASK invert and  dup m0x !  endmask m0 !
    vx1 @ 1- PXMASK invert and  dup m1x !  endmask m1 !
;

: text-how ( -- ) \ whether it is styled, or only adds its black
    italic @ bold @ or inverse @ or
INK-BLACK? [if] [else]
    drop true
[then]
    styled !
    r1 @ r0 @ - tn !  r1 @ font @ @ < tclip !
INK-BLACK? [if]
    text-or @ inverse @ 0= and tor !
[else]
    d# 0 tor !
[then]
;

: text-gather ( -- ) \ into cells, stored as they fill
    tband tn @ + tend !  d# 0 tband tn @ cells-fill
    tx @ PXMASK invert and cx !  tx @ PXMASK and hi !
    gbuf gn @ bounds
    begin 2dup xor while
        dup @ gset  band+
        hi @ gw @ +  dup PXCELL < if  hi !  else
            PXCELL in-place -        ( pixels over )
            tflush  ?dup if dup hi !  gw @ swap - band> then
        then  1+
    repeat
    2drop  hi @ if tflush then
;

: text-body ( -- ) \ rows r0 up to r1 of the string
    font-rows
    glyphs
    gn @ 0= if exit then
    text-span  text-how
INK-BLACK? [if] BANKED? [if] [else]
    tor @  styled @ 0= and  mrgn @ 0= ttrim @ or
    and if text-fast exit then
[then] [then]
    text-gather
;

\ the clip trimmed to the region's run, if the rows are in one
: text-masked ( -- none? ) \ true if none of it can be seen
    ty @ r0 @ + rmask
    ty @ r1 @ + mk1 @ > 0= if
        mnone @ if true exit then
        mone @ if
            true ttrim !
            clip @ tcx0 !  clip CELL2 + @ tcx1 !
            clip @ mx0 @ max clip !
            clip CELL2 + @ mx1 @ min clip CELL2 + !
        then
    then
    ttrim @ 0= if true mforce !  band-stale then
    false
;

: (draw-chars) ( a i u x y -- )
    ty ! tx ! tu ! tf ! ta !
    ox @ tx +!  oy @ ty +!
    clip CELL + @ ty @ - d# 0 max r0 !
    clip c# 3 + @ ty @ - font @ @ min MAX-ROWS min r1 !
    r0 @ r1 @ < 0= if exit then
    d# 0 ttrim !
    mrgn @ if text-masked if exit then then
    text-body
    ttrim @ if tcx0 @ clip !  tcx1 @ clip CELL2 + ! then
    mforce @ if d# 0 mforce !  band-stale then
;

nearby

\ u characters from character i of the cells from a
header draw-chars : draw-chars ( a i u x y -- )
    text-busy @ if
        dp @ >r
        text-end text-state - borrow >r
        ( R: where memory was, the copy )
        text-state r@ text-end text-state - cells-move
        (draw-chars)
        r> text-state text-end text-state - cells-move
        r> dp !
        exit
    then
    true text-busy !  (draw-chars)  d# 0 text-busy !
;

header draw-text : draw-text ( addr u x y -- )
    >r >r >r >chars r> r> r> draw-chars
;

header draw-char : draw-char ( c x y -- )
    dp @ >r
    rot d# 1 borrow tuck !                          ( x y a )
    -rot >r >r  d# 0 d# 1  r> r> draw-chars
    r> dp !
;
nearby-end

\ ---- ports that are not in use

header port \ the record of the port in use, or 0
    create port 0 ,
header screen-port create screen-port 0 ,

nearby
header port! : port! ( port -- )
    port @ ?dup if gstate swap PORT-CELLS cells-move then
    dup port !
    gstate PORT-CELLS cells-move
    reclip
;

: plain-font ( -- font ) SYSTEM-FONT ;
nearby-end

\ the whole screen, black, copying, plain text
header new-port : new-port ( -- port )
    PORT-CELLS cells >c halloc >r
    d# 0 r@ PORT-CELLS cells-fill
    plain-font r@ c# 11 + !                   \ font
    fb-w @ r@ c# 4 + !  fb-h @ r@ c# 5 + !    \ clip: right, end
    fb-w @ r@ c# 15 + !  fb-h @ r@ c# 16 + !  \ and clip0's
    p-black r@ c# 6 + !                       \ pattern
    ['] m-copy r@ c# 7 + !                    \ 'mode
    r>
;

header display-init
: display-init ( -- )
    ['] dplot 'plot !
    d# 0 text-busy !  d# 0 copy-busy !
    d# 0 port !
    new-port dup screen-port ! port!
;
