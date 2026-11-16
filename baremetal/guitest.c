//
// guitest KERNEL.elf DISK.img PICTURE.tiff FORTH OUT
//
// The desktop on the AArch64 kernel under QEMU, and what is on its screen.
//
// The checks are those of test/deskcheck.c, which test/desktop_test.c runs
// on the reference VM with cells of each size.  Here the machine they are
// run on is QEMU's (qemu.h), with its devices.  With SCALE in the
// environment it checks as well that every pixel of every screen it took
// was a whole block.
//

#include <stdio.h>
#include "../test/deskcheck.h"
#include "../test/tables.h"
#include "qemu.h"

int main(int argc, char *argv[]) {
    if (argc != 6) {
        fprintf(stderr, "Usage: %s KERNEL.elf DISK.img PICTURE.tiff FORTH OUT\n", argv[0]);
        return(1);
    }
    tables_are(argv[4]);
    qemu_start(argv[1], argv[2]);

    // The newest words of the kernel's dictionary are the system's own.
    desk on = { {"cold", "desktop"}, " catch drop", true, argv[3], argv[5] };
    int failed = desk_checks(&on);
    if (qemu_scale > 1) {
        printf("%s every pixel is a block of %d by %d: %d of %d screens were not exact\n",
               qemu_inexact ? "FAIL" : "ok  ", qemu_scale, qemu_scale, qemu_inexact, qemu_shots);
        failed += qemu_inexact != 0;
    }
    printf("the desktop on QEMU: %d checks failed; screens in %s/\n", failed, argv[5]);
    return(failed != 0);
}
