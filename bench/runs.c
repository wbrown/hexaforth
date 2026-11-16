//
// runs PLAIN.hex PLAIN.tables IMAGE.hex IMAGE.tables
//
// The system's code as it is typed, beside what cross.fs makes of it.
//
// An image built with the peephole off (PLAIN) is the words as they were
// typed, an instruction for each, with the same calls compiled in place as
// the image built with it on (cross.fs, the .inline file).  Both are cut
// here into straight-line runs and the instructions that leave a run: a
// call, a jump, a branch.  A run ends at a return, at a write to a port,
// and before anything a branch lands on.  The two images are the same
// runs and the same calls in the same order, and this stops if they are
// not.
//
// Prints a line a run: the word it is in and where it is in IMAGE
// (word@place), the run's instructions in PLAIN, a bar, and its
// instructions in IMAGE.
//
// What a field of an instruction is, and where a jump goes, is vm.c's and
// vm_instruction.h's: nothing here says it again.  A call to a word on
// another page is the word's place pushed and a call to execute, and which
// calls are such depends on where the pages are: so here it is one call,
// in both images.
//

#include "../vm.c"
#include "../util/image.h"
#include "../util/words.c"

#define WORDS 8192
#define ITEMS (1 << 17)
#define PLACES (1 << 17)

typedef struct {
    char run;                           // a run, or an instruction that leaves one
    long at, len;
} item;

typedef struct {
    uint16_t* code;
    long instructions;
    word words[WORDS];
    int n;
    item items[ITEMS];
    long count;
    unsigned char lands[PLACES];        // a branch lands here, or (2) a word starts here
} cut_image;

static cut_image PLAIN, MADE;

static instruction ins_of(uint16_t w) { return(*(instruction*)&w); }
static int in_a_run(uint16_t w) { return(ins_of(w).lit.lit_f || ins_of(w).alu.op_type == OP_TYPE_ALU); }
static int ends_a_run(uint16_t w) {
    instruction ins = ins_of(w);
    return(!ins.lit.lit_f && (ins.alu.r_eip || ins.alu.out_mux == OUTPUT_IO_T));
}
static long goes_to(long at, uint16_t w) { return(branch(at + 1, ins_of(w).jmp.target)); }

// What a number pushed is: a push, and what is added to it after.
static int pushes_number(uint16_t w, int adds) {
    instruction ins = ins_of(w);
    return(ins.lit.lit_f && ins.lit.lit_shifts != 3 && ins.lit.lit_add == adds);
}
static long number_in(uint16_t w) { return((long)ins_of(w).lit.lit_v << (ins_of(w).lit.lit_shifts * LIT_BITS)); }

static void add(cut_image* m, int run, long at, long len) {
    if (m->count == ITEMS) { fprintf(stderr, "runs: too many runs\n"); exit(1); }
    m->items[m->count].run = run; m->items[m->count].at = at; m->items[m->count].len = len;
    m->count++;
}

static void cut(cut_image* m, const char* hex, const char* tables) {
    uint8_t* bytes;
    size_t len = load_image(hex, &bytes);
    m->code = (uint16_t*)bytes;
    m->instructions = len / 2 < PLACES ? len / 2 : PLACES;
    m->n = words_of(bytes, len, tables, m->words, WORDS);
    long execute = place_of(m->words, m->n, "execute");

    without_tables(bytes, len, m->words, m->n);     // the nucleus's tables are not code

    // Code is the pages that words are in, each up to its last instruction.
    static long last[PLACES / PAGE];
    static unsigned char has_code[PLACES / PAGE];
    memset(has_code, 0, sizeof(has_code));
    for(int idx=0; idx < m->n; idx++) {
        if (m->words[idx].at >= m->instructions) continue;
        has_code[m->words[idx].at / PAGE] = 1;
        m->lands[m->words[idx].at] = 2;
    }
    for(long page=0; page < PLACES / PAGE; page++) last[page] = -1;
    for(long at=0; at < m->instructions; at++) {
        uint16_t w = m->code[at];
        if (!has_code[at / PAGE] || !w) continue;
        last[at / PAGE] = at;
        if (!in_a_run(w) && goes_to(at, w) < PLACES && !m->lands[goes_to(at, w)]) m->lands[goes_to(at, w)] = 1;
    }

    long start = 0, run = 0;
    for(long at=0; at < m->instructions; at++) {
        uint16_t w = m->code[at];
        if (at > last[at / PAGE]) {
            if (run) add(m, 1, start, run);
            run = 0;
            continue;
        }
        if (run && m->lands[at]) { add(m, 1, start, run); run = 0; }
        if (in_a_run(w)) {
            if (!run) start = at;
            run++;
            if (ends_a_run(w)) { add(m, 1, start, run); run = 0; }
            continue;
        }
        if (run) add(m, 1, start, run);
        run = 0;
        // a word's place pushed, and a call or a jump to execute, is a call of that word
        instruction ins = ins_of(w);
        item* before = m->count ? &m->items[m->count - 1] : NULL;
        if (ins.jmp.op_type != OP_TYPE_CJMP && goes_to(at, w) == execute && before && before->run && before->at + before->len == at) {
            const uint16_t* c = m->code + before->at;
            long k = before->len;
            while (k && pushes_number(c[k - 1], 1)) k--;
            if (k && pushes_number(c[k - 1], 0)) {
                long pushed = 0;
                for(long j=k - 1; j < before->len; j++) pushed += number_in(c[j]);
                if (pushed < PLACES && m->lands[pushed] == 2) {
                    if (k - 1) before->len = k - 1; else m->count--;
                }
            }
        }
        add(m, 0, at, 1);
    }
    if (run) add(m, 1, start, run);
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "usage: runs PLAIN.hex PLAIN.tables IMAGE.hex IMAGE.tables\n");
        return(1);
    }
    cut(&PLAIN, argv[1], argv[2]);
    cut(&MADE, argv[3], argv[4]);
    long idx = 0;
    while (idx < PLAIN.count && idx < MADE.count && PLAIN.items[idx].run == MADE.items[idx].run) idx++;
    if (idx != PLAIN.count || idx != MADE.count) {
        fprintf(stderr, "runs: the two images are not the same runs and calls in the same order: %ld and %ld, and they part at %lx and %lx\n",
                PLAIN.count, MADE.count, PLAIN.items[idx].at, MADE.items[idx].at);
        return(1);
    }
    long runs = 0;
    for(idx=0; idx < MADE.count; idx++) {
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
