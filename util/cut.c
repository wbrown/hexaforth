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
// An image is as long as it is, and has as many words and runs as it has:
// what is said of it here has room for all of it (room.c).
//

#include "image.h"
#include "words.c"

typedef struct {
    char run;                           // a run, or an instruction that leaves one
    long at, len;
    long to;                            // where one that leaves a run goes
} item;

typedef struct {
    uint16_t* code;
    long instructions;                  // as many as the image has
    word* words;                        // its words, in order of where they are,
    int n;                              // and how many
    item* items;                        // its runs and what leaves them, in order,
    long count, room;                   // how many, and how many there is room for
    unsigned char* lands;               // of each instruction: a branch lands here, or (2) a word starts here
    long pages;                         // the pages the image is,
    unsigned char* has_code;            // and of each, whether a word is in it
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
    ONE_MORE(m->items, m->count, m->room);
    item* it = &m->items[m->count++];
    it->run = run; it->at = at; it->len = len; it->to = -1;
    return(it);
}

// Is a place in the image the start of a word?
static int word_starts(const cut_image* m, long at) {
    return(at >= 0 && at < m->instructions && m->lands[at] == 2);
}

static void cut(cut_image* m, const char* hex, const char* tables) {
    uint8_t* bytes;
    size_t len = load_image(hex, &bytes);
    memset(m, 0, sizeof(cut_image));
    m->code = (uint16_t*)bytes;
    m->instructions = len / sizeof(uint16_t);
    m->n = words_of(bytes, len, tables, &m->words);
    long execute = place_of(m->words, m->n, "execute");
    without_tables(bytes, len, m->words, m->n);

    m->pages = m->instructions / PAGE + 1;
    m->has_code = room_for(m->pages, 1);
    m->lands = room_for(m->instructions + 1, 1);
    long* last = room_for(m->pages, sizeof(long));      // the last instruction each page has
    for(int idx=0; idx < m->n; idx++) {
        if (m->words[idx].at >= m->instructions) continue;
        m->has_code[m->words[idx].at / PAGE] = 1;
        m->lands[m->words[idx].at] = 2;
    }
    for(long page=0; page < m->pages; page++) last[page] = -1;
    for(long at=0; at < m->instructions; at++) {
        uint16_t w = m->code[at];
        if (!m->has_code[at / PAGE] || !w) continue;
        last[at / PAGE] = at;
        long to = in_a_run(w) ? -1 : goes_to(at, w);
        if (to >= 0 && to < m->instructions && !m->lands[to]) m->lands[to] = 1;
    }

    // What is nothing and in no word is not code: a page may have room left
    // between its words, as the kernel's has between the end of its ROM's
    // code and what is laid in its upper half.  (Such room was once taken
    // for as many jumps to the start, and two builds of one source for
    // different, by as many items as the room was instructions.)
    unsigned char* in_word = room_for(m->instructions + 1, 1);
    for(int idx=0; idx < m->n; idx++) {
        const word* is = &m->words[idx];
        for(long at=is->at; at < is->at + is->size && at < m->instructions; at++) in_word[at] = 1;
    }

    long start = 0, run = 0;
    for(long at=0; at < m->instructions; at++) {
        uint16_t w = m->code[at];
        if (at > last[at / PAGE] || (!w && !in_word[at])) {
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
                if (word_starts(m, pushed)) {
                    from = before->at + k - 1;
                    if (k - 1) before->len = k - 1; else m->count--;
                    to = pushed;
                }
            }
        }
        add_item(m, 0, from, at + 1 - from)->to = to;
    }
    if (run) add_item(m, 1, start, run);
    free(last);
    free(in_word);
}

// Are two images the same runs and the same instructions that leave them,
// in the same order?  Where they stop being, or less than nothing.
static long part(const cut_image* a, const cut_image* b) {
    long idx = 0;
    while (idx < a->count && idx < b->count && a->items[idx].run == b->items[idx].run) idx++;
    return(idx == a->count && idx == b->count ? -1 : idx);
}
