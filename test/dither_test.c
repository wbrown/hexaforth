//
// dither_test IMAGE.hex [SEED]
//
// The dither of forth/atkinson.fs, against a dither a pixel at a time.
//
// It is a contrastive test: the dither it is compared with (dither.c) is
// made apart from the machine's, and nothing is built from it.  The
// machine works a cell of lanes at a time and that a pixel at a time, as
// the dither is described, and the two are to make the same bitmap, every
// cell of it.
//
// The machine is the nucleus on vm.c, typed at (nucleus.h).  For each test
// the machine is started again, a picture's bytes are put in the memory it
// is about to give out, and what dithers it is typed at the prompt: image-is,
// and then atkinson, or dither-part or dither-half and dither-row.  What
// the typing leaves on the data stack says where the bitmap is, and the
// bitmap is read out of the machine's memory.
//
// The pictures are of one sample and of three, and of widths that are and
// are not whole cells of the image's own bytes, of lanes and of pixels, so
// that both ways the grays are had and every end of a row are tried.  They
// are dithered whole, a part of them, and at half their size, and two a
// row of each in turn, each with its own image and its own dither.  A
// picture's bytes are taken by chance, the same every time for a seed.
//
// It says how many pictures and how many cells of bitmap it compared, and
// how many of the pictures the machine had the grays of a cell at a time.
// It fails if either is none: then it found nothing to test.
//

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "nucleus.h"
#include "dither.h"

#define BITS    CELL_BITS
#define BYTES   CELL_BYTES

// An image: pixels across and down, and the samples of a pixel, one or three.
typedef struct {
    int w, h;
    int samples;
} picture;

typedef struct {
    const char* label;
    picture a;
    bool half;                  // it is dithered at half its size
    int px, py, pw, ph;         // the part of what is dithered that is kept: all of it, if pw is 0
    picture b;                  // another, dithered a row in turn with the first, if it has a width
} dither_test;

// The tests are of the machine's cell, which is known when this runs: so
// they are said then (tests_said).
static dither_test TESTS[64];
static void tests_said(void) {
  const dither_test now[] = {
    // Whole, at their own size.
    {.label = "three samples, eight pixels to three cells", .a = {64, 10, 3}},
    {.label = "three samples, a width that is whole cells", .a = {40, 9, 3}},
    {.label = "three samples, an odd width", .a = {37, 11, 3}},
    {.label = "three samples, more than two cells of pixels", .a = {130, 6, 3}},
    {.label = "three samples, one row", .a = {8, 1, 3}},
    {.label = "three samples, three pixels across", .a = {3, 5, 3}},
    {.label = "three samples, a few rows", .a = {72, 3, 3}},
    {.label = "three samples, two rows", .a = {100, 2, 3}},
    {.label = "one sample", .a = {50, 8, 1}},
    {.label = "one sample, one pixel across", .a = {1, 3, 1}},
    {.label = "one sample, sixteen across", .a = {16, 4, 1}},
    {.label = "one sample, an odd width", .a = {33, 7, 1}},
    {.label = "one sample, more than a cell of pixels", .a = {100, 8, 1}},
    {.label = "one sample, a cell of the image", .a = {BYTES, 3, 1}},
    {.label = "one sample, one row of two cells", .a = {2 * BYTES, 1, 1}},
    {.label = "one sample, six cells to a row", .a = {6 * BYTES, 7, 1}},
    {.label = "one sample, two cells of pixels", .a = {2 * BITS, 6, 1}},
    {.label = "one sample, a cell more than two cells of pixels", .a = {2 * BITS + BYTES, 4, 1}},
    {.label = "one sample, a pixel more than whole cells", .a = {5 * BYTES + 1, 6, 1}},

    // Two at once.
    {.label = "two at once, of three samples and of one", .a = {48, 7, 3}, .b = {30, 7, 1}},
    {.label = "two at once, both of three samples", .a = {64, 5, 3}, .b = {64, 5, 3}},
    {.label = "two at once, both of one sample", .a = {6 * BYTES, 7, 1}, .b = {9 * BYTES, 7, 1}},

    // A part: where it starts across is where a cell of the bitmap starts.
    {.label = "a part, three samples",
     .a = {3 * BITS + 7, 12, 3}, .px = BITS, .py = 3, .pw = BITS + 9, .ph = 5},
    {.label = "a part that is one cell across, three samples",
     .a = {4 * BITS, 10, 3}, .px = 2 * BITS, .py = 2, .pw = BITS, .ph = 6},
    {.label = "a part, one sample",
     .a = {2 * BITS + 3, 9, 1}, .px = 0, .py = 4, .pw = BITS + 5, .ph = 5},
    {.label = "a part, one sample in whole cells",
     .a = {3 * BITS, 9, 1}, .px = BITS, .py = 2, .pw = BITS + 9, .ph = 5},

    // At half their size.  A row of an odd number of pixels has one over,
    // and an odd number of rows a row.
    {.label = "at half its size, three samples", .a = {4 * BITS, 10, 3}, .half = true},
    {.label = "at half its size, three samples, a pixel and a row over",
     .a = {4 * BITS + 6 + 1, 9, 3}, .half = true, .px = BITS, .py = 1, .pw = BITS + 3, .ph = 3},
    {.label = "at half its size, a part, three samples",
     .a = {8 * BITS, 6, 3}, .half = true, .px = 2 * BITS, .py = 1, .pw = BITS, .ph = 2},
    {.label = "at half its size, one sample, a pixel over",
     .a = {2 * BITS + 5, 8, 1}, .half = true},
    {.label = "at half its size, one sample in whole cells", .a = {8 * BYTES, 10, 1}, .half = true},
    {.label = "at half its size, one sample, cells of pixels", .a = {4 * BITS, 8, 1}, .half = true},
    {.label = "at half its size, a part, one sample, a row over",
     .a = {6 * BITS, 9, 1}, .half = true, .px = BITS, .py = 1, .pw = BITS + BYTES, .ph = 3},
    {.label = "at half its size, one sample, a row over", .a = {2 * BITS + 2 * BYTES, 7, 1}, .half = true},
    {.label = ""}};
  _Static_assert(sizeof(now) <= sizeof(TESTS), "dither_test.c: there are more tests than TESTS has room for");
  memcpy(TESTS, now, sizeof(now));
}

static uint64_t room;           // the cell the machine gives out first

// Bytes by chance, the same every time.
static uint64_t seed;
static uint8_t chance(void) {
    seed ^= seed << 13;
    seed ^= seed >> 7;
    seed ^= seed << 17;
    return(seed >> 32);
}

// The cells a bitmap should be: of a picture's bytes, dithered whole or at
// half its size a pixel at a time (dither.c), the part that is kept.
// Returns how many cells there are, a row being *stride of them.
static int expected(const dither_test* test, const picture* p, const uint8_t* bytes,
                    uint64_t** cells, int* stride) {
    int w, h;
    int* gray = grays_of(bytes, p->w, p->h, p->samples, test->half, &w, &h);
    uint8_t* black = calloc((size_t)w * h + 1, 1);
    dither(gray, w, h, black);

    int px = test->pw ? test->px : 0;
    int py = test->pw ? test->py : 0;
    int pw = test->pw ? test->pw : w;
    int ph = test->pw ? test->ph : h;
    *cells = cells_of(black, w, px, py, pw, ph, BITS, DISP_LEFT_LOW, DISP_INK_BLACK, stride);
    free(gray);
    free(black);
    return(*stride * ph);
}

static int pictures, compared, by_cell, failed;

// One test: its pictures put in the machine's memory and dithered there,
// and each bitmap looked at.
static void run(const dither_test* test) {
    const picture* p[2] = { &test->a, &test->b };
    int count = test->b.w ? 2 : 1;
    size_t len[2] = {0, 0}, cells[2] = {0, 0};
    uint8_t* bytes[2] = {NULL, NULL};
    for(int n=0; n < count; n++) {
        len[n] = (size_t)p[n]->w * p[n]->h * p[n]->samples;
        cells[n] = (len[n] + CELL_BYTES - 1) / CELL_BYTES;
        bytes[n] = malloc(len[n]);
        for(size_t idx=0; idx < len[n]; idx++) bytes[n][idx] = chance();
    }

    // The pictures are one after the other in what the machine gives out
    // first, each from where a cell starts, a cell's lowest byte its first.
    start();
    if (!in_memory(room, cells[0] + cells[1])) {
        fprintf(stderr, "dither_test: %s: the machine has no room for it\n", test->label);
        exit(EXIT_FAILURE);
    }
    memcpy(cell_at(room), bytes[0], len[0]);
    if (count > 1) memcpy(cell_at(room + cells[0]), bytes[1], len[1]);

    line("%d halloc constant r0", (int)((cells[0] + cells[1]) * CELL_BYTES));
    if (count > 1) line("r0 %d + constant r1", (int)cells[0]);
    for(int n=0; n < count; n++) {
        line("create b%d 0 , 0 ,", n);
        line("r%d %d %d %d image-is constant i%d", n, p[n]->w, p[n]->h, p[n]->samples, n);
    }
    if (count > 1) {
        line("b0 i0 dither-start constant d0  b1 i1 dither-start constant d1");
        line(": rows %d 0 do d0 dither-row d1 dither-row loop ; rows", p[0]->h);
    } else if (test->half || test->pw) {
        int w = test->half ? p[0]->w / 2 : p[0]->w;
        int h = test->half ? p[0]->h / 2 : p[0]->h;
        int py = test->pw ? test->py : 0;
        int ph = test->pw ? test->ph : h;
        line("b0 i0 %d %d %d %d %s constant d0", test->pw ? test->px : 0, py, test->pw ? test->pw : w, ph,
             test->half ? "dither-half" : "dither-part");
        line(": rows %d 0 do d0 dither-row loop ; rows", py + ph);
    } else {
        line("b0 i0 atkinson");
    }
    // Left on the stack: where the pictures are, and of each whether its
    // grays are had a cell at a time, where its bitmap is, and the cells
    // of a row of that.
    line("r0");
    for(int n=0; n < count; n++) {
        line("i%d image! pixels %s @ 0<>  b%d @  b%d 1+ @", n, test->half ? "'pairs" : "'grays", n, n);
    }
    type();

    bool ran = machine.SP >= 1 + 3 * count && left(3 * count) == room;
    if (!ran) {
        printf("FAIL %s: the machine did not do as it was asked.  It said:\n%s\n", test->label, said);
        pictures += count;
        failed += count;
    }
    for(int n=0; ran && n < count; n++) {
        int at = 3 * (count - 1 - n);
        bool lanes = left(at + 2) != 0;
        uint64_t bitmap = left(at + 1), row = left(at);
        uint64_t* want;
        int stride;
        int total = expected(test, p[n], bytes[n], &want, &stride);
        int differ = 0, first = -1;
        if (row != (uint64_t)stride || !in_memory(bitmap, total)) {
            differ = total;
        } else {
            for(int idx=0; idx < total; idx++) {
                if (fetch(bitmap + idx) != want[idx]) {
                    if (first < 0) first = idx;
                    differ++;
                }
            }
        }
        pictures++;
        compared += total;
        by_cell += lanes;
        if (differ) failed++;
        printf("%s %s%s: %d by %d, %d to a pixel, its grays %s: %d cells of bitmap, %d differ\n",
               differ ? "FAIL" : "ok  ", test->label, count > 1 ? (n ? ", the second" : ", the first") : "",
               p[n]->w, p[n]->h, p[n]->samples, lanes ? "a cell at a time" : "a pixel at a time",
               total, differ);
        if (first >= 0) {
            printf("     cell %d of row %d should be %llx and is %llx\n", first % stride, first / stride,
                   (unsigned long long)want[first], (unsigned long long)fetch(bitmap + first));
        } else if (differ) {
            printf("     a row of the bitmap is %llu cells at %llx, and should be %d\n",
                   (unsigned long long)row, (unsigned long long)bitmap, stride);
        }
        free(want);
    }
    free(bytes[0]);
    free(bytes[1]);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s IMAGE.hex [SEED]\n", argv[0]);
        return(1);
    }
    int given = argc > 2 ? atoi(argv[2]) : 1;
    seed = 0x9e3779b97f4a7c15ull * (uint64_t)(given + 1);
    if (!seed) seed = 1;        // nothing comes of nothing
    alarm(600);                 // a machine that does not stop is stopped

    nucleus_is(argv[1]);
    tests_said();

    // What the machine gives out first is where it puts a picture, and is
    // the same every time it is started.
    start();
    line("0 halloc");
    type();
    if (machine.SP < 1) {
        printf("FAIL the machine did not say where its memory is.  It said:\n%s\n", said);
        return(1);
    }
    room = left(0);

    for(dither_test* test = TESTS; strlen(test->label); test++) {
        run(test);
    }
    printf("the dither, cells of %d bits, seed %d: %d pictures, %d not as the dither a pixel at a time "
           "makes them; %d cells of bitmap compared, and the grays of %d pictures had a cell at a time\n",
           CELL_BITS, given, pictures, failed, compared, by_cell);
    return(failed || !compared || !by_cell);
}
