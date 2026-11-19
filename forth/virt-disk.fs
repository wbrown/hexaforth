\ virt-disk.fs - the disk of the machine QEMU calls virt, for
\ its ROM

\ That machine has 32 places for a virtio device, from
\ $0a000000 of its addresses and $200 apart.  Its disk is the
\ first block device in them.  The AArch64 kernels are the
\ machine on it (baremetal/).

include virtio-disk.fs

\ the disk, if there is one: its words to read and to write a
\ block (nuc.fs)
: disk? ( -- read write true | false )
    h# 0a000000 h# 4000 bounds
    begin
        2dupxor
    while
        dup vdisk? if nip vdisk exit then
        h# 200 +
    repeat
    xor
;
