//
// quads PICTURE.tiff OUT-DIRECTORY
//
// Ways to store a picture in one byte per 2 by 2 block of pixels.  That is
// the size of the grays at half size, a quarter of the full grays, and as
// many bytes as the full-size bitmap has bits.  A gray is 8 bits, but the
// dither turns each pixel into 1, so the byte could be spent differently.
// Each way below stores the picture, expands it back to full-size grays,
// and dithers those (test/dither.c) at full size.
//
//   grays         the full-size grays, a byte per pixel: the reference,
//                 at four times the size of the rest
//   mean          the mean of each 2 by 2, used for all four pixels: the
//                 half-size grays shown at full size
//   mean-smooth   the same, with each pixel interpolated between the four
//                 nearest means (bilinear)
//   four          2 bits per pixel: each gray rounded to the nearest of 4
//                 levels
//   four-passed   the same, with each rounding error passed on to the
//                 neighbours, as the dither does
//   pattern       a 4-bit mean, in 16 levels with the error passed on
//                 between blocks, plus one bit per pixel saying whether it
//                 is lighter than the mean.  Lighter pixels are raised and
//                 darker ones lowered by a fixed amount, keeping the mean.
//                 The amount is the median over all blocks of the gap
//                 between a block's lighter and darker pixels, and is
//                 stored once for the whole picture
//
// For each it prints how many bytes are stored and the root mean square
// difference from the picture's grays, out of 255.  Judge by the
// pictures: each dither is written to OUT-DIRECTORY as NAME.png.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../test/dither.h"
#include "tiff.h"
#include "../util/png_file.c"

static int pw, ph;                      // the picture's size, which is even
static const int* gray;                 // its grays

static int clamp(int v) {
    return(v < 0 ? 0 : v > 255 ? 255 : v);
}

// The four pixels of the 2 by 2 at bx, by.
static int at(const int* g, int bx, int by, int which) {
    return(g[(2 * by + (which >> 1)) * pw + 2 * bx + (which & 1)]);
}

static int mean_of(int bx, int by) {
    int sum = 0;
    for(int which=0; which < 4; which++) sum += at(gray, bx, by, which);
    return((sum + 2) / 4);
}

static void made_as_grays(int* made) {
    memcpy(made, gray, sizeof(int) * pw * ph);
}

static void made_as_mean(int* made) {
    for(int y=0; y < ph; y++) for(int x=0; x < pw; x++) made[y * pw + x] = mean_of(x / 2, y / 2);
}

static void made_as_mean_smooth(int* made) {
    int bw = pw / 2, bh = ph / 2;
    for(int y=0; y < ph; y++) {
        double sy = (y + 0.5) / 2 - 0.5;
        if (sy < 0) sy = 0;
        if (sy > bh - 1) sy = bh - 1;
        int y0 = (int)sy, y1 = y0 + 1 < bh ? y0 + 1 : y0;
        double fy = sy - y0;
        for(int x=0; x < pw; x++) {
            double sx = (x + 0.5) / 2 - 0.5;
            if (sx < 0) sx = 0;
            if (sx > bw - 1) sx = bw - 1;
            int x0 = (int)sx, x1 = x0 + 1 < bw ? x0 + 1 : x0;
            double fx = sx - x0;
            double v = (mean_of(x0, y0) * (1 - fx) + mean_of(x1, y0) * fx) * (1 - fy) +
                       (mean_of(x0, y1) * (1 - fx) + mean_of(x1, y1) * fx) * fy;
            made[y * pw + x] = (int)floor(v + 0.5);
        }
    }
}

// Round each of w by h values to the nearest of `levels` levels from 0 to
// 255.  With `passed`, each rounding error is passed on as the dither does:
// an eighth to each of the six neighbours ahead.
static void to_levels(const int* from, int w, int h, int levels, bool passed, int* to) {
    double* owed = calloc((size_t)w * h, sizeof(double));
    static const int AHEAD[6][2] = { {1, 0}, {2, 0}, {-1, 1}, {0, 1}, {1, 1}, {0, 2} };
    double step = 255.0 / (levels - 1);
    for(int y=0; y < h; y++) {
        for(int x=0; x < w; x++) {
            double v = from[y * w + x] + owed[y * w + x];
            int level = (int)floor(v / step + 0.5);
            if (level < 0) level = 0;
            if (level > levels - 1) level = levels - 1;
            to[y * w + x] = (int)floor(level * step + 0.5);
            if (!passed) continue;
            double e = (v - level * step) / 8;
            for(int a=0; a < 6; a++) {
                int nx = x + AHEAD[a][0], ny = y + AHEAD[a][1];
                if (nx >= 0 && nx < w && ny < h) owed[ny * w + nx] += e;
            }
        }
    }
    free(owed);
}

static void made_as_four(int* made) {
    to_levels(gray, pw, ph, 4, false, made);
}

static void made_as_four_passed(int* made) {
    to_levels(gray, pw, ph, 4, true, made);
}

// The gap between a 2 by 2's lighter and darker pixels: the mean of those
// above the block's mean, less the mean of the rest; 0 if none is above.
static int spread_of(int bx, int by) {
    int mean = mean_of(bx, by), light = 0, dark = 0, lights = 0;
    for(int which=0; which < 4; which++) {
        int g = at(gray, bx, by, which);
        if (g > mean) light += g, lights++;
        else dark += g;
    }
    if (lights == 0) return(0);
    return(light / lights - dark / (4 - lights));
}

static int by_value(const void* a, const void* b) {
    return(*(const int*)a - *(const int*)b);
}

static void made_as_pattern(int* made) {
    int bw = pw / 2, bh = ph / 2;
    int* means = malloc(sizeof(int) * bw * bh);
    int* spreads = malloc(sizeof(int) * bw * bh);
    for(int by=0; by < bh; by++) for(int bx=0; bx < bw; bx++) {
        means[by * bw + bx] = mean_of(bx, by);
        spreads[by * bw + bx] = spread_of(bx, by);
    }
    int* kept = malloc(sizeof(int) * bw * bh);
    to_levels(means, bw, bh, 16, true, kept);
    qsort(spreads, (size_t)bw * bh, sizeof(int), by_value);
    int spread = spreads[bw * bh / 2];
    for(int by=0; by < bh; by++) {
        for(int bx=0; bx < bw; bx++) {
            int mean = mean_of(bx, by), m = kept[by * bw + bx], lights = 0;
            for(int which=0; which < 4; which++) lights += at(gray, bx, by, which) > mean;
            for(int which=0; which < 4; which++) {
                bool light = at(gray, bx, by, which) > mean;
                int v = light ? m + spread * (4 - lights) / 4 : m - spread * lights / 4;
                made[(2 * by + (which >> 1)) * pw + 2 * bx + (which & 1)] = clamp(v);
            }
        }
    }
    printf("(the pattern's lighter and darker are %d apart)\n", spread);
    free(means), free(spreads), free(kept);
}

static double rms(const int* made) {
    double sum = 0;
    for(long i=0; i < (long)pw * ph; i++) sum += (double)(made[i] - gray[i]) * (made[i] - gray[i]);
    return(sqrt(sum / ((double)pw * ph)));
}

// Write the dithered bitmap to DIR/NAME.png.
static void written(const char* dir, const char* name, const uint8_t* black) {
    size_t row = (size_t)(pw + 7) / 8;
    unsigned char* rows = room_for(row * ph, 1);
    for(int y=0; y < ph; y++) {
        for(int x=0; x < pw; x++) if (black[y * pw + x]) rows[y * row + x / 8] |= 0x80 >> (x % 8);
    }
    char* path = NULL;
    text_more(&path, "%s/%s.png", dir, name);
    if (!png_bits(path, rows, pw, ph)) exit(EXIT_FAILURE);
    free(path), free(rows);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "usage: quads PICTURE.tiff OUT-DIRECTORY\n");
        return(1);
    }
    tiff picture = read_tiff(argv[1]);
    gray = grays_of(picture.bytes, picture.w, picture.h, picture.samples, false, &pw, &ph);
    if (pw % 2 || ph % 2) {
        fprintf(stderr, "%s is %d by %d, and a side that is odd has no 2 by 2 at its end\n", argv[1], pw, ph);
        return(1);
    }
    static const struct { const char* name; void (*made)(int*); int bits; } KEPT[] = {
        {"grays", made_as_grays, 32}, {"mean", made_as_mean, 8}, {"mean-smooth", made_as_mean_smooth, 8},
        {"four", made_as_four, 8}, {"four-passed", made_as_four_passed, 8}, {"pattern", made_as_pattern, 8} };
    int* made = room_for((size_t)pw * ph, sizeof(int));
    uint8_t* black = room_for((size_t)pw * ph, 1);
    printf("%s: %d by %d, a bitmap of %d bytes\n\n", argv[1], pw, ph, pw * ph / 8);
    printf("%-12s %8s %8s\n", "kept as", "bytes", "rms");
    for(int k=0; k < (int)(sizeof(KEPT) / sizeof(KEPT[0])); k++) {
        KEPT[k].made(made);
        dither(made, pw, ph, black);
        printf("%-12s %8d %8.2f\n", KEPT[k].name, pw / 2 * (ph / 2) * KEPT[k].bits / 8, rms(made));
        written(argv[2], KEPT[k].name, black);
        fflush(stdout);
    }
    printf("\nrms: how far the grays made from what is kept are from the picture's, of 255.  Each is\n"
           "dithered at the picture's own size, and what the dither made is NAME.png in %s.\n", argv[2]);
    return(0);
}
