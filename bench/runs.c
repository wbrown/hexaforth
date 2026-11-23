//
// runs PLAIN.hex PLAIN.tables IMAGE.hex IMAGE.tables
//
// Prints the system's code as it was written, beside what cross.fs
// compiled it to.
//
// An image built with the peephole off (PLAIN) has the words as they were
// written, one instruction for each, with the same calls inlined as in the
// image built with the peephole on (cross.fs, the .inline file).  Both
// images are cut into straight-line runs and the instructions that leave a
// run (cut.h defines these).  The two images must have the same runs and
// the same calls in the same order, and the program stops if they do not.
//
// It prints one line for each run: the word the run is in and its address
// in IMAGE (word@address), the run's instructions in PLAIN, a bar, and its
// instructions in IMAGE.
//

#include "../vm.c"
#include "../util/cut.h"

static cut_image PLAIN, MADE;

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "usage: runs PLAIN.hex PLAIN.tables IMAGE.hex IMAGE.tables\n");
        return(1);
    }
    cut(&PLAIN, argv[1], argv[2]);
    cut(&MADE, argv[3], argv[4]);
    long parted = part(&PLAIN, &MADE);
    if (parted >= 0) {
        fprintf(stderr, "runs: the two images are not the same runs and calls in the same order: %ld and %ld, and they part at %lx and %lx\n",
                PLAIN.count, MADE.count, PLAIN.items[parted].at, MADE.items[parted].at);
        return(1);
    }
    long runs = 0;
    for(long idx=0; idx < MADE.count; idx++) {
        if (!MADE.items[idx].run) continue;
        runs++;
        printf("%s@%lx", word_at(MADE.words, MADE.n, MADE.items[idx].at), MADE.items[idx].at);
        for(long j=0; j < PLAIN.items[idx].len; j++) printf(" %04x", PLAIN.code[PLAIN.items[idx].at + j]);
        printf(" |");
        for(long j=0; j < MADE.items[idx].len; j++) printf(" %04x", MADE.code[MADE.items[idx].at + j]);
        printf("\n");
    }
    if (!runs) {
        fprintf(stderr, "runs: no runs were found\n");
        return(1);
    }
    return(0);
}
