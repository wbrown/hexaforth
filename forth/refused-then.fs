\ refused-then.fs - a then that is given where a loop began,
\ which is no branch: cross.fs is to stop at it (the test
\ `refused-then`).
meta
    4 org
target

: down ( n -- 0 ) begin dup while 1- then ;
header main : main d# 3 down drop halt ;
header quit : quit halt ;
