\ desk.fs - the desktop: bring up the devices, open the windows, run

create t-repl ," Forth"
create t-edit ," Editor"
create t-view ," Mandrill"
create t-calc ," Calculator"
create t-paint ," HexaPaint"
create t-up   ," desktop"

\ The windows are opened by one word and the machine's devices brought up
\ by another, so that a machine with other devices, or with none, opens
\ the same windows.  The picture and the editor are there if there is a
\ disk, which is to say if a driver has said how a block is read.

\ Where each window goes: its left, top, width and height, for the
\ picture, the editor, the Words window, the calculator and the Forth
\ window, which are opened in that order, the last in front, and for
\ HexaPaint, which is opened before them all and so is at the back.  There are
\ two arrangements.  One is for a screen of 768 by 576.  The other is for
\ a smaller one, down to 512 by 342, where a window is as big as it was
\ but for the picture's, which shows 256 by 256 of it: they lie over each
\ other more, each with some of its title to be seen, and the editor,
\ whose 64 characters are as wide as the screen, has its frame off both
\ sides of it.

create big-desk
    #250 , #40 , #514 , #532 ,
    #30 , #270 , #518 , #300 ,
    #140 , #296 , #482 , #276 ,
    #626 , #300 , #138 , #238 ,
    #12 , #32 , #406 , #280 ,
    #180 , #140 , #431 , #297 ,

create small-desk
    #246 , #22 , #258 , #276 ,
    -3 , #40 , #518 , #300 ,
    #4 , #60 , #482 , #276 ,
    #366 , #96 , #138 , #238 ,
    #6 , #26 , #406 , #280 ,
    #110 , #30 , #287 , #193 ,

: place ( i -- x y w h ) \ where window i goes, on this screen
    d# 2 lshift  fb-h @ d# 576 < if small-desk else big-desk then  +
    dup @ swap  dup 1+ @ swap  dup CELL2 + @ swap  c# 3 + @
;

header windows
: windows ( -- ) \ open them, and run until ^Q
    d# 0 nwin !
    d# 5 place t-paint                              \ at the back: it is as big as its canvas, and has no canvas
    ['] paint-draw ['] ignore-key ['] paint-click window drop      \ until it is drawn in
    d# 0 >r                                         ( R: the picture's window, or 0 if there is no disk )
    'blk-read @ if
        d# 0 place t-view
        ['] view-draw ['] ignore-key d# 0 window  dup view-menu  rdrop >r
        d# 1 place t-edit
        ['] edit-draw ['] edit-key d# 0 window drop
    then
    d# 2 place t-words
    ['] words-draw ['] words-key ['] words-click window drop
    d# 3 place t-calc
    ['] calc-draw ['] calc-key ['] calc-click window drop
    tclear term-ok
    repl-start
    d# 4 place t-repl
    ['] term-draw ['] repl-key d# 0 window drop
    \ The picture is started last, after all the windows and the event
    \ loop's regions (wm-regions) are allocated.  Then the memory reading
    \ it takes is the most recent allocation, and is fully returned when
    \ the last row is made (view.fs).
    wm-regions
    r> ?dup if
        >r  PICTURE-BLK  d# 0 place 2swap 2drop  swap d# 2 - swap d# 20 -     \ the picture, and the size its window shows
        ['] view-load catch if 2drop drop d# 0 then  r> win-data !         \ the viewer, or 0 if there is no picture
    then
    t-up >c count type cr
    event-loop
;

\ The QEMU virt machine: keyboards and pointers are virtio input devices,
\ and the disk a virtio block device.  The console's keys are taken too.
\ A machine with other devices brings them up itself and runs `windows`.

W64? [if]

: virt-poll ( -- ) input-poll uart-poll ;

header desktop
: desktop ( -- ) \ runs until ^Q
    display-init
    ['] on-event 'event !
    input-init
    nidev @ 0= d# 37 -throw                         \ nothing to work it with
    ['] virt-poll 'poll !
    ['] vblk-init catch drop
    windows
;

\ The kernel runs a word called cold, if there is one, before its own
\ prompt.  With no keyboard or pointer this fails at once and the serial
\ prompt follows.

header cold
: cold ( -- ) ['] desktop catch drop ;

[then]
