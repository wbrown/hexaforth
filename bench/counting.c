//
// counting IMAGE.hex [DISK.img [MEMORY.out]]
//
// The reference VM, counting how often the instruction at each place is
// run.  It is main.c and vm.c as they are, with what vm.c tells of an
// instruction about to be run and of a character written (RUNS, SAYS)
// taken up here.
//
// When the machine stops, the counts are written to the file that the
// environment's RUN_COUNTS names: a count of 64 bits for each of the first
// PLACES instructions of memory, which is where code is.
//
// A test that looks at the screen has the machine print every cell of it
// as a number, between a [ and a ] (test/desktop_test.py).  That is the
// test's work and not the machine's, and it is most of what is run.  With
// SCREEN_NOT_COUNTED in the environment nothing is counted between a [
// written to the console and the ] after it.
//

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define PLACES 0x20000
static uint64_t RAN[PLACES];
static int counted = 1;             // not so while the screen is being printed
static int screen_not_counted;

static void says(int c) {
    if (!screen_not_counted) return;
    if (c == '[') counted = 0;
    if (c == ']') counted = 1;
}

#define RUNS(at)    (RAN[(at) & (PLACES - 1)] += counted)
#define SAYS(c)     says(c)
#include "../vm.c"

#define main run_image
#include "../main.c"
#undef main

int main(int argc, char *argv[]) {
    screen_not_counted = getenv("SCREEN_NOT_COUNTED") != NULL;
    run_image(argc, argv);
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
    return(0);
}
