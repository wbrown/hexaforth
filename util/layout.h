//
// layout.h - describes how an image is laid out, for the tools that
// include it: its pages of code, and what the rest of it holds.  It
// includes words.h and bench/files.h.
//
// Code is in pages of 4,096 instructions.  Page 0 is the kernel's, and its
// first KROM_BYTES bytes are the kernel's ROM.
//
// The rest of an image is cells.  The dictionary's headers are found by
// walking the dictionary as a search does (words.h).  The data starts at
// the cell VAR_BASE, and the nucleus's variable `dp` holds the byte where
// the data ends.  Any non-zero cell past that point, outside the headers
// and the code pages, is data that was placed in an area of its own: what
// cross.fs calls bulk and high.
//

#ifndef HEXAFORTH_LAYOUT_H
#define HEXAFORTH_LAYOUT_H

#include "image.h"
#include "words.h"
#include "../bench/files.h"

typedef struct {
    long words;                 // the number of words that start on it
    long code;                  // the number of its instructions that are in a word
    long code_end;              // the offset on the page where the last word ends
    long end;                   // the offset just past the last non-zero instruction
} code_page;

typedef struct {
    uint8_t* bytes;             // the image as loaded
    size_t len;
    long instructions;          // its length in instructions
    word* words;                // its words, each with the length of its code
    int n;
    code_page* pages;           // one entry for each page the image spans
    long npages;
    long kernel;                // the kernel's length in instructions: the code of page 0 that is in its ROM
    size_t headers_from, headers_to;    // the dictionary, as byte offsets
    int headers;                        // the number of words with a header
    size_t data_from, data_to;          // the data, as byte offsets, up to the data pointer
    size_t past;                        // the number of non-zero bytes in cells past the data pointer
} layout;

// Returns the number pushed by a word that only pushes a number.  A word
// made with `create` pushes the address of its data.
static uint64_t pushed_by(const uint8_t* image, long at) {
    uint64_t pushed = 0;
    for(const instruction* ins = (const instruction*)image + at; ins->lit.lit_f; ins++) {
        pushed += (uint64_t)ins->lit.lit_v << (ins->lit.lit_shifts * LIT_BITS);
    }
    return(pushed);
}

// Returns 1 if a byte of the image is on a page that has code.
static int on_a_code_page(const layout* m, size_t byte) {
    long page = (long)(byte / sizeof(instruction)) / PAGE;
    return(page < m->npages && m->pages[page].words);
}

// Loads an image and fills in its layout.
static void lay_out(layout* m, const char* hex, const char* tables) {
    memset(m, 0, sizeof(*m));
    m->len = load_image(hex, &m->bytes);
    m->instructions = m->len / sizeof(instruction);
    m->n = words_of(m->bytes, m->len, tables, &m->words);

    const uint16_t* code = (const uint16_t*)m->bytes;
    m->npages = (m->instructions + PAGE - 1) / PAGE;
    m->pages = room_for(m->npages, sizeof(code_page));
    for(int idx=0; idx < m->n; idx++) {
        const word* w = &m->words[idx];
        if (!w->size || w->at / PAGE >= m->npages) continue;
        code_page* page = &m->pages[w->at / PAGE];
        page->words++;
        page->code += w->size;
        if (w->at % PAGE + w->size > page->code_end) page->code_end = w->at % PAGE + w->size;
        if (w->at < KROM_BYTES / (long)sizeof(instruction) && w->at + w->size > m->kernel) m->kernel = w->at + w->size;
    }
    // A kernel that is longer than its ROM continues past the ROM's end.
    // The words that follow with no gap are part of it too.  The words are
    // sorted by address, so one pass finds them.  Counting only the words
    // that start inside the ROM would understate how far such a kernel
    // overruns.
    if (m->kernel > KROM_BYTES / (long)sizeof(instruction)) {
        for(int idx=0; idx < m->n; idx++) {
            const word* w = &m->words[idx];
            if (w->size && w->at == m->kernel) m->kernel = w->at + w->size;
        }
    }
    for(long at=0; at < m->instructions; at++) {
        code_page* page = &m->pages[at / PAGE];
        if (page->words && code[at]) page->end = at % PAGE + 1;
    }
    for(long page=0; page < m->npages; page++) {
        code_page* has = &m->pages[page];
        if (has->end < has->code_end) has->end = has->code_end;
    }

    m->headers = headers_of(m->bytes, m->len, &m->headers_from, &m->headers_to);

    // The data runs up to the nucleus's data pointer.  Count the non-zero
    // bytes past it.
    m->data_from = (size_t)VAR_BASE * CELL_BYTES;
    m->data_to = m->data_from;
    long dp = place_of(m->words, m->n, "dp");
    if (dp >= 0) {
        uint64_t cell = pushed_by(m->bytes, dp);
        if ((cell + 1) * CELL_BYTES <= m->len) m->data_to = (size_t)cell_of(m->bytes, cell);
    }
    for(size_t at=m->data_from; at + CELL_BYTES <= m->len; at += CELL_BYTES) {
        if (at < m->data_to || on_a_code_page(m, at) || (at >= m->headers_from && at < m->headers_to)) continue;
        for(int idx=0; idx < CELL_BYTES; idx++) if (m->bytes[at + idx]) m->past++;
    }
}

// A source file's share of an image's code: the number of words that
// start in it, the number of its instructions that are in a word, and the
// pages they are on, one bit per page.  A file's code is one contiguous
// stretch of one page, plus any part of it placed on the kernel's page
// (files.h).
typedef struct {
    long words, code;
    unsigned long pages;
} held;

static void on_page(held* has, long page) {
    if (page >= 0 && page < (long)(8 * sizeof(has->pages))) has->pages |= 1ul << page;
}

// Computes each file's share: out[idx] for files[idx], and out[n] for the
// nucleus itself, which is the code that belongs to none of the files
// (files.h).  A stretch of a file that is placed on the kernel's page is
// counted for the file, not separately.
static void held_by(const layout* m, const span* files, int n, held* out) {
    for(int idx=0; idx <= n; idx++) memset(&out[idx], 0, sizeof(held));
    for(int idx=0; idx < m->n; idx++) {
        const word* w = &m->words[idx];
        if (!w->size) continue;
        int of = file_holding(files, n, w->at);
        held* has = &out[of < 0 ? n : of];
        has->words++;
        has->code += w->size;
        on_page(has, w->at / PAGE);
    }
}

// Returns 1 if a file is included by another file, whose figures then
// contain its own.  cross.fs records how deeply each file is included
// (files.h).
static int is_included(const span* files, int n, int which) {
    (void)n;
    return(files[which].deep > 1);
}

#endif // HEXAFORTH_LAYOUT_H
