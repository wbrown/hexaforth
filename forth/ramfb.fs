\ ramfb.fs - display driver for QEMU's ramfb
\
\ ramfb shows whatever is in a region of guest memory.  It is set up by
\ writing one structure to the firmware configuration file "etc/ramfb".  The
\ firmware configuration device is at 0x09020000 on `virt`: an 8-byte data
\ register, a 16-bit selector at +8 and a 64-bit DMA address at +16, all
\ big-endian.  The pixels are 32 bits each; a set bit is drawn white.

$09020000 constant FWCFG

create fb        0 ,    \ the pixels
create fb-stride 0 ,    \ bytes per row
create fwc 64 allot     \ a directory entry, or the DMA request and the config

create ramfb-name 'e' c, 't' c, 'c' c, '/' c, 'r' c, 'a' c, 'm' c, 'f' c,

: fw-select ( key -- )
    dup d# 8 lshift swap d# 8 rshift or h# ffff and
    FWCFG d# 8 + reg16!
;

: fw-entry ( -- ) \ the next 64-byte directory entry, into fwc
    fwc d# 64 bounds
    begin
        2dupxor
    while
        FWCFG reg64@ over !
        d# 8 +
    repeat
    2drop
;

: fw-find ( -- key | 0 ) \ selector of the file etc/ramfb
    h# 19 fw-select                         \ the file directory
    FWCFG reg32@ fwc !  fwc be32@           ( entries )
    begin
        dup
    while
        fw-entry                            \ size, select, reserved, name
        fwc d# 8 + @ ramfb-name @ =         \ "etc/ramf"
        fwc d# 16 + uw@ [char] b = and      \ "b", then the terminator
        if drop fwc d# 4 + be16@ exit then
        1-
    repeat
;

: ramfb-plot ( on? x y -- )
    fb-stride @ * swap d# 2 lshift + fb @ +
    swap if h# ffffff else d# 0 then
    swap l!
;

header ramfb-init
: ramfb-init ( w h -- )
    fb-h ! fb-w !
    fb-w @ d# 2 lshift fb-stride !
    fb-stride @ fb-h @ * halloc fb !
    fw-find dup 0= d# 37 -throw             ( key )

    fb @ >phys      fwc d# 16 + be64!       \ config: address,
    h# 34325258     fwc d# 24 + be32!       \ format XRGB8888,
    d# 0            fwc d# 28 + be32!       \ flags,
    fb-w @          fwc d# 32 + be32!       \ width, height, stride
    fb-h @          fwc d# 36 + be32!
    fb-stride @     fwc d# 40 + be32!

    d# 16 lshift h# 18 or  fwc be32!        \ request: select the file, write
    d# 28           fwc d# 4 + be32!        \ 28 bytes
    fwc d# 16 + >phys  fwc d# 8 + be64!     \ from the config
    fwc >phys       fwc d# 48 + be64!       \ and its own address, big-endian
    fwc d# 48 + @   FWCFG d# 16 + reg64!
    fwc be32@ d# 38 -throw                  \ control is zero once done

    ['] ramfb-plot 'plot !
;
