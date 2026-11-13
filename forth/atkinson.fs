\ atkinson.fs - Atkinson dithering of `gray` onto the display
\
\ Each pixel is thresholded, and one eighth of its error goes to each of six
\ neighbours:
\             *  1  1
\          1  1  1
\             1
\ Only six eighths are passed on, which is what gives the look.
\
\ Errors are kept in a buffer of cells with a margin of two on each side and
\ two rows below, so the neighbours never need a bounds check.  Each one is
\ a single +! instruction: memory, through the adder, back to memory.

create awork 0 ,        \ the error buffer
create arow  0 ,        \ from one row of it to the next
create ap    0 ,        \ current cell
create asrc  0 ,        \ current byte of gray
create ax    0 ,
create ay    0 ,
create aox   0 ,        \ where the image goes on the display
create aoy   0 ,

: a+! ( e off -- e ) ap @ + over swap +! ;

header atkinson
: atkinson ( x0 y0 -- ) \ draw with its top left corner at x0,y0
    aoy ! aox !
    tw @ d# 4 + cells arow !
    tw @ d# 4 + th @ d# 2 + * d# 3 lshift halloc awork !
    gray @ asrc !
    d# 0 ay !
    begin
        ay @ th @ <
    while
        awork @ ay @ arow @ * + CELL2 + ap !
        d# 0 ax !
        begin
            ax @ tw @ <
        while
            asrc @ c@ ap @ @ +                      ( old )
            dup d# 127 > if d# 255 else d# 0 then   ( old new )
            dup ax @ aox @ + ay @ aoy @ + plot
            - d# 1024 + d# 3 rshift d# 128 -        ( error/8, rounded down )
            CELL a+!
            CELL2 a+!
            arow @ CELL - a+!
            arow @ a+!
            arow @ CELL + a+!
            arow @ 2* a+!
            drop
            CELL ap +!
            d# 1 asrc +!
            d# 1 ax +!
        repeat
        d# 1 ay +!
    repeat
;
