//
// reach.h - finds what reaches each word of an image, for the tools and
// tests that include it.  It includes cut.h, which cuts an image into
// straight-line runs and the instructions that leave a run.
//
// A word can be reached in four ways:
//
//   A call, a jump or a branch in another word targets its first
//   instruction.  A call to a word on another page, which is a push of the
//   word's address and a call of `execute`, counts as a call of that word
//   (cut.h).
//
//   Its address is pushed as a number, which is how an execution token is
//   written.  A number that only happens to equal a word's address is
//   counted too, because nothing here can tell the two apart.  That is why
//   pushes are counted separately from calls.
//
//   Its address is in a cell of data, as a vector's is, or an entry of a
//   table of words.  A cell that only happens to hold that number is
//   counted too.
//
//   The word before it falls through into it.
//
// A word that jumps to its own start, as a loop does, is not reached by
// that jump.  A word with a header can also be typed, whatever else
// reaches it, unless a newer header with the same name shadows it: a
// search finds the newest.  An image's .shadows file lists the names it
// defines more than once.
//

#ifndef HEXAFORTH_REACH_H
#define HEXAFORTH_REACH_H

#include "cut.h"

typedef struct {
    long calls[2], pushed[2];           // [0] from the kernel, [1] from the other pages
    long cells;                         // the number of data cells that hold its address
    int into;                           // 1 if the word before it falls through into it
    int typed;                          // 1 if it has a header that no newer header of the same name shadows
} reached;

// Returns the index of the word that starts at an address, or -1.  Where
// several names share the address, it is the one that has the
// instructions.
static int starting_at(const cut_image* m, long at) {
    int lo = 0, hi = m->n;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (m->words[mid].at < at) lo = mid + 1; else hi = mid;
    }
    return(lo < m->n && m->words[lo].at == at && m->words[lo].size ? lo : -1);
}

// Returns the index of the word that contains the instruction at an
// address, or -1.
static int holding(const cut_image* m, long at) {
    int lo = 0, hi = m->n;              // binary search for the first word that starts after it
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (m->words[mid].at <= at) lo = mid + 1; else hi = mid;
    }
    if (!lo) return(-1);
    int in = starting_at(m, m->words[lo - 1].at);
    return(in >= 0 && at < m->words[in].at + m->words[in].size ? in : -1);
}

// Returns 1 if execution continues from an instruction to the one after
// it.  It does not after a jump, or after an instruction with the return
// in it.
static int runs_on(uint16_t w) {
    instruction ins = ins_of(w);
    if (ins.lit.lit_f) return(1);
    if (ins.jmp.op_type == OP_TYPE_JMP) return(0);
    return(!(ins.alu.op_type == OP_TYPE_ALU && ins.alu.r_eip));
}

// Returns the index of the word that a call, a jump or a branch targets
// from another word.  Returns -1 if the target is not the start of a word,
// or if the instruction is inside the word it targets.
static int comes_to(const cut_image* m, const item* it) {
    int to = !it->run && it->to >= 0 ? starting_at(m, it->to) : -1;
    return(to >= 0 && to != holding(m, it->at) ? to : -1);
}

// Finds what reaches each word of an image.  `how` must have one entry for
// each of the image's words: how[idx] describes word idx.
static void reach(const cut_image* m, reached* how) {
    const uint8_t* bytes = (const uint8_t*)m->code;
    size_t len = (size_t)m->instructions * sizeof(instruction);
    memset(how, 0, m->n * sizeof(reached));

    // Count calls, jumps and branches from other words, and pushed
    // numbers.
    for(long idx=0; idx < m->count; idx++) {
        const item* it = &m->items[idx];
        int far = it->at / PAGE != 0;
        if (!it->run) {
            int to = comes_to(m, it);
            if (to >= 0) how[to].calls[far]++;
            continue;
        }
        const uint16_t* run = m->code + it->at;
        for(long at=0; at < it->len; ) {
            long end = at + 1;
            if (pushes_number(run[at], 0)) {
                while (end < it->len && pushes_number(run[end], 1)) end++;
                long pushed = 0;
                for(long k=at; k < end; k++) pushed += number_in(run[k]);
                int to = pushed > 0 ? starting_at(m, pushed) : -1;
                if (to >= 0) how[to].pushed[far]++;
            }
            at = end;
        }
    }

    // Count data cells that hold a word's address.  Data is every page
    // with no code (cut.h records which pages have code), except the
    // dictionary, where every header holds its word's address.
    const unsigned char* has_code = m->has_code;
    size_t headers_from, headers_to;
    headers_of(bytes, len, &headers_from, &headers_to);
    // A header makes its word reachable by typing, unless a newer header
    // with the same name shadows it.  The dictionary is walked in search
    // order, and a search stops at the first header with a name.
    char (*seen)[NAME_ROOM] = NULL;
    long nseen = 0, seen_room = 0;
    for(uint32_t head = first_header(bytes, len); head; ) {
        uint64_t xt;
        char name[NAME_ROOM];
        int hidden = 0;
        head = read_header(bytes, len, head, &xt, name);
        for(long idx=0; idx < nseen && !hidden; idx++) hidden = !strcmp(seen[idx], name);
        if (hidden) continue;
        ONE_MORE(seen, nseen, seen_room);
        strcpy(seen[nseen++], name);
        int is = xt < (uint64_t)m->instructions ? starting_at(m, (long)xt) : -1;
        if (is >= 0) how[is].typed = 1;
    }
    free(seen);
    for(size_t at=0; at + CELL_BYTES <= len; at += CELL_BYTES) {
        if (has_code[at / sizeof(instruction) / PAGE] || (at >= headers_from && at < headers_to)) continue;
        uint64_t held = cell_of(bytes, at / CELL_BYTES);
        int to = held && held < (uint64_t)m->instructions ? starting_at(m, (long)held) : -1;
        if (to >= 0) how[to].cells++;
    }

    // Record whether the word before each word falls through into it.
    for(int idx=0; idx < m->n; idx++) {
        long at = m->words[idx].at;
        if (!m->words[idx].size || at % PAGE == 0 || at >= m->instructions) continue;
        how[idx].into = holding(m, at - 1) >= 0 && runs_on(m->code[at - 1]);
    }
}

// Returns 1 if anything reaches a word other than typing its name.
static int is_reached(const reached* how) {
    return(how->calls[0] || how->calls[1] || how->pushed[0] || how->pushed[1] || how->cells || how->into);
}

#endif // HEXAFORTH_REACH_H
