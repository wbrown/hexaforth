//
// desktop_test IMAGE.hex DISK.img PICTURES.img PICTURE.tiff FORTH OUT [names [rom]]
//
// Runs the desktop on the reference VM and checks what is on its screen.
//
// The machine has two disks, each no bigger than a floppy.  DISK.img is
// the one it starts from, or is given.  PICTURES.img has the picture under
// its label (mkdisk --disk), and the viewer finds the picture there.
// PICTURE.tiff is the same picture as a file.
//
// With `rom` after `names`, the machine starts with nothing in memory but
// its ROM, and DISK.img must be a disk it can start from (mkdisk
// --boot-desk).  IMAGE.hex is then the boot image (BOOT=on), which the
// machine reads from the disk.  The checks run on the desktop that the
// boot compiled from the source on the disk (doc/boot.md).
//
// The checks are the ones in deskcheck.c, and the machine they run on is
// the reference VM (deskvm.h), at the cell size of the image: 64, 32 or 16
// bits.  DISK.img is a disk mkdisk made the way it makes QEMU's: block 1
// for the editor, and the tables of names if the `names` argument says the
// build made them.  The machine is given a copy of the disk in OUT, where
// the screens are saved too.
//
// The program can save what the desktop left and what it ran.  If MEMORY
// is set in the environment, the machine's memory is written to the file
// it names when the checks are done.  If the program was built with the
// counting VM (bench/counting.c), its counts are written.  If it was built
// with the VM that watches the heap (bench/heap.c), that VM's report is
// printed.  If BOOTED is set, the memory of a machine started from its ROM
// alone is written to the file it names once the machine has booted,
// before anything is typed at its prompt (deskvm.c).
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
    if (argc < 7) {
        fprintf(stderr, "Usage: %s IMAGE.hex DISK.img PICTURES.img PICTURE.tiff FORTH OUT [names [rom]]\n", argv[0]);
        return(1);
    }
    alarm(1800);                // kill the test if the machine never stops
    setvbuf(stdout, NULL, _IOLBF, 0);   // line-buffer stdout, so everything printed before that has been written
    tables_are(argv[5]);
    desk on = deskvm_start(argv[1], argv[2], argv[3], argv[4], argv[6], argc > 7, argc > 8);
    int failed = desk_checks(&on);

    if (getenv("MEMORY")) {
        FILE* out = fopen(getenv("MEMORY"), "wb");
        if (out == NULL) {
            perror(getenv("MEMORY"));
            return(1);
        }
        fwrite(machine.memory, 1, MEM_BYTES, out);
        fclose(out);
    }
#ifdef COUNTING_A_TEST
    counts_written();
#endif
#ifdef WATCHING_A_TEST
    heap_said();
#endif
    printf("the desktop on the reference VM, cells of %d bits, %d by %d: %d checks failed; screens in %s/\n",
           CELL_BITS, DISP_W, DISP_H, failed, argv[6]);
    return(failed != 0);
}
