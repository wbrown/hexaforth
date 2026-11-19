\ boot.fs - block 1 of the disk that test/boot_test.c starts a
\ machine from: what the machine loads once its ROM has read the
\ rest of the system from the disk.

: booted ( -- n ) 6 7 * ;
