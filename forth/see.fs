\ see.fs - a decompiler, and a window that browses the dictionary with it
\
\ The decompiler turns compiled code back into words, one token at a time,
\ and hands each token to the routine in 'tok along with the execution
\ token it stands for, or -1 for none.  SEE types the tokens.  The Words
\ window draws them, and makes each one that names code a link.
\
\ A definition is read twice.  The first reading finds where it ends and
\ where its loops begin; the second produces the tokens.
\
\   literal, then imm+ ...            one number
\   literal, then call execute        a call to code on another page
\   0branch forwards                  if      (then, where it lands)
\   jump forwards, over an if's end   else
\   0branch or jump backwards         until, again   (begin, where it lands)
\   0branch forwards to just past a
\     jump backwards                  while, and that jump is repeat
\   jump anywhere else                a call that does not come back
\   an instruction with RET           its word, then exit
\
\ An ALU instruction is shown as words from the table in fused.fs, which is
\ made from the image and has every instruction that is in it: the plain
\ words a programmer would write for it, up to four, each by the address
\ of its header.  Where it is more than one word, sbold is set, so that an
\ instruction that is several words can be told from words compiled one by
\ one.  An instruction that is not in the table is shown as its number in
\ brackets.
\
\ The tokens are laid out as source is written.  The name has the first
\ line and the semicolon the last.  What is inside an if or a begin is
\ indented, and an if, an else, a then or a store ends its line.  The
\ routine in 'nl starts a new line; the indent to give the next token is
\ in sind, and sfresh is true while nothing is on the line yet.

create wi 0 ,           \ a loop counter

create 'tok 0 ,
create 'nl 0 ,
create sind 0 ,
create sfresh 0 ,
create sbold 0 ,        \ true for the words of a fused instruction
create sital 0 ,        \ true for a name that is a label, not in the dictionary
create slab 0 ,         \ true if the last xt>hdr found a label
create squiet 0 ,       \ true on the first reading
create sxt 0 ,          \ the definition
create spc 0 ,          \ the instruction being read
create send 0 ,         \ its last instruction
create smax 0 ,         \ the furthest a forward branch has reached
create sacc 0 ,         \ a number being put together
create slit 0 ,         \ true if there is one
create sfw #128 allot   \ where forward branches land
create nfw 0 ,
create sbg #128 allot   \ where backward branches land
create nbg 0 ,
create swh #64 allot    \ where a while leaves its loop
create nwh 0 ,
create stmp 0 ,

create k-colon  ," :"
create k-noname ," :noname"
create k-semi   ," ;"
create k-imm    ," immediate"
create k-if     ," if"
create k-else   ," else"
create k-then   ," then"
create k-begin  ," begin"
create k-until  ," until"
create k-again  ," again"
create k-while  ," while"
create k-repeat ," repeat"
create k-exit   ," exit"
create k-plus   ," +"
create k-halt   ," halt"
create k-tick   ," [']"
create k-fetch  ," @"
create k-store  ," !"

: tok ( addr u xt -- )
    squiet @ if drop 2drop exit then
    'tok @ execute  d# 0 sfresh !
;
: wt  ( cstr -- ) >c count d# -1 tok ;
: snl ( -- ) squiet @ sfresh @ or if exit then 'nl @ execute  true sfresh ! ;
: s+  ( -- ) d# 1 sind +! ;
: s-  ( -- ) sind @ 1- d# 0 max sind ! ;

: ins@ ( a -- w ) i@ ;
: hxt  ( hdr -- xt ) cell+ @ d# 1 lshift d# 1 rshift ;

\ The names of the definitions that have no header, the labels, are not in
\ the image: they are for the decompiler, and are on the disk.  With them
\ are two tables of the names as they were when the image was built, the
\ dictionary's words and the labels: by execution token, and those that
\ are a number (a word that only pushes one, a label for data) by the
\ number.  A table is how many, then a cell for each in order: the key in
\ the upper half, and in the lower where the name is, with the top bit of
\ that half set for a label.  For a word that is the address of its
\ header.  For a label it is how many cells it is from the start of the
\ labels, since they are wherever memory was found for them; and a label
\ is laid out as a header is, the one before it, its value, and its name.
\
\ The file is on the disk from block 1024: a stamp that must be the
\ image's, how many cells follow, the two tables, and the labels.  It is
\ read into memory when there is decompiling to do (names-load) and the
\ memory is given back after (names-unload), if nothing has taken more
\ since.  While it is in, only the words made since the build are searched
\ for one at a time, back to forth0, which was the newest then.  While it
\ is not, or with no disk, the whole dictionary is searched, and what has
\ no header is shown as a number.
create forth0 0 ,
header tstamp create tstamp 0 ,     \ in the dictionary, so that a tool can find it in an image
create xtab 0 ,         \ the tables, or 0 while they are not in memory
create dtab 0 ,
create lbase 0 ,        \ and where the labels start
create nm-mark 0 ,      \ where memory was before they were read in,
create nm-end 0 ,       \ and after
create nm-head 0 , 0 ,  \ the first two cells of the file
create nm-lo 0 ,        \ where the part of a table still to look in starts,
create nm-n 0 ,         \ and how many entries it is

$100000 constant NAMES-AT   \ where on the disk, in bytes: block 1024

: (names-load) ( -- )
    nm-head >c d# 16 NAMES-AT disk-read
    nm-head @ tstamp @ <> if exit then
    himem @ nm-mark !
    nm-head CELL + @ >c halloc                      ( a )
    himem @ nm-end !
    dup >c  nm-head CELL + @ >c  NAMES-AT d# 16 +  disk-read
    dup dup @ + 1+ dtab !
    dtab @ dup @ + 1+ lbase !
    \ The labels: each holds how far the one before it is from the start,
    \ and the first cell how far the newest is.  Make addresses of them.
    lbase @ @ ?dup if
        lbase @ + dup labels !
        begin
            dup @ ?dup
        while
            lbase @ +  2dup swap !  nip
        repeat
        drop
    then
    xtab !
;

header names-load
: names-load ( -- )
    xtab @ if exit then
    'blk-read @ if ['] (names-load) catch drop then
;

header names? : names? ( -- f ) xtab @ 0<> ;        \ are they in memory?

header names-unload
: names-unload ( -- )
    xtab @ 0= if exit then
    himem @ nm-end @ <> if exit then                \ something else has memory after them
    nm-mark @ himem !
    d# 0 xtab !  d# 0 dtab !  d# 0 labels !
;

: tfind ( key table -- entry|0 ) \ by halving; what is given is the lower half
    dup @ nm-n !  1+ nm-lo !
    begin
        nm-n @
    while
        nm-n @ d# 1 rshift                          ( key half )
        2dup nm-lo @ + @                            ( key half key entry )
        dup>r d# 32 rshift
        2dup = if 2drop 2drop r> d# 32 lshift d# 32 rshift exit then
        u> if                                       \ it is after the middle one,
            dup 1+ nm-lo +!  invert nm-n +!
        else                                        \ or before it
            nm-n !
        then
        rdrop
    repeat
    drop d# 0
;

: tstop ( -- hdr|0 ) \ how far back the dictionary is searched a word at a time
    xtab @ if forth0 @ else d# 0 then
;
: label? ( entry -- f ) d# 31 rshift 0<> ;
: >hdr   ( entry -- hdr ) \ where the name is
    dup d# 33 lshift d# 33 rshift  swap label? if lbase @ + then
;

: xt>hdr ( xt -- hdr|0 ) \ the word that starts at xt: in the dictionary, or a label
    stmp !  d# 0 slab !
    tstop >r forth @
    begin
        dup r@ <>
    while
        dup hxt stmp @ = if rdrop exit then
        @
    repeat
    rdrop drop
    xtab @ 0= if d# 0 exit then
    stmp @ xtab @ tfind dup if dup label? slab !  >hdr then
;

: data>lbl ( a -- lbl|0 ) \ the label for the data at a
    dtab @ 0= if drop d# 0 exit then
    dtab @ tfind dup if dup label? if >hdr else drop d# 0 then then
;

\ A number on its own is not an address of code unless it is in the kernel
\ and system pages, or in the pages from 8 up.
: code? ( x -- f ) dup h# 2000 u< swap h# 8000 - h# 8000 u< or ;

: h4 ( n -- ) dup d# 12 rshift hex1 dup d# 8 rshift hex1 dup d# 4 rshift hex1 hex1 ;

: sraw ( w -- ) sb-begin [char] [ emit h4 [char] ] emit sb-end  sb sbn @ d# -1 tok ;
: sname ( hdr -- ) \ a word, and a new line after one that stores
    dup CELL2 + >c count rot hxt
    >r 2dup + 1- c@ r> swap >r
    tok
    r> [char] ! = if snl then
;

\ A number in data space that some word does nothing but push is that
\ word: a variable, or anything else made with create.  The word being
\ shown is left as its number, since that is its whole definition.
create hp 0 ,
create hv 0 ,

: lv ( w -- n ) dup h# fff and swap d# 12 rshift d# 3 and d# 12 * lshift ;

: hval ( xt -- n true | false ) \ the number, if the code there only pushes one
    hp !
    hp @ ins@ dup h# c000 and h# 8000 <>  over h# 3000 and h# 3000 = or
    if drop false exit then
    lv hv !
    begin
        d# 1 hp +!
        hp @ ins@ dup h# c000 and h# c000 =  over h# 3000 and h# 3000 <> and
    while
        lv hv +!
    repeat
    h# 7403 = if hv @ true else false then
;

: lit-walk ( stop -- hdr|0 ) \ the newest word, back to stop, that only pushes the number in stmp
    >r forth @
    begin
        dup r@ <>
    while
        dup hxt dup sxt @ <> if
            hval if stmp @ = if rdrop exit then then
        else
            drop
        then
        @
    repeat
    rdrop drop d# 0
;

\ The word being shown is not one of them: that is its whole definition.
\ If the table has it for the number, an older word may push it too.
: lit>hdr ( n -- hdr|0 )
    stmp !
    tstop lit-walk
    dtab @ 0= if exit then
    ?dup if exit then
    stmp @ dtab @ tfind dup 0= if exit then
    dup label? if drop d# 0 exit then
    >hdr dup hxt sxt @ = if drop d# 0 lit-walk then
;

\ A name found by xt>hdr, leaning if it is a label.
: sxname ( hdr -- ) slab @ sital !  sname  d# 0 sital ! ;

\ An execution token as a number is most often one given to catch or
\ stored in a vector.  Code is in page 1 and from page 8 up.
: xt? ( n -- f ) dup h# 8000 u< 0= swap h# 1001 - h# fff u< or ;

: snum ( n -- )
    dup d# 2048 cells - d# 6144 cells u< if     \ in data space
        dup lit>hdr ?dup if nip sname exit then
        dup data>lbl ?dup if
            nip true sital !  CELL2 + >c count d# -1 tok  d# 0 sital !  exit
        then
    then
    dup xt? if
        dup xt>hdr ?dup if nip k-tick wt sxname exit then
    then
    sb-begin . sb-end  sb sbn @ 1- d# -1 tok
;
: slink ( xt -- ) \ a word by name, or by address if it has no name
    dup xt>hdr ?dup if nip sxname exit then
    sb-begin [char] $ emit dup h4 sb-end
    >r sb sbn @ r> tok
;

: fused? ( w -- a|0 ) \ the words of a fused instruction: how many, then the header of each
    fused
    begin
        dup @
    while
        2dup @ = if nip cell+ exit then
        cell+ dup @ + 1+                            \ past the count and the headers
    repeat
    2drop d# 0
;

: sfused ( w -- f ) \ show an instruction as its word or words, if it is in the table
    fused? ?dup 0= if false exit then
    dup @ d# 1 > sbold !                            \ more than one: the peephole made it
    dup @ swap 1+ swap bounds                       ( end a )
    begin
        2dupxor
    while
        dup @ sname
        1+
    repeat
    2drop
    d# 0 sbold !
    true
;

: sflush ( -- ) slit @ if d# 0 slit !  sacc @ snum then ;
: stail  ( -- ) spc @ send @ < if k-exit wt then ;
: reach  ( a -- ) smax @ max smax ! ;

: btarget ( w pc -- a ) \ where a branch at pc goes
    over h# 1000 and if
        h# fff invert and swap h# fff and or
    else
        drop h# fff and
    then
;

: starget ( w -- a ) spc @ btarget ;

: fw-add ( a -- )
    nfw @ d# 16 < if nfw @ cells sfw + !  d# 1 nfw +! else drop then
;

: fw-else ( a -- f ) \ if an if ends just after spc, make it end at a instead
    d# 0
    begin
        dup nfw @ <
    while
        dup cells sfw + @ spc @ 1+ = if cells sfw + ! true exit then
        1+
    repeat
    2drop false
;

: bg-add ( a -- ) \ on the first reading only
    squiet @ nbg @ d# 16 < and if nbg @ cells sbg + !  d# 1 nbg +! else drop then
;

: sthens ( -- )
    d# 0
    begin
        dup nfw @ <
    while
        dup cells sfw + @ spc @ = if
            sflush snl s- k-then wt snl     \ a number before it comes first
            d# -1 over cells sfw + !
        then
        1+
    repeat
    drop
;

: sbegins ( -- )
    d# 0
    begin
        dup nbg @ <
    while
        dup cells sbg + @ spc @ = if sflush snl k-begin wt snl s+ then
        1+
    repeat
    drop
;

: s-lit ( w -- )
    dup h# 3000 and h# 3000 = if                    \ a variable, fetched or stored
        sflush
        dup h# fff and VARS + sacc !  true slit !  sflush
        h# 4000 and if k-store wt snl else k-fetch wt then
        exit
    then
    dup h# fff and over d# 12 rshift d# 3 and d# 12 * lshift
    swap h# 4000 and if
        slit @ if sacc +! else snum k-plus wt then
    else
        sflush sacc !  true slit !
    then
;

: s-call ( w -- )
    starget
    slit @ if
        dup ['] execute = sacc @ code? and if
            drop d# 0 slit !  sacc @ slink exit
        then
    then
    sflush slink
;

: while? ( a -- f ) \ is the instruction before a a jump back into this word?
    1- dup ins@
    dup h# e000 and 0= over 0<> and 0= if 2drop false exit then
    swap btarget
    dup sxt @ < 0= swap spc @ > 0= and
;

: wh-add ( a -- )
    nwh @ d# 8 < if nwh @ cells swh + !  d# 1 nwh +! else drop then
;

: wh? ( a -- f )
    d# 0
    begin
        dup nwh @ <
    while
        2dup cells swh + @ = if 2drop true exit then
        1+
    repeat
    2drop false
;

: s-jump ( w -- )
    starget
    slit @ if                                       \ a far call that does not come back
        dup ['] execute = sacc @ code? and if
            drop d# 0 slit !  sacc @ slink stail exit
        then
    then
    sflush
    dup spc @ > if
        dup fw-else if reach snl s- k-else wt snl s+ exit then
        slink stail exit
    then
    dup sxt @ < if slink stail exit then
    bg-add  snl s-  spc @ 1+ wh? if k-repeat else k-again then wt  snl
;

: s-0br ( w -- )
    sflush starget
    dup spc @ > if
        dup reach
        dup while? if wh-add k-while wt snl exit then
        fw-add k-if wt snl s+ exit
    then
    bg-add  snl s- k-until wt snl
;

\ An instruction is shown as the plain words for it, from the table, which
\ has every instruction the image was built with and every one that has a
\ name: never as some other word of the dictionary that happens to be that
\ one instruction.
: sop ( w -- ) \ an instruction with no return in it
    dup sfused if drop exit then
    sraw
;

: s-alu ( w -- )
    sflush
    dup h# 1000 and if
        dup h# 7403 = if
            drop
        else
            dup d# 3 and d# 3 = if                  \ a return folded in: the instruction
                h# 1003 invert and sop
            else
                sraw
            then
        then
        stail exit
    then
    sop
;

: sterm? ( -- f ) \ does the instruction at spc end the definition?
    spc @ ins@
    dup h# e000 and 0= if
        drop true
    else
        h# f000 and h# 7000 =
    then
    spc @ smax @ < 0= and
;

: swalk ( -- )
    sxt @ spc !  d# 0 nfw !  d# 0 nwh !  d# 0 slit !  d# 1 sind !
    begin
        spc @ send @ > 0=
    while
        sthens sbegins
        spc @ ins@
        dup 0= if
            drop sflush k-halt wt
        else
            dup h# 8000 and if
                s-lit
            else
                dup h# 6000 and
                dup h# 6000 = if
                    drop s-alu
                else
                    dup h# 4000 = if
                        drop s-call
                    else
                        h# 2000 = if s-0br else s-jump then
                    then
                then
            then
        then
        squiet @ if sterm? if spc @ send ! then then
        d# 1 spc +!
    repeat
    sflush
;

: sdecomp ( xt -- )
    dup sxt !  dup smax !  d# 400 + send !  d# 0 nbg !
    true squiet !  swalk  d# 0 squiet !
    d# 0 sind !  true sfresh !
    sxt @ xt>hdr ?dup if
        k-colon wt  sxname
    else
        k-noname wt
    then
    snl
    swalk
    snl  d# 0 sind !
    k-semi wt
    sxt @ xt>hdr ?dup if cell+ @ 0< if k-imm wt then then
;

: sspaces ( n -- )
    negate
    begin
        dup 0<
    while
        space 1+
    repeat
    drop
;

: ttok ( addr u xt -- ) drop  sfresh @ if sind @ 2* sspaces then  type space ;

\ SEE takes a name from the dictionary or, failing that, a label.  For a
\ label on data it only says where the data is.  It has the names in
\ memory for as long as it runs, if they were not already.
: names-done ( were-in -- ) 0= if names-unload then ;
header see
: see ( "name" -- )
    xtab @ >r  names-load
    parse-name sfind ?dup 0= if
        forth @ >r  labels @ forth !  sfind  r> forth !
        dup 0= if r@ names-done then
        dup 0= d# 13 -throw
        0> if cr .  r> names-done  exit then
    else
        drop
    then
    ['] ttok 'tok !  ['] cr 'nl !  cr
    sdecomp  r> names-done
;

\ ---- the Words window: the dictionary on the left, a definition on the right
\
\ Up and Down move through the dictionary, PgUp and PgDn by a page.  A click
\ on a name shows it.  A click on an underlined word follows it, and
\ Backspace comes back.  Left and Right scroll a long definition by half
\ a window.

$2d constant SCOLS      \ characters across the definition

create winit 0 ,
create wxt 0 ,          \ the definition shown
create wsel 0 ,         \ its place in the dictionary, or -1
create wtop 0 ,         \ the first name listed
create wsrc 0 ,         \ the first line of the definition shown
create wrows 0 ,        \ how many lines it has
create whist #64 allot
create nhist 0 ,
create scol 0 ,
create srow 0 ,
create lk #1536 allot   \ links drawn: place and size, then execution token
create nlk 0 ,
create lkx 0 ,
create lky 0 ,
create lku 0 ,
create crow 0 ,
create ccol 0 ,
create wtmp 0 ,
create wwin 0 ,         \ the window
header t-words create t-words ," Words"

\ The tables of names are in memory while the window is the one in front.
: words-front ( -- ) top wwin @ = if names-load else names-unload then ;

: nwords ( -- n )
    d# 0 forth @
    begin
        dup
    while
        swap 1+ swap @
    repeat
    drop
;

: nth ( i -- hdr|0 )
    negate forth @
    begin
        over 0< over 0<> and
    while
        @ swap 1+ swap
    repeat
    nip
;

: hdr>i ( hdr -- i ) \ -1 if it is not in the dictionary
    wtmp !  d# 0 forth @
    begin
        dup
    while
        dup wtmp @ = if drop exit then
        @ swap 1+ swap
    repeat
    2drop d# -1
;

: wpick ( i -- ) \ show the i'th word, and keep it in the list
    d# 0 max nwords 1- min
    dup nth hxt wxt !  d# 0 wsrc !
    dup wtop @ < if dup wtop ! then
    dup wtop @ d# 15 + > if dup d# 15 - wtop ! then
    wsel !
;

: wshow ( xt -- )
    dup wxt !  d# 0 wsrc !
    xt>hdr ?dup if
        hdr>i dup 0< 0= if
            dup wtop @ - d# 16 u< 0= if dup wtop ! then
        then
        drop
    then
;

: wgo ( xt -- ) \ follow a link, remembering where it was followed from
    nhist @ d# 8 < if wxt @ nhist @ cells whist + !  d# 1 nhist +! then
    wshow
;

: wback ( -- )
    nhist @ if d# -1 nhist +!  nhist @ cells whist + @ wshow then
;

: gtok ( addr u xt -- )
    >r dup lku !
    sfresh @ if sind @ 2* scol ! then
    dup scol @ + SCOLS >  scol @ sind @ 2* >  and if    \ wrap, a little further in
        sind @ 1+ 2* scol !  d# 1 srow +!
    then
    srow @ wsrc @ - dup d# 16 u< if
        d# 4 lshift lky !
        scol @ d# 3 lshift d# 112 + lkx !
        sbold @ bold !  sital @ italic !
        lkx @ lky @ draw-text
        d# 0 bold !  d# 0 italic !
        r@ 0< 0= if
            false lkx @ lky @ d# 15 + lku @ d# 3 lshift hline
            nlk @ d# 96 < if
                scol @  srow @ wsrc @ - d# 8 lshift or  lku @ d# 16 lshift or
                nlk @ 2* cells lk + !
                r@ nlk @ 2* cells lk + cell+ !
                d# 1 nlk +!
            then
        then
    else
        drop 2drop
    then
    r> drop
    lku @ 1+ scol +!
;

: gnl ( -- ) d# 1 srow +! ;

header words-draw : words-draw ( win -- )
    wwin !
    winit @ 0= if
        true winit !  d# 0 wpick
        ['] words-front 'front !  words-front
    then
    wxt @ xt>hdr ?dup if hdr>i else d# -1 then wsel !
    d# 0 wi !
    wtop @ nth
    begin
        dup 0<> wi @ d# 16 < and
    while
        wi @ wtop @ + wsel @ = inverse !
        dup CELL2 + >c count d# 13 min
        d# 2  wi @ d# 4 lshift  draw-text
        d# 0 inverse !
        @
        d# 1 wi +!
    repeat
    drop
    false d# 108 d# 0 d# 1 d# 256 fill-rect
    d# 0 scol !  d# 0 srow !  d# 0 nlk !
    ['] gtok 'tok !  ['] gnl 'nl !
    wxt @ sdecomp
    srow @ 1+ wrows !
;

\ What a key or a click changes is put out of date, and no more: the
\ definition, and the two lines of the list the choice moved between.  If
\ the list itself moved, or the choice was made by following a link, it is
\ everything inside the frame.

create wsel0 0 ,        \ the choice and the top of the list before
create wtop0 0 ,

: wall ( -- )
    d# 0 d# 0  wwin @ CELL2 + @ d# 2 -  wwin @ c# 3 + @ d# 20 -  wwin @ win-inval
;

: wpane ( -- ) \ the definition
    d# 108 d# 0  wwin @ CELL2 + @ d# 110 -  wwin @ c# 3 + @ d# 20 -  wwin @ win-inval
;

: wline ( i -- ) \ a line of the list
    wtop @ - dup d# 16 u< if
        >r d# 0 r> d# 4 lshift d# 108 d# 16 wwin @ win-inval
    else
        drop
    then
;

: wbefore ( win -- ) wwin !  wsel @ wsel0 !  wtop @ wtop0 ! ;

: wpicked ( -- ) \ after wpick
    wtop @ wtop0 @ <> if wall exit then
    wsel0 @ wline  wsel @ wline  wpane
;

header words-key : words-key ( c win -- )
    wbefore
    dup d# 128 = if drop wsel @ 1- wpick wpicked exit then
    dup d# 129 = if drop wsel @ 1+ wpick wpicked exit then
    dup d# 132 = if drop wsel @ d# 16 - wpick wpicked exit then
    dup d# 133 = if drop wsel @ d# 16 + wpick wpicked exit then
    dup d# 130 = if drop wsrc @ d# 8 - d# 0 max wsrc ! wpane exit then
    dup d# 131 = if drop wsrc @ d# 8 + wrows @ 1- min wsrc ! wpane exit then
    d# 8 = if wback then
    wall
;

header words-click : words-click ( x y win -- )
    wbefore d# 4 rshift crow !
    dup d# 108 < if drop crow @ wtop @ + wpick wpicked exit then
    wall
    d# 112 - d# 3 rshift ccol !
    d# 0
    begin
        dup nlk @ <
    while
        dup 2* cells lk + @ wtmp !
        wtmp @ d# 8 rshift h# ff and crow @ =
        ccol @ wtmp @ h# ff and -  wtmp @ d# 16 rshift u<  and if
            2* cells lk + cell+ @ wgo exit
        then
        1+
    repeat
    drop
;
