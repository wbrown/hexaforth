//
// cut.h - an image's code cut into straight-line runs and the
// instructions that leave a run, for the tools that have this in them
// (bench/runs.c, test/builds_test.c).  What a field of an instruction is,
// and where a jump goes, is the machine's to say (vm_instruction.h).
//
// A run is instructions of the ALU and numbers, one after another.  It
// ends at a return, at a write to a port, and before anything a branch
// lands on or a word starts at.  What leaves a run is a call, a jump or a
// branch, and each has a run before it, which is of no instructions if
// there are none between it and what is before.  A call to a word on another page is the word's place pushed and
// a call to execute, and which calls are such depends on where the pages
// are: here it is one call, to that word.
//
// Code is the pages that words are in, each up to its last instruction;
// the nucleus's tables, which are instructions laid down after its code,
// are not code (words.h).
//
// An image is as long as it is, and has as many words and runs as it has:
// what is said of it here has room for all of it (room.h).
//

#ifndef HEXAFORTH_CUT_H
#define HEXAFORTH_CUT_H

#include "image.h"
#include "words.h"

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
    word* defined;                      // its words that have code, in the order the source defines them (its listing),
    long ndefined;                      // and how many
    item* items;                        // its runs and what leaves them, in order,
    long count, room;                   // how many, and how many there is room for
    unsigned char* lands;               // of each instruction: a branch lands here, or (2) a word starts here
    long pages;                         // the pages the image is,
    unsigned char* has_code;            // and of each, whether a word is in it
    long* item_of;                      // of each instruction: the item that starts there, or less than nothing (part)
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
    m->ndefined = listing_of(hex, &m->defined);
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
        // What leaves a run has a run before it, if of nothing: one build may
        // make nothing of a run that the other has (`8 3 rshift *` is nothing
        // where a cell is eight bytes), and the two are still cut alike.
        add_item(m, 1, run ? start : at, run);
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
                    before->len = k - 1;
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

// The word an item is in: which of the image's words that have code, or
// less than nothing if it is in none, as the two instructions an image
// starts with are.
static int owner_of(const cut_image* m, long at) {
    int lo = 0, hi = m->n;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (m->words[mid].at <= at) lo = mid + 1; else hi = mid;
    }
    for(int idx=lo - 1; idx >= 0 && m->words[idx].at + PAGE > at; idx--) {
        if (m->words[idx].size) return(at < m->words[idx].at + m->words[idx].size ? idx : -1);
    }
    return(-1);
}

// Which of the words of its name a word is, of those that have code, in the
// order the source defines them, which the image's listing has: a name may
// be given to more than one word, and two builds of a source do not lay
// them in the same order of place (one of them may be in the kernel's
// upper half, or on a page that is gone back to).  A word the listing has
// not is the only one of its name; if it is not, nothing says which it is,
// and the image is not cut.
static int rank_of(const cut_image* m, int which) {
    const word* is = &m->words[which];
    int rank = 0, named = 0;
    for(long idx=0; idx < m->ndefined; idx++) {
        if (strcasecmp(m->defined[idx].name, is->name)) continue;
        if (m->defined[idx].at == is->at) return(rank);
        rank++;
    }
    for(int idx=0; idx < m->n; idx++) named += m->words[idx].size && !strcmp(m->words[idx].name, is->name);
    if (named > 1) {
        fprintf(stderr, "%d words are named %s, and the image's listing (.lst) has not the one at %lx: "
                        "nothing says which of them it is\n", named, is->name, is->at);
        exit(EXIT_FAILURE);
    }
    return(0);
}

// Every instruction's item, if an item starts there: m->item_of.
static void index_items(cut_image* m) {
    free(m->item_of);
    m->item_of = room_for(m->instructions + 1, sizeof(long));
    for(long at=0; at <= m->instructions; at++) m->item_of[at] = -1;
    for(long idx=m->count - 1; idx >= 0; idx--) if (m->items[idx].at <= m->instructions) m->item_of[m->items[idx].at] = idx;
}

// The item of an image that starts at a place, or less than nothing.
static long item_at(const cut_image* m, long at) {
    return(at >= 0 && at <= m->instructions && m->item_of ? m->item_of[at] : -1);
}

// Are two images the same words, and in each word the same runs and the
// same instructions that leave them, in the same order?  Where they stop
// being, as an item of b, or less than nothing.
//
// Two builds of one source have their words in the order they are written
// only if neither goes back to a page: the image that is run does, where
// a list says so (cross.fs, code-page), and the one built with the
// peephole off never does.  So the words are paired by name, the first of
// a name by place with the first, and a's items are put in the order of
// b's: after this the same item of each is the same thing, as whoever
// asks takes it to be, and item_at finds an item of either by its place.
static long part(cut_image* a, const cut_image* b_as_it_is) {
    cut_image* b = (cut_image*)b_as_it_is;
    index_items(a);
    index_items(b);
    // where each of a's words' items start, and how many: they are one
    // after another, the items being in order of place; and the same of
    // those that are in no word, which are at the end of the list
    long* first = room_for(a->n + 2, sizeof(long));
    long* many = room_for(a->n + 2, sizeof(long));
    for(long idx=0; idx < a->count; idx++) {
        int of = owner_of(a, a->items[idx].at);
        if (of < 0) of = a->n;
        if (!many[of]++) first[of] = idx;
    }
    // b's words by a's: the word of the same name, and the same one of them
    int* same = room_for(b->n + 2, sizeof(int));
    int* a_rank = room_for(a->n + 1, sizeof(int));
    for(int idx=0; idx < a->n; idx++) a_rank[idx] = a->words[idx].size ? rank_of(a, idx) : -1;
    for(int idx=0; idx < b->n; idx++) {
        same[idx] = -1;
        if (!b->words[idx].size) continue;
        int rank = rank_of(b, idx);
        for(int k=0; k < a->n && same[idx] < 0; k++) {
            if (a_rank[k] == rank && !strcmp(a->words[k].name, b->words[idx].name)) same[idx] = k;
        }
    }
    same[b->n] = a->n;
    item* paired = room_for(b->count + 1, sizeof(item));
    long* taken = room_for(b->n + 2, sizeof(long));     // how many of each of b's words' items have been paired
    long parted = a->count == b->count ? -1 : (a->count < b->count ? a->count : b->count) - 1;
    for(long idx=0; idx < b->count && parted < 0; idx++) {
        int of = owner_of(b, b->items[idx].at);
        if (of < 0) of = b->n;
        int in_a = same[of];
        long k = taken[of]++;
        if (in_a < 0 || k >= many[in_a] || a->items[first[in_a] + k].run != b->items[idx].run) { parted = idx; break; }
        paired[idx] = a->items[first[in_a] + k];
    }
    if (parted < 0) {
        memcpy(a->items, paired, b->count * sizeof(item));
        index_items(a);
    } else if (parted >= a->count) {
        parted = a->count ? a->count - 1 : 0;           // an item both have, for whoever says where
    }
    free(first); free(many); free(same); free(a_rank); free(paired); free(taken);
    return(parted);
}

#endif // HEXAFORTH_CUT_H
