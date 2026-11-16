display-init
: mark 30 emit ;
: t2 300 0 do here >c 48 64 40 draw-text loop ;
: f1 100 0 do p-gray pattern ! 10 10 400 200 pat-rect loop ;
: s1 30 0 do 20 20 400 256 0 -16 scroll-rect loop ;
mark t2 mark f1 mark s1 mark
8000 execute
