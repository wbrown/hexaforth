\ see.fs - a decompiler, and a window that browses the
\ dictionary with it

\ A definition is read twice: for where it ends and its loops
\ begin, and then for its tokens.

create wi 0 ,           \ a loop counter

create 'tok 0 ,        \ ( addr u xt|-1 -- )  what a token
                       \ goes to
create 'nl 0 ,         \ and a new line
create sind 0 ,        \ how far lines are indented
create sfresh 0 ,      \ true while the line has no token
create sbold 0 ,       \ true for the words of a
                       \ fused instruction
create sital 0 ,       \ true for a name that is a label
create slabel 0 ,      \ true if the last xt>hdr found a label
create squiet 0 ,      \ true on the first reading
create sxt 0 ,         \ the definition
create spc 0 ,         \ the instruction being read
create send 0 ,        \ its last instruction
create smax 0 ,        \ the furthest a forward branch
                       \ has reached
create sacc 0 ,        \ a number being put together

create slit 0 ,        \ true if there is one
create sfw #128 allot  \ where forward branches land
create nfw 0 ,
create sbg #128 allot  \ where backward branches land
create nbg 0 ,
create swh #64 allot   \ where a while leaves its loop
create nwh 0 ,
create stmp 0 ,        \ the xt or number being looked up

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
: snl ( -- )
    squiet @ sfresh @ or if exit then 'nl @ execute
    true sfresh ! ;
: s+  ( -- ) d# 1 sind +! ;
: s-  ( -- ) sind @ 1- d# 0 max sind ! ;

: ins@ ( a -- w ) i@ ;
: hxt  ( hdr -- xt ) cell+ @ d# 1 lshift d# 1 rshift ;

\ ---- the names on the disk, from NAMES-BLK (cross.fs,
\ name-tables), a block at a time
create nm-buf 0 ,   \ the buffer, or 0 while it is not taken
create nm-blk 0 ,   \ the block of the file it holds
create nm-ix 0 ,    \ the first block's counts and keys, kept
create nm-mark 0 ,  \ where memory was before the buffer was
create nm-end 0 ,   \ taken, and after

: nm-block ( blk -- a ) \ a block of the file, in the buffer
    dup nm-blk @ = if drop nm-buf @ exit then
    d# -1 nm-blk !
    dup NAMES-BLK +  nm-buf @ swap blk-read
    nm-blk !  nm-buf @
;

header names? : names? ( -- f ) \ are they in?
    nm-buf @ 0<> ;

: names-gone ( -- ) nm-mark @ himem !  d# 0 nm-buf ! ;

: (names-load) ( -- ) \ if the file is this image's
    flush
    BLKCELLS >c halloc nm-buf !  d# -1 nm-blk !
    d# 0 nm-block
    dup @ tstamp @ <> if drop names-gone exit then
    1+  dup @ over 1+ @ + CELL2 +       ( from cells )
    dup >c halloc  dup nm-ix !  swap cells-move
    himem @ nm-end !
;

header names-load
: names-load ( -- )
    names?  'blk-read @ 0=  or if exit then
    himem @ nm-mark !
    ['] (names-load) catch if names-gone then
;

header names-unload
: names-unload ( -- )
    names? 0= if exit then
    himem @ nm-end @ <> if exit then  \ memory taken since
    names-gone
;

\ a table is the blocks from b0 up to b1: by xt, and by number
: xrun ( -- b1 b0 ) nm-ix @ @ 1+  d# 1 ;
: nrun ( -- b1 b0 ) nm-ix @ dup @ 1+  swap 1+ @ over +  swap ;
: bkey ( b -- key ) nm-ix @ + 1+ @ ;  \ the first key of block b

: nm-blk-of ( key b1 b0 -- b|0 ) \ the block a key is in
    dup >r  rot >r                      ( b1 b ) ( R: b0 key )
    begin
        2dup xor if dup bkey r@ u> 0= else false then
    while
        1+
    repeat
    nip 1-  rdrop  dup r> u< if drop d# 0 then
;

: rec-next ( rec -- rec' ) \ past a record
    dup @ 0< 0= if CELL2 + exit then    \ a word's
    CELL2 +  begin dup >r 1+ r> @  0< until
;

: rec-find ( key a -- rec|0 ) \ in a block
    dup @ negate >r 1+                  ( key rec ) ( R: -n )
    begin
        r@
    while
        2dup cell+ @ = if nip rdrop exit then
        rec-next  r> 1+ >r
    repeat
    2drop rdrop d# 0
;

: nm-find ( key b1 b0 -- rec|0 ) \ in a table
    rot dup >r -rot nm-blk-of  r> swap
    ?dup if nm-block rec-find exit then
    drop d# 0
;

\ the names of the words a boot compiled (files.fs, own-kept)
: own-block ( b -- a ) own-kept @ + NAMES-BLK - nm-block ;

: own-has? ( xt b -- f ) \ may block b have the word at xt?
    2* own-ix +  2dup @ < if 2drop false exit then
    1+ @ > 0= ;

\ the record of the word at xt: not one whose key is a number
: own-in ( xt a -- rec|0 ) \ of the records of a block
    dup @ negate >r 1+                      ( xt rec ) ( R: -k )
    begin r@ while
        2dup cell+ @ =  over @ d# 2 and 0=  and if
            nip rdrop exit
        then
        rec-next  r> 1+ >r
    repeat
    2drop rdrop d# 0
;

: own>rec ( xt -- rec|0 )
    names? 0= if drop d# 0 exit then
    >r  own-kept 1+ @ negate                ( -n ) ( R: xt )
    begin dup while
        r@ over own-kept 1+ @ + own-has? if
            r@ over own-kept 1+ @ + own-block own-in
            ?dup if nip rdrop exit then
        then
        1+
    repeat
    rdrop
;

: tstop ( -- hdr|0 ) \ how far back the dictionary is walked
    names? if forth0 @ else d# 0 then
;

: xt>file ( xt -- hdr|0 ) \ a word's header, or a label's record
    names? 0= if drop d# 0 exit then
    xrun nm-find dup if
        dup @ dup 0< dup slabel !  if drop else nip then
    then
;

\ the word at xt: a header, a label, or one the boot kept
: xt>hdr ( xt -- hdr|0 )
    stmp !  d# 0 slabel !
    tstop >r forth @
    begin
        dup r@ <>
    while
        dup hxt stmp @ = if rdrop exit then
        @
    repeat
    rdrop drop  stmp @ xt>file  ?dup if exit then
    stmp @ own>rec  dup 0<> slabel !
;

: data>lbl ( a -- lbl|0 ) \ the label for the data at a
    names? 0= if drop d# 0 exit then
    nrun nm-find dup if dup @ 0< 0= if drop d# 0 then then
;

: code? ( x -- f ) \ is it where code is?
    dup h# 2000 u< swap CODE-BASE - CODE-SIZE u< or ;

: h4 ( n -- )
    dup d# 12 rshift hex1  dup d# 8 rshift hex1
    dup d# 4 rshift hex1  hex1 ;

create smach 0 ,  \ the instruction whose words are being shown
create sgap 0 ,   \ the line the word before of it was on, or -1

: sraw ( w -- )
    dup smach !  d# -1 sgap !
    sb-begin [char] [ emit h4 [char] ] emit sb-end
    sb sbn @ d# -1 tok
    d# 0 smach !
;
: hname ( hdr -- addr u ) CELL2 + name> ;

: sname ( hdr -- ) \ and a new line after one that stores
    dup hname rot hxt
    >r 2dup + 1- c@ r> swap >r
    tok
    r> [char] ! = if snl then
;

\ the record of a word the boot compiled that only pushed n
: data-in ( n a -- rec|0 ) \ of the records of a block
    dup @ negate >r 1+                      ( n rec ) ( R: -k )
    begin r@ while
        2dup cell+ @ =  over @ d# 2 and 0<>  and if
            nip rdrop exit
        then
        rec-next  r> 1+ >r
    repeat
    2drop rdrop d# 0
;

: own-data ( n -- rec|0 )
    names? 0= if drop d# 0 exit then
    own-kept 1+ @ negate                    ( n -b )
    begin dup while
        2dup own-kept 1+ @ + own-block data-in
        ?dup if nip nip exit then
        1+
    repeat
    nip
;

\ the newest word, back to stop, that only pushes stmp's number
: lit-walk ( stop -- hdr|0 )
    >r forth @
    begin
        dup r@ <>
    while
        dup hxt dup sxt @ <> if
            pushes if stmp @ = if rdrop exit then then
        else
            drop
        then
        @
    repeat
    rdrop drop d# 0
;

\ a word other than the one shown that only pushes n
: lit>hdr ( n -- hdr|0 )
    stmp !
    tstop lit-walk
    names? 0= if exit then
    ?dup if exit then
    stmp @ nrun nm-find dup 0= if exit then
    @ dup 0< if drop d# 0 exit then     \ a label is not a word
    dup hxt sxt @ = if drop d# 0 lit-walk then
;

: sxname ( hdr -- ) slabel @ sital !  sname  d# 0 sital ! ;

: xt? ( n -- f ) \ may the number be an execution token?
    dup CODE-BASE - CODE-SIZE u< swap h# 1001 - h# fff u< or ;

meta
:: DATA-CELLS \ cells of data and of headers, from VARS
    code-base 2* data-base - byte-shift rshift literal ;
target

: snum ( n -- )
    dup VARS - DATA-CELLS u< if                 \ in data space
        dup lit>hdr ?dup if nip sname exit then
        dup data>lbl ?dup 0= if dup own-data then
        ?dup if
            nip true sital !  hname d# -1 tok  d# 0 sital !
            exit
        then
    then
    dup xt? if
        dup xt>hdr ?dup if nip k-tick wt sxname exit then
    then
    sb-begin . sb-end  sb sbn @ 1- d# -1 tok
;

: slink ( xt -- ) \ a word by name, or by address
    dup xt>hdr ?dup if nip sxname exit then
    sb-begin [char] $ emit dup h4 sb-end
    >r sb sbn @ r> tok
;

\ fused.fs's table is fields of sixteen bits, the first of a
\ cell its lowest: an instruction, how many words it is, and
\ each word's header.  A field that is zero ends it.
: ffield ( n -- u ) \ its nth field
    2* fused-at @ swap ifield ;

\ an instruction's words: the field that has how many, which
\ the header of each comes after
: fused? ( w -- n|0 )
    >r d# 0                                 ( n ) ( R: w )
    begin
        dup ffield ?dup
    while
        r@ = if rdrop 1+ exit then
        1+ dup ffield + 1+
    repeat
    rdrop drop d# 0
;

: sfused ( w -- f ) \ as its words, if it is in the table
    dup fused? ?dup 0= if drop false exit then      ( w n )
    swap smach !  d# -1 sgap !
    dup ffield d# 1 > sbold !
    dup ffield swap 1+ swap bounds                  ( end n )
    begin
        2dup xor
    while
        dup ffield sname
        1+
    repeat
    2drop
    d# 0 sbold !  d# 0 smach !
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
    nfw @ d# 16 < if
        nfw @ cells sfw + !  d# 1 nfw +!
    else
        drop
    then
;

: fw-else ( a -- f ) \ an if that ends after spc ends at a
    d# 0
    begin
        dup nfw @ <
    while
        dup cells sfw + @ spc @ 1+ = if
            cells sfw + ! true exit
        then
        1+
    repeat
    2drop false
;

: bg-add ( a -- ) \ on the first reading only
    squiet @ nbg @ d# 16 < and if
        nbg @ cells sbg + !  d# 1 nbg +!
    else
        drop
    then
;

: sthens ( -- )
    d# 0
    begin
        dup nfw @ <
    while
        dup cells sfw + @ spc @ = if
            sflush snl s- k-then wt snl
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
        dup cells sbg + @ spc @ = if
            sflush snl k-begin wt snl s+
        then
        1+
    repeat
    drop
;

: s-lit ( w -- )
    dup h# 3000 and h# 3000 = if  \ a variable's
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

: while? ( a -- f ) \ is a jump back into this word before a?
    1- dup ins@
    dup h# e000 and 0= over 0<> and 0= if 2drop false exit then
    swap btarget
    dup sxt @ < 0= swap spc @ > 0= and
;

: wh-add ( a -- )
    nwh @ d# 8 < if
        nwh @ cells swh + !  d# 1 nwh +!
    else
        drop
    then
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
    slit @ if  \ a far call that does not come back
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
    bg-add  snl s-
    spc @ 1+ wh? if k-repeat else k-again then wt  snl
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

: sop ( w -- ) \ an instruction with no return in it
    dup sfused if drop exit then
    sraw
;

: s-ret ( w -- ) \ an instruction with a return in it
    dup h# 7403 = if drop exit then
    dup d# 3 and d# 3 = if h# 1003 invert and sop exit then
    sraw
;

: s-alu ( w -- )
    sflush
    dup h# 1000 and if s-ret stail exit then
    sop
;

: sterm? ( -- f ) \ does the instruction at spc end the word?
    spc @ ins@
    dup h# e000 and 0= if
        drop true
    else
        h# f000 and h# 7000 =
    then
    spc @ smax @ < 0= and
;

: s-ins ( w -- ) \ an instruction, by its kind
    dup 0= if drop sflush k-halt wt exit then
    dup h# 8000 and if s-lit exit then
    dup h# 6000 and
    dup h# 6000 = if drop s-alu exit then
    dup h# 4000 = if drop s-call exit then
    h# 2000 = if s-0br else s-jump then
;

: swalk ( -- )
    sxt @ spc !  d# 0 nfw !  d# 0 nwh !  d# 0 slit !
    d# 1 sind !
    begin
        spc @ send @ > 0=
    while
        sthens sbegins
        spc @ ins@ s-ins
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

: ttok ( addr u xt -- )
    drop  sfresh @ if sind @ INDENT-COLS * sspaces then
    type space
;

: names-done ( were-in -- ) 0= if names-unload then ;

\ the newest label of skey's name: its place and its value
: newer ( p v rec -- p' v' ) \ the record, if it is one of those
    dup @ 0< 0= if drop exit then       \ a word's
    dup samename? 0= if drop exit then
    >r  over r@ @ < if 2drop r@ @ r@ cell+ @ then  rdrop
;

: newest-in ( p v a -- p' v' ) \ of the records of a block
    dup @ negate >r 1+                  ( p v rec ) ( R: -n )
    begin
        r@
    while
        dup >r newer r> rec-next  r> 1+ >r
    repeat
    drop rdrop
;

: newest ( p v b1 b0 -- p' v' ) \ of the blocks of a table
    begin
        2dup xor
    while
        swap >r dup >r nm-block newest-in  r> 1+ r> swap
    repeat
    2drop
;

\ the newest word of the name among those the boot kept: what
\ its record has where the place goes, and the record's first
\ cell
: named-in ( x first a -- x' first' ) \ of a block's records
    dup @ negate >r 1+                      ( x first rec )
    begin r@ while
        dup samename? if nip nip dup cell+ @ over @ rot then
        rec-next  r> 1+ >r
    repeat
    drop rdrop
;

: own-named ( -- x first | 0 0 )
    d# 0 d# 0  own-kept 1+ @ negate         ( x first -b )
    begin dup while
        >r  r@ own-kept 1+ @ + own-block named-in  r> 1+
    repeat
    drop
;

: own-find ( -- x data? true | false ) \ by the name in the key
    own-named ?dup 0= if drop false exit then
    d# 2 and if true true exit then
    dup pushes if nip true true exit then
    false true
;

: lfind ( c-addr u -- x data? true | false )
    names? 0= if 2drop false exit then
    >key drop
    own-find if true exit then
    d# 1 TOPBIT lshift d# 0  xrun newest      ( p1 v1 )
    over d# 0  nrun newest                     ( p1 v1 p2 v2 )
    >r rot over = if
        rdrop  d# 1 TOPBIT lshift = if drop false exit then
        false true exit
    then
    2drop r> true true
;

header see
: see ( "name" -- ) \ a word, a label, or a label's data
    names? >r  names-load
    parse-name sfind 0= if
        lfind dup 0= if r@ names-done then
        dup 0= d# 13 -throw  drop
        if cr .  r> names-done  exit then
    then
    ['] ttok 'tok !  ['] cr 'nl !  cr
    sdecomp  r> names-done
;

\ ---- the Words window: the dictionary on the left, a
\ definition on the right

$2d constant SCOLS      \ characters across the definition

create winit 0 ,        \ true once the window has shown a word
create wxt 0 ,          \ the definition shown
create wsel 0 ,         \ its place in the dictionary, or -1
create wtop 0 ,         \ the first name listed
create wsrc 0 ,         \ the first line of the definition shown
create whist #64 allot  \ the definitions shown before: up to 8,
create nhist 0 ,        \ and how many
create scol 0 ,         \ the column the next token goes at,
create srow 0 ,         \ and the line

meta
:: WCELLS #2048 cell-bytes / literal ;  \ 2,048 characters
target
create wtext 0 , 0 , 0 , 0 , 0 , 0 ,    \ the text (text.fs),
create wdone 0 ,                         \ the definition in it,
create sface 0 ,                         \ and the face it is in

\ A note is three cells: a line, a place in it (the column, and
\ how many characters from bit 6), and an xt or an instruction.
#256 constant WLINKS    \ the most links noted,
#256 constant WINSTRS   \ and instructions
create lk 0 ,    \ links noted
create nlk 0 ,   \ and how many
create lku 0 ,   \ the characters of the token being placed
create crow 0 ,  \ the line the pointer is on,
create ccol 0 ,  \ and the column
create wtmp 0 ,  \ the xt sought in the dictionary

create wwin 0 ,  \ the window

create mt 0 ,    \ instructions noted
create nmt 0 ,
create hov 0 ,   \ the instruction the pointer is over, or 0,
create hovp 0 ,  \ where it is noted in mt,
create hovx 0 ,  \ and its box: left, top and width
create hovy 0 ,
create hovw 0 ,
header t-words create t-words ," Words"

create wfrom 0 ,  \ where memory was before the text's,
create wto 0 ,    \ and after

: (words-in) ( -- )
    WCELLS dup >c halloc swap false wtext text-new
    WLINKS d# 3 * >c halloc lk !  WINSTRS d# 3 * >c halloc mt !
;

: words-in ( -- )
    names-load  wtext @ if exit then
    himem @ wfrom !  d# 0 wdone !
    ['] (words-in) catch if wfrom @ himem !  d# 0 wtext ! then
    himem @ wto !
;

: words-out ( -- )
    wtext @ if
        himem @ wto @ = if wfrom @ himem !  d# 0 wtext ! then
    then
    names-unload
;

: words-front ( win -- ) drop words-in ;

: nwords ( -- n )
    d# 0 forth @
    dup if begin >r 1+ r> @  dup 0= until then
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

: wlines ( -- n ) \ how many lines the window shows
    wwin @ c# 3 + @ d# 20 - d# 4 rshift ;

: wpick ( i -- ) \ show the i'th word, and keep it in the list
    d# 0 max nwords 1- min
    dup nth hxt wxt !  d# 0 wsrc !
    dup wtop @ < if dup wtop ! then
    dup wtop @ wlines + < 0= if dup wlines - 1+ wtop ! then
    wsel !
;

: wshow ( xt -- )
    dup wxt !  d# 0 wsrc !
    xt>hdr ?dup if
        hdr>i dup 0< 0= if
            dup wtop @ - wlines u< 0= if dup wtop ! then
        then
        drop
    then
;

: wgo ( xt -- ) \ follow a link, noting where from
    nhist @ d# 8 < if
        wxt @ nhist @ cells whist + !  d# 1 nhist +!
    then
    wshow
;

: wback ( -- )
    nhist @ if
        d# -1 nhist +!  nhist @ cells whist + @ wshow
    then
;

: gface ( f -- ) \ what comes after in face f
    dup sface @ = if drop exit then
    dup sface !  FACE + wtext text-code
;

: gnl ( -- ) wtext text-nl  d# 1 srow +!  d# 0 sface ! ;

: gindent ( n -- ) \ the line begun n indents in
    dup INDENT-COLS * scol !
    negate begin dup while INDENT wtext text-code 1+ repeat
    drop
;

: gwrap ( u -- u ) \ a token too long for its line goes on a new
    sfresh @ if sind @ gindent then
    dup scol @ + SCOLS >  scol @ sind @ INDENT-COLS * >  and if
        gnl  sind @ 1+ gindent  true sfresh !
    then
;

: gsame? ( -- f ) \ is the token more of the instruction before?
    sbold @  sgap @ srow @ =  and ;
: gsep ( -- ) \ the space before a token, in the face if so
    gsame? 0= if d# 0 gface then  bl wtext text-code ;

\ where the token is, from n characters before it
: gplace ( n -- place )
    >r  scol @ r@ -  lku @ r> + d# 6 lshift or ;

: rec ( i a -- a' ) \ the ith note of those at a
    swap dup 2* + + ;
: gnote ( x place a -- ) \ the note at a: the line, the place, x
    srow @ over !  1+ tuck !  1+ ! ;

: glink ( xt -- ) \ a link to xt, noted
    dup 0< nlk @ WLINKS < 0= or if drop exit then
    d# 0 gplace  nlk @ lk @ rec  gnote  d# 1 nlk +!
;

: ginstr ( -- ) \ an instruction, noted
    nmt @ WINSTRS < 0= if exit then
    smach @  gsame? negate gplace  nmt @ mt @ rec  gnote
    d# 1 nmt +!
;

: gtok ( addr u xt -- )
    >r  dup lku !  gwrap  sfresh @ 0= if gsep then
    r@ 0< 0= F-UNDER and  sbold @ F-BOLD and or
    sital @ F-LEAN and or  gface
    swap >chars rot wtext text-add
    r> glink  smach @ if ginstr  srow @ sgap ! then
    lku @ 1+ scol +!
;

\ ---- the box that says an instruction's number and fields

: nth-name ( n table -- addr u ) \ of counted strings end to end
    >c swap negate
    begin
        dup
    while
        swap count + swap 1+
    repeat
    drop count
;

: .delta ( n c -- ) \ how a stack moves, after its letter
    over 0= if 2drop exit then
    space emit
    dup d# 1 = if drop [char] + emit [char] 1 emit exit then
    [char] - emit
    d# 3 = if [char] 1 else [char] 2 then emit
;

: .mach ( w -- )
    dup h4 space
    dup d# 10 rshift d# 3 and f-in nth-name type space
    dup d# 4 rshift d# 15 and f-op nth-name type space
    dup d# 8 rshift d# 3 and f-out nth-name type
    dup d# 2 rshift d# 3 and [char] d .delta
    d# 3 and [char] r .delta
;

: hov-box ( -- ) \ under the words, or over the last lines
    hov @ sb-begin .mach sb-end
    sbn @ d# 3 lshift d# 6 + hovw !
    hovp @ 1+ @ h# 3f and d# 3 lshift d# 110 +
    wwin @ CELL2 + @ d# 2 - hovw @ - min  d# 108 max  hovx !
    hovp @ @ wsrc @ -
    dup wlines d# 2 - < if
        1+ d# 4 lshift
    else
        d# 4 lshift d# 18 -
    then
    hovy !
;

: hov-draw ( -- )
    hov @ 0= if exit then
    hov @ sb-begin .mach sb-end
    true  hovx @ hovy @ hovw @ d# 18 fill-rect
    false hovx @ hovy @ hovw @ d# 18 frame-rect
    hovx @ d# 3 + hovy @ 1+ sb-draw
;

\ the box is the window's overlay (wm.fs, 'overlay)
create hovk 0 , 0 ,  \ what is under it: a bitmap of its cells
create hovon 0 ,     \ true while it is on the screen,
create hovox 0 ,     \ and where the window's content was then
create hovoy 0 ,

: hrect ( -- x y w h ) \ as much of the box as is in the window
    hovx @ hovy @
    wwin @ CELL2 + @ d# 2 - hovx @ -  hovw @ min  d# 18 ;

: hov-on ( -- ) \ what is under it kept, and it drawn
    hovon @  hov @ 0=  or  top wwin @ <>  or if exit then
    wwin @ content hovoy ! hovox !
    wwin @ win-port  hovk hrect keep-bits  hov-draw
    screen-port @ port!  true hovon !
;

: hmoved? ( -- f ) \ has the window moved since it was put on?
    wwin @ content  hovoy @ <>  swap hovox @ <>  or ;

: hov-off ( -- ) \ what was under it put back
    hovon @ 0= if exit then
    d# 0 hovon !
    hmoved? if exit then
    wwin @ win-port  copy-mode  hovk hrect put-bits
    screen-port @ port!
;

: hov-overlay ( on? -- ) if hov-on else hov-off then ;

: at? ( a -- f ) \ is the pointer on what is noted at a?
    dup @ crow @ =  swap 1+ @
    ccol @ over h# 3f and -  swap d# 6 rshift u<  and
;

\ the instruction the pointer is on, and its note; or 0 0
: hover ( x y win -- a ins )
    wwin @ <> if 2drop d# 0 d# 0 exit then
    d# 4 rshift
    dup wlines u< 0= if 2drop d# 0 d# 0 exit then
    wsrc @ + crow !
    d# 112 - d# 3 rshift ccol !
    d# 0
    begin dup nmt @ < while
        dup mt @ rec  dup at? if nip dup CELL2 + @ exit then
        drop 1+
    repeat
    drop d# 0 d# 0
;

: words-moved ( x y win -- ) \ what 'moved runs
    hover
    over hovp @ =  over hov @ =  and if 2drop exit then
    overlays-off  hov !  hovp !  hov @ if hov-box then
    overlays-on
;

: wfirst ( -- ) \ the first time it is drawn
    true winit !  d# 0 wpick
    wwin @ CELL2 + @ PXSHIFT rshift d# 2 +  dup hovk 1+ !
    d# 18 * >c halloc hovk !
    ['] words-front wwin @ win-front !
    ['] words-moved 'moved !  ['] hov-overlay 'overlay !
;

\ wsel is kept while it is a header of the word shown, and found
\ from the word when it is not
: wchosen ( -- )
    wsel @ dup 0< if drop else
        nth ?dup if hxt wxt @ = if exit then then
    then
    wxt @ xt>hdr ?dup if hdr>i else d# -1 then wsel !
;

: wlist ( -- ) \ the words, the one chosen white on black
    wchosen
    d# 0 wi !
    wtop @ nth
    begin
        dup 0<> wi @ wlines < and
    while
        wi @ wtop @ + wsel @ = inverse !
        dup hname d# 13 min
        d# 2  wi @ d# 4 lshift  draw-text
        d# 0 inverse !
        @
        d# 1 wi +!
    repeat
    drop
;

: wrows ( -- n ) \ the lines of the definition
    wtext tb.lines @ ;

: wdecomp ( -- ) \ the definition shown, into the text
    wxt @ wdone !  wtext text-clear
    d# 0 scol !  d# 0 srow !  d# 0 nlk !  d# 0 nmt !
    d# 0 sface !
    ['] gtok 'tok !  ['] gnl 'nl !
    wxt @ sdecomp
;

: wmarks ( -- ) \ where more is above and below
    SCOLS d# 3 lshift d# 112 +
    wsrc @ if dup d# 0 mark-up then
    wsrc @ wlines + wrows < if
        dup wlines 1- d# 4 lshift mark-down
    then
    drop
;

: wdef ( -- ) \ the definition, on the right
    false d# 108 d# 0 d# 1 wlines d# 4 lshift fill-rect
    wtext @ 0= if exit then
    wxt @ wdone @ <> if wdecomp then
    wsrc @ wtext text-line  wrows wsrc @ - wlines min
    d# 112 d# 0 draw-lines  wmarks
;

header words-draw : words-draw ( win -- )
    wwin !  winit @ 0= if wfirst then
    words-in  wlist wdef  top wwin @ <> if words-out then
;

create wsel0 0 ,  \ the choice and the top of the list before
create wtop0 0 ,  \ a key or a click

: wall ( -- )
    d# 0 d# 0  wwin @ CELL2 + @ d# 2 -  wwin @ c# 3 + @ d# 20 -
    wwin @ win-inval
;

: wpane ( -- ) \ the definition
    d# 108 d# 0  wwin @ CELL2 + @ d# 110 -
    wwin @ c# 3 + @ d# 20 -  wwin @ win-inval
;

: wline ( i -- ) \ a line of the list
    wtop @ - dup wlines u< if
        >r d# 0 r> d# 4 lshift d# 108 d# 16 wwin @ win-inval
    else
        drop
    then
;

: wbefore ( win -- ) \ a key or a click takes the box away
    wwin !  wsel @ wsel0 !  wtop @ wtop0 !
    overlays-off  d# 0 hov !  d# 0 hovp !  overlays-on
;

: wpicked ( -- ) \ what wpick put out of date
    wtop @ wtop0 @ <> if wall exit then
    wsel0 @ wline  wsel @ wline  wpane
;

: wpage ( -- n ) wlines 1- ;

: wscroll ( n -- ) \ the definition from its nth line
    wrows wlines - min  d# 0 max  wsrc !  wpane ;

header words-key : words-key ( c win -- )
    wbefore
    dup d# 128 = if drop wsel @ 1- wpick wpicked exit then
    dup d# 129 = if drop wsel @ 1+ wpick wpicked exit then
    dup d# 130 = if
        drop wsel @ wlines - wpick wpicked exit
    then
    dup d# 131 = if
        drop wsel @ wlines + wpick wpicked exit
    then
    dup d# 132 = if drop wsrc @ wpage - wscroll exit then
    dup d# 133 = if drop wsrc @ wpage + wscroll exit then
    d# 8 = if wback then
    wall
;

header words-click : words-click ( x y win -- )
    wbefore d# 4 rshift crow !
    dup d# 108 < if
        drop crow @ wtop @ + wpick wpicked exit
    then
    wall
    d# 112 - d# 3 rshift ccol !  wsrc @ crow +!
    d# 0
    begin dup nlk @ < while
        dup lk @ rec  dup at? if nip CELL2 + @ wgo exit then
        drop 1+
    repeat
    drop
;
