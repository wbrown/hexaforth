//
// cut.c - an image's code cut into straight-line runs and the
// instructions that leave a run, for the tools that have this in them
// (bench/runs.c, test/builds_test.c).  It is had after vm.c, whose it is
// to say what a field of an instruction is and where a jump goes.
//
// A run is instructions of the ALU and numbers, one after another.  It
// ends at a return, at a write to a port, and before anything a branch
// lands on or a word starts at.  What leaves a run is a call, a jump or a
// branch.  A call to a word on another page is the word's place pushed and
// a call to execute, and which calls are such depends on where the pages
// are: here it is one call, to that word.
//
// Code is the pages that words are in, each up to its last instruction;
// the nucleus's tables, which are instructions laid down after its code,
// are not code (words.c).
//

#include "image.h"
#include "words.c"

#define CUT_WORDS 8192
#define CUT_ITEMS (1 << 17)
#define CUT_PLACES (1 << 17)

typedef struct {
    char run;                           // a run, or an instruction that leaves one
    long at, len;
    long to;                            // where one that leaves a run goes
} item;

typedef struct {
    uint16_t* code;
    long instructions;
    word words[CUT_WORDS];
    int n;
    item items[CUT_ITEMS];
    long count;
    unsigned char lands[CUT_PLACES];        // a branch lands here, or (2) a word starts here
} cut_image;

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

static item* add_item(cut_image* m, int run, long at, long len) {
    if (m->count == CUT_ITEMS) { fprintf(stderr, "there are too many runs\n"); exit(1); }
    item* it = &m->items[m->count++];
    it->run = run; it->at = at; it->len = len; it->to = -1;
    return(it);
}

static void cut(cut_image* m, const char* hex, const char* tables) {
    uint8_t* bytes;
    size_t len = load_image(hex, &bytes);
    m->code = (uint16_t*)bytes;
    m->instructions = len / 2 < CUT_PLACES ? len / 2 : CUT_PLACES;
    m->n = words_of(bytes, len, tables, m->words, CUT_WORDS);
    long execute = place_of(m->words, m->n, "execute");
    without_tables(bytes, len, m->words, m->n);

    static long last[CUT_PLACES / PAGE];
    static unsigned char has_code[CUT_PLACES / PAGE];
    memset(has_code, 0, sizeof(has_code));
    for(int idx=0; idx < m->n; idx++) {
        if (m->words[idx].at >= m->instructions) continue;
        has_code[m->words[idx].at / PAGE] = 1;
        m->lands[m->words[idx].at] = 2;
    }
    for(long page=0; page < CUT_PLACES / PAGE; page++) last[page] = -1;
    for(long at=0; at < m->instructions; at++) {
        uint16_t w = m->code[at];
        if (!has_code[at / PAGE] || !w) continue;
        last[at / PAGE] = at;
        if (!in_a_run(w) && goes_to(at, w) < CUT_PLACES && !m->lands[goes_to(at, w)]) m->lands[goes_to(at, w)] = 1;
    }

    long start = 0, run = 0;
    for(long at=0; at < m->instructions; at++) {
        uint16_t w = m->code[at];
        if (at > last[at / PAGE]) {
            if (run) add_item(m, 1, start, run);
            run = 0;
            continue;
        }
        if (run && m->lands[at]) { add_item(m, 1, start, run); run = 0; }
        if (in_a_run(w)) {
            if (!run) start = at;
            run++;
            if (ends_a_run(w)) { add_item(m, 1, start, run); run = 0; }
            continue;
        }
        if (run) add_item(m, 1, start, run);
        run = 0;
        // a word's place pushed, and a call or a jump to execute, is a call of
        // that word: the push is part of it, and it starts where the push does
        instruction ins = ins_of(w);
        long to = goes_to(at, w), from = at;
        item* before = m->count ? &m->items[m->count - 1] : NULL;
        if (ins.jmp.op_type != OP_TYPE_CJMP && to == execute && before && before->run && before->at + before->len == at) {
            const uint16_t* c = m->code + before->at;
            long k = before->len;
            while (k && pushes_number(c[k - 1], 1)) k--;
            if (k && pushes_number(c[k - 1], 0)) {
                long pushed = 0;
                for(long j=k - 1; j < before->len; j++) pushed += number_in(c[j]);
                if (pushed < CUT_PLACES && m->lands[pushed] == 2) {
                    from = before->at + k - 1;
                    if (k - 1) before->len = k - 1; else m->count--;
                    to = pushed;
                }
            }
        }
        add_item(m, 0, from, at + 1 - from)->to = to;
    }
    if (run) add_item(m, 1, start, run);
}

// Are two images the same runs and the same instructions that leave them,
// in the same order?  Where they stop being, or less than nothing.
static long part(const cut_image* a, const cut_image* b) {
    long idx = 0;
    while (idx < a->count && idx < b->count && a->items[idx].run == b->items[idx].run) idx++;
    return(idx == a->count && idx == b->count ? -1 : idx);
}
