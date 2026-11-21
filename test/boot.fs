\ boot.fs - block 1 of test/boot_test.c's disk: it says it is
\ read, and loads a source of the disk's by its label.

.( block 1 is read )
include boot-more.fs
: booted ( -- n ) 6 7 * ;
