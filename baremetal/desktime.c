//
// desktime KERNEL.elf KERNEL.symbols DISK.img PICTURES.img PICTURE.tiff FORTH NAMES.tables OUT [SECONDS]
//
// Reports when the desktop is ready and when its picture is finished.
//
// PICTURES.img is the machine's second disk, which has PICTURE.tiff under
// the picture's label (mkdisk --disk).
//
// It boots the desktop on QEMU as guitest does (qemu.h) and watches it
// without sending it anything.  It records five times: when the console
// says the desktop is up; when the Forth window has its prompt on the
// screen; and when the first row of the picture, half its rows, and all
// its rows have been made.  The times are in seconds from the start of the
// machine.  On a kernel held to a speed (the target `demo-time`, with
// MIPS), they are what a person would wait on a machine of that speed.
// With the disk held to a speed as well (DISK=40 in the environment), they
// are what a person would wait on such a machine with a floppy disk.
// These are the times to measure before and after a change to how the
// picture is read, dithered or shown.  The program gives up after SECONDS,
// or after 600 seconds if SECONDS is not given.  The copy of the disk that
// the machine is given is kept in the directory OUT.
//
// The screen is searched for the prompt the way the desktop's checks read
// the screen (test/deskcheck.h), and only after the console has said the
// desktop is up, as in guitest.  Until the kernel has told QEMU's display
// its size, QEMU's screen has a size of its own, and the prompt cannot be
// on the screen before the desktop is.  The number of rows of the picture
// that are made is read from the machine's memory, which is in QEMU's
// memory at the kernel's symbol `image` (symbols.h).  `vw` in view.fs
// holds the viewer in use, and the third cell of a viewer's record is the
// number of rows of its bitmap that are made (`bmh`).  If either of those
// changes, this program must change too.  It stops with a message if the
// image's names have no `vw`.
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

#define ROWS_MADE 2             // the offset of bmh in a viewer's record, in cells (view.fs)

static struct timespec started;

// Returns the seconds since the machine was started.
static double now(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return((double)(t.tv_sec - started.tv_sec) + (t.tv_nsec - started.tv_nsec) / 1e9);
}

enum { UP, PROMPT, FIRST, HALF, ALL, TIMES };
static double when[TIMES];      // the time each event first happened, or -1 if it has not happened

// Records the time of an event, the first time it happens.
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
    uint64_t memory = symbol_at(argv[2], "image");      // the address of the machine's cell 0 in QEMU's memory
    if (!vw) {
        fprintf(stderr, "%s has no label vw: view.fs has changed, and this must change with it\n", argv[7]);
        return(1);
    }
    tables_are(argv[6]);
    desk watched = { true, argv[5], argv[8] };
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
    char half[sizeof(half_is) + 3 * sizeof(int)];       // room for the text and the number: an int prints as at most three digits for each of its bytes
    snprintf(half, sizeof(half), half_is, height);
    const char* what[TIMES] = { "the console says the desktop is up", "the Forth window has its prompt",
                                "the first row of the picture is made", half, "the picture is finished" };
    for(int n=0; n < TIMES; n++) {
        if (when[n] >= 0) printf("%-38s %6.1f s\n", what[n], when[n]);
        else printf("%-38s not in %.0f s\n", what[n], limit);
    }
    return(when[ALL] >= 0 && when[PROMPT] >= 0 ? 0 : 1);
}
