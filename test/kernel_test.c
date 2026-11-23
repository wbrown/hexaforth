//
// kernel_test IMAGE.hex IMAGE.tables [typed | to NAME...]
//
// Checks that the kernel contains no word that nothing reaches.
//
// A kernel word with no header cannot be typed, so it exists only for the
// code that calls it.  The same is true of a word whose header is shadowed
// by a newer header of the same name, because a search finds the newest:
// the kernel's `;` was such a word, since join.fs has its own.  This
// program finds what reaches each word of the image (reach.h).  If nothing
// reaches a word that cannot be typed, the kernel's ROM is carrying
// instructions that are never used.  The test then fails and names the
// word.  So a word has to be removed when its last caller is.
//
// A word with a header that nothing calls is not an error, because it can
// be typed: the kernel has it for use at the prompt.  Such words are
// counted.  With `typed` they are also listed, each with its length in
// instructions, for anyone looking at what the kernel could do without
// (CLAUDE.md, Making the kernel smaller).
//
// With `to` and some word names, it prints instead what reaches each of
// those words from anywhere in the image: every call, jump and branch,
// with the word it is in and its address, and then the other ways the
// word is reached.  Use it before a word is made shorter, moved or
// removed.  It fails if the image has no word with one of the names.
//
// The kernel is the contents of its ROM: the words that start in the first
// KROM_BYTES of the image.  It prints how many words it examined, and
// fails if there were none, because then it checked nothing.
//

#include "../vm.c"
#include "../util/cut.h"
#include "../util/reach.h"

static cut_image IMAGE;
static reached* HOW;                    // how each of the image's words is reached

// Prints what reaches each word that has a given name, and returns how
// many words have it.  A word can have more than one name (`1+` and
// `cell+`, nuc.fs), and any of its names selects it.
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
