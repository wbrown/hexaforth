\ boot-more.fs - what test/boot.fs loads by its label: a word of
\ its own, found only until the file has been read, and a word
\ with a header under another name, which stays.

: eight ( -- n ) 8 ;
header more : more-of ( -- n ) eight ;
