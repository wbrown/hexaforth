//
// guitest KERNEL.elf DISK.img PICTURES.img PICTURE.tiff FORTH OUT [boot]
//
// Runs the desktop's checks on the AArch64 kernel under QEMU.
//
// The machine has two disks: DISK.img, and PICTURES.img, which has
// PICTURE.tiff under the picture's label (mkdisk --disk).
//
// The checks are those in test/deskcheck.c.  test/desktop_test.c runs the
// same checks on the reference VM at each cell size.  Here they run on
// QEMU's machine (qemu.h), with its devices.  The machine is given a copy
// of the disk, which is kept in the directory OUT with the screens.  If
// SCALE is set in the environment, the program also checks that every
// pixel of every screen it read was a whole block.
//
// With `boot`, the kernel has only its ROM in it.  The machine reads its
// image from the disk and compiles its desktop from the source there
// (doc/boot.md).  It must print each file as it starts it, and its screen
// must show that text while it compiles.  The test fails if the boot
// prints nothing, or if the screen shows nothing until the desktop is up.
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

    // The kernel's symbols, which are in a file beside the kernel, give
    // the address of the machine's cell 0.  The checks read the machine's
    // dictionary from there.
    char symbols[strlen(argv[1]) + sizeof(".symbols")];
    strcpy(symbols, argv[1]);
    char* dot = strrchr(symbols, '.');
    strcpy(dot ? dot : symbols + strlen(symbols), ".symbols");
    uint64_t image = symbol_at(symbols, "image");
    qemu_image(image);

    desk on = { true, argv[4], argv[6] };
    int failed = desk_checks(&on);
    // If MEMORY is set in the environment, the machine's memory as the
    // checks left it is written to the file MEMORY names.  The tools that
    // read the reference VM's memory can read that file (disasm_hex
    // --memory).
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
