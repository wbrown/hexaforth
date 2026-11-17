//
// A picture kept as gray and drawn through a mask, a pixel at a time, as
// forth/gray.fs says it does.
//
// It is what gray.fs is compared with, by its test (gray_test.c) and by the
// desktop's checks of the picture's menu (deskcheck.c).  It is made apart
// from the machine's, which keeps its levels in planes of bits and draws a
// cell at a time, and nothing of the machine's is built from it.
//
// It is had by #include, as dither.c is not: gray_test.c and deskcheck.c
// each have it in them.
//

#ifndef HEXAFORTH_GRAY_H
#define HEXAFORTH_GRAY_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Bayer's threshold of 8 by 8, as a byte: as bench/masks.c has it, four
// times.
static int gray_bayer(int x, int y) {
    int v = 0, xx = x & 7, yy = (y ^ x) & 7;
    for(int b=0; b < 3; b++) v = (v << 2) | ((yy >> b & 1) << 1) | (xx >> b & 1);
    return(v * 4);
}

// A floored, as an arithmetic shift is: an eighth below nothing too.
static int gray_eighth(int e) {
    return(e >= 0 ? e / 8 : -((-e + 7) / 8));
}

// The levels a picture of w by h grays is kept as: a sample to 1 << sh
// pixels a side, the average of its pixels to the nearest, each the nearest
// of 1 << bits levels to it and what it is owed, and what it is out by is
// passed on, an eighth to each of six, as the dither passes it on.  A pixel
// or a row that is over is left out.  There are *gw by *gh of them, and the
// memory is the caller's to free.
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

// The levels drawn through a mask of n by n thresholds, a byte each, laid
// over the picture as drawn in tiles: each threshold cut to a level, and
// br levels added to it, no further than the first level or the last.  A
// sample is 1 << dsh pixels a side as drawn, and what is drawn is the bw by
// bh pixels from x0, y0 of that.  A pixel is white if its level is more
// than the threshold there; black[] is a byte a pixel, 1 for black.
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
