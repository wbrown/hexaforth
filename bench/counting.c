//
// counting IMAGE.hex [DISK.img [MEMORY.out]]
//
// The reference VM, counting how often the instruction at each place is
// run.  It is main.c and vm.c as they are, with what vm.c tells of an
// instruction about to be run (RUNS) taken up here.
//
// When the machine stops, the counts are written to the file that the
// environment's RUN_COUNTS names: a count of 64 bits for each place, up
// to one past the highest that was run.  Whoever reads them has how many
// there are from how long the file is.
//
// A test that has the VM in it is built with this in place of vm.c, and
// with COUNTING_A_TEST said: then this is the VM alone, and the test has
// the counts written when it is done (counts_written).
//

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "counting.h"
#include "../util/room.c"

uint64_t* RAN;
long RAN_TO;
static long ran_room;

// The instruction at a place is about to be run: there is a count for it,
// whatever place it is.
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
