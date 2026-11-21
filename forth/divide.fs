\ divide.fs - / and mod

\ cross.fs runs these too, where source divides as it is
\ compiled: gforth compiles this file for it (host-included).

header / : cw-/ ( n1 n2 -- n3 ) \ the quotient, towards zero
    2dup xor >r  abs swap abs swap  u/mod nip
    r> 0< if negate then
;
header mod : cw-mod ( n1 n2 -- n3 )
    2dup cw-/ * -
;
