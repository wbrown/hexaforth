\ refused-if.fs - an if that no then ends: cross.fs is to stop
\ at the end of the definition (the test `refused-if`).
meta
    4 org
target

: some ( f -- ) if d# 1 drop ;
header main : main d# 0 some halt ;
header quit : quit halt ;
