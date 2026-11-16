//
// desktime KERNEL.elf DISK.img PICTURE.tiff FORTH NAMES.tables MEMORY [SECONDS]
//
// When the desktop is ready, and when its picture is made.
//
// Boots the desktop on QEMU as guitest does (qemu.h) and watches it,
// without touching it: when the console says the desktop is up, when the
// Forth window has its prompt on the screen, and when the first row of
// the picture, half its rows and all of them have been made.  The times
// are in seconds from when the machine was started.  On a kernel held to
// a speed (make demo-time MIPS=1) they are what a machine of that speed is
// like to wait for, and with the disk held to one as well (DISK=40, from
// the environment) one with a floppy.  They are what to measure before
// and after a change to how the picture is read, dithered or shown.  It
// gives up after SECONDS, 600 if none is given.
//
// The screen is looked at for the prompt, as the desktop's checks read it
// (test/deskcheck.h).  How many rows of the picture are made is read out
// of the machine's memory, which is at MEMORY in QEMU's: `vw` in view.fs
// is the viewer in hand, and the third cell of a viewer's record is the
// rows of its bitmap that are made (`bmh`).  If either changes, so must
// this; it stops and says so if the names that go with the image have no
// `vw`.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../bench/tiff.h"
#include "../test/deskcheck.h"
#include "../test/tables.h"
#include "../util/words.c"
#include "qemu.h"

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
    if (argc < 7) {
        fprintf(stderr, "Usage: %s KERNEL.elf DISK.img PICTURE.tiff FORTH NAMES.tables MEMORY [SECONDS]\n", argv[0]);
        return(1);
    }
    double limit = argc > 7 ? atof(argv[7]) : 600;
    int height = read_tiff(argv[3]).h;
    uint64_t vw = data_called(argv[5], "vw");
    uint64_t memory = strtoull(argv[6], NULL, 16);      // where the machine's cell 0 is
    if (!vw) {
        fprintf(stderr, "%s has no label vw: view.fs has changed, and this must change with it\n", argv[5]);
        return(1);
    }
    tables_are(argv[4]);
    desk watched = { {"cold", "desktop"}, " catch drop", true, argv[3], "." };
    for(int what=0; what < TIMES; what++) when[what] = -1;

    clock_gettime(CLOCK_MONOTONIC, &started);
    qemu_start(argv[1], argv[2]);
    static char said[1 << 16];
    while (when[ALL] < 0 && now() < limit) {
        qemu_said(said, sizeof(said));
        uint64_t viewer = qemu_cell(memory + 8 * vw);
        int rows = viewer ? (int)qemu_cell(memory + 8 * (viewer + ROWS_MADE)) : 0;
        if (strstr(said, "desktop")) note(UP);
        if (rows) note(FIRST);
        if (2 * rows >= height) note(HALF);
        if (rows >= height) note(ALL);
        if (when[PROMPT] < 0 && desk_sees(&watched, desk_screen(), "forth", 0, "ok")) note(PROMPT);
        desk_turns(0);
    }

    char half[64];
    snprintf(half, sizeof(half), "half its %d rows are made", height);
    const char* what[TIMES] = { "the console says the desktop is up", "the Forth window has its prompt",
                                "the first row of the picture is made", half, "the picture is finished" };
    for(int n=0; n < TIMES; n++) {
        if (when[n] >= 0) printf("%-38s %6.1f s\n", what[n], when[n]);
        else printf("%-38s not in %.0f s\n", what[n], limit);
    }
    return(when[ALL] >= 0 && when[PROMPT] >= 0 ? 0 : 1);
}
