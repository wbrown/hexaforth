//
// patterns PICTURE.tiff own|half [TOP]
//
// How often each cell of the dithered picture occurs: the black and white
// bitmap the dither makes of the picture, at its own size or at half, cut
// into the cells of a row, 16, 32 and 64 pixels wide, as the machines with
// those cells store it.  Only the cells a row is stored in are looked at,
// from its left edge; a cell that runs past the right edge is white past
// it.
//
// The bitmap is the dither a pixel at a time (test/dither.c), which the
// benchmarks hold every machine's bitmap to, cell for cell; and the cells
// are cut by cells_of there.  A cell's value is the same pattern on every
// machine but for the order of its bits, so how often each occurs does not
// depend on the machine.
//
// For each width it prints how many cells there are and how many different
// ones, how many are all white and all black, how many patterns occur only
// once, how much of the picture the commonest 1, 10 and 100 are, and the
// commonest TOP (10 if not said), drawn a pixel to a character: # is black.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../test/dither.h"
#include "tiff.h"

typedef struct { uint64_t cell; long count; } seen;

static int by_cell(const void* a, const void* b) {
    uint64_t x = *(const uint64_t*)a, y = *(const uint64_t*)b;
    return(x < y ? -1 : x > y);
}

static int by_count(const void* a, const void* b) {
    const seen* x = a;
    const seen* y = b;
    if (x->count != y->count) return(x->count < y->count ? 1 : -1);
    return(x->cell < y->cell ? -1 : x->cell > y->cell);
}

// A cell as its pixels, the leftmost first.
static void draw(uint64_t cell, int bits) {
    for(int px=0; px < bits; px++) putchar(cell >> (bits - 1 - px) & 1 ? '#' : '.');
}

static void width(const uint8_t* black, int w, int h, int bits, int top) {
    int stride;
    // The leftmost pixel highest, and black a set bit, so that a cell is
    // drawn as it reads.
    uint64_t* cells = cells_of(black, w, 0, 0, w, h, bits, false, true, &stride);
    long n = (long)stride * h;
    uint64_t all = bits == 64 ? ~0ull : (1ull << bits) - 1;
    long white = 0, dark = 0;
    for(long idx=0; idx < n; idx++) {
        white += cells[idx] == 0;
        dark += cells[idx] == all;
    }
    qsort(cells, n, sizeof(uint64_t), by_cell);
    seen* kinds = malloc((n ? n : 1) * sizeof(seen));
    if (kinds == NULL) {
        fprintf(stderr, "there is no room for %ld patterns\n", n);
        exit(EXIT_FAILURE);
    }
    long distinct = 0, once = 0;
    for(long idx=0; idx < n; ) {
        long from = idx;
        while (idx < n && cells[idx] == cells[from]) idx++;
        kinds[distinct++] = (seen){ cells[from], idx - from };
        once += idx - from == 1;
    }
    qsort(kinds, distinct, sizeof(seen), by_count);

    printf("cells of %d pixels: %ld cells, %ld different (%.1f%%); %ld all white, %ld all black; "
           "%ld patterns occur once\n", bits, n, distinct, 100.0 * distinct / n, white, dark, once);
    int marks[] = { 1, 10, 100 };
    for(int m=0; m < 3; m++) {
        long covered = 0;
        for(long k=0; k < distinct && k < marks[m]; k++) covered += kinds[k].count;
        printf("    the commonest %d: %ld cells, %.1f%%\n", marks[m], covered, 100.0 * covered / n);
    }
    for(long k=0; k < distinct && k < top; k++) {
        printf("    %8ld  %5.2f%%  ", kinds[k].count, 100.0 * kinds[k].count / n);
        draw(kinds[k].cell, bits);
        putchar('\n');
    }
    printf("\n");
    free(kinds);
    free(cells);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "usage: patterns PICTURE.tiff own|half [TOP]\n");
        return(1);
    }
    bool half = !strcmp(argv[2], "half");
    int top = argc > 3 ? atoi(argv[3]) : 10;
    tiff picture = read_tiff(argv[1]);
    int w, h;
    int* gray = grays_of(picture.bytes, picture.w, picture.h, picture.samples, half, &w, &h);
    uint8_t* black = calloc((size_t)w * h + 1, 1);
    if (black == NULL) {
        fprintf(stderr, "there is no room for a bitmap of %d by %d\n", w, h);
        return(1);
    }
    dither(gray, w, h, black);
    printf("%s at %s size, dithered: %d by %d\n\n", argv[1], half ? "half" : "its own", w, h);
    width(black, w, h, 16, top);
    width(black, w, h, 32, top);
    width(black, w, h, 64, top);
    return(0);
}
