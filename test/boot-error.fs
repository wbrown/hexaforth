\ boot-error.fs - block 1 of a disk test/boot_test.c starts a
\ machine from, which has on its third line a word none knows.
: before ( -- n ) 1 ;  nonesuch  : after ( -- n ) 2 ;
