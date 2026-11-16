//
// counting IMAGE.hex [DISK.img [MEMORY.out]]
//
// The reference VM, counting how often the instruction at each place is
// run.  It is main.c and vm.c as they are, with what vm.c tells of an
// instruction about to be run (RUNS) taken up here.
//
// When the machine stops, the counts are written to the file that the
// environment's RUN_COUNTS names: a count of 64 bits for each of the first
// PLACES instructions of memory, which is where code is.
//
// A test that has the VM in it is built with this in place of vm.c, and
// with COUNTING_A_TEST said: then this is the VM alone, and the test has
// the counts written when it is done (counts_written).
//

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define PLACES 0x20000
static uint64_t RAN[PLACES];

#define RUNS(at)    (RAN[(at) & (PLACES - 1)]++)
#include "../vm.c"

void counts_written(void) {
    const char* path = getenv("RUN_COUNTS");
    if (path) {
        FILE* out = fopen(path, "wb");
        if (!out) {
            perror(path);
            exit(EXIT_FAILURE);
        }
        fwrite(RAN, sizeof(RAN[0]), PLACES, out);
        fclose(out);
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
