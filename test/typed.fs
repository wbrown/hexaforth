\ typed.fs - input for the tests typed-N (hexaforth --typed):
\ defines a word, prints its result, and leaves 3 4 on the stack
: twice ( n -- 2n ) 2 * ;
21 twice .
3 4
