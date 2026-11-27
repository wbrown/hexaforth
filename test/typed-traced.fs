\ typed-traced.fs - input for the tests typed-traced and
\ typed-trace-most: trace-on after two definitions, which are
\ then run, one calling the other
: twice ( n -- 2n ) dup + ;
: sum ( n -- total ) 0 swap 0 do i twice + loop ;
trace-on
3 sum .
trace-off
4 twice .
