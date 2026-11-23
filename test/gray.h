//
// Stores a picture as gray and draws it through a mask, one pixel at a
// time, as forth/gray.fs describes.
//
// It is the reference that gray.fs is compared with, by its test
// (gray_test.c) and by the desktop's checks of the picture's menu
// (deskcheck.c).  It was written independently of the machine's version,
// which keeps levels in bit planes and draws a cell at a time.  Nothing in
// the machine is built from it.
//
// Unlike dither.c, it is used by #include: gray_test.c and deskcheck.c
// each include it.
//

#ifndef HEXAFORTH_GRAY_H
#define HEXAFORTH_GRAY_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Returns the 8 by 8 Bayer threshold at x, y as a byte, which is the value
// bench/masks.c uses times four.
static int gray_bayer(int x, int y) {
    int v = 0, xx = x & 7, yy = (y ^ x) & 7;
    for(int b=0; b < 3; b++) v = (v << 2) | ((yy >> b & 1) << 1) | (xx >> b & 1);
    return(v * 4);
}

// Returns e / 8 rounded down, for negative e too, as an arithmetic shift
// gives.
static int gray_eighth(int e) {
    return(e >= 0 ? e / 8 : -((-e + 7) / 8));
}

// Returns the levels a w by h picture of grays is stored as.  Each sample
// covers a square of 1 << sh pixels a side and is their rounded average.
// The sample, plus the error passed to it, is rounded to the nearest of
// 1 << bits levels, and the rounding error is passed on as the dither
// passes it: an eighth to each of six neighbours.  Pixels and rows left
// over at the edges are ignored.  The result is *gw by *gh levels in
// allocated memory, which the caller frees.
static int* gray_kept(const int* gray, int w, int h, int sh, int bits, int* gw, int* gh) {
    int sw = w >> sh, shh = h >> sh, levels = 1 << bits, step = 255 / (levels - 1);
    int* level = calloc((size_t)sw * shh + 1, sizeof(int));
    int* owed = calloc((size_t)(shh + 2) * (sw + 4), sizeof(int));
    if (level == NULL || owed == NULL) {
        fprintf(stderr, "there is no room for a picture kept as gray\n");
        exit(EXIT_FAILURE);
    }
    for(int sy=0; sy < shh; sy++) {
        for(int sx=0; sx < sw; sx++) {
            int sum = 0;
            for(int dy=0; dy < 1 << sh; dy++) {
                for(int dx=0; dx < 1 << sh; dx++) sum += gray[(size_t)((sy << sh) + dy) * w + (sx << sh) + dx];
            }
            int avg = (sum + ((1 << (2 * sh)) >> 1)) >> (2 * sh);
            int* here = owed + sy * (sw + 4) + sx + 2;
            int old = avg + *here, l = 0, g = 0;
            if (old > 0) {
                l = (old + step / 2) / step;
                if (l > levels - 1) l = levels - 1;
                g = l * step;
            }
            level[sy * sw + sx] = l;
            int e = gray_eighth(old - g);
            here[1] += e, here[2] += e;
            here[sw + 4 - 1] += e, here[sw + 4] += e, here[sw + 4 + 1] += e;
            here[2 * (sw + 4)] += e;
        }
    }
    free(owed);
    *gw = sw, *gh = shh;
    return(level);
}

// Draws the levels through an n by n mask of byte thresholds, tiled over
// the picture as drawn.  Each threshold is scaled to a level, then br
// levels are added, and the result is clamped to the lowest and highest
// level.  A sample is drawn as a square of 1 << dsh pixels a side, and the
// bw by bh pixels starting at x0, y0 are drawn.  A pixel is white if its
// level is above the threshold at its position.  black[] receives one byte
// per pixel, 1 for black.
static void gray_drawn(const int* level, int gw, int bits, const uint8_t* mask, int n, int br,
                       int dsh, int x0, int y0, int bw, int bh, uint8_t* black) {
    int levels = 1 << bits;
    for(int y=0; y < bh; y++) {
        for(int x=0; x < bw; x++) {
            int px = x0 + x, py = y0 + y;
            int t = ((mask[(py % n) * n + px % n] * (levels - 1)) >> 8) + br;
            t = t < 0 ? 0 : t > levels - 1 ? levels - 1 : t;
            black[(size_t)y * bw + x] = !(level[(py >> dsh) * gw + (px >> dsh)] > t);
        }
    }
}

#endif //HEXAFORTH_GRAY_H
