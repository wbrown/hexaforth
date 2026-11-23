//
// check IMAGE.hex picture PICTURE.tiff own|half|doubled MEMORY CONSOLE TIMED WHAT ON [PIECE ...]
// check IMAGE.hex gray PICTURE.tiff SHIFT DSHIFT N AT MEMORY CONSOLE TIMED WHAT ON [PIECE ...]
// check IMAGE.hex display MEMORY MEMORY TIMED WHAT ON [PIECE ...]
// check table RESULT ...
//
// Checks what a benchmark produced, and only then prints what the work
// cost.  A benchmark whose result was not checked has measured nothing,
// so this program is the only place its figure is printed.
//
// IMAGE.hex is the image the machine ran.  The cell size and the display
// are those of the machine the image was built for, which is recorded
// beside the image (util/image.h).  A bitmap stores its pixels the way
// the display does, so one description serves for both.
//
// `picture` checks a bitmap the machine dithered from a picture.  Every
// cell is compared with the reference dither, which works one pixel at a
// time (test/dither.c).  MEMORY is the machine's memory when it stopped
// (rtl/tb.cpp writes it).  CONSOLE is what the machine printed.  The last
// line typed at the machine was
//     b @ . b 1+ @ .
// which prints the bitmap's address and the number of cells in one of its
// rows.  The picture was dithered at full size (own), at half size
// (half), or from its half-size grays with each gray repeated as a 2 by 2
// square (doubled).  If the machine printed an error at its console, it
// could not do the work and has measured nothing, so the result is a FAIL.
//
// `gray` checks a bitmap the machine drew from the picture stored as gray
// (forth/gray.fs).  The gray has one sample for each square of
// 1 << SHIFT pixels a side.  The machine drew all of it with each sample
// 1 << DSHIFT pixels a side, through an N by N blue-noise mask.  The
// bitmap is compared with the same drawing done one pixel at a time
// (test/gray.h).  The display is compared too: the machine copied the
// bitmap to the top of the display, AT pixels from the left.
//
// `display` compares the display in the memory of two machines that were
// typed the same input: the reference VM (main.c writes its memory) and
// the Verilog CPU.  Every cell must be the same in both, and something
// must have been drawn.
//
// TIMED is the Verilog test bench's report of the run: the cycles between
// each mark the machine made and the next (figure.h).  WHAT names the
// work and ON names the machine.  Each PIECE names the work done between
// one mark and the next, in order.  If no PIECE is given, the whole work
// is one piece.  The program prints one line for each piece, or one line
// that says FAIL with the reason.  It writes what it compared to stderr.
// Its exit status is 0 in both cases.  The status is non-zero only when
// the check could not be run: wrong arguments, or a file that cannot be
// read.
//
// check table RESULT ...
//
// `table` prints the results the other forms wrote, in the order the
// files are given.  That is the table of the benchmarks.  It exits with 1
// if a result says FAIL, or if a result file is empty, which means the
// work was never checked.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../vm_constants.h"
#include "../util/image.h"
#include "../test/dither.h"
#include "../test/gray.h"
#include "../util/bluenoise.h"
#include "figure.h"
#include "tiff.h"

// Reads a whole file and returns it with a NUL after its last byte.
// `*len` receives its length.  Exits if the file cannot be read.
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

// Returns a number the machine printed with `.` as an unsigned cell.  `.`
// prints a cell as a signed number.
static uint64_t ucell(long long x) {
    const int spare = 64 - CELL_BITS;
    return(((uint64_t)x << spare) >> spare);
}

// Compares the display in two memory files.  DISP_ROW(y) is the address
// of row y (vm_constants.h).  Returns a description of what is wrong, or
// NULL if the two displays are the same and something was drawn.
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

// If the machine printed an error at its console, returns a message with
// the error's number, as newly allocated text.  Returns NULL if it printed
// no error.
static char* could_not(const char* said) {
    static const char error[] = "error: ";
    const char* at = strstr(said, error);
    if (!at) return(NULL);
    char* is = NULL;
    text_more(&is, "the machine could not: %.*s", (int)(strlen(error) + strspn(at + strlen(error), "-0123456789")), at);
    return(is);
}

// Compares a bitmap the machine made, w by h pixels, with the cells it
// should have: `want`, with `stride` cells to a row.  `is_memory` is the
// file of the machine's memory.  `said` is what the machine printed, which
// has the bitmap's address in it.  Returns a description of what is wrong,
// or NULL if the bitmap is right.
static char* bitmaps(const uint64_t* want, int stride, int w, int h, const char* is_memory, const char* said) {
    int total = stride * h;
    size_t len;
    uint8_t* memory = (uint8_t*)read_all(is_memory, &len);
    static const char asked[] = "b @ . b 1+ @ . ";
    char* at = strstr(said, asked);
    char* end;
    char* why = NULL;
    if (at == NULL) {
        text_more(&why, "NOT CHECKED: the machine was not asked where its bitmap is");
        free(memory);
        return(why);
    }
    uint64_t bitmap = ucell(strtoll(at + strlen(asked), &end, 10));
    uint64_t row = ucell(strtoll(end, NULL, 10));
    if (row != (uint64_t)stride || (bitmap + total) * CELL_BYTES > len) {
        text_more(&why, "NOT CHECKED: a bitmap of %d cells a row at cell %llu, where %d cells a row were wanted",
                  (int)row, (unsigned long long)bitmap, stride);
        free(memory);
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
    free(memory);
    return(why);
}

// Checks a bitmap the machine dithered from a picture against the
// reference dither.  `size` says which grays were dithered: the picture's
// at full size (own), at half size (half), or at half size with each gray
// repeated as a 2 by 2 square (doubled).  Returns a description of what is
// wrong, or NULL if the bitmap is right.
static char* pictures(const char* is_picture, const char* size, const char* is_memory, const char* said) {
    tiff picture = read_tiff(is_picture);
    int w, h, stride;
    int* gray = grays_of(picture.bytes, picture.w, picture.h, picture.samples, strcmp(size, "own"), &w, &h);
    if (!strcmp(size, "doubled")) {
        int* doubled = malloc(sizeof(int) * 4 * w * h);
        for(int y=0; y < 2 * h; y++) for(int x=0; x < 2 * w; x++) doubled[y * 2 * w + x] = gray[(y / 2) * w + x / 2];
        free(gray);
        gray = doubled, w *= 2, h *= 2;
    }
    uint8_t* black = calloc((size_t)w * h + 1, 1);
    dither(gray, w, h, black);
    uint64_t* want = cells_of(black, w, 0, 0, w, h, CELL_BITS, DISP_LEFT_LOW, DISP_INK_BLACK, &stride);
    char* why = bitmaps(want, stride, w, h, is_memory, said);
    free(picture.bytes), free(gray), free(black), free(want);
    return(why);
}

// Compares the picture on the display with `black`, which has one byte
// for each pixel of a w by h picture, non-zero where the pixel is black.
// The machine copied the picture to the top of the display, `dx` pixels
// from the left.  Pixels past the display's right or bottom edge are
// skipped.  Returns a description of what is wrong, or NULL if the
// display is right.
static char* on_display(const uint8_t* black, int w, int h, int dx, const char* is_memory) {
    size_t len;
    uint8_t* memory = (uint8_t*)read_all(is_memory, &len);
    int total = 0, differ = 0;
    for(int y=0; y < h && y < DISP_H; y++) {
        for(int x=0; x < w && x + dx < DISP_W; x++) {
            int sx = x + dx, bit = sx % CELL_BITS;
            size_t at = ((size_t)DISP_ROW(y) + sx / CELL_BITS) * CELL_BYTES;
            uint64_t cell = 0;
            if (at + CELL_BYTES <= len) memcpy(&cell, memory + at, CELL_BYTES);
            else differ++;
            bool set = cell >> (DISP_LEFT_LOW ? bit : CELL_BITS - 1 - bit) & 1;
            if ((DISP_INK_BLACK ? set : !set) != (bool)black[(size_t)y * w + x]) differ++;
            total++;
        }
    }
    free(memory);
    fprintf(stderr, "the display, from %d across: %d pixels compared, %d differ\n", dx, total, differ);
    char* why = NULL;
    if (differ || !total) text_more(&why, "the display, from %d across: %d pixels compared, %d differ", dx, total, differ);
    return(why);
}

// Checks a bitmap the machine drew from a picture stored as gray in
// sixteen levels.  The gray has one sample for each square of 1 << shift
// pixels a side.  The machine drew all of it with each sample 1 << dshift
// pixels a side, through an n by n blue-noise mask.  The bitmap is
// compared with the same drawing done one pixel at a time (test/gray.h),
// using the thresholds the disk's masks are made from (util/bluenoise.h).
// If the bitmap is right, the display is compared as well.  Returns a
// description of what is wrong, or NULL if both are right.
static char* grays(const char* is_picture, int shift, int dshift, int n, int dx, const char* is_memory, const char* said) {
    tiff picture = read_tiff(is_picture);
    int fw, fh, gw, gh, stride;
    int* gray = grays_of(picture.bytes, picture.w, picture.h, picture.samples, false, &fw, &fh);
    int* level = gray_kept(gray, fw, fh, shift, 4, &gw, &gh);
    uint8_t* mask = malloc((size_t)n * n);
    blue_bytes(n, mask);
    int w = gw << dshift, h = gh << dshift;
    uint8_t* black = calloc((size_t)w * h + 1, 1);
    gray_drawn(level, gw, 4, mask, n, 0, dshift, 0, 0, w, h, black);
    uint64_t* want = cells_of(black, w, 0, 0, w, h, CELL_BITS, DISP_LEFT_LOW, DISP_INK_BLACK, &stride);
    char* why = bitmaps(want, stride, w, h, is_memory, said);
    if (!why) why = on_display(black, w, h, dx, is_memory);
    free(picture.bytes), free(gray), free(level), free(mask), free(black), free(want);
    return(why);
}

// Prints each result file.  Returns 1 if any of them says FAIL or is
// empty, and 0 otherwise.
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
    const char* self = argv[0];
    bool told = argc >= 3 && machine_of(argv[1]);       // take the cell size and the display from the image
    if (told) argv++, argc--;
    bool display = argc >= 7 && !strcmp(argv[1], "display"), picture = argc >= 9 && !strcmp(argv[1], "picture");
    bool gray = argc >= 12 && !strcmp(argv[1], "gray");
    if (!told || (!display && !picture && !gray)) {
        fprintf(stderr, "Usage: %s IMAGE.hex picture PICTURE.tiff own|half|doubled MEMORY CONSOLE TIMED WHAT ON [PIECE ...]\n"
                        "       %s IMAGE.hex gray PICTURE.tiff SHIFT DSHIFT N AT MEMORY CONSOLE TIMED WHAT ON [PIECE ...]\n"
                        "       %s IMAGE.hex display MEMORY MEMORY TIMED WHAT ON [PIECE ...]\n"
                        "       %s table RESULT ...\n"
                        "IMAGE.hex is the image the machine ran, which says its machine beside it.\n",
                self, self, self, self);
        return(1);
    }
    int before = display ? 4 : picture ? 6 : 9;         // the index of TIMED in argv
    char** rest = argv + before;                        // TIMED, WHAT, ON, and then the names of the pieces
    int pieces = argc - before - 3;
    const char* timed = rest[0];
    const char* what = rest[1];
    const char* on = rest[2];

    char* why;
    if (display) {
        why = displays(argv[2], argv[3]);
    } else {
        size_t said_len;
        char* said = read_all(argv[before - 1], &said_len);
        why = could_not(said);
        if (!why && picture) why = pictures(argv[2], argv[3], argv[4], said);
        if (!why && gray) why = grays(argv[2], atoi(argv[3]), atoi(argv[4]), atoi(argv[5]), atoi(argv[6]), argv[7], said);
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
