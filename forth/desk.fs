\ desk.fs - the desktop: bring up the devices, open the windows, run

create t-repl ," Forth"
create t-edit ," Editor"
create t-view ," Mandrill"
create t-calc ," Calculator"
create t-up   ," desktop"

header desktop
: desktop ( -- ) \ runs until ^Q
    display-init
    ['] on-event 'event !
    input-init
    nidev @ 0= d# 37 -throw                         \ nothing to work it with
    d# 0 nwin !
    ['] vblk-init catch 0= if                       \ these two need the disk
        d# 16 ['] view-load catch if drop then
        d# 250 d# 40 d# 514 d# 532 t-view
        ['] view-draw ['] ignore-key d# 0 window drop
        d# 30 d# 270 d# 518 d# 300 t-edit
        ['] edit-draw ['] edit-key d# 0 window drop
    then
    d# 140 d# 296 d# 482 d# 276 t-words
    ['] words-draw ['] words-key ['] words-click window drop
    d# 626 d# 300 d# 138 d# 238 t-calc
    ['] calc-draw ['] calc-key ['] calc-click window drop
    tclear term-ok
    d# 12 d# 32 d# 406 d# 280 t-repl
    ['] term-draw ['] repl-key d# 0 window drop
    t-up >c count type cr
    event-loop
;

\ The kernel runs a word called cold, if there is one, before its own
\ prompt.  With no keyboard or pointer this fails at once and the serial
\ prompt follows.

header cold
: cold ( -- ) ['] desktop catch drop ;
