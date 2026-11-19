//
// desktop_test IMAGE.hex DISK.img PICTURE.tiff FORTH OUT [names [rom]]
//
// The desktop on the reference VM, and what is on its screen.
//
// With `rom` after `names` the machine is started with nothing in memory
// but its ROM, and DISK.img is one it can start from (mkdisk --boot-desk):
// IMAGE.hex is then the image a machine boots with (BOOT=on), which the
// machine reads from the disk, and the desktop the checks are run on is
// the one its boot compiled from the disk's source (doc/boot.md).
//
// The checks are those of deskcheck.c, and the machine they are run on is
// the reference VM (deskvm.h), which is built into the test for a machine
// of 64 bits, 32 or 16.  DISK.img is one that mkdisk made as it makes
// QEMU's: block 1 for the editor, the picture as a TIFF, which
// PICTURE.tiff is too, and the tables of names if the last argument says
// the build made any.  The machine is given a copy of it, in OUT, where
// the screens are kept too.
//
// For whoever wants what the desktop left, or what it ran: with MEMORY in
// the environment the machine's memory is written to the file it names
// when the checks are done; if the VM in this is the one that counts
// (bench/counting.c), its counts are written; and if it is the one that
// watches the heap (bench/heap.c), what it found is.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "deskvm.h"
#include "nucleus.h"
#include "tables.h"

#ifdef COUNTING_A_TEST
void counts_written(void);
#endif
#ifdef WATCHING_A_TEST
void heap_said(void);
#endif

int main(int argc, char *argv[]) {
    if (argc < 6) {
        fprintf(stderr, "Usage: %s IMAGE.hex DISK.img PICTURE.tiff FORTH OUT [names [rom]]\n", argv[0]);
        return(1);
    }
    alarm(1800);                // a machine that does not stop is stopped
    tables_are(argv[4]);
    desk on = deskvm_start(argv[1], argv[2], argv[3], argv[5], argc > 6, argc > 7);
    int failed = desk_checks(&on);

    if (getenv("MEMORY")) {
        FILE* out = fopen(getenv("MEMORY"), "wb");
        if (out == NULL) {
            perror(getenv("MEMORY"));
            return(1);
        }
        fwrite(machine.memory, 1, sizeof(machine.memory), out);
        fclose(out);
    }
#ifdef COUNTING_A_TEST
    counts_written();
#endif
#ifdef WATCHING_A_TEST
    heap_said();
#endif
    printf("the desktop on the reference VM, cells of %d bits, %d by %d: %d checks failed; screens in %s/\n",
           CELL_BITS, DISP_W, DISP_H, failed, argv[5]);
    return(failed != 0);
}
