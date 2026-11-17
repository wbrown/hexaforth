: mark 30 emit ;
create b 0 , 0 ,
: rows 0 do dup dither-row loop drop ;
: own tiff-open b swap atkinson ;
: half tiff-open b swap dup image! 0 0 tw @ 2/ th @ 2/ dither-half th @ 2/ rows ;
