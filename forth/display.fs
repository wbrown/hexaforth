\ display.fs - the display interface: one bit per pixel
\
\ A driver stores its routines in these vectors.  plot is enough to draw
\ anything; the others are there because text and windows are made of
\ horizontal runs.  A set bit is white.

create fb-w   0 ,       \ width and height in pixels
create fb-h   0 ,
create 'plot  0 ,       \ ( on? x y -- )        one pixel
create 'hspan 0 ,       \ ( on? x y n -- )      n pixels to the right
create 'blit8 0 ,       \ ( bits x y -- )       8 pixels, leftmost in bit 7
create 'xor8  0 ,       \ ( bits x y -- )       invert where bits are set

header plot
: plot  ( on? x y -- )   'plot  @ dup 0= d# 37 -throw execute ;
: hspan ( on? x y n -- ) 'hspan @ execute ;
: blit8 ( bits x y -- )  'blit8 @ execute ;
: xor8  ( bits x y -- )  'xor8  @ execute ;
