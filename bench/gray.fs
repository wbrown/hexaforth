: mark 30 emit ;
create b 0 , 0 ,
create g 0 ,
create m 0 ,
: keep ( blk shift -- ) swap tiff-open swap gray-keep g ! ;
: draw ( dshift -- ) >r  b g @ r@ gray-bitmap  b m @ g @ r> gray-draw ;
