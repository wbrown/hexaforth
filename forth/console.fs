\ console.fs - the screen as the console, beside the serial line

create con-x 0 ,                \ where the next character goes,
create con-y 0 ,                \ in pixels

: con-h ( -- n ) font @ @ ;

create con-c 0 ,
: con-w ( c -- n ) con-c !  con-c >c d# 1 text-width ;

: con-nl ( -- ) \ the screen goes up, if the last line is full
    d# 0 con-x !
    con-y @ con-h +  dup con-h + fb-h @ > if
        drop
        d# 0 d# 0 fb-w @ fb-h @  d# 0 con-h negate  scroll-rect
    else
        con-y !
    then
;

: con-char ( c -- )
    dup con-w  con-x @ +  fb-w @ > if con-nl then
    dup con-x @ con-y @ draw-char
    con-w con-x +!
;

: con-draw ( c -- )
    dup d# 10 = if drop con-nl exit then
    dup d# 13 = if drop d# 0 con-x ! exit then
    dup d# 8 = if
        drop  con-x @ bl con-w -  d# 0 max  con-x !  exit
    then
    dup bl < if drop exit then
    con-char
;

header con-emit
: con-emit ( c -- ) \ to the serial line, and on the screen
    dup uart-emit  con-draw  key? drop
;

header console
: console ( -- ) \ the screen, white, is the console too
    display-init
    p-white pattern !
    d# 0 d# 0 fb-w @ fb-h @ pat-rect
    p-black pattern !
    d# 0 con-x !  d# 0 con-y !
    ['] con-emit dup 'emit !  'console !
;

\ what the ROM runs once it has read the system (nuc.fs, main)
header boot
: boot ( -- ) console  find-disks  SOURCE-BLK load ;
