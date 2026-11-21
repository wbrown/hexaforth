\ literals.fs - numbers, characters and strings written in
\ source: d# h# c# ," char [char]

\ cross.fs runs these too: gforth compiles this file for it
\ (host-included).

: base-number ( base "n" -- ) \ compiled, or left if not
    base @ >r  base !
    parse-name numbered
    r> base !
;

header-imm d# : (d#) ( "n" -- ) d# 10 base-number ;
header-imm h# : (h#) ( "n" -- ) d# 16 base-number ;
header-imm c# : (c#) ( "n" -- ) d# 10 base-number ;

header ," : (,") ( "ccc<quote>" -- ) \ a counted string, here
    d# 34 parse s, ;

header char : cw-char ( "c" -- c ) parse-name drop c@ ;
header-imm [char] : cw-[char] ( "c" -- ) \ compiled
    parse-name drop c@ tliteral ;
