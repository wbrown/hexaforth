//
// kernel_test IMAGE.hex IMAGE.tables [typed]
//
// The kernel has nothing in it that nothing comes to.
//
// A word of the kernel that has no header cannot be typed: it is there for
// what calls it, and for nothing else.  What reaches each word is asked of
// the image (reach.c).  If nothing reaches a word that has no header, it is
// instructions the kernel's ROM carries for nothing: this fails, and says
// which.  So a word is taken out when the last thing that called it is.
//
// A word with a header that nothing calls is not wrong, since it can be
// typed: it is what the kernel has for the prompt alone.  Those are
// counted, and with `typed` they are printed, each with how many
// instructions it is, for whoever is asking what the kernel could be
// without (CLAUDE.md, Making the kernel smaller).
//
// The kernel is what is in its ROM: the words that start in the first
// KROM_BYTES of the image.  It says how many it looked at, and fails if
// that is none: then it looked at nothing.
//

#include "../vm.c"
#include "../util/cut.c"
#include "../util/reach.c"

static cut_image IMAGE;
static reached* HOW;                    // of each of its words

int main(int argc, char *argv[]) {
    if (argc < 3 || argc > 4 || (argc == 4 && strcmp(argv[3], "typed"))) {
        fprintf(stderr, "usage: kernel_test IMAGE.hex IMAGE.tables [typed]\n");
        return(1);
    }
    cut(&IMAGE, argv[1], argv[2]);
    HOW = room_for(IMAGE.n, sizeof(reached));
    reach(&IMAGE, HOW);

    long words = 0, instructions = 0, dead = 0, typed = 0, typed_instructions = 0;
    for(int idx=0; idx < IMAGE.n; idx++) {
        const word* w = &IMAGE.words[idx];
        if (!w->size || w->at >= KROM_BYTES / (long)sizeof(instruction)) continue;
        words++;
        instructions += w->size;
        if (is_reached(&HOW[idx])) continue;
        if (!HOW[idx].typed) {
            dead++;
            printf("FAIL %s, at %ld: %ld instructions that nothing comes to, and it has no header\n",
                   w->name, w->at, w->size);
        } else {
            typed++;
            typed_instructions += w->size;
            if (argc == 4) printf("     %-20s %4ld\n", w->name, w->size);
        }
    }
    printf("the kernel, cells of %d bits: %ld words, %ld instructions; %ld without a header that nothing comes to; "
           "%ld with one that are there only to be typed, %ld instructions\n",
           CELL_BITS, words, instructions, dead, typed, typed_instructions);
    return(dead || !words);
}
