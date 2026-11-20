\ apps.fs - what the boot compiles from the disk

\ The system is one list (system.fs), and this is its end:
\ cross.fs lays it in the whole image, and the image a machine
\ boots from a disk (BOOT=on) is the rest without it.  The disk
\ has this as block 1, so it is to be no longer than a block;
\ the boot compiles each file here from its source on the disk
\ (doc/boot.md).  Where code pages are few the desktop is in the
\ upper half of the kernel's page (system.fs, low-page).

BOOTING? [if] W32+? [if]
include apps-wide.fs
[then] [then]
meta low-page target
include desk.fs         \ the desktop itself
meta low-page-end target
