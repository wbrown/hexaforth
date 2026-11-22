//
// kernel_test IMAGE.hex IMAGE.tables [typed | to NAME...]
//
// The kernel has nothing in it that nothing comes to.
//
// A word of the kernel that has no header cannot be typed: it is there for
// what calls it, and for nothing else.  Nor can one whose header a newer
// header of the same name hides, since a search finds the newest: the
// kernel's `;` was such a word, join.fs having its own.  What reaches each
// word is asked of the image (reach.h).  If nothing reaches a word that
// cannot be typed, it is instructions the kernel's ROM carries for nothing:
// this fails, and says which.  So a word is taken out when the last thing
// that called it is.
//
// A word with a header that nothing calls is not wrong, since it can be
// typed: it is what the kernel has for the prompt alone.  Those are
// counted, and with `typed` they are printed, each with how many
// instructions it is, for whoever is asking what the kernel could be
// without (CLAUDE.md, Making the kernel smaller).
//
// With `to` and the names of words it says instead what comes to each, of
// the whole image: every call, jump and branch, by the word it is in and
// its place, and then what else reaches the word.  It is what is asked
// before a word is made shorter, moved or taken out, and it fails if the
// image has no word of a name.
//
// The kernel is what is in its ROM: the words that start in the first
// KROM_BYTES of the image.  It says how many it looked at, and fails if
// that is none: then it looked at nothing.
//

#include "../vm.c"
#include "../util/cut.h"
#include "../util/reach.h"

static cut_image IMAGE;
static reached* HOW;                    // of each of its words

// What comes to the words of a name: how many words have the name.  A
// word may have more names than one (`1+` and `cell+`, nuc.fs), and is
// found by any of them.
static int comes(const char* name) {
    int named = 0;
    for(int said=0; said < IMAGE.n; said++) {
        if (strcasecmp(IMAGE.words[said].name, name)) continue;
        int idx = starting_at(&IMAGE, IMAGE.words[said].at);
        if (idx < 0) continue;
        const word* w = &IMAGE.words[idx];
        named++;
        printf("%s, at %ld, %ld instructions:\n", name, w->at, w->size);
        for(long k=0; k < IMAGE.count; k++) {
            const item* it = &IMAGE.items[k];
            if (comes_to(&IMAGE, it) != idx) continue;
            long last = it->at + it->len - 1;
            int in = holding(&IMAGE, last);
            instruction ins = ins_of(IMAGE.code[last]);
            printf("     %-8s from %-20s at %ld\n",
                   ins.jmp.op_type == OP_TYPE_CALL ? "a call" : ins.jmp.op_type == OP_TYPE_CJMP ? "a branch" : "a jump",
                   in >= 0 ? IMAGE.words[in].name : "no word", last);
        }
        const reached* how = &HOW[idx];
        printf("     %ld calls, jumps and branches from the kernel's page and %ld from the others; "
               "its place pushed %ld times there and %ld in the others, and in %ld cells of data;%s%s\n",
               how->calls[0], how->calls[1], how->pushed[0], how->pushed[1], how->cells,
               how->into ? " the word before runs on into it;" : "",
               how->typed ? " it has a header that is found" : " it has no header that is found");
    }
    if (!named) printf("FAIL %s: the image has no word of that name that has code\n", name);
    return(named);
}

int main(int argc, char *argv[]) {
    int to = argc > 4 && !strcmp(argv[3], "to");
    if (argc < 3 || (!to && (argc > 4 || (argc == 4 && strcmp(argv[3], "typed"))))) {
        fprintf(stderr, "usage: kernel_test IMAGE.hex IMAGE.tables [typed | to NAME...]\n");
        return(1);
    }
    cut(&IMAGE, argv[1], argv[2]);
    HOW = room_for(IMAGE.n, sizeof(reached));
    reach(&IMAGE, HOW);

    if (to) {
        int missing = 0;
        for(int arg=4; arg < argc; arg++) missing += !comes(argv[arg]);
        return(missing != 0);
    }

    long words = 0, instructions = 0, dead = 0, typed = 0, typed_instructions = 0;
    for(int idx=0; idx < IMAGE.n; idx++) {
        const word* w = &IMAGE.words[idx];
        if (!w->size || w->at >= KROM_BYTES / (long)sizeof(instruction)) continue;
        words++;
        instructions += w->size;
        if (is_reached(&HOW[idx])) continue;
        if (!HOW[idx].typed) {
            dead++;
            printf("FAIL %s, at %ld: %ld instructions that nothing comes to, and it has no header that is found\n",
                   w->name, w->at, w->size);
        } else {
            typed++;
            typed_instructions += w->size;
            if (argc == 4) printf("     %-20s %4ld\n", w->name, w->size);
        }
    }
    printf("the kernel, cells of %d bits: %ld words, %ld instructions; %ld with no header that is found that nothing comes to; "
           "%ld with one that are there only to be typed, %ld instructions\n",
           CELL_BITS, words, instructions, dead, typed, typed_instructions);
    return(dead || !words);
}
