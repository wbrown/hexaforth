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
\ that half set for a label.  A key that half a cell does not hold is not
\ in a table.  For a word that is the address of its
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

\ An entry of the tables is two halves of a cell, and the cells are the
\ machine's.  A machine with cells of 16 bits is built with none: half of
\ such a cell does not hold an execution token.
W32+? [if]
$100000 constant NAMES-AT   \ where on the disk, in bytes: block 1024

: (names-load) ( -- )
    nm-head >c CELLB 2* NAMES-AT disk-read
    nm-head @ tstamp @ <> if exit then
    himem @ nm-mark !
    nm-head CELL + @ >c halloc                      ( a )
    himem @ nm-end !
    dup >c  nm-head CELL + @ >c  NAMES-AT CELLB 2* +  disk-read
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
[else]
: (names-load) ( -- ) ;
[then]

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
        dup>r HALFBITS rshift
        2dup = if 2drop 2drop r> HALFBITS lshift HALFBITS rshift exit then
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
: label? ( entry -- f ) HALFBITS 1- rshift 0<> ;
: >hdr   ( entry -- hdr ) \ where the name is
    dup HALFBITS 1+ lshift HALFBITS 1+ rshift  swap label? if lbase @ + then
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
\ or page 1, or in the pages after the data and the headers.
: code? ( x -- f ) dup h# 2000 u< swap CODE-BASE - CODE-SIZE u< or ;

: h4 ( n -- ) dup d# 12 rshift hex1 dup d# 8 rshift hex1 dup d# 4 rshift hex1 hex1 ;

\ While the word or words of an instruction are shown, or its number if it
\ has none, smach holds the instruction: the Words window keeps it with
\ where they went, to show the machine's form of it.  sgap is the line the
\ word before was on, of the same instruction, or less than nothing.
create smach 0 ,
create sgap 0 ,

: sraw ( w -- )
    dup smach !  d# -1 sgap !
    sb-begin [char] [ emit h4 [char] ] emit sb-end  sb sbn @ d# -1 tok
    d# 0 smach !
;
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
\ stored in a vector.  Code is in page 1 and in the pages after the data
\ and the headers.
: xt? ( n -- f ) dup CODE-BASE - CODE-SIZE u< swap h# 1001 - h# fff u< or ;

meta
:: DATA-CELLS  code-base 2* $4000 - byte-shift rshift literal ;     \ cells of data and of headers, from VARS
target

: snum ( n -- )
    dup VARS - DATA-CELLS u< if                 \ in data space
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
    dup fused? ?dup 0= if drop false exit then      ( w a )
    swap smach !  d# -1 sgap !
    dup @ d# 1 > sbold !                            \ more than one word: they are one instruction
    dup @ swap 1+ swap bounds                       ( end a )
    begin
        2dupxor
    while
        dup @ sname
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
\ What is drawn of a definition is noted, so that the pointer can be told
\ what it is on: a place is the column, the line (from bit 6) and how many
\ characters (from bit 10), which a cell of any size holds.  A link is its
\ place and the execution token it goes to, and an instruction its place
\ and the instruction: two cells each, 96 links and 192 instructions, in
\ memory that is asked for when the window is first drawn.
create lk 0 ,           \ links drawn
create nlk 0 ,
create lkx 0 ,
create lky 0 ,
create lku 0 ,
create crow 0 ,
create ccol 0 ,
create wtmp 0 ,
create wwin 0 ,         \ the window
create mt 0 ,           \ instructions drawn
create nmt 0 ,
create hov 0 ,          \ the instruction the pointer is over, or nothing,
create hovp 0 ,         \ where its words are, as mt has it,
create hovx 0 ,         \ and the box that says what it is: left, top and width
create hovy 0 ,
create hovw 0 ,
header t-words create t-words ," Words"

\ The tables of names are in memory while the window is the one in front,
\ and whenever it is drawn.  It takes them in when it comes to the front,
\ and to be drawn if they are not in (words-draw); it gives them back when
\ it has been drawn and is not the one in front.  So it does not give them
\ back the moment another window comes to the front: it is painted once
\ more then, behind the other, and with the tables gone every name in it
\ was found by going through the dictionary, which made leaving this
\ window four times the work of leaving any other.  And what it shows is
\ the same whenever it is drawn: a word with no header by its name, where
\ behind another window it was a number.
: words-front ( -- ) top wwin @ = if names-load then ;

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
                scol @  srow @ wsrc @ - d# 6 lshift or  lku @ d# 10 lshift or
                nlk @ 2* lk @ + !
                r@ nlk @ 2* lk @ + 1+ !
                d# 1 nlk +!
            then
        then
        smach @ if                                  \ an instruction: the pointer on it says which
            \ Words that are one instruction between them are underlined as
            \ one: the line runs on under the space before each but the first.
            sbold @  sgap @ srow @ =  and
            dup if false lkx @ d# 8 - lky @ d# 15 + d# 8 hline then
            negate >r                               ( R: xt, and 1 if the space is its too )
            nmt @ d# 192 < if
                scol @ r@ -  srow @ wsrc @ - d# 6 lshift or  lku @ r@ + d# 10 lshift or
                nmt @ 2* mt @ + !
                smach @ nmt @ 2* mt @ + 1+ !
                d# 1 nmt +!
            then
            r> drop
        then
    else
        drop 2drop
    then
    r> drop
    smach @ if srow @ sgap ! then
    lku @ 1+ scol +!
;

: gnl ( -- ) d# 1 srow +! ;

\ ---- what an instruction is to the machine
\
\ Words in bold are one instruction between them, and are underlined as
\ one, the space between them too; a number in brackets is an instruction
\ that has no words; and many a plain word is an instruction by itself.
\ With the pointer on any of them, a small
\ box under it says what the instruction is: its number, and its fields as
\ vm_opcodes.h names them, which is how it can be written in source.  The
\ names are in fused.fs, laid end to end: f-in, f-op and f-out.

: nth-name ( n table -- addr u ) \ the nth of some counted strings laid end to end
    >c swap negate
    begin
        dup
    while
        swap count + swap 1+
    repeat
    drop count
;

: .delta ( n c -- ) \ how a stack moves, after the stack's letter; nothing if it does not
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

: hov-box ( -- ) \ where the box goes: under the words, or over them on the last lines
    hov @ sb-begin .mach sb-end
    sbn @ d# 3 lshift d# 6 + hovw !
    hovp @ h# 3f and d# 3 lshift d# 110 +
    wwin @ CELL2 + @ d# 2 - hovw @ - min  d# 108 max  hovx !
    hovp @ d# 6 rshift h# f and
    dup d# 14 < if 1+ d# 4 lshift else d# 4 lshift d# 18 - then  hovy !
;

: hov-inval ( -- ) hov @ if hovx @ hovy @ hovw @ d# 18 wwin @ win-inval then ;

: hov-draw ( -- )
    hov @ 0= if exit then
    hov @ sb-begin .mach sb-end
    true  hovx @ hovy @ hovw @ d# 18 fill-rect
    false hovx @ hovy @ hovw @ d# 18 frame-rect
    hovx @ d# 3 + hovy @ 1+ sb-draw
;

: hover ( -- place ins ) \ what the pointer is on, of the instructions drawn; or nothing twice
    wwin @ dup 0= if d# 0 exit then
    dup top <> if drop d# 0 d# 0 exit then
    my @ over CELL + @ - d# 19 -  d# 4 rshift crow !
    mx @ swap @ - 1- d# 112 -  d# 3 rshift ccol !
    d# 0
    begin
        dup nmt @ <
    while
        dup 2* mt @ + @ wtmp !
        wtmp @ d# 6 rshift h# f and crow @ =
        ccol @ wtmp @ h# 3f and -  wtmp @ d# 10 rshift u<  and if
            2* mt @ +  dup @ swap 1+ @  exit
        then
        1+
    repeat
    drop d# 0 d# 0
;

\ The window manager runs this when the pointer has moved.  The box is
\ part of what the window draws, so it is put out of date where it was and
\ where it is to be, and painted with the rest.
: words-moved ( -- )
    hover
    over hovp @ =  over hov @ =  and if 2drop exit then
    hov-inval
    hov !  hovp !
    hov @ if hov-box hov-inval then
;

header words-draw : words-draw ( win -- )
    wwin !
    winit @ 0= if
        true winit !  d# 0 wpick
        d# 192 >c halloc lk !  d# 384 >c halloc mt !
        ['] words-front 'front !  words-front
        ['] words-moved 'moved !
    then
    names-load                                      \ in for as long as it is drawn, if not in already
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
    d# 0 scol !  d# 0 srow !  d# 0 nlk !  d# 0 nmt !
    ['] gtok 'tok !  ['] gnl 'nl !
    wxt @ sdecomp
    srow @ 1+ wrows !
    hov-draw
    top wwin @ <> if names-unload then              \ drawn behind another: the tables can go
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

: wbefore ( win -- ) \ before a key or a click, which takes the box away
    wwin !  wsel @ wsel0 !  wtop @ wtop0 !
    hov-inval  d# 0 hov !  d# 0 hovp !
;

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
        dup 2* lk @ + @ wtmp !
        wtmp @ d# 6 rshift h# f and crow @ =
        ccol @ wtmp @ h# 3f and -  wtmp @ d# 10 rshift u<  and if
            2* lk @ + 1+ @ wgo exit
        then
        1+
    repeat
    drop
;
