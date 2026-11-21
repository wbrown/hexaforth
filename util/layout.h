//
// layout.h - how an image is laid out, for the tools that have this in
// them: its pages of code, and what the rest of it is.  It has words.h in
// it, and bench/files.h.
//
// Code is paged, 4,096 instructions to a page, and page 0 is the kernel's:
// the first KROM_BYTES of it are the kernel's ROM.  What a page has is its
// words, and after the last of them the nucleus's tables, which are laid
// down after the code of their page and are not code (join.fs, and
// words.h, which finds them).  A table is as long as where it lies: some
// of what is in one is nothing, as a count is until it is counted.
//
// The rest of an image is cells.  The dictionary's headers are found by
// going through it from the newest (words.h).  The data starts at the cell
// VAR_BASE, and the nucleus keeps where it has got to, in bytes, in dp.
// What is not nothing past that, outside the headers and the pages of
// code, is data that was laid down in a place of its own: what cross.fs
// calls bulk, and high.
//

#ifndef HEXAFORTH_LAYOUT_H
#define HEXAFORTH_LAYOUT_H

#include "image.h"
#include "words.h"
#include "../bench/files.h"

typedef struct {
    long words;                 // the words that start on it
    long code;                  // its instructions that are in a word
    long code_end;              // where on it the last word ends,
    long end;                   // and the last instruction that is not nothing
    long tables;                // the instructions between those two
} code_page;

typedef struct {
    uint8_t* bytes;             // the image as it is,
    uint8_t* plain;             // and with the nucleus's tables made nothing
    size_t len;
    long instructions;          // how many instructions that is
    word* words;                // its words, each as long as its code is
    int n;
    code_page* pages;           // a page for every one its length has
    long npages;
    long kernel;                // the instructions of the kernel: what page 0 has in its ROM
    size_t headers_from, headers_to;    // bytes: the dictionary,
    int headers;                        // and how many words have a header
    size_t data_from, data_to;          // bytes: the data, as far as the data pointer
    size_t past;                        // bytes that are not nothing in cells past it
} layout;

// What a word that only pushes a number pushes: a word made with create
// pushes where its data is.
static uint64_t pushed_by(const uint8_t* image, long at) {
    uint64_t pushed = 0;
    for(const instruction* ins = (const instruction*)image + at; ins->lit.lit_f; ins++) {
        pushed += (uint64_t)ins->lit.lit_v << (ins->lit.lit_shifts * LIT_BITS);
    }
    return(pushed);
}

// Is a byte of the image on a page that has code?
static int on_a_code_page(const layout* m, size_t byte) {
    long page = (long)(byte / sizeof(instruction)) / PAGE;
    return(page < m->npages && m->pages[page].words);
}

// An image, laid out.
static void lay_out(layout* m, const char* hex, const char* tables) {
    memset(m, 0, sizeof(*m));
    m->len = load_image(hex, &m->bytes);
    m->instructions = m->len / sizeof(instruction);
    m->plain = room_for(m->len + CELL_BYTES, 1);
    memcpy(m->plain, m->bytes, m->len);

    // The words are as long as their code is once the tables are nothing:
    // the last word of a page is as long as what the page has after it.
    word* with_tables;
    int had = words_of(m->bytes, m->len, tables, &with_tables);
    without_tables(m->plain, m->len, with_tables, had);
    free(with_tables);
    m->n = words_of(m->plain, m->len, tables, &m->words);

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
    // A kernel that is longer than its ROM runs on past it, and the words
    // of it that start there are its too: those that follow with nothing
    // between, the words being in the order of their places.  (It was
    // once taken to end with the last word that starts in the ROM, and a
    // kernel 136 bytes too long was said to be 6.)
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
        has->tables = has->end - has->code_end;
    }

    m->headers = headers_of(m->bytes, m->len, &m->headers_from, &m->headers_to);

    // The data, as far as the nucleus's data pointer says; and what is past it.
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

// What a source file has of an image's code: the words that start in it,
// its instructions that are in a word and those that are in a table, and
// the pages they are on, a bit a page.  A file's code is one run of one
// page, and whatever of it is laid in the kernel's page besides (files.h).
typedef struct {
    long words, code, tables;
    unsigned long pages;
} held;

static void on_page(held* has, long page) {
    if (page >= 0 && page < (long)(8 * sizeof(has->pages))) has->pages |= 1ul << page;
}

// What each file has: out[idx] for files[idx], and out[n] for the
// nucleus's own, which is what no file of them has (files.h).  A stretch
// of a file that is laid in the kernel's page is the file's, and its own
// line has nothing.
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
    for(long page=0; page < m->npages; page++) {
        for(long at=page * PAGE + m->pages[page].code_end; at < page * PAGE + m->pages[page].end; at++) {
            int of = file_holding(files, n, at);
            held* has = &out[of < 0 ? n : of];
            has->tables++;
            on_page(has, page);
        }
    }
}

// Is a file inside another: one that includes it, whose figures have its
// own in them?  cross.fs says how deep each is included (files.h).
static int is_included(const span* files, int n, int which) {
    (void)n;
    return(files[which].deep > 1);
}

#endif // HEXAFORTH_LAYOUT_H
