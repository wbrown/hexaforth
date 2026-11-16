//
// words.c - the words of an image, for the tools that have this in them:
// every definition that has a header or a label, where its code starts,
// and how many instructions it is.
//
// A header is in the image (read_header, vm_opcodes.c).  A label is not:
// cross.fs writes the labels to the file of names beside the image
// (build/nuc.tables), after a stamp, a count and the decompiler's two
// tables, and says there how they are laid out (name-tables and (label)).
// A word is as long as it is to the next word of its page, or for the last
// of a page to the last instruction the page has.
//

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../vm_opcodes.h"

#define PAGE 4096                       // instructions to a page of code

typedef struct {
    long at, size;
    char name[160];
} word;

static int by_place(const void* a, const void* b) {
    const word* x = a;
    const word* y = b;
    if (x->at != y->at) return(x->at < y->at ? -1 : 1);
    return(strcmp(x->name, y->name));
}

static uint64_t cell_of(const uint8_t* cells, size_t which) {
    uint64_t cell = 0;
    memcpy(&cell, cells + which * CELL_BYTES, CELL_BYTES);
    return(cell);
}

// The words of an image, in order of where they are: how many there are.
// A second name for the same code has no instructions of its own.
static int words_of(const uint8_t* image, size_t len, const char* tables, word* out, int most) {
    int n = 0;
    for(uint32_t head = newest_header(image, len); head && n < most; ) {
        uint64_t xt;
        head = read_header(image, len, head, &xt, out[n].name);
        out[n++].at = xt;
    }
    FILE* in = tables ? fopen(tables, "rb") : NULL;
    if (in) {
        static uint8_t cells[1 << 20];
        size_t have = fread(cells, CELL_BYTES, sizeof(cells) / CELL_BYTES, in);
        fclose(in);
        size_t at = 2;                                  // past the stamp and the count
        if (have > at) at += 1 + 2 * cell_of(cells, at);        // and the two tables
        if (have > at) at += 1 + 2 * cell_of(cells, at);
        for(uint64_t place = have > at ? cell_of(cells, at) : 0; place && at + place + 2 < have && n < most;
            place = cell_of(cells, at + place)) {
            uint64_t value = cell_of(cells, at + place + 1);
            if (value >> (CELL_BITS - 1)) continue;             // a label for data
            decode_name(cells + (at + place + 2) * CELL_BYTES, (have - at - place - 2) * CELL_BYTES, out[n].name);
            out[n++].at = value;
        }
    }
    qsort(out, n, sizeof(word), by_place);
    const uint16_t* code = (const uint16_t*)image;
    long instructions = len / 2;
    for(int idx=0; idx < n; idx++) {
        long at = out[idx].at, end = (at / PAGE + 1) * PAGE;
        out[idx].size = 0;
        if (idx && out[idx - 1].at == at) continue;
        int next = idx + 1;
        while (next < n && out[next].at == at) next++;
        if (next < n && out[next].at < end) {
            end = out[next].at;
        } else {
            if (end > instructions) end = instructions;
            while (end > at && !code[end - 1]) end--;
        }
        out[idx].size = end - at;
    }
    return(n);
}

// The labels for data that the file of names has: a variable or a table
// that has no header, by its name, with the cell it is of as its place.
// Returns how many there are.
static int data_of(const char* tables, word* out, int most) {
    static uint8_t cells[1 << 20];
    FILE* in = fopen(tables, "rb");
    if (!in) return(0);
    size_t have = fread(cells, CELL_BYTES, sizeof(cells) / CELL_BYTES, in);
    fclose(in);
    size_t at = 2;                                      // past the stamp and the count
    if (have > at) at += 1 + 2 * cell_of(cells, at);    // and the two tables
    if (have > at) at += 1 + 2 * cell_of(cells, at);
    int n = 0;
    for(uint64_t place = have > at ? cell_of(cells, at) : 0; place && at + place + 2 < have && n < most;
        place = cell_of(cells, at + place)) {
        uint64_t value = cell_of(cells, at + place + 1);
        if (!(value >> (CELL_BITS - 1))) continue;      // a label for code
        decode_name(cells + (at + place + 2) * CELL_BYTES, (have - at - place - 2) * CELL_BYTES, out[n].name);
        out[n].at = (long)(value & ~(1ull << (CELL_BITS - 1)));
        out[n++].size = 0;
    }
    return(n);
}

// The cell a label for data is of, or 0 if the file of names has no such
// label.
static uint64_t data_called(const char* tables, const char* name) {
    static word labels[1 << 12];
    int n = data_of(tables, labels, 1 << 12);
    for(int idx=0; idx < n; idx++) if (!strcmp(labels[idx].name, name)) return((uint64_t)labels[idx].at);
    return(0);
}

// Where a word is, or less than nothing if the image has none of that name.
static long place_of(const word* words, int n, const char* name) {
    for(int idx=0; idx < n; idx++) if (!strcmp(words[idx].name, name)) return(words[idx].at);
    return(-1);
}

// The nucleus's tables (join.fs) are instructions laid down after its
// code, each to the end of what its page has, and join-rules says where
// they are.  They are not code: here they are made nothing, in an image
// that is to be read as code.
static void without_tables(uint8_t* image, size_t len, const word* words, int n) {
    uint16_t* code = (uint16_t*)image;
    long rules = place_of(words, n, "join-rules");
    if (rules < 0) return;
    uint64_t cells = 0;                 // the word pushes where its cells are
    for(const instruction* ins = (const instruction*)code + rules; ins->lit.lit_f; ins++) {
        cells += (uint64_t)ins->lit.lit_v << (ins->lit.lit_shifts * LIT_BITS);
    }
    for(int which=0; which < 4; which++) {
        if (which == 1) continue;       // how many rules: not a place
        uint64_t at = cell_of(image, cells + which);
        for(uint64_t idx=at; at && idx < (at / PAGE + 1) * PAGE && idx < len / 2; idx++) code[idx] = 0;
    }
}

// The word that the instruction at a place is in.
static const char* word_at(const word* words, int n, long at) {
    const char* in = "?";
    for(int idx=0; idx < n && words[idx].at <= at; idx++) if (words[idx].size) in = words[idx].name;
    return(in);
}
