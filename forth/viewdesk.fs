\ viewdesk.fs - the desktop of the picture-only
\ system (picture.fs)

create t-view ," Mandrill"

: picture-window ( -- ) \ its window, and the picture in it
    d# 0 d# 20  fb-w @  fb-h @ d# 20 -  t-view
    ['] view-draw ['] ignore-key d# 0 window  dup view-menu
    >r
    wm-regions
    picture-blk  fb-w @ d# 2 -  fb-h @ d# 40 -
    ['] view-load catch if 2drop drop d# 0 then
    r> win-data !
;

header windows
: windows ( -- ) \ open it; ^Q with none open ends it
    no-windows
    find-disks
    'blk-read @ if picture-window then
    event-loop
;
