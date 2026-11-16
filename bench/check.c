//
// check picture PICTURE.tiff own|half MEMORY CONSOLE TIMED WHAT ON [PIECE ...]
// check display MEMORY MEMORY TIMED WHAT ON [PIECE ...]
//
// What a benchmark made, looked at, and then what it cost: one that has
// not been checked has measured nothing, so the figure is printed here and
// only after.
//
// The first is a bitmap the machine made of a picture, against the dither
// a pixel at a time (test/dither.c), every cell of it.  MEMORY is the
// machine's memory when it had stopped (rtl/tb.cpp writes it), and CONSOLE
// what it printed, in which
//     b @ . b 1+ @ .
// was typed last: where the bitmap is, and the cells of a row of it.  The
// picture was dithered at its own size, or at half of it.  A machine that
// has not the memory for the picture says so at its console, and then
// that is what is printed, and nothing is wrong.
//
// The second is the display, in the memory of two machines that were
// typed the same at: the reference VM (main.c writes its memory) and the
// Verilog CPU.  Every cell of it is to be the same in both, and something
// is to have been drawn.
//
// TIMED is what the Verilog's test bench said of the run: what passed
// between each mark the machine made and the next (figure.c).  WHAT is the
// work and ON the machine; a PIECE is the name of what was done between a
// mark and the next, in order, and with none named there is one piece and
// it is the work.  A line a piece is printed, or one line that says FAIL
// and why; what was compared is said to stderr.  Having said either, it
// has done what it is for, and its status says only whether it could look.
//
// check table RESULT ...
//
// What each of those printed, in the order they are named: the table of
// the benchmarks.  Its status is the benchmarks': it fails if a result
// says FAIL, and if one says nothing, which is work that was not checked.
//
// It is built for the size of machine the memory is of, and for its
// display, since a bitmap has its pixels as the display has them.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../vm_constants.h"
#include "../test/dither.h"
#include "figure.c"
#include "tiff.h"

// A file, whole, with nothing after it.
static char* read_all(const char* path, size_t* len) {
    FILE* in = fopen(path, "rb");
    if (in == NULL) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    fseek(in, 0, SEEK_END);
    *len = ftell(in);
    fseek(in, 0, SEEK_SET);
    char* bytes = malloc(*len + 1);
    if (fread(bytes, 1, *len, in) != *len) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    fclose(in);
    bytes[*len] = '\0';
    return(bytes);
}

// A number the machine printed, as a cell that is not negative.
static uint64_t ucell(long long x) {
#if CELL_BITS == 64
    return((uint64_t)x);
#else
    return((uint64_t)x & ((1ull << CELL_BITS) - 1));
#endif
}

// The display in two memories: row y is where vm_constants.h says it is.
// What is wrong with it, or nothing.
static char* displays(const char* one, const char* other) {
    size_t len, other_len;
    uint8_t* a = (uint8_t*)read_all(one, &len);
    uint8_t* b = (uint8_t*)read_all(other, &other_len);
    int across = DISP_W / CELL_BITS, total = 0, differ = 0, drawn = 0;
    for(int y=0; y < DISP_H; y++) {
        size_t row = (size_t)DISP_ROW(y) * CELL_BYTES;
        if (row + across * CELL_BYTES > len || row + across * CELL_BYTES > other_len) break;
        for(int idx=0; idx < across * CELL_BYTES; idx++) {
            if (a[row + idx] != b[row + idx]) differ++;
            if (a[row + idx]) drawn++;
        }
        total += across;
    }
    char* why = NULL;
    fprintf(stderr, "the display: %d cells compared, %d bytes differ from the reference VM's\n", total, differ);
    if (!drawn) text_more(&why, "NOT CHECKED: nothing was drawn");
    else if (differ || !total) text_more(&why, "the display: %d cells compared, %d bytes differ from the reference VM's", total, differ);
    return(why);
}

// What the machine said it could not do, if it said so: the text, which is
// made here.
static char* could_not(const char* said) {
    static const char error[] = "error: ";
    const char* at = strstr(said, error);
    if (!at) return(NULL);
    char* is = NULL;
    text_more(&is, "the machine could not: %.*s", (int)(strlen(error) + strspn(at + strlen(error), "-0123456789")), at);
    return(is);
}

// A bitmap the machine made of a picture, against the dither a pixel at a
// time.  What is wrong with it, or nothing.
static char* pictures(const char* is_picture, bool half, const char* is_memory, const char* said) {
    tiff picture = read_tiff(is_picture);
    int w, h, stride;
    int* gray = grays_of(picture.bytes, picture.w, picture.h, picture.samples, half, &w, &h);
    uint8_t* black = calloc((size_t)w * h + 1, 1);
    dither(gray, w, h, black);
    uint64_t* want = cells_of(black, w, 0, 0, w, h, CELL_BITS, DISP_LEFT_LOW, DISP_INK_BLACK, &stride);
    int total = stride * h;

    size_t len;
    uint8_t* memory = (uint8_t*)read_all(is_memory, &len);
    static const char asked[] = "b @ . b 1+ @ . ";
    char* at = strstr(said, asked);
    char* end;
    char* why = NULL;
    if (at == NULL) {
        text_more(&why, "NOT CHECKED: the machine was not asked where its bitmap is");
        return(why);
    }
    uint64_t bitmap = ucell(strtoll(at + strlen(asked), &end, 10));
    uint64_t row = ucell(strtoll(end, NULL, 10));
    if (row != (uint64_t)stride || (bitmap + total) * CELL_BYTES > len) {
        text_more(&why, "NOT CHECKED: a bitmap of %d cells a row at cell %llu, where %d cells a row were wanted",
                  (int)row, (unsigned long long)bitmap, stride);
        return(why);
    }

    int differ = 0;
    for(int idx=0; idx < total; idx++) {
        uint64_t got = 0;
        memcpy(&got, memory + (bitmap + idx) * CELL_BYTES, CELL_BYTES);
        if (got != want[idx]) differ++;
    }
    fprintf(stderr, "%d by %d: %d cells of bitmap compared, %d differ\n", w, h, total, differ);
    if (differ || !total) text_more(&why, "%d by %d: %d cells of bitmap compared, %d differ", w, h, total, differ);
    return(why);
}

// The results, and whether all of them are figures.
static int table(char** results, int count) {
    int failed = 0;
    for(int n=0; n < count; n++) {
        size_t len = 0;
        char* said = read_all(results[n], &len);
        fputs(said, stdout);
        if (!len) printf("%s: FAIL nothing was checked\n", results[n]);
        if (!len || strstr(said, "FAIL")) failed++;
        free(said);
    }
    return(failed != 0);
}

int main(int argc, char *argv[]) {
    if (argc >= 3 && !strcmp(argv[1], "table")) return(table(argv + 2, argc - 2));
    bool display = argc >= 7 && !strcmp(argv[1], "display"), picture = argc >= 9 && !strcmp(argv[1], "picture");
    if (!display && !picture) {
        fprintf(stderr, "Usage: %s picture PICTURE.tiff own|half MEMORY CONSOLE TIMED WHAT ON [PIECE ...]\n"
                        "       %s display MEMORY MEMORY TIMED WHAT ON [PIECE ...]\n"
                        "       %s table RESULT ...\n", argv[0], argv[0], argv[0]);
        return(1);
    }
    char** rest = argv + (display ? 4 : 6);             // TIMED WHAT ON, and the pieces
    int pieces = argc - (display ? 7 : 9);
    const char* timed = rest[0];
    const char* what = rest[1];
    const char* on = rest[2];

    char* why;
    if (display) {
        why = displays(argv[2], argv[3]);
    } else {
        size_t said_len;
        char* said = read_all(argv[5], &said_len);
        char* could = could_not(said);
        if (could) {
            figure(what, NULL, on, could);
            return(0);
        }
        why = pictures(argv[2], !strcmp(argv[3], "half"), argv[4], said);
    }
    if (why) {
        char* failed = NULL;
        text_more(&failed, "FAIL %s", why);
        figure(what, NULL, on, failed);
        return(0);
    }
    for(int piece=0; piece < (pieces ? pieces : 1); piece++) {
        char* is = between(timed, piece + 1);
        figure(what, pieces ? rest[3 + piece] : NULL, on, is ? is : "FAIL the test bench said nothing of this mark");
    }
    return(0);
}
