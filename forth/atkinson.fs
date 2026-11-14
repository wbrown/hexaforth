\ atkinson.fs - Atkinson dithering of the image tiff.fs has read
\
\ Each pixel is thresholded, and one eighth of its error goes to each of six
\ neighbours:
\             *  1  1
\          1  1  1
\             1
\ Only six eighths are passed on, which is what gives the look.
\
\ Nothing here is done a pixel at a time but the threshold itself.
\
\ The two neighbours on the same row are the next two pixels, so their
\ share never goes to memory: it is the errors of the last two pixels,
\ aprev and aprev2.
\
\ The other four are on the two rows below, and they are reached a cell at
\ a time.  A row's errors are kept in lanes, sixteen bits each, as many to
\ a cell as fit.  When the row is done, a cell of them is added to the same
\ cell of the row two below; and that cell, itself a lane to the left and
\ itself a lane to the right are added to the row below, which is the
\ three neighbours there.  The lane that leaves a cell at either end goes
\ to the cell beside it.  With one lane to a cell the shifts by a lane give
\ nothing and the ends are the whole cell, so the same words do.
\
\ So that a lane never borrows from the next, what is kept of an error is
\ never negative: an error e is kept as e + 128.  A pixel is given four of
\ them from the rows above, so its lane holds what it is owed and 512 more.
\ A row has a lane before its first pixel and one after its last, which
\ hold an error of nothing (128), and the first two rows start with what
\ the rows that are not there would have given.  The last two pixels'
\ errors are kept the same way, so the sum a pixel is thresholded on is 768
\ too many; the threshold and the error allow for that, and nothing is
\ subtracted anywhere, which the machine would need three instructions for.
\
\ The pixels go into a cell of the bitmap as the display has them, and the
\ cell is stored when it is full.

create abm   0 ,        \ the bitmap: its descriptor
create an    0 ,        \ cells in a row of lanes
create acur  0 ,        \ what this row is owed,
create anxt  0 ,        \ and the next,
create anx2  0 ,        \ and the one after: each a row of lanes
create aerr  0 ,        \ this row's errors
create asp   0 ,        \ where in acur
create aep   0 ,        \ where in aerr
create ash   0 ,        \ the lane, as a shift
create aew   0 ,        \ the cell of errors being made
create aprev  0 ,       \ the last pixel's error, and the one before's
create aprev2 0 ,
create aout  0 ,        \ the cell of pixels being made
create amask 0 ,        \ the next pixel in it
create adst  0 ,        \ where it goes
create ay    0 ,
create aj    0 ,

: alane ( e+128 -- ) \ put an error in its lane, and go on to the next
    ash @ lshift aew @ or aew !
    ash @ d# 16 +
    dup CELLBITS = if
        drop
        aew @ aep @ !  d# 0 aew !
        d# 1 aep +!  d# 1 asp +!
        d# 0
    then
    ash !
;

: adot ( white? -- ) \ a pixel of the bitmap
INK-BLACK? [if]
    0= if amask @ aout @ or aout ! then
[else]
    if amask @ aout @ or aout ! then
[then]
    amask @ d# 1 PIXR
    dup 0= if
        drop
        aout @ adst @ !  d# 0 aout !
        d# 1 adst +!
        LEFT1
    then
    amask !
;

: apixel ( -- )
    'pixel @ execute
    asp @ @ ash @ rshift h# ffff and +
    aprev @ + aprev2 @ +                            ( old+768 )
    dup d# 895 > if                                 \ old > 127: white, which is 255
        1+ true                                     \ old - 255 + 1024
    else
        d# 256 + false                              \ old - 0 + 1024
    then
    adot
    d# 3 rshift                                     ( error/8, rounded down, and 128 )
    aprev @ aprev2 !
    dup aprev !
    alane
;

: arow ( -- ) \ one row of the image
    acur @ asp !  aerr @ aep !
    d# 0 ash !  d# 0 aew !  d# 128 aprev !  d# 128 aprev2 !
    d# 0 aout !  LEFT1 amask !
    d# 128 alane
    tw @ begin dup while apixel 1- repeat drop
    d# 128 alane
    ash @ if aew @ aep @ ! then
    amask @ LEFT1 <> if aout @ adst @ ! then
;

: aspread ( -- ) \ the row's errors, to the two rows below
    d# 0 aj !
    begin
        aj @ an @ <
    while
        aerr @ aj @ + @                             ( e )
        dup anx2 @ aj @ + +!
        dup dup d# 16 lshift + over d# 16 rshift +  anxt @ aj @ + +!
        dup CELLBITS d# 16 - rshift  anxt @ aj @ + 1+ +!
        CELLBITS d# 16 - lshift  anxt @ aj @ + 1- +!
        d# 1 aj +!
    repeat
;

: alanes ( x a -- ) \ set a row of lanes, and the cell at each end of it
    1- an @ d# 2 +
    begin
        dup
    while
        >r 2dup ! 1+ r> 1-
    repeat
    drop 2drop
;

: anew ( x -- a ) an @ d# 2 + >c halloc 1+  tuck alanes ;

header atkinson
: atkinson ( bitmap -- ) \ dither the image into a new bitmap, as the display is
    abm !
    tw @ PXMASK + PXSHIFT rshift abm @ CELL + !
    abm @ CELL + @ th @ * >c halloc abm @ !
    tw @ d# 2 + d# 4 lshift CELLBITS 1- + CELLBITS u/mod nip an !
    d# 512 LANE1 * anew acur !
    d# 128 LANE1 * anew anxt !
    d# 0 anew anx2 !
    d# 0 anew aerr !
    pixels
    d# 0 ay !
    begin
        ay @ th @ <
    while
        abm @ @  ay @ abm @ CELL + @ * +  adst !
        arow
        aspread
        acur @  anxt @ acur !  anx2 @ anxt !  dup anx2 !
        d# 0 swap alanes
        d# 1 ay +!
    repeat
;
