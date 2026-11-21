\ placement.fs - where data and code are laid: bulk, high, and
\ the words that say a page

\ cross.fs lays an image's regions and pages.  The nucleus has
\ dp and the bulk's pointer, and compiles code where cp is: here
\ high is bulk, and the words that say a page do nothing.

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

header meta : cw-meta ( -- ) ;
header target : cw-target ( -- ) ;
header code-page : cw-code-page ( n -- ) drop ;
header sys-page : cw-sys-page ( n -- ) drop ;
header big-page : cw-big-page ( n -- ) drop ;
header small-page : cw-small-page ( n -- ) drop ;
header small-first : cw-small-first ( -- ) ;
header low-page : cw-low-page ( -- ) ;
header low-page-end : cw-low-page-end ( -- ) ;
