//
// guitest KERNEL.elf DISK.img PICTURES.img PICTURE.tiff FORTH OUT [boot]
//
// The desktop on the AArch64 kernel under QEMU, and what is on its screen.
//
// The machine has two disks: DISK.img, and PICTURES.img, which has the
// picture under its label and is PICTURE.tiff as a disk (mkdisk --disk).
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
#include <stdlib.h>
#include <string.h>
#include "../vm_constants.h"
#include "../test/deskcheck.h"
#include "../test/tables.h"
#include "qemu.h"
#include "symbols.h"

int main(int argc, char *argv[]) {
    int boots = argc == 8 && !strcmp(argv[7], "boot");
    if (argc != 7 + boots) {
        fprintf(stderr, "Usage: %s KERNEL.elf DISK.img PICTURES.img PICTURE.tiff FORTH OUT [boot]\n", argv[0]);
        return(1);
    }
    tables_are(argv[5]);
    qemu_start(argv[1], argv[2], argv[3], argv[6]);

    // Where the machine's cell 0 is, the kernel's symbols say, which are
    // beside the kernel: the checks read the machine's dictionary there.
    char symbols[strlen(argv[1]) + sizeof(".symbols")];
    strcpy(symbols, argv[1]);
    char* dot = strrchr(symbols, '.');
    strcpy(dot ? dot : symbols + strlen(symbols), ".symbols");
    uint64_t image = symbol_at(symbols, "image");
    qemu_image(image);

    desk on = { true, argv[4], argv[6] };
    int failed = desk_checks(&on);
    // For whoever wants what the machine was left as: with MEMORY in the
    // environment its memory is written to the file that names, which the
    // tools that read the reference VM's read (disasm_hex --memory).
    if (getenv("MEMORY")) qemu_memory(image, (uint64_t)CELL_BYTES << ADDR_BITS, getenv("MEMORY"));
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
    printf("the desktop on QEMU: %d checks failed; screens in %s/\n", failed, argv[6]);
    return(failed != 0);
}
