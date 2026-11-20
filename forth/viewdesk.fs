\ viewdesk.fs - the desktop of the picture-only
\ system (picture.fs)

\ One window, the picture's, filling the screen below the menu
\ bar; the viewer shows the picture at half size if it is bigger
\ than that.  As on the whole desktop (desk.fs), the picture is
\ shown only if there is a disk.

create t-view ," Mandrill"

\ The window first, as in desk.fs, then the event loop's
\ regions, and then the picture and the size its window shows;
\ the viewer, or 0 if there is no picture.
: picture-window ( -- )
    d# 0 d# 20  fb-w @  fb-h @ d# 20 -  t-view
    ['] view-draw ['] ignore-key d# 0 window  dup view-menu
    >r
    wm-regions
    picture-blk  fb-w @ d# 2 -  fb-h @ d# 40 -
    ['] view-load catch if 2drop drop d# 0 then
    r> win-data !
;

header windows
: windows ( -- ) \ open the window, and run until ^Q
    d# 0 nwin !
    find-disks  \ the picture is on one of them, by its label
    'blk-read @ if picture-window then
    event-loop
;
