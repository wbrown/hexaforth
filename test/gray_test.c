//
// gray_test IMAGE.hex [SEED]
//
// Tests storing a picture as gray and drawing it through a mask
// (forth/gray.fs), by comparison with the same done here one pixel at a
// time.
//
// It is a contrastive test.  The expected bitmap comes from gray.h, which
// was written from the description in gray.fs independently of the
// machine's code, and nothing is built from it.  The machine keeps levels
// in bit planes and draws a cell at a time.  gray.h keeps one level per
// sample and draws a pixel at a time.  The two must produce the same
// bitmap, in every cell.
//
// Each picture's bytes are random, the same for a given seed, and are
// copied into the memory the machine is about to allocate (image-is, as
// dither_test does).  A picture is stored with one sample for each pixel,
// for each 2 by 2 pixels and for each 4 by 4 pixels, from sources with one
// sample per pixel and with three, and is drawn through Bayer's mask and
// through 32 by 32 and 64 by 64 blue noise.  The masks are copied into the
// machine's memory in the disk's layout (mask_planes(), util/bluenoise.h)
// and loaded from there (mask-from).  The expected bitmap uses the same
// thresholds before they are put in that layout.
//
// A picture is either a whole number of cells wide, which the machine
// reads a cell at a time, or a few pixels wider, which it reads a pixel at
// a time.  It is also one row taller than a whole number of samples, to
// check that a leftover row is ignored.  Each picture is drawn whole, at the
// size it was stored for.  A picture stored at 2 by 2, 4 by 4 and 8 by 8
// is also drawn the way the viewer's menu draws it (gray-draw-start): at
// half that sample size, starting one cell in and three rows down, through
// a mask made darker or lighter.
//
// It prints how many pictures and bitmap cells it compared, and fails if
// any differ or if it compared nothing.
//

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "nucleus.h"
#include "dither.h"
#include "gray.h"
#include "../util/bluenoise.h"

#define BITS    CELL_BITS

static uint64_t room;           // the first cell the machine will allocate

static uint64_t seed;
static uint8_t chance(void) {
    seed ^= seed << 13;
    seed ^= seed >> 7;
    seed ^= seed << 17;
    return(seed >> 32);
}

// How a picture is drawn.  A sample is drawn as a square of 1 << dsh
// pixels a side.  The bitmap is the part of the drawn picture that starts
// at pixel x0 (a cell boundary) and row y0, and is `across` cells by
// `down` rows.  br is added to every threshold.
typedef struct { int dsh, x0, y0, across, down, br; } drawn;

// Computes the bitmap the machine should draw, one pixel at a time.
// Returns its size in cells, and sets `*stride` to the number of cells in
// a row.
static int expected(const uint8_t* bytes, int w, int h, int samples, int sh, int bits,
                    const uint8_t* mask, int n, const drawn* d, uint64_t** cells, int* stride) {
    int fw, fh, gw, gh;
    int* gray = grays_of(bytes, w, h, samples, false, &fw, &fh);
    int* level = gray_kept(gray, w, h, sh, bits, &gw, &gh);
    int bw = d->across * BITS, bh = d->down;
    uint8_t* black = calloc((size_t)bw * bh + 1, 1);
    gray_drawn(level, gw, bits, mask, n, d->br, d->dsh, d->x0, d->y0, bw, bh, black);
    *cells = cells_of(black, bw, 0, 0, bw, bh, BITS, DISP_LEFT_LOW, DISP_INK_BLACK, stride);
    free(gray), free(level), free(black);
    return(*stride * bh);
}

static int pictures, compared, failed;

// A mask: its name, its size, its thresholds as bytes, and the same in the
// disk's layout (mask_planes, util/bluenoise.h).
typedef struct { const char* name; int n; const uint8_t* bytes; const uint8_t* planes; } a_mask;

#define BITS_KEPT 4             // bits per level, as the machine stores them

// Runs one picture.  Without `part` the picture is drawn whole, each
// sample at the size it was stored for.  With `part` it is drawn the way
// the viewer draws it (forth/view.fs): a part of it, each sample at half
// that size, starting one cell in and three rows down, through a mask made
// darker or lighter.  A picture row is a whole number of cells (`over`
// false) or has some pixels left over.  The machine reads whole cells a
// cell at a time, and the other rows a pixel at a time.
static void run(int samples, int sh, const a_mask* m, bool part, bool over) {
    const int bits = BITS_KEPT;
    int w = (part ? 4 : 2) * BITS + (over ? (1 << sh) - 1 : 0), h = (part ? 9 : 5) * (1 << sh) + 1;
    drawn d = { part ? sh - 1 : sh, part ? BITS : 0, part ? 3 : 0, 0, 0, part ? (sh & 1 ? 1 : -2) : 0 };
    d.across = ((w >> sh) << d.dsh) / BITS - d.x0 / BITS;
    d.down = ((h >> sh) << d.dsh) - d.y0;
    size_t len = (size_t)w * h * samples, cells = (len + CELL_BYTES - 1) / CELL_BYTES;
    uint8_t* bytes = malloc(len);
    for(size_t idx=0; idx < len; idx++) bytes[idx] = chance();
    size_t mask_len = (size_t)m->n * DISK_MASK_PLANES * DISK_MASK_WIDTH / 8;   // the mask goes after the picture, in whole cells
    size_t mask_cells = mask_len / CELL_BYTES;

    start();
    if (!in_memory(room, cells + mask_cells)) {
        fprintf(stderr, "gray_test: the machine has no room for a picture\n");
        exit(EXIT_FAILURE);
    }
    memcpy(cell_at(room), bytes, len);
    memcpy(cell_at(room + cells), m->planes, mask_len);
    line("%d halloc constant r0", (int)((cells + mask_cells) * CELL_BYTES));
    line("r0 %d %d %d image-is %d gray-keep constant g", w, h, samples, sh);
    line("r0 %d + %d %d mask-from constant m", (int)cells, m->n, d.br);
    line("create bm 0 , %d ,  %d halloc bm !", d.across, d.across * d.down * CELL_BYTES);
    if (part) {
        line("bm m g %d %d %d gray-draw-start constant dr", d.dsh, d.x0, d.y0);
        line(": rows ( n -- ) 0 do i dr gray-draw-row loop ;");
        line("%d rows  dr gray-draw-done  r0 bm @ bm 1+ @", d.down);
    } else {
        line("bm m g %d gray-draw  r0 bm @ bm 1+ @", sh);
    }
    type();

    uint64_t* want;
    int stride;
    int total = expected(bytes, w, h, samples, sh, bits, m->bytes, m->n, &d, &want, &stride);
    bool ran = machine.SP >= 3 && left(2) == room;
    int differ = 0, first = -1;
    if (!ran) {
        printf("FAIL %s, %d to a pixel, a sample to %d by %d: the machine did not do as it was asked.  "
               "It said:\n%s\n", m->name, samples, 1 << sh, 1 << sh, said);
        differ = total;
    } else if (left(0) != (uint64_t)stride || !in_memory(left(1), total)) {
        differ = total;
    } else {
        for(int idx=0; idx < total; idx++) {
            if (fetch(left(1) + idx) != want[idx]) {
                if (first < 0) first = idx;
                differ++;
            }
        }
    }
    pictures++;
    compared += total;
    if (differ) failed++;
    printf("%s %d by %d, %d to a pixel, a sample to %d by %d, through %s",
           differ ? "FAIL" : "ok  ", w, h, samples, 1 << sh, 1 << sh, m->name);
    if (part) printf(" %+d, drawn %d by %d from %d, %d", d.br, 1 << d.dsh, 1 << d.dsh, d.x0, d.y0);
    printf(": %d cells of bitmap, %d differ\n", total, differ);
    if (ran && first >= 0) {
        printf("     cell %d of row %d should be %llx and is %llx\n", first % stride, first / stride,
               (unsigned long long)want[first], (unsigned long long)fetch(left(1) + first));
    }
    free(want);
    free(bytes);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s IMAGE.hex [SEED]\n", argv[0]);
        return(1);
    }
    int given = argc > 2 ? atoi(argv[2]) : 1;
    seed = 0x9e3779b97f4a7c15ull * (uint64_t)(given + 1);
    if (!seed) seed = 1;
    alarm(600);

    nucleus_is(argv[1]);
    start();
    line("0 halloc");
    type();
    if (machine.SP < 1) {
        printf("FAIL the machine did not say where its memory is.  It said:\n%s\n", said);
        return(1);
    }
    room = left(0);

    // The masks.  Bayer's thresholds come from gray.h, and its disk layout
    // from the Bayer mask util/bluenoise.h makes, which mkdisk uses.  The
    // other two are blue noise.
    uint8_t bayer8[64], bayer_disk[64];
    for(int y=0; y < 8; y++) for(int x=0; x < 8; x++) bayer8[y * 8 + x] = gray_bayer(x, y);
    bayer_bytes(bayer_disk);
    uint8_t* blue32 = malloc(32 * 32);
    uint8_t* blue64 = malloc(64 * 64);
    blue_bytes(32, blue32);
    blue_bytes(64, blue64);
    a_mask masks[] = { {"Bayer's mask", 8, bayer8, bayer_disk}, {"blue noise of 32 by 32", 32, blue32, blue32},
                       {"blue noise of 64 by 64", 64, blue64, blue64} };
    for(int k=0; k < 3; k++) {
        uint8_t* planes = malloc((size_t)masks[k].n * DISK_MASK_PLANES * DISK_MASK_WIDTH / 8);
        mask_planes(masks[k].planes, masks[k].n, planes);
        masks[k].planes = planes;
        for(int samples=1; samples <= 3; samples += 2) {
            for(int sh=0; sh <= 2; sh++) {
                run(samples, sh, &masks[k], false, true);
                run(samples, sh, &masks[k], false, false);
            }
            for(int sh=1; sh <= 3; sh++) {
                run(samples, sh, &masks[k], true, true);
                run(samples, sh, &masks[k], true, false);
            }
        }
    }
    printf("a picture kept as gray, cells of %d bits, seed %d: %d pictures, %d not as made a pixel at a time; "
           "%d cells of bitmap compared\n", CELL_BITS, given, pictures, failed, compared);
    return(failed || !compared);
}
