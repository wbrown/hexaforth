\ boot-more.fs - a source of the disk that test/boot_test.c
\ starts a machine from, which its block 1 loads by its label.
\ It has a word of its own, which is found until the file has
\ been read and not after, and a word with a header, under
\ another name than the definition's, which stays.

: eight ( -- n ) 8 ;
header more : more-of ( -- n ) eight ;
