//
// cut.h - cuts an image's code into straight-line runs and the
// instructions that leave a run, for the tools that include it
// (bench/runs.c, test/builds_test.c).  The meaning of an instruction's
// fields, and the target of a jump, come from the machine's own
// definitions (vm_instruction.h).
//
// A run is a sequence of ALU instructions and literals.  It ends at a
// return, at a write to a port, and before any instruction that a branch
// targets or that starts a word.  The instructions that leave a run are
// calls, jumps and branches.  Each of those has a run before it, which is
// empty if no instructions separate it from the item before.  A call to a
// word on another page is compiled as a push of the word's address
// followed by a call of `execute`.  Which calls take that form depends on
// where the pages are, so here such a pair is one item: a call to that
// word.
//
// Code is the pages that contain words, each up to its last instruction.
//
// Nothing here has a fixed limit: the lists grow to fit the image
// (room.h).
//

#ifndef HEXAFORTH_CUT_H
#define HEXAFORTH_CUT_H

#include "image.h"
#include "words.h"

typedef struct {
    char run;                           // 1 for a run, 0 for an instruction that leaves one
    long at, len;
    long to;                            // the target of an instruction that leaves a run
} item;

typedef struct {
    uint16_t* code;
    long instructions;                  // the number of instructions in the image
    word* words;                        // its words, sorted by address
    int n;                              // the number of words
    word* defined;                      // its words that have code, in the order the source defines them (its listing)
    long ndefined;                      // the number of those
    item* items;                        // its runs and the instructions that leave them, in order
    long count, room;                   // the number of items, and the list's capacity
    unsigned char* lands;               // for each instruction: 1 if a branch targets it, 2 if a word starts there
    long pages;                         // the number of pages in the image
    unsigned char* has_code;            // for each page: 1 if a word is on it
    long* item_of;                      // for each instruction: the index of the item that starts there, or -1 (set by part())
} cut_image;

static instruction ins_of(uint16_t w) { return(*(instruction*)&w); }
static int in_a_run(uint16_t w) { return(ins_of(w).lit.lit_f || ins_of(w).alu.op_type == OP_TYPE_ALU); }
static int ends_a_run(uint16_t w) {
    instruction ins = ins_of(w);
    return(!ins.lit.lit_f && (ins.alu.r_eip || ins.alu.out_mux == OUTPUT_IO_T));
}
static long goes_to(long at, uint16_t w) { return(branch(at + 1, ins_of(w).jmp.target)); }

// A pushed number is a push literal followed by add literals.
// pushes_number() tests for the push (`adds` 0) or for an add (`adds` 1),
// and number_in() returns a literal's value.
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

// Returns 1 if a word starts at an address in the image.
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

    m->pages = m->instructions / PAGE + 1;
    m->has_code = room_for(m->pages, 1);
    m->lands = room_for(m->instructions + 1, 1);
    long* last = room_for(m->pages, sizeof(long));      // the address of the last instruction on each page
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

    // A zero that is in no word is not code.  A page can have unused space
    // between its words, as the kernel's page has between the end of the
    // ROM's code and the code in its upper half.  A zero instruction is a
    // jump to address 0, so without this check every unused instruction
    // would be counted as a jump, and two builds of one source would
    // differ by as many items as the gap is long.
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
        // An instruction that leaves a run always gets a run before it, even
        // an empty one.  One build may reduce a run to no instructions where
        // the other keeps it (`8 3 rshift *` compiles to nothing when a cell
        // is eight bytes), and the two builds must still be cut alike.
        add_item(m, 1, run ? start : at, run);
        run = 0;
        // A push of a word's address followed by a call or a jump to
        // `execute` is a call of that word.  The push is part of the item,
        // which starts where the push starts.
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

// Returns the index of the word that contains an address, among the
// image's words that have code, or -1 if no word does.  The two
// instructions an image starts with are in no word.
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

// Returns a word's rank among the words with its name that have code: 0
// for the first the source defines, 1 for the second, and so on.  The
// order of definition comes from the image's listing.  It is needed
// because a name can belong to more than one word, and two builds of a
// source do not put those words in the same order in memory (one may be
// in the kernel's upper half, or on a page that code returns to later).
// A word missing from the listing must be the only word with its name.
// If it is not, nothing identifies it, and the program exits.
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

// Fills m->item_of: for each instruction, the index of the item that
// starts there, or -1.
static void index_items(cut_image* m) {
    free(m->item_of);
    m->item_of = room_for(m->instructions + 1, sizeof(long));
    for(long at=0; at <= m->instructions; at++) m->item_of[at] = -1;
    for(long idx=m->count - 1; idx >= 0; idx--) if (m->items[idx].at <= m->instructions) m->item_of[m->items[idx].at] = idx;
}

// Returns the index of the item that starts at an address, or -1.
static long item_at(const cut_image* m, long at) {
    return(at >= 0 && at <= m->instructions && m->item_of ? m->item_of[at] : -1);
}

// Checks that two images have the same words, and in each word the same
// sequence of runs and of instructions that leave them.  Returns -1 if
// they match, or the index of the item of `b` where they first differ.
//
// Two builds of one source have their words in source order only if
// neither returns to an earlier page.  The image that is run does return
// to pages, where a list of files says so (cross.fs, code-page), and the
// image built with the peephole off never does.  So the words are paired
// by name and rank (rank_of()), and a's items are reordered to match b's.
// After this, item i of each image is the same thing, which callers rely
// on, and item_at() finds an item of either image by its address.
static long part(cut_image* a, const cut_image* b_as_it_is) {
    cut_image* b = (cut_image*)b_as_it_is;
    index_items(a);
    index_items(b);
    // For each of a's words, the index of its first item and the number
    // of its items.  A word's items are consecutive, because the items are
    // in address order.  The items that are in no word are counted under
    // the extra index a->n.
    long* first = room_for(a->n + 2, sizeof(long));
    long* many = room_for(a->n + 2, sizeof(long));
    for(long idx=0; idx < a->count; idx++) {
        int of = owner_of(a, a->items[idx].at);
        if (of < 0) of = a->n;
        if (!many[of]++) first[of] = idx;
    }
    // For each of b's words, the index of a's word with the same name and
    // the same rank.
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
    long* taken = room_for(b->n + 2, sizeof(long));     // for each of b's words, the number of its items paired so far
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
        parted = a->count ? a->count - 1 : 0;           // report an item that both images have
    }
    free(first); free(many); free(same); free(a_rank); free(paired); free(taken);
    return(parted);
}

#endif // HEXAFORTH_CUT_H
