\ shadowed.fs - names defined again, each way that cross.fs
\ notes one (cross.fs, names defined again).  The test `shadows`
\ holds what cross.fs writes of this to test/shadowed.shadows.

meta
    4 org
target

header quit : quit halt ;

\ again: a word of a name the program has defined
: twice ( n -- n' ) dup + ;
: twice ( n -- n' ) d# 2 * ;

\ own: no header, and a name that has a header
header kept : kept ( -- n ) d# 1 ;
create kept 0 ,

\ header: a header of a name that has a header
header said : said-once ( -- n ) d# 1 ;
header said : said-twice ( -- n ) d# 2 ;

\ meta: between meta and target, a name that is a word's, and
\ one that gforth has
meta
: twice ( n -- n' ) 2 * ;
: spaces ( n -- ) drop ;
target

\ of-meta: a word named as something between meta and target was
meta 7 constant seven target
: seven ( -- n ) d# 7 ;

\ Not noted: a baseword given its body, and a name at: defines
\ where the word its header names pushes that address.
header nip :noname nip ;
create three 0 , 0 , 0 ,
header second : second ( -- a ) three CELL + ;
$1 at: three second

header main : main twice seven kept said-twice second 2drop halt ;
