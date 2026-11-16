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
#include "room.c"

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

// The labels the file of names has, of code or of data, put after what a
// list has: how many the list then has.  A label of data has the top bit
// of its place set, which is taken off here.
static long labels_of(const char* tables, int of_data, word** list, long n, long* room) {
    uint8_t* cells;
    size_t have = tables ? file_bytes(tables, &cells) / CELL_BYTES : 0;
    if (!have) return(n);
    size_t at = 2;                                      // past the stamp and the count
    if (have > at) at += 1 + 2 * cell_of(cells, at);    // and the two tables
    if (have > at) at += 1 + 2 * cell_of(cells, at);
    for(uint64_t place = have > at ? cell_of(cells, at) : 0; place && at + place + 2 < have;
        place = cell_of(cells, at + place)) {
        uint64_t value = cell_of(cells, at + place + 1), data = value >> (CELL_BITS - 1);
        if (!data != !of_data) continue;
        ONE_MORE(*list, n, *room);
        decode_name(cells + (at + place + 2) * CELL_BYTES, (have - at - place - 2) * CELL_BYTES, (*list)[n].name);
        (*list)[n].at = (long)(value & ~(1ull << (CELL_BITS - 1)));
        (*list)[n++].size = 0;
    }
    free(cells);
    return(n);
}

// The words of an image, in order of where they are: how many there are,
// and the list, which is made here and has room for them all.  A second
// name for the same code has no instructions of its own.
static int words_of(const uint8_t* image, size_t len, const char* tables, word** list) {
    word* out = NULL;
    long n = 0, room = 0;
    for(uint32_t head = newest_header(image, len); head; ) {
        uint64_t xt;
        ONE_MORE(out, n, room);
        head = read_header(image, len, head, &xt, out[n].name);
        out[n++].at = xt;
    }
    n = labels_of(tables, 0, &out, n, &room);
    ONE_MORE(out, n, room);                             // so that there is a list, if there are none
    *list = out;
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
// How many there are, and the list, which is made here, with how much it
// has room for, for whoever puts more after them (ONE_MORE).
static long data_of(const char* tables, word** list, long* room) {
    *list = NULL;
    *room = 0;
    long n = labels_of(tables, 1, list, 0, room);
    ONE_MORE(*list, n, *room);                          // so that there is a list, if there are none
    return(n);
}

// The cell a label for data is of, or 0 if the file of names has no such
// label.
static uint64_t data_called(const char* tables, const char* name) {
    word* labels;
    long room, n = data_of(tables, &labels, &room);
    uint64_t is = 0;
    for(long idx=0; idx < n && !is; idx++) if (!strcmp(labels[idx].name, name)) is = (uint64_t)labels[idx].at;
    free(labels);
    return(is);
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
