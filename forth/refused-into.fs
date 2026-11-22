\ refused-into.fs - a word that runs on into another than the
\ one it says: cross.fs is to stop at the definition between
\ them (the test `refused-into`).
meta
    4 org
target

: one ( -- 1 ) d# 1 ;into three
: two ( -- 2 ) d# 2 ;
: three ( -- 3 ) d# 3 ;
header main : main one drop halt ;
header quit : quit halt ;
