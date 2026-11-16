\ source_test's block: what is to be read, and what is not
: seven ( -- n ) 7 ;            \ 99 throw
( : seven 0 ; ) : nine ( -- n ) 9 ;
: both ( -- a b ) \ a definition over lines, a comment in each
    seven ( drop ) nine         \ ;
;
: open 3 ; ( a comment in brackets goes on over lines
    : open 0 ;  \ and a \ in it is nothing: ) : shut 4 ;
: after 5 ; \
: full 6 ;                                                     \
 : last 8 ;
2 block drop : back 10 ;    \ another block was in the buffer
2 load : inner 11 ;         \ a block of nothing was loaded
: end 12 ; ( and one that is not ended ends with the block
