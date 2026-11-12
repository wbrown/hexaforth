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
create fwc #64 allot    \ a directory entry, or the DMA request and the config

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

: fb-addr ( x y -- a ) fb-stride @ * swap d# 2 lshift + fb @ + ;

: ramfb-plot ( on? x y -- )
    fb-addr
    swap if h# ffffff else d# 0 then
    swap l!
;

\ Two pixels are one 64-bit cell, so runs are drawn a pair at a time.  pix2
\ holds the four possible pairs; the left pixel is the low half.

create pix2  0 ,  $00ffffff00000000 ,  $00ffffff ,  $00ffffff00ffffff ,

: pair ( bits shift -- cell ) rshift d# 3 and cells pix2 + @ ;

\ A pair starts on an even pixel.  At an odd x the first and last pixels are
\ each half of a cell, and the six between them are three pairs.

: hi! ( cell a -- ) dup >r @ lo32 or r> ! ;             \ replace the high half
: lo! ( cell a -- ) dup >r @ dup lo32 xor or r> ! ;     \ replace the low half

: odd-addr ( bits x y -- bits a ) swap 1- swap fb-addr ;

: ramfb-blit8 ( bits x y -- )
    over d# 1 and if
        odd-addr
        over d# 7 pair over hi!
        over d# 5 pair over d# 8 + !
        over d# 3 pair over d# 16 + !
        over d# 1 pair over d# 24 + !
        swap 2* d# 0 pair swap d# 32 + lo!
        exit
    then
    fb-addr
    over d# 6 pair over !
    over d# 4 pair over d# 8 + !
    over d# 2 pair over d# 16 + !
    swap d# 0 pair swap d# 24 + !
;

: x! ( cell a -- ) dup >r @ xor r> ! ;

: ramfb-xor8 ( bits x y -- )
    over d# 1 and if
        odd-addr
        over d# 7 pair over x!
        over d# 5 pair over d# 8 + x!
        over d# 3 pair over d# 16 + x!
        over d# 1 pair over d# 24 + x!
        swap 2* d# 0 pair swap d# 32 + x!
        exit
    then
    fb-addr
    over d# 6 pair over x!
    over d# 4 pair over d# 8 + x!
    over d# 2 pair over d# 16 + x!
    swap d# 0 pair swap d# 24 + x!
;

: ramfb-hspan ( on? x y n -- )
    >r >r                                           ( on? x ) ( r: n y )
    dup d# 1 and if                                 \ odd x: one pixel first
        2dup r@ ramfb-plot
        1+ r> r> 1- >r >r
    then
    r> fb-addr
    swap if pix2 d# 24 + @ else d# 0 then swap      ( cell a )
    r>
    begin
        dup d# 1 >
    while
        >r 2dup ! d# 8 + r> d# 2 -
    repeat
    0> if swap lo32 swap l! else 2drop then
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

    ['] ramfb-plot  'plot !
    ['] ramfb-hspan 'hspan !
    ['] ramfb-blit8 'blit8 !
    ['] ramfb-xor8  'xor8 !
;
