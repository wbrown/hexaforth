\ display.fs - the display interface: one bit per pixel

create fb-w  0 ,        \ width and height in pixels
create fb-h  0 ,
create 'plot 0 ,        \ ( on? x y -- )

header plot
: plot ( on? x y -- ) 'plot @ dup 0= d# 37 -throw execute ;
