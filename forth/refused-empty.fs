\ refused-empty.fs - a word of no instructions that runs on into
\ another and has no header: cross.fs is to stop at it (the test
\ `refused-empty`).
meta
    4 org
target

: none ( -- 3 ) ;into three
header three : three ( -- 3 ) d# 3 ;
header main : main none drop halt ;
header quit : quit halt ;
