\ apps-wide.fs - the applications the boot compiles where every
\ word has a header

\ Part of the system's list (system.fs): in the whole image, and
\ in the image a machine of more than 64K cells boots from, not;
\ that machine's boot compiles these from the disk (apps.fs).
\ What is between meta and target is cross.fs's, which says what
\ page of code each goes on; the boot does nothing with it.

meta 2 big-page  2 small-page target

include gray.fs         \ kept as gray, drawn through a mask
include view.fs         \ image viewer, and its menu

\ The calculator and HexaPaint.  Where there are pages to spare
\ they are on this one.  Where there are not, they are in the
\ upper half of the kernel's page, which the kernel, a ROM of
\ 4KB, leaves empty: 4KB of code that any page calls with one
\ instruction.

meta low-page target

include calc.fs         \ RPN calculator
include paint.fs        \ a window to draw in

meta low-page-end target

\ The QEMU virt machine's devices, which the AArch64 kernels run
\ on, are reached through the interface's vectors, and are with
\ what brings them up: the desktop, and the picture by itself.
\ Where memory is more than 64K cells they are on the next page,
\ with the desktop.

meta 3 big-page target

W64? [if]
include virtio.fs       \ shared by the virtio drivers
include virtio-input.fs \ keyboard and pointer driver
include mandrill.fs     \ the demo, without the desktop
[then]
