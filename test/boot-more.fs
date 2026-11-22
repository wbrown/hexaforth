\ boot-more.fs - what test/boot.fs loads by its label: a word of
\ its own, found only until the file has been read, and a word
\ with a header under another name, which stays.

: eight ( -- n ) 8 ;
header more : more-of ( -- n ) eight ;

\ A cell of its own, and a word between meta and target: neither
\ has code once the file is read, and what uses them stays.
create own-five 5 ,
meta : twice ( n -- n' ) 2 * ; target
header five-twice
: five-twice ( -- n ) own-five @ [ 3 twice ] literal + ;
