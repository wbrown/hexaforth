//
// words.h - the words of an image, for the tools that include it: every
// definition that has a header or a label, where its code starts, and how
// many instructions it has.
//
// A header is in the image (read_header, vm_opcodes.c).  A label is not:
// cross.fs writes the labels to the names file beside the image
// (build/64/nuc.tables), in the format of the decompiler's records, which
// cross.fs describes at name-tables.
//
// A word's length runs to the next word on its page, or to the end of the
// page for the last word on it, minus any trailing zeros.  Code does not
// always start right after the code before it: with 16-bit cells the
// kernel's ROM is half of page 0, other code goes in the rest of the page,
// and the space between the two is zero.
//

#ifndef HEXAFORTH_WORDS_H
#define HEXAFORTH_WORDS_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "../vm_opcodes.h"
#include "room.h"

#define PAGE 4096                       // instructions to a page of code

typedef struct {
    long at, size;
    char name[NAME_ROOM];
} word;

// Orders words by address.  Where a label and a header share an address,
// the label sorts first and gets the instructions: the label names the
// word whose code is there, and the header belongs to a word with no
// instructions that falls through into it (cross.fs, ;into).  Until the
// list is sorted, `size` is 1 for a label and 0 for a header.
static int by_place(const void* a, const void* b) {
    const word* x = a;
    const word* y = b;
    if (x->at != y->at) return(x->at < y->at ? -1 : 1);
    if (x->size != y->size) return(x->size > y->size ? -1 : 1);
    return(strcmp(x->name, y->name));
}

static uint64_t cell_of(const uint8_t* cells, size_t which) {
    uint64_t cell = 0;
    memcpy(&cell, cells + which * CELL_BYTES, CELL_BYTES);
    return(cell);
}

// Appends the labels in the names file to a list, and returns the list's
// new length.  `of_data` selects the labels of data; otherwise the labels
// of code are read.
//
// The file's first block holds a stamp, the number of blocks in the run
// sorted by execution token, the number of blocks in the run sorted by
// number, and the first key of each block.  The runs follow it.  A block
// of a run is a count of records and then the records.  A record is a
// word's header address and its key; or, for a label, the label's position
// in cross.fs's label table with the top bit set, its key, and its name.
// The labels of data are the labels in the run sorted by number.
//
// The labels are appended newest first, which is the order cross.fs
// created them in.  They are sorted by that position, which is kept in
// `size` until the sort is done.
static int newest_first(const void* a, const void* b) {
    const word* x = a;
    const word* y = b;
    return(x->size == y->size ? 0 : x->size > y->size ? -1 : 1);
}

static long labels_of(const char* tables, int of_data, word** list, long n, long* room) {
    uint8_t* cells;
    size_t have = tables ? file_bytes(tables, &cells) / CELL_BYTES : 0;
    if (!have) return(n);
    const size_t block = DISK_BLOCK / CELL_BYTES;
    const uint64_t top = 1ull << (CELL_BITS - 1);
    long start = n;
    uint64_t runs = have > 2 ? cell_of(cells, 1) : 0, numbers = have > 2 ? cell_of(cells, 2) : 0;
    uint64_t from = of_data ? 1 + runs : 1, to = from + (of_data ? numbers : runs);
    for(uint64_t b=from; b < to && b * block < have; b++) {
        size_t at = b * block;
        uint64_t records = cell_of(cells, at++);
        for(uint64_t r=0; r < records && at + 1 < have; r++) {
            uint64_t first = cell_of(cells, at), key = cell_of(cells, at + 1);
            at += 2;
            if (!(first & top)) continue;                       // a word with a header, not a label
            ONE_MORE(*list, n, *room);
            decode_name(cells + at * CELL_BYTES, (have - at) * CELL_BYTES, (*list)[n].name);
            (*list)[n].at = (long)key;
            (*list)[n++].size = (long)(first & ~top);
            while (at < have && !(cell_of(cells, at) & top)) at++;
            at++;                                               // past the name's last cell
        }
    }
    free(cells);
    if (n > start) qsort(*list + start, n - start, sizeof(word), newest_first);
    for(long idx=start; idx < n; idx++) (*list)[idx].size = 0;
    return(n);
}

// Lists the words of an image, sorted by where their code starts.  Returns
// their number and allocates the list.  A second name for the same code
// gets a size of 0.  Where a label and a header share an address, the
// label has the instructions (by_place).  `more` is extra words to
// include, each a name and the address of its code: the names a machine
// kept for the words its boot compiled (kept_of, below).
static int words_and(const uint8_t* image, size_t len, const char* tables, const word* more, long nmore, word** list) {
    word* out = NULL;
    long n = 0, room = 0;
    for(uint32_t head = first_header(image, len); head; ) {
        uint64_t xt;
        ONE_MORE(out, n, room);
        head = read_header(image, len, head, &xt, out[n].name);
        out[n].size = 0;
        out[n++].at = xt;
    }
    for(long idx=0; idx < nmore; idx++) {
        ONE_MORE(out, n, room);
        out[n] = more[idx];
        out[n++].size = 0;
    }
    long labels = n;
    n = labels_of(tables, 0, &out, n, &room);
    for(long idx=labels; idx < n; idx++) out[idx].size = 1;
    ONE_MORE(out, n, room);                             // so that the list is allocated even when it is empty
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
        } else if (end > instructions) {
            end = instructions;
        }
        while (end > at && !code[end - 1]) end--;
        out[idx].size = end - at;
    }
    return(n);
}

static int words_of(const uint8_t* image, size_t len, const char* tables, word** list) {
    return(words_and(image, len, tables, NULL, 0, list));
}

// Lists the labels of data in the names file: each variable or table that
// has no header, with its name and the address of its cell.  Returns their
// number, allocates the list, and sets `*room` to the list's capacity, so
// that a caller can append to it (ONE_MORE).
static long data_of(const char* tables, word** list, long* room) {
    *list = NULL;
    *room = 0;
    long n = labels_of(tables, 1, list, 0, room);
    ONE_MORE(*list, n, *room);                          // so that the list is allocated even when it is empty
    return(n);
}

// Returns the address of the cell a data label names, or 0 if the names
// file has no such label.
static uint64_t data_called(const char* tables, const char* name) {
    word* labels;
    long room, n = data_of(tables, &labels, &room);
    uint64_t is = 0;
    for(long idx=0; idx < n && !is; idx++) if (!strcmp(labels[idx].name, name)) is = (uint64_t)labels[idx].at;
    free(labels);
    return(is);
}

// Reads the listing beside an image (IMAGE.lst, which cross.fs writes):
// every word that has code, in the order the source defines them, each
// with the address of its first instruction, in hex, and its name.
// Returns their number and allocates the list; the number is 0 if the
// image has no listing.  The listing tells two words with the same name
// apart: their order of definition is the same in every build of a source,
// and their addresses are not.
static long listing_of(const char* hex, word** list) {
    word* out = NULL;
    long n = 0, room = 0;
    size_t len = strlen(hex);
    char* path = room_for(len + 5, 1);
    strcpy(path, hex);
    if (len > 4 && !strcmp(path + len - 4, ".hex")) strcpy(path + len - 4, ".lst");
    FILE* in = fopen(path, "r");
    char* line = NULL;
    long line_room = 0;
    while (in && line_of(in, &line, &line_room)) {
        long at;
        int skip = 0;
        if (sscanf(line, "%lx %n", &at, &skip) != 1 || !line[skip]) continue;
        line[strcspn(line, "\r\n")] = 0;
        ONE_MORE(out, n, room);
        out[n].at = at;
        out[n].size = 0;
        snprintf(out[n++].name, NAME_ROOM, "%s", line + skip);
    }
    if (in) fclose(in);
    free(line);
    free(path);
    ONE_MORE(out, n, room);                             // so that the list is allocated even when it is empty
    *list = out;
    return(n);
}

// Returns the address of a word's code, or -1 if the image has no word
// with that name.
static long place_of(const word* words, int n, const char* name) {
    for(int idx=0; idx < n; idx++) if (!strcmp(words[idx].name, name)) return(words[idx].at);
    return(-1);
}

// Returns the number that the code at an address pushes: the sum of its
// literal instructions.
static uint64_t pushed_at(const uint8_t* image, long at) {
    uint64_t is = 0;
    for(const instruction* ins = (const instruction*)image + at; ins->lit.lit_f; ins++) {
        is += (uint64_t)ins->lit.lit_v << (ins->lit.lit_shifts * LIT_BITS);
    }
    return(is);
}

// Reads the decompiler's table (forth/fused.fs) from an image or from a
// machine's memory.  The variable `fused-at` holds the table's address.
// The table is 16-bit fields packed into cells, lowest bits first.  An
// entry is the instruction, the number of words it is shown as, and the
// address of each word's header; a zero field ends the table.  Returns the
// number of entries and allocates the list, or returns 0 if there is no
// table.
#define FUSED_WORDS 4                   // the most words an entry has
typedef struct { uint16_t ins; int n; long head[FUSED_WORDS]; } fused_entry;

static long fused_of(const uint8_t* image, size_t len, const word* words, int n, fused_entry** list) {
    fused_entry* out = NULL;
    long entries = 0, room = 0;
    *list = NULL;
    long at = place_of(words, n, "fused-at");
    const size_t cells_there = len / CELL_BYTES;
    if (at < 0) return(0);
    uint64_t told = pushed_at(image, at);
    if (told >= cells_there) return(0);
    uint64_t starts = cell_of(image, told);
    if (!starts || starts >= cells_there) return(0);
    const uint8_t* fields = image + starts * CELL_BYTES;
    const size_t most = (len - starts * CELL_BYTES) / sizeof(uint16_t);    // the most fields that fit between there and the end
    #define FUSED_FIELD(k) ((uint16_t)(fields[2 * (k)] | fields[2 * (k) + 1] << 8))
    for(size_t k=0; k + 2 < most && FUSED_FIELD(k); ) {
        fused_entry e = { .ins = FUSED_FIELD(k), .n = FUSED_FIELD(k + 1) };
        if (e.n < 1 || e.n > FUSED_WORDS || k + 2 + e.n > most) break;
        for(int w=0; w < e.n; w++) e.head[w] = (long)FUSED_FIELD(k + 2 + w) * CELL_BYTES;
        ONE_MORE(out, entries, room);
        out[entries++] = e;
        k += 2 + e.n;
    }
    #undef FUSED_FIELD
    *list = out;
    return(entries);
}

// Appends the names a machine kept for the words its boot compiled, and
// returns the list's new length.  Those names are private to their files
// and are not in the dictionary (forth/files.fs).  `memory` is the
// machine's memory after it stopped, `words` is the words of its image,
// and `disk` is the disk it booted from, as the boot left it.  The
// machine's variable `own-kept` holds the first block the names are in and
// the number of blocks used, and files.fs describes the layout of a block.
// A record that has a number instead of an address names no code, and is
// left out.  Each word's `size` is set to 1 if the word is immediate, for
// a caller that needs to know before the list is sorted.
static long kept_of(const uint8_t* memory, size_t len, const word* words, int n,
                    const uint8_t* disk, size_t disk_len, word** list, long have, long* room) {
    long told = place_of(words, n, "own-kept");
    if (told < 0) return(have);
    uint64_t cell = pushed_at(memory, told);
    if ((cell + 2) * CELL_BYTES > len) return(have);
    uint64_t first = cell_of(memory, cell) & ((1u << DISK_BLOCK_BITS) - 1), used = cell_of(memory, cell + 1);
    const size_t block = DISK_BLOCK / CELL_BYTES;
    const uint64_t top = 1ull << (CELL_BITS - 1);
    for(uint64_t b=first; b < first + used && (b + 1) * DISK_BLOCK <= disk_len; b++) {
        const uint8_t* cells = disk + b * DISK_BLOCK;
        size_t at = 1;
        for(uint64_t r = cell_of(cells, 0); r && at + 2 < block; r--) {
            bool a_number = cell_of(cells, at) & 2;
            ONE_MORE(*list, have, *room);
            (*list)[have].size = (long)(cell_of(cells, at) & 1);
            (*list)[have].at = (long)cell_of(cells, at + 1);
            at += 2;
            decode_name(cells + at * CELL_BYTES, (block - at) * CELL_BYTES, (*list)[have].name);
            if (!a_number) have++;
            while (at < block && !(cell_of(cells, at) & top)) at++;
            at++;                                               // past the name's last cell
        }
    }
    return(have);
}

// Returns how many of a word's instructions a machine compiled when it
// booted.  `memory` is the machine's memory after the boot, `with` is the
// image it booted with, and `pages` is the address of the table that
// records how far each page's code has reached (nuc.fs).  Returns 0 for a
// word that was already in the image, for a second name, and for code that
// was zeroed.  The count stops where the page's code ends: past that point
// the nucleus's peephole keeps instructions it has set aside while it
// tries a word inline (join.fs, keep), and those are not code.
static long compiled_of(const uint8_t* with, const uint8_t* memory, uint64_t pages, const word* w) {
    const uint16_t* booted_with = (const uint16_t*)with;
    const uint16_t* code = (const uint16_t*)memory;
    long at = w->at, size = w->size;
    if (!size || booted_with[at] || !code[at]) return(0);
    long got_to = (long)cell_of(memory, pages + at / PAGE);
    if (got_to && at + size > got_to) size = got_to - at;
    return(size > 0 ? size : 0);
}

// Returns the name of the word that contains the instruction at an
// address.
static const char* word_at(const word* words, int n, long at) {
    const char* in = "?";
    for(int idx=0; idx < n && words[idx].at <= at; idx++) if (words[idx].size) in = words[idx].name;
    return(in);
}

// Measures the dictionary of an image.  Sets `*from` to the byte offset of
// the lowest header in memory and `*to` to the byte offset just past the
// highest, and returns the number of headers.
static int headers_of(const uint8_t* image, size_t len, size_t* from, size_t* to) {
    int n = 0;
    *from = *to = 0;
    for(uint32_t head = first_header(image, len); head; n++) {
        uint64_t xt;
        char name[NAME_ROOM];
        size_t end = header_end(image, len, head);
        if (!n || head < *from) *from = head;
        if (end > *to) *to = end;
        head = read_header(image, len, head, &xt, name);
    }
    return(n);
}

#endif // HEXAFORTH_WORDS_H
