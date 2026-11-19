\ apps.fs - what the boot compiles from the disk

\ The system is one list (system.fs), and this is its end:
\ cross.fs lays it in the whole image, and the image a machine
\ boots from a disk (BOOT=on) is the rest without it.  The disk
\ such a machine boots from has this as block 1, so that the
\ boot compiles each file here from its source on the disk
\ (doc/boot.md).  Where every word has a header the applications
\ before the desktop are compiled so too (apps-wide.fs).

BOOTING? [if] W32+? [if]
include apps-wide.fs
[then] [then]

include desk.fs         \ the desktop itself
