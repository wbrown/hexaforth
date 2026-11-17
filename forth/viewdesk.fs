\ viewdesk.fs - the desktop of the system with the picture alone (picture.fs)
\
\ One window, the picture's, as wide as the screen and down to its bottom
\ under the menu bar; the viewer shows the picture at half its size if it
\ is bigger than that.  As with the whole desktop (desk.fs), the picture is
\ there if there is a disk.

create t-view ," Mandrill"

header windows
: windows ( -- ) \ open it, and run until ^Q
    d# 0 nwin !
    'blk-read @ if
        d# 0 d# 20  fb-w @  fb-h @ d# 20 -  t-view
        ['] view-draw ['] ignore-key d# 0 window  dup view-menu  >r        \ the window first, as desk.fs has it,
        wm-regions                                                          \ and what the event loop has
        PICTURE-BLK  fb-w @ d# 2 -  fb-h @ d# 40 -     \ the picture, and what its window shows
        ['] view-load catch if 2drop drop d# 0 then  r> win-data !         \ the viewer, or 0 if there is no picture
    then
    event-loop
;
