\ mandrill.fs - the picture on the disk, shown by itself

header devices
: devices ( -- )
    display-init
;

create showbm 0 , 0 ,

header show
: show ( blk# -- ) \ the TIFF at a block, dithered and centred
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
