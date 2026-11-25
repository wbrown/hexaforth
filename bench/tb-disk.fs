: tb-4 ( x -- ) 4 0 do dup emit 8 rshift loop drop ;
: tb-read ( a blk -- ) 29 emit tb-4 tb-4 ;
: tb-write ( a blk -- ) 28 emit tb-4 tb-4 ;
' tb-read 'blk-read !
' tb-write 'blk-write !
