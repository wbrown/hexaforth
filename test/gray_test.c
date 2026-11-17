//
// gray_test IMAGE.hex [SEED]
//
// A picture kept as gray and drawn through a mask (forth/gray.fs), against
// the same made here a pixel at a time.
//
// It is a contrastive test: what the machine's bitmap is compared with is
// made from what gray.fs says it does, apart from the machine's (gray.h),
// and nothing is built from it.  The machine keeps its levels in planes of
// bits and draws a cell at a time; that keeps a level a sample and draws a
// pixel at a time.  They are to make the same bitmap, every cell of it.
//
// A picture's bytes are taken by chance, the same every time for a seed,
// and put in the memory the machine is about to give out (image-is, as
// dither_test does).  It is kept with a sample to each pixel, to 2 by 2 and
// to 4 by 4, of one sample to a pixel and of three, and drawn through
// Bayer's mask and through masks of blue noise of 32 by 32 and 64 by 64,
// which are put in the machine's memory as the disk has them (mask_planes,
// util/bluenoise.c) and had from there (mask-from); what they are compared
// with draws through the thresholds as they were before they were laid out
// so.  A picture is as many whole cells across as are drawn, which the
// machine reads a cell at a time, or a few pixels wider, which it reads a
// pixel at a time; and a few rows deeper, so that what is over is left
// out.  Each is drawn whole, at the size it was kept for; and kept to 2 by
// 2, 4 by 4 and 8 by 8, it is drawn as the viewer's menu draws it too
// (gray-draw-start): at half that, the part from a cell in and three rows
// down, darker or lighter (gbright).
//
// It says how many pictures and cells of bitmap it compared, and fails if
// any differ, or if it compared nothing.
//

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "nucleus.h"
#include "dither.h"
#include "gray.h"
#include "../util/bluenoise.c"

#define BITS    CELL_BITS

static uint64_t room;           // the cell the machine gives out first

static uint64_t seed;
static uint8_t chance(void) {
    seed ^= seed << 13;
    seed ^= seed >> 7;
    seed ^= seed << 17;
    return(seed >> 32);
}

// How it is drawn: a sample to 1 << dsh pixels a side, the bitmap being the
// part of the picture as drawn from pixel x0 (where a cell starts) and row
// y0, across cells by down rows; and what is added to every threshold.
typedef struct { int dsh, x0, y0, across, down, br; } drawn;

// The bitmap the machine should draw, a pixel at a time, and how many cells
// it is, a row being *stride of them.
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

// A mask: what it is called, its size, its thresholds as this has them,
// and as the disk has them (mask_planes, util/bluenoise.c).
typedef struct { const char* name; int n; const uint8_t* bytes; const uint8_t* planes; } a_mask;

#define BITS_KEPT 4             // the bits of a level, which is what the machine keeps

// The picture drawn whole, a sample to as many pixels as it was kept for;
// or, as the viewer draws it (forth/viewmenu.fs), a part of it, a sample to
// half as many, from a cell in and three rows down, darker or lighter.  A
// row of the picture is whole cells (`over` false) or has some pixels over:
// whole cells are read a cell at a time, the others a pixel at a time.
static void run(int samples, int sh, const a_mask* m, bool part, bool over) {
    const int bits = BITS_KEPT;
    int w = (part ? 4 : 2) * BITS + (over ? (1 << sh) - 1 : 0), h = (part ? 9 : 5) * (1 << sh) + 1;
    drawn d = { part ? sh - 1 : sh, part ? BITS : 0, part ? 3 : 0, 0, 0, part ? (sh & 1 ? 1 : -2) : 0 };
    d.across = ((w >> sh) << d.dsh) / BITS - d.x0 / BITS;
    d.down = ((h >> sh) << d.dsh) - d.y0;
    size_t len = (size_t)w * h * samples, cells = (len + CELL_BYTES - 1) / CELL_BYTES;
    uint8_t* bytes = malloc(len);
    for(size_t idx=0; idx < len; idx++) bytes[idx] = chance();
    size_t mask_len = (size_t)m->n * DISK_MASK_PLANES * DISK_MASK_WIDTH / 8;   // after the picture, in cells
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
    line("%d gbright !  r0 %d + %d mask-from constant m  0 gbright !", d.br, (int)cells, m->n);
    line("create bm 0 , %d ,  %d halloc bm !", d.across, d.across * d.down * CELL_BYTES);
    if (part) {
        line(": rows ( n -- ) 0 do i gray-draw-row loop ;");
        line("bm m g %d %d %d gray-draw-start  %d rows  gray-draw-done  r0 bm @ bm 1+ @", d.dsh, d.x0, d.y0, d.down);
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

    // The masks: Bayer's as this has it (gray.h), and as the disk has it
    // from what makes the disk's (util/bluenoise.c); and blue noise.
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
