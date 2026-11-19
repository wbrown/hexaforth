1 2 + .
: sq dup * ; 12 sq .
: f dup 1 > if dup 1- recurse * then ; 7 f .
: g 0 swap begin dup while tuck + swap 1- repeat drop ; 10 g .
: h 0= if 7 else 9 then ; 0 h . 1 h . hex ff . decimal
create t 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
create n 0 ,
:noname 1 n +! ; 128 128 t spawn pause pause pause n @ .
: lp 3 0 do i . pause loop ; lp n @ .
bye
