//
// guitest KERNEL.elf DISK.img PICTURE.tiff FORTH OUT [boot]
//
// The desktop on the AArch64 kernel under QEMU, and what is on its screen.
//
// The checks are those of test/deskcheck.c, which test/desktop_test.c runs
// on the reference VM with cells of each size.  Here the machine they are
// run on is QEMU's (qemu.h), with its devices, and a copy of the disk that
// is kept in OUT with the screens.  With SCALE in the environment it checks
// as well that every pixel of every screen it took was a whole block.
//
// With `boot` the kernel has only its ROM in it, and the machine reads its
// image from the disk and compiles its desktop from the source there
// (doc/boot.md).  It is then to say each file as it starts, and its screen
// is to show that while it compiles: a boot that says nothing, or whose
// screen has nothing on it until the desktop is up, fails.
//

#include <stdio.h>
#include <string.h>
#include "../test/deskcheck.h"
#include "../test/tables.h"
#include "qemu.h"

int main(int argc, char *argv[]) {
    int boots = argc == 7 && !strcmp(argv[6], "boot");
    if (argc != 6 + boots) {
        fprintf(stderr, "Usage: %s KERNEL.elf DISK.img PICTURE.tiff FORTH OUT [boot]\n", argv[0]);
        return(1);
    }
    tables_are(argv[4]);
    qemu_start(argv[1], argv[2], argv[5]);

    // The newest words of the kernel's dictionary are the system's own.
    desk on = { {"cold", "desktop"}, " catch drop", true, argv[3], argv[5] };
    int failed = desk_checks(&on);
    if (boots) {
        int shown = qemu_boot_said && qemu_boot_shown;
        printf("%s the boot said what it compiled (%s), and the screen showed it while it did (%s)\n",
               shown ? "ok  " : "FAIL", qemu_boot_said ? "it did" : "it said nothing",
               qemu_boot_shown ? "it did" : "it showed nothing");
        failed += !shown;
    }
    if (qemu_scale > 1) {
        printf("%s every pixel is a block of %d by %d: %d of %d screens were not exact\n",
               qemu_inexact ? "FAIL" : "ok  ", qemu_scale, qemu_scale, qemu_inexact, qemu_shots);
        failed += qemu_inexact != 0;
    }
    printf("the desktop on QEMU: %d checks failed; screens in %s/\n", failed, argv[5]);
    return(failed != 0);
}
