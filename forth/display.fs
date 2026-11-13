\ display.fs - the display interface: one bit per pixel
\
\ A driver stores its routines in these vectors.  plot is enough to draw
\ anything; the others are there because text and windows are made of
\ horizontal runs.  A set bit is white.

header fb-w create fb-w   0 ,       \ width and height in pixels
header fb-h create fb-h   0 ,
header 'plot create 'plot  0 ,       \ ( on? x y -- )        one pixel
header 'hspan create 'hspan 0 ,       \ ( on? x y n -- )      n pixels to the right
header 'blit8 create 'blit8 0 ,       \ ( bits x y -- )       8 pixels, leftmost in bit 7
header 'xor8 create 'xor8  0 ,       \ ( bits x y -- )       invert where bits are set

header plot
: plot  ( on? x y -- )   'plot  @ dup 0= d# 37 -throw execute ;
header hspan : hspan ( on? x y n -- ) 'hspan @ execute ;
header blit8 : blit8 ( bits x y -- )  'blit8 @ execute ;
header xor8 : xor8  ( bits x y -- )  'xor8  @ execute ;
