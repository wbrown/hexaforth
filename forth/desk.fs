\ desk.fs - the desktop: bring up the devices, open the
\ windows, run

create t-repl ," Forth"
create t-edit ," Editor"
create t-view ," Mandrill"
create t-calc ," Calculator"
create t-paint ," HexaPaint"
create t-up   ," desktop"

\ left, top, width and height: of the picture, the editor, the
\ Words window, the calculator, the Forth window and HexaPaint

create big-desk     \ for a screen of 576 rows or more
    #250 , #40 , #514 , #532 ,
    #250 , #150 , #518 , #300 ,
    #140 , #184 , #482 , #388 ,
    #626 , #300 , #138 , #238 ,
    #12 , #32 , #406 , #280 ,
    #180 , #140 , #431 , #297 ,

create small-desk
    #246 , #22 , #258 , #276 ,
    -3 , #40 , #518 , #300 ,
    #4 , #34 , #482 , #308 ,
    #366 , #96 , #138 , #238 ,
    #6 , #26 , #406 , #280 ,
    #110 , #30 , #287 , #193 ,

: place ( i -- x y w h ) \ where window i goes, on this screen
    d# 2 lshift
    fb-h @ d# 576 < if small-desk else big-desk then  +
    dup @ swap  dup 1+ @ swap  dup CELL2 + @ swap  c# 3 + @
;

: back-window ( -- ) \ HexaPaint
    d# 5 place t-paint
    ['] paint-draw ['] ignore-key ['] paint-click window drop
;

\ the picture's window, and the editor's, if there is a disk
: disk-windows ( -- win|0 )
    'blk-read @ 0= if d# 0 exit then
    d# 0 place t-view
    ['] view-draw ['] ignore-key d# 0 window  dup view-menu
    d# 1 place t-edit
    ['] edit-draw ['] edit-key d# 0 window drop
;

: tool-windows ( -- ) \ the Words window, the calculator, Forth
    d# 2 place t-words
    ['] words-draw ['] words-key ['] words-click window drop
    d# 3 place t-calc
    ['] calc-draw ['] calc-key ['] calc-click window drop
    tclear term-ok
    repl-start
    d# 4 place t-repl
    ['] term-draw ['] repl-key d# 0 window drop
;

: picture-start ( win|0 -- ) \ the picture, in its window
    ?dup 0= if exit then
    >r  picture-blk  d# 0 place 2swap 2drop
    swap d# 2 - swap d# 20 -
    ['] view-load catch if 2drop drop d# 0 then
    r> win-data !
;

header windows
: windows ( -- ) \ open them; ^Q with none open ends it
    no-windows
    find-disks
    back-window  disk-windows  tool-windows
    wm-regions  picture-start
    t-up >c count type cr
    event-loop
;

\ QEMU's virt: virtio input devices, and the console's keys

W64? [if]
: virt-poll ( -- ) input-poll uart-poll ;
header desktop
: desktop ( -- ) \ runs until ^Q with no window open
    display-init
    ['] on-event 'event !
    input-init
    nidev @ 0= d# 37 -throw
    ['] virt-poll 'poll !
    windows
;
[then]

W64? [if]
header cold
: cold ( -- ) ['] desktop catch drop ;
[then]
