//
// desktime KERNEL.elf KERNEL.symbols DISK.img PICTURES.img PICTURE.tiff FORTH NAMES.tables OUT [SECONDS]
//
// PICTURES.img is the machine's second disk, which has PICTURE.tiff under
// the picture's label (mkdisk --disk).
//
// When the desktop is ready, and when its picture is made.
//
// Boots the desktop on QEMU as guitest does (qemu.h) and watches it,
// without touching it: when the console says the desktop is up, when the
// Forth window has its prompt on the screen, and when the first row of
// the picture, half its rows and all of them have been made.  The times
// are in seconds from when the machine was started.  On a kernel held to
// a speed (the target `demo-time`, with MIPS) they are what a machine of
// that speed is like to wait for, and with the disk held to one as well
// (DISK=40, from the environment) one with a floppy.  They are what to
// measure before and after a change to how the picture is read, dithered
// or shown.  It gives up after SECONDS, 600 if none is given.  The copy of
// the disk the machine is given is kept in OUT.
//
// The screen is looked at for the prompt, as the desktop's checks read it
// (test/deskcheck.h), once the console has said the desktop is up, as
// guitest does: until the kernel has told QEMU's display how big it is,
// QEMU has a screen of its own size, and the prompt cannot be on the
// screen before the desktop is.  How many rows of the picture are made is read out
// of the machine's memory, which is in QEMU's where the kernel's symbol
// `image` is (symbols.h): `vw` in view.fs is the viewer in hand, and the
// third cell of a viewer's record is the rows of its bitmap that are made
// (`bmh`).  If either changes, so must this; it stops and says so if the
// names that go with the image have no `vw`.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../bench/tiff.h"
#include "../test/deskcheck.h"
#include "../test/tables.h"
#include "../util/words.h"
#include "qemu.h"
#include "symbols.h"

#define ROWS_MADE 2             // bmh: where it is in a viewer's record (view.fs)

static struct timespec started;
static double now(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return((double)(t.tv_sec - started.tv_sec) + (t.tv_nsec - started.tv_nsec) / 1e9);
}

enum { UP, PROMPT, FIRST, HALF, ALL, TIMES };
static double when[TIMES];      // when each was first so, or less than nothing
static void note(int what) {
    if (when[what] < 0) when[what] = now();
}

int main(int argc, char *argv[]) {
    if (argc < 9) {
        fprintf(stderr, "Usage: %s KERNEL.elf KERNEL.symbols DISK.img PICTURES.img PICTURE.tiff FORTH NAMES.tables OUT "
                        "[SECONDS]\n", argv[0]);
        return(1);
    }
    double limit = argc > 9 ? atof(argv[9]) : 600;
    int height = read_tiff(argv[5]).h;
    uint64_t vw = data_called(argv[7], "vw");
    uint64_t memory = symbol_at(argv[2], "image");      // where the machine's cell 0 is
    if (!vw) {
        fprintf(stderr, "%s has no label vw: view.fs has changed, and this must change with it\n", argv[7]);
        return(1);
    }
    tables_are(argv[6]);
    desk watched = { {"cold", "desktop"}, " catch drop", true, argv[5], argv[8] };
    for(int what=0; what < TIMES; what++) when[what] = -1;

    clock_gettime(CLOCK_MONOTONIC, &started);
    qemu_start(argv[1], argv[3], argv[4], argv[8]);
    while (when[ALL] < 0 && now() < limit) {
        const char* said = qemu_said();
        uint64_t viewer = qemu_cell(memory + CELL_BYTES * vw);
        int rows = viewer ? (int)qemu_cell(memory + CELL_BYTES * (viewer + ROWS_MADE)) : 0;
        if (strstr(said, "desktop")) note(UP);
        if (rows) note(FIRST);
        if (2 * rows >= height) note(HALF);
        if (rows >= height) note(ALL);
        if (when[UP] >= 0 && when[PROMPT] < 0 && desk_sees(&watched, desk_screen(), "forth", 0, "ok")) note(PROMPT);
        desk_turns(0);
    }

    static const char half_is[] = "half its %d rows are made";
    char half[sizeof(half_is) + 3 * sizeof(int)];       // and the number, which is no more digits than three a byte
    snprintf(half, sizeof(half), half_is, height);
    const char* what[TIMES] = { "the console says the desktop is up", "the Forth window has its prompt",
                                "the first row of the picture is made", half, "the picture is finished" };
    for(int n=0; n < TIMES; n++) {
        if (when[n] >= 0) printf("%-38s %6.1f s\n", what[n], when[n]);
        else printf("%-38s not in %.0f s\n", what[n], limit);
    }
    return(when[ALL] >= 0 && when[PROMPT] >= 0 ? 0 : 1);
}
