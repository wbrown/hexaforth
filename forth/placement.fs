\ placement.fs - where data and code are laid: bulk, high, the
\ scratch, and the words that say a page

\ cross.fs lays an image's regions and pages.  The nucleus has
\ dp and the bulk's pointer, and compiles code where cp is: here
\ high is bulk.

\ dp and dpl change places with the bulk's, if it has a place
: bulk-swap ( -- )
    bdp @ 0= if exit then
    dp @ dpl @ u> d# 8 -throw
    dp @ bdp @ dp ! bdp !  dpl @ bdl @ dpl ! bdl !
;
header bulk : cw-bulk ( -- ) bulk-swap ;
header unbulk : cw-unbulk ( -- ) bulk-swap ;
header high : cw-high ( -- ) bulk-swap ;
header unhigh : cw-unhigh ( -- ) bulk-swap ;

header uncoded
: uncoded ( end from -- ) \ the code between is made zero
    begin 2dup xor while  d# 0 over i!  1+  repeat  2drop ;

\ ---- the scratch

\ Between meta and target, code is needed only while the file
\ is compiled.  It goes to the scratch, the last $400
\ instructions of the last code page, which is given back when
\ the file has been read (files.fs).  In a definition the two
\ do nothing.
meta
code-base code-size + $400 - constant scratch-from
:: SCRATCH0 scratch-from literal ;
target
header scratch          \ where the scratch's next code goes
create scratch  meta scratch-from target ,
header cp-kept          \ cp while the scratch has it, or 0
create cp-kept 0 ,

header-imm meta : cw-meta ( -- ) \ -8 if code has reached it
    state @ cp-kept @ or if exit then
    cp @ SCRATCH0 u< 0= d# 8 -throw
    cp @ cp-kept !  scratch @ cp ! ;
header-imm target : cw-target ( -- )
    cp-kept @ ?dup 0= if exit then
    cp @ scratch !  cp !  d# 0 cp-kept ! ;

header scratch-back
: scratch-back ( x -- ) \ the scratch from x on is given back
    cw-target  dup scratch !
    CODE-BASE CODE-SIZE + swap uncoded ;

\ In code that is kept, a word of the scratch is in place.  One
\ that cannot be, or whose xt is asked for, is not a word of
\ that code's: -13.
: kept? ( xt -- f ) \ is xt a word code that is kept may call?
    SCRATCH0 u<  cp-kept @ or ;
: kept, ( xt -- ) \ what compile, does (nuc.fs)
    dup kept? if pcompile, exit then
    dup small dup 0< d# 13 -throw  placed ;
meta t' kept, t' 'compile tvar! target
header-imm [']
:noname ( "name" -- )
    tick  dup kept? 0= d# 13 -throw  tliteral ;

\ ---- the pages of code

\ pages (nuc.fs) has how far each page's code has got.  Source
\ of a block says the page its code goes on, as it does to
\ cross.fs; typed, the words that say one do nothing.

: 'cp ( -- a ) \ the cell that says where code is laid next
    cp-kept @ if cp-kept else cp then ;

: page-kept ( -- ) \ how far the page code is on has got
    'cp @  dup d# 12 rshift pages + ! ;

: highest ( -- ia ) \ how far the last page with code has got
    d# 0  pages CODE-PAGES bounds
    begin 2dup xor while
        dup @ ?dup if >r rot drop r> -rot then  1+
    repeat
    2drop ;

: lay-at ( ia -- ) \ code goes on from ia: -8 if past the pages
    dup CODE-BASE CODE-SIZE + u< 0= d# 8 -throw  'cp ! ;

\ after all the code there is: on the page after, if a
\ definition may not begin where that has got to (newpage?)
: after-all ( -- ia ) \ no sooner than the pages past page 1
    page-kept  highest CODE-BASE max
    dup h# fff and cpr @ u> if h# fff or 1+ then ;
header code-on
: code-on ( -- ) after-all lay-at ;

\ Block source is done.  newpage? takes code from page 1 to
\ cpn, and from a later page to the one after: so cpn is after
\ all there is, and so is code that is past page 1.
header pages-kept
: pages-kept ( -- )
    cp @ CODE-BASE u< if after-all cpn ! exit then  code-on ;

\ before a definition: off a page newpage? would leave, which
\ goes to the page after whatever that has
header code-room
: code-room ( -- )
    cp-kept @ if exit then
    cp @ h# fff and cpr @ u> if code-on then ;

\ a definition from ia ran off its page: what it laid is zero,
\ the page has that room again, and code goes on on another
header code-off
: code-off ( ia -- )
    cp @ over uncoded  dup cp !  page-kept
    highest over = if h# fff or 1+ lay-at exit then
    drop code-on ;

header code-page
: cw-code-page ( n -- ) \ on from where page n's code had got to
    blk @ 0= if drop exit then
    page-kept  dup pages + @ ?dup if nip else d# 12 lshift then
    lay-at ;
header sys-page
: cw-sys-page ( n -- ) CODE-BASE d# 12 rshift + cw-code-page ;

SMALL? [if]
header big-page   : cw-big-page ( n -- ) drop ;
header small-page : cw-small-page ( n -- ) cw-sys-page ;
header small-first : cw-small-first ( -- ) d# 1 cw-code-page ;
[else]
header big-page   : cw-big-page ( n -- ) cw-sys-page ;
header small-page : cw-small-page ( n -- ) drop ;
header small-first : cw-small-first ( -- ) ;
[then]
header low-page : cw-low-page ( -- ) ;
header low-page-end : cw-low-page-end ( -- ) ;
