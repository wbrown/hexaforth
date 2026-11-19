\ boot.fs - block 1 of the disk that test/boot_test.c starts a
\ machine from: what the machine loads once its ROM has read the
\ rest of the system from the disk.  It says it is read, and
\ loads a source of the disk's by its label.

.( block 1 is read )
include boot-more.fs
: booted ( -- n ) 6 7 * ;
