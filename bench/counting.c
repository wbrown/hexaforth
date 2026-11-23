//
// counting IMAGE.hex [DISK.img [MEMORY.out]]
//
// The reference VM, with a count of how many times the instruction at
// each address runs.  It is main.c and vm.c unchanged.  vm.c calls
// RUNS(at) before it runs the instruction at an address, and this file
// defines RUNS to count it.
//
// When the machine stops, the counts are written to the file named by
// RUN_COUNTS in the environment: one 64-bit count for each address, up to
// one past the highest address that ran.  A reader gets the number of
// counts from the file's length.  RUN_WORK names a file that receives the
// same counts in the format of a profile's work (work.h), which the tool
// that reports costs reads (costs.c).  In that file everything counts as
// work, and an idle turn runs nothing.
//
// A test that includes the VM can be built with this file in place of
// vm.c, with COUNTING_A_TEST defined.  This file is then only the VM, and
// the test calls counts_written() when it is done.
//

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "counting.h"
#include "../util/room.h"
#include "work.h"

uint64_t* RAN;
long RAN_TO;
static long ran_room;

// Counts the instruction at an address, which is about to run.  The array
// grows to cover any address.
static inline void counted(int64_t at) {
    if (at < 0) return;
    if (at >= RAN_TO) {
        AS_MANY(RAN, at + 1, ran_room);
        RAN_TO = at + 1;
    }
    RAN[at]++;
}

#define RUNS(at)    counted(at)
#include "../vm.c"

void counts_written(void) {
    const char* path = getenv("RUN_COUNTS");
    if (path) {
        FILE* out = fopen(path, "wb");
        if (!out || fwrite(RAN, sizeof(RAN[0]), RAN_TO, out) != (size_t)RAN_TO || fclose(out)) {
            perror(path);
            exit(EXIT_FAILURE);
        }
    }
    path = getenv("RUN_WORK");
    if (path) {
        double* work = room_for(RAN_TO, sizeof(double));
        double* idle = room_for(RAN_TO, sizeof(double));       // all zeros: an idle turn runs nothing
        for(long at=0; at < RAN_TO; at++) work[at] = (double)RAN[at];
        if (!work_written(path, work, idle, RAN_TO)) exit(EXIT_FAILURE);
        free(work);
        free(idle);
    }
}

#ifndef COUNTING_A_TEST
#define main run_image
#include "../main.c"
#undef main

int main(int argc, char *argv[]) {
    run_image(argc, argv);
    counts_written();
    return(0);
}
#endif
