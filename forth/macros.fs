\ macros.fs - :: on the machine: a word that runs as source is
\ compiled

\ To cross.fs a :: word is a definition of gforth's.  Here it
\ is an immediate word, and in its body literal and s" do what
\ they do there: literal compiles a number when the :: word
\ runs, and s" keeps its text.

create macro-xt 0 ,     \ where the :: word being made starts

header :: : cw-:: ( "name" -- )
    mkheader t]  thisxt @ macro-xt !  timmediate
;
header-imm literal : cw-literal ( x -- )
    thisxt @ macro-xt @ = if
        ['] tliteral 'compile @ execute exit
    then
    tliteral
;

\ a :: word's text, kept under dpl, which comes down to it
: text-kept ( c-addr u -- c-addr' u )
    dpl @ over -  CELLB negate and          ( c-addr u to )
    dup dp @ u< d# 8 -throw  dup dpl !
    swap >r  tuck r@ cmove  r>
;

header-imm s" : cw-s" ( "ccc<quote>" -- c-addr u )
    d# 34 parse  state @ 0= if exit then
    thisxt @ macro-xt @ = if
        text-kept swap tliteral tliteral exit
    then
    here over >r >r  s, align  r> >c 1+ tliteral  r> tliteral
;
