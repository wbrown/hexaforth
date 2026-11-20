\ console.fs - the screen as the console, beside the serial line

\ A machine started from its ROM says what it is doing while it
\ loads (doc/boot.md): what is printed then goes to the serial
\ line and is drawn on the screen as well, in the port's font, a
\ line under the one before, and the screen goes up a line when
\ the last is full.  After each character key? is asked, since
\ QEMU's bridge copies the screen to its window only then: a
\ line that is added to a character at a time, as a file's is
\ with a dot for each block of it, is seen as it grows.  (It was
\ asked at the end of a line, and such a line was not seen until
\ it ended.)  After an error the prompt prints here as well
\ ('console, nuc.fs).

create con-x 0 ,                \ where the next character goes,
create con-y 0 ,                \ in pixels

: con-h ( -- n ) font @ @ ;     \ a line: as high as the font

\ How wide a character is in the port's font: it is put in a
\ cell that is kept for it, and measured as text.  (It was put
\ at here, as where nothing is.  While a file is compiled here
\ may be a cell that is half laid, and the console prints then:
\ a dot said for a block of the file went over the end of a
\ label the file had just laid.)
create con-c 0 ,
: con-w ( c -- n ) con-c !  con-c >c d# 1 text-width ;

: con-nl ( -- ) \ the start of the next line
    d# 0 con-x !
    con-y @ con-h +  dup con-h + fb-h @ > if
        drop  \ the last line: the screen goes up
        d# 0 d# 0 fb-w @ fb-h @  d# 0 con-h negate  scroll-rect
    else
        con-y !
    then
;

: con-char ( c -- )
    \ drawn where the cursor is, which moves past it
    dup con-w  con-x @ +  fb-w @ > if con-nl then
    dup con-x @ con-y @ draw-char
    con-w con-x +!
;

: con-draw ( c -- ) \ drawn, if it is anything that is
    dup d# 10 = if drop con-nl exit then
    dup d# 13 = if drop d# 0 con-x ! exit then
    dup d# 8 = if                   \ back over a character
        drop  con-x @ bl con-w -  d# 0 max  con-x !  exit
    then
    dup bl < if drop exit then
    con-char
;

header con-emit
: con-emit ( c -- ) \ sent to the serial line, drawn, and shown
    dup uart-emit  con-draw  key? drop
;

header console
: console ( -- ) \ the screen made white, and made the console
    display-init
    p-white pattern !
    d# 0 d# 0 fb-w @ fb-h @ pat-rect
    p-black pattern !
    d# 0 con-x !  d# 0 con-y !
    ['] con-emit dup 'emit !  'console !
;

\ What a machine started from its ROM goes on with, once the ROM
\ has read the rest of the system from the disk it found
\ (nuc.fs): the screen is the console, each disk is asked
\ whether it has an index (files.fs), and the block of what is
\ loaded is loaded (doc/boot.md).
header boot
: boot ( -- ) console  find-disks  SOURCE-BLK load ;
