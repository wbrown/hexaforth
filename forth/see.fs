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
\ An ALU instruction is named by finding a word that is that instruction
\ and a return.  One that the peephole made by fusing two or three words is
\ shown as those words, from the table in fused.fs, with sbold set so that
\ they can be told from words that were compiled one by one.  One that is
\ in neither place is shown as its number in brackets.
\
\ The tokens are laid out as source is written.  The name has the first
\ line and the semicolon the last.  What is inside an if or a begin is
\ indented, and an if, an else, a then or a store ends its line.  The
\ routine in 'nl starts a new line; the indent to give the next token is
\ in sind, and sfresh is true while nothing is on the line yet.

create 'tok 0 ,
create 'nl 0 ,
create sind 0 ,
create sfresh 0 ,
create sbold 0 ,        \ true for the words of a fused instruction
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
create fa 0 ,
create fn 0 ,

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

: tok ( addr u xt -- )
    squiet @ if drop 2drop exit then
    'tok @ execute  d# 0 sfresh !
;
: wt  ( cstr -- ) count d# -1 tok ;
: snl ( -- ) squiet @ sfresh @ or if exit then 'nl @ execute  true sfresh ! ;
: s+  ( -- ) d# 1 sind +! ;
: s-  ( -- ) sind @ 1- d# 0 max sind ! ;

: ins@ ( a -- w ) 2* uw@ ;
: hxt  ( hdr -- xt ) cell+ @ d# 1 lshift d# 1 rshift ;

: xt>hdr ( xt -- hdr|0 ) \ the word that starts at xt
    stmp !  forth @
    begin
        dup
    while
        dup hxt stmp @ = if exit then
        @
    repeat
;

: ret>hdr ( w -- hdr|0 ) \ the newest word whose code is the one instruction w
    stmp !  forth @
    begin
        dup
    while
        dup hxt ins@ stmp @ = if exit then
        @
    repeat
;

\ A word is the instruction in stmp if it is that instruction with the
\ return folded in, or that instruction and then a return.
: op? ( hdr -- f )
    hxt dup ins@
    stmp @ d# 3 and 0= if
        dup stmp @ h# 1003 or = if 2drop true exit then
    then
    stmp @ = swap 1+ ins@ h# 7403 = and
;

\ >r, r> and their like are compiling words: a call to inline: and then the
\ instruction to compile.  Such a word names its instruction best of all.
: inl>hdr ( -- hdr|0 ) \ for the instruction in stmp
    forth @
    begin
        dup
    while
        dup hxt dup ins@ ['] inline: h# 4000 or =
        swap 1+ ins@ stmp @ = and if exit then
        @
    repeat
;

: op>hdr ( w -- hdr|0 ) \ the word that is the instruction w
    stmp !  inl>hdr ?dup if exit then
    forth @
    begin
        dup
    while
        dup op? if exit then
        @
    repeat
;

\ A number on its own is not an address of code unless it is in the kernel
\ and system pages, or in the pages from 8 up.
: code? ( x -- f ) dup h# 2000 u< swap h# 8000 - h# 8000 u< or ;

: h4 ( n -- ) dup d# 12 rshift hex1 dup d# 8 rshift hex1 dup d# 4 rshift hex1 hex1 ;

: snum ( n -- ) sb-begin . sb-end  sb sbn @ 1- d# -1 tok ;
: sraw ( w -- ) sb-begin [char] [ emit h4 [char] ] emit sb-end  sb sbn @ d# -1 tok ;
: sname ( hdr -- ) \ a word, and a new line after one that stores
    dup CELL2 + count rot hxt
    >r 2dup + 1- c@ r> swap >r
    tok
    r> [char] ! = if snl then
;

: slink ( xt -- ) \ a word by name, or by address if it has no name
    dup xt>hdr ?dup if nip sname exit then
    sb-begin [char] $ emit dup h4 sb-end
    >r sb sbn @ r> tok
;

: fused? ( w -- a|0 ) \ the text for a fused instruction
    fused
    begin
        dup @
    while
        2dup @ = if nip cell+ exit then
        d# 32 +
    repeat
    2drop d# 0
;

: sword ( addr u -- ) \ one word of a fused instruction: a link if it is in the dictionary
    sfind ?dup if
        drop xt>hdr ?dup if sname exit then
        exit
    then
    d# -1 tok
;

: sfused ( w -- f ) \ show a fused instruction as its words, if it is in the table
    fused? ?dup 0= if false exit then
    count fn ! fa !
    true sbold !
    begin
        fn @
    while
        fa @ c@ d# 32 = if
            d# 1 fa +!  d# -1 fn +!
        else
            fa @ d# 0                               ( start length )
            begin
                fn @ if fa @ c@ d# 32 <> else false then
            while
                1+  d# 1 fa +!  d# -1 fn +!
            repeat
            sword
        then
    repeat
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
    sflush starget
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

: sop ( w -- ) \ an instruction with no return in it
    dup op>hdr ?dup if nip sname exit then
    dup sfused if drop exit then
    sraw
;

: s-alu ( w -- )
    sflush
    dup h# 1000 and if
        dup h# 7403 = if
            drop
        else
            dup ret>hdr ?dup if
                nip sname
            else
                dup d# 3 and d# 3 = if h# 1003 invert and sop else sraw then
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
        k-colon wt  sname
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
    begin
        dup 0>
    while
        space 1-
    repeat
    drop
;

: ttok ( addr u xt -- ) drop  sfresh @ if sind @ 2* sspaces then  type space ;

header see
: see ( "name" -- ) tick  ['] ttok 'tok !  ['] cr 'nl !  cr sdecomp ;

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
create t-words ," Words"

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
    forth @
    begin
        over 0> over 0<> and
    while
        @ swap 1- swap
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
        hdr>i dup wtop @ - d# 16 u< 0= if dup wtop ! then drop
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
        d# 4 lshift ty0 @ + lky !
        scol @ d# 3 lshift tx0 @ + d# 112 + lkx !
        sbold @ bold !
        lkx @ lky @ draw-text
        d# 0 bold !
        r@ 0< 0= if
            false lkx @ lky @ d# 15 + lku @ d# 3 lshift hline
            nlk @ d# 96 < if
                scol @  srow @ wsrc @ - d# 8 lshift or  lku @ d# 16 lshift or
                nlk @ d# 4 lshift lk + !
                r@ nlk @ d# 4 lshift lk + cell+ !
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

: words-draw ( win -- )
    content ty0 ! tx0 !
    winit @ 0= if true winit !  d# 0 wpick then
    wxt @ xt>hdr ?dup if hdr>i else d# -1 then wsel !
    d# 0 ti !
    wtop @ nth
    begin
        dup 0<> ti @ d# 16 < and
    while
        ti @ wtop @ + wsel @ = inverse !
        dup CELL2 + count d# 13 min
        tx0 @ d# 2 +  ty0 @ ti @ d# 4 lshift +  draw-text
        d# 0 inverse !
        @
        d# 1 ti +!
    repeat
    drop
    false tx0 @ d# 108 + ty0 @ d# 1 d# 256 fill-rect
    d# 0 scol !  d# 0 srow !  d# 0 nlk !
    ['] gtok 'tok !  ['] gnl 'nl !
    wxt @ sdecomp
    srow @ 1+ wrows !
;

: words-key ( c win -- )
    drop
    dup d# 128 = if drop wsel @ 1- wpick exit then
    dup d# 129 = if drop wsel @ 1+ wpick exit then
    dup d# 132 = if drop wsel @ d# 16 - wpick exit then
    dup d# 133 = if drop wsel @ d# 16 + wpick exit then
    dup d# 130 = if drop wsrc @ d# 8 - d# 0 max wsrc ! exit then
    dup d# 131 = if drop wsrc @ d# 8 + wrows @ 1- min wsrc ! exit then
    d# 8 = if wback then
;

: words-click ( x y win -- )
    drop d# 4 rshift crow !
    dup d# 108 < if drop crow @ wtop @ + wpick exit then
    d# 112 - d# 3 rshift ccol !
    d# 0
    begin
        dup nlk @ <
    while
        dup d# 4 lshift lk + @ wtmp !
        wtmp @ d# 8 rshift h# ff and crow @ =
        ccol @ wtmp @ h# ff and -  wtmp @ d# 16 rshift u<  and if
            d# 4 lshift lk + cell+ @ wgo exit
        then
        1+
    repeat
    drop
;
