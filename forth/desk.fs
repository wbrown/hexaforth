\ desk.fs - the desktop: bring up the devices, open the windows, run

create t-repl ," Forth"
create t-edit ," Editor"
create t-view ," Mandrill"
create t-calc ," Calculator"
create t-up   ," desktop"

\ The windows are opened by one word and the machine's devices brought up
\ by another, so that a machine with other devices, or with none, opens
\ the same windows.  The picture and the editor are there if there is a
\ disk, which is to say if a driver has said how a block is read.

header windows
: windows ( -- ) \ open them, and run until ^Q
    d# 0 nwin !
    'blk-read @ if
        d# 16 ['] view-load catch if drop d# 0 then >r       ( R: the viewer, or 0 if there is no picture )
        d# 250 d# 40 d# 514 d# 532 t-view
        ['] view-draw ['] ignore-key d# 0 window
        r> swap win-data !
        d# 30 d# 270 d# 518 d# 300 t-edit
        ['] edit-draw ['] edit-key d# 0 window drop
    then
    d# 140 d# 296 d# 482 d# 276 t-words
    ['] words-draw ['] words-key ['] words-click window drop
    d# 626 d# 300 d# 138 d# 238 t-calc
    ['] calc-draw ['] calc-key ['] calc-click window drop
    tclear term-ok
    repl-start
    d# 12 d# 32 d# 406 d# 280 t-repl
    ['] term-draw ['] repl-key d# 0 window drop
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
