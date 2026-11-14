\ mandrill.fs - bring up the devices and show the image on the disk

header devices
: devices ( -- ) \ display and block device
    display-init
    vblk-init
;

create showbm 0 , 0 ,

header show
: show ( blk# -- ) \ dither the TIFF starting at a block, centred
    tiff-open tiff-decode
    showbm atkinson
    copy-mode
    showbm d# 0 d# 0
    fb-w @ tw @ - 2/
    fb-h @ th @ - 2/
    tw @ th @ copy-bits
;

header mandrill
: mandrill ( -- )
    devices
    d# 16 show
;
