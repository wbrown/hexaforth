//
// patterns PICTURE.tiff own|half [TOP]
//
// Counts how often each cell value occurs in the dithered picture.  The
// black and white bitmap the dither makes of the picture, at full size or
// at half size, is cut into the cells of each row at 16, 32 and 64 pixels
// wide, the way the machines with those cell sizes store it.  Only the
// cells a row is stored in are counted, starting at its left edge.  A cell
// that extends past the right edge is white beyond it.
//
// The bitmap comes from the reference dither, one pixel at a time
// (test/dither.c).  The benchmarks compare every machine's bitmap with it
// cell for cell, and cells_of() in that file cuts the cells.  A cell's
// value is the same pattern on every machine apart from the order of its
// bits, so the counts do not depend on the machine.
//
// For each width it prints the number of cells and the number of distinct
// values; how many cells are all white and how many all black; how many
// values occur only once; the share of the picture covered by the 1, 10
// and 100 most common values; and the TOP most common values (10 if TOP is
// not given), drawn one character per pixel, with # for black.
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

// Prints a cell as its pixels, leftmost first.
static void draw(uint64_t cell, int bits) {
    for(int px=0; px < bits; px++) putchar(cell >> (bits - 1 - px) & 1 ? '#' : '.');
}

static void width(const uint8_t* black, int w, int h, int bits, int top) {
    int stride;
    // Cut the cells with the leftmost pixel in the highest bit and black
    // as a set bit, so that a printed cell reads the way it looks.
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
