\ mandrill.fs - bring up the devices and show the image on the disk

header devices
: devices ( -- ) \ display and block device
    display-init
    vblk-init
;

header show
: show ( blk# -- ) \ dither the TIFF starting at a block, centred
    tiff-open tiff-decode
    fb-w @ tw @ - 2/
    fb-h @ th @ - 2/
    atkinson
;

header mandrill
: mandrill ( -- )
    devices
    d# 16 show
;
