\ mandrill.fs - bring up the display and show the image on
\ the disk, which the kernel's ROM found (nuc.fs)

header devices
: devices ( -- ) \ the display
    display-init
;

create showbm 0 , 0 ,

header show
: show ( blk# -- )
    \ dither the TIFF starting at a block, centred
    tiff-open
    showbm swap atkinson
    copy-mode
    showbm d# 0 d# 0
    fb-w @ tw @ - 2/
    fb-h @ th @ - 2/
    tw @ th @ copy-bits
;

header mandrill
: mandrill ( -- )
    devices
    find-disks  picture-blk show
;
