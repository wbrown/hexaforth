\ names.fs - names that source uses and the kernel has not

\ Source is written for cross.fs, and the nucleus compiles it
\ too.  A header here has the name source uses, on a word
\ named otherwise.

\ the kernel's evaluate, align, , and c, by cross.fs's names
header tevaluate : cw-tevaluate ( c-addr u -- ) tevaluate ;
header tcalign : cw-tcalign ( -- ) align ;
header tcell, : cw-tcell, ( x -- ) t, ;
header tc, : cw-tc, ( c -- ) tc, ;

header 2variable : cw-2variable ( "name" -- )
    tcreate  d# 0 t,  d# 0 t,
;

\ what cross.fs and the machine's definitions name: a word each
size-words      \ cross.fs's constants (both-constant)
baseword-words  \ the basewords, and the machine's sizes
field-names     \ an instruction's fields, for the decompiler
machine-words   \ cross.fs's macros (both::)
flag-words      \ and its flags (both-flag::)
