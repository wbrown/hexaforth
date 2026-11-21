//
// masks PICTURE.tiff own|half [OUT-DIRECTORY]
//
// A picture kept as gray, in few bits and maybe at less than its size, and
// made black and white only when it is drawn: each pixel is black where its
// gray is less than a threshold from a mask that is laid over the screen in
// tiles.  On the machine that is done a cell at a time, the grays and the
// mask each kept as planes of bits: whether a cell's grays are more than the
// mask's is a few ands, ors and inverts of whole cells, a few for each bit
// of gray, and so drawing is near what a copy costs.  What this measures is
// how small it is and how it looks, before any of it is written for the
// machine.
//
// The gray is kept in BITS bits (2, 3 or 4), at the picture's resolution,
// at half (one gray for each 2 by 2) or at a quarter (one for each 4 by 4).
// It is made of the picture's grays as the dither makes black and white of
// them, but to as many levels as BITS gives: each is the nearest level, and
// what it is out by is passed on to its neighbours, with the dither's
// weights.  That is done once, when the picture is kept.  A pixel is then
// white if its level is more than the mask's threshold there, the mask's
// thresholds being cut to one less than as many levels: so a level is that
// share of white, in the mask's pattern, and what is between levels is
// what was passed on.
//
// The masks are Bayer's of 8 by 8, and blue noise of 64 by 64 (blue), 32 by
// 32 (blue32) and 16 by 16 (blue16), made by Ulichney's void and cluster
// (util/bluenoise.h, which the disk's masks are made by too): its
// thresholds are in the order that keeps the pixels of each level as far
// apart as can be, so that what it makes looks like what error diffusion
// makes, where Bayer's is a grid.  The smaller the mask the less room it
// takes, and the sooner its pattern is seen to repeat.
//
// With 1 bit at the picture's resolution, a level is black or white and
// this is the dither's way, though not its pixels: the levels are worked
// out with fractions where the dither rounds what it passes on to a whole
// number, and error diffusion is such that a difference that small moves
// which pixels are black, though not how dark a place is.
//
// With each mask, each is also kept in other tones: darker in the middle
// (g1.3, a gamma of 1.3), sharpened before it is made smaller (sharp, over
// 3 by 3; wide, over 5 by 5 and half as much again), and each of those
// darker too.  Kept at less than the picture's size, a dark line or edge is
// averaged with what is round it and drawn as scattered dots: there is as
// much black as there should be, but less of it together, and the picture
// looks lighter than it is.  These are what may put that back.  And kept at
// less than the picture's size, each is drawn as well with the levels made
// smooth between the samples (-smooth), so that a sample is not a square
// of one tone.
//
// For each it prints how many bytes the gray is, against the bitmap's; how
// far it is from the picture's grays when both are blurred over 5 by 5,
// which measures tone and is no judge of detail; how much of it is black;
// and its contrast after that blur against the picture's.  The pictures
// are the judge.  With OUT-DIRECTORY each is written there as a PBM
// (util/png makes a PNG of one): RESOLUTION-BITS-MASK.pbm, with -TONE
// after it for the other tones, and dither.pbm.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../test/dither.h"
#include "tiff.h"
#include "../util/bluenoise.h"

static int pw, ph;
static const int* gray;

static void* room(size_t n, size_t size) {
    void* p = calloc(n ? n : 1, size);
    if (p == NULL) {
        fprintf(stderr, "there is no room for %zu of %zu bytes\n", n, size);
        exit(EXIT_FAILURE);
    }
    return(p);
}

// A mask: its size, and a threshold at each place, of 0 up to `of`.
typedef struct { int n; int* at; int of; const char* name; } mask;

static mask bayer(void) {
    mask m = { 8, room(64, sizeof(int)), 64, "bayer" };
    for(int y=0; y < 8; y++) {
        for(int x=0; x < 8; x++) {
            int v = 0, xx = x, yy = y ^ x;
            for(int b=0; b < 3; b++) {          // the bits of x and of y xor x, interleaved and reversed
                v = (v << 2) | ((yy >> b & 1) << 1) | (xx >> b & 1);
            }
            m.at[y * 8 + x] = v;
        }
    }
    return(m);
}

// Blue noise of n by n (util/bluenoise.h).
static mask blue(int n, const char* name) {
    return((mask){ n, blue_noise(n), n * n, name });
}

// The grays as they are kept, before they are cut to levels: the picture's,
// sharpened (each made further from the mean of the 3 by 3 round it, by as
// much again) if `sharp`, and then each g made 255 (g / 255) ^ gamma, which
// for a gamma over 1 is darker in the middle and the same at the ends.  On
// the machine the gamma could as well be put in the mask's thresholds when
// it is drawn, and cost nothing.
static double* toned(int sharp, double amount, double gamma) {
    double* g = room((size_t)pw * ph, sizeof(double));
    for(int y=0; y < ph; y++) {
        for(int x=0; x < pw; x++) {
            double v = gray[y * pw + x];
            if (sharp) {                        // the mean of the square `sharp` pixels round it
                double sum = 0;
                int n = 0;
                for(int dy=-sharp; dy <= sharp; dy++) for(int dx=-sharp; dx <= sharp; dx++) {
                    int px = x + dx, py = y + dy;
                    if (px >= 0 && px < pw && py >= 0 && py < ph) sum += gray[py * pw + px], n++;
                }
                v += amount * (v - sum / n);
            }
            if (v < 0) v = 0;
            if (v > 255) v = 255;
            g[y * pw + x] = 255.0 * pow(v / 255.0, gamma);
        }
    }
    return(g);
}

// The grays kept, at a resolution (1, 2 or 4 pixels to a side), cut to
// `levels` levels with what each is out by passed on as the dither passes
// it on: the level of each.
static int* levels_of(const double* kept, int scale, int levels, int* lw, int* lh) {
    *lw = (pw + scale - 1) / scale, *lh = (ph + scale - 1) / scale;
    int w = *lw, h = *lh;
    double* g = room((size_t)w * h, sizeof(double));
    for(int y=0; y < h; y++) {
        for(int x=0; x < w; x++) {
            double sum = 0;
            int n = 0;
            for(int dy=0; dy < scale; dy++) for(int dx=0; dx < scale; dx++) {
                int px = x * scale + dx, py = y * scale + dy;
                if (px < pw && py < ph) sum += kept[py * pw + px], n++;
            }
            g[y * w + x] = sum / n / 255.0 * (levels - 1);
        }
    }
    int* level = room((size_t)w * h, sizeof(int));
    static const int AHEAD[6][2] = { {1, 0}, {2, 0}, {-1, 1}, {0, 1}, {1, 1}, {0, 2} };
    for(int y=0; y < h; y++) {
        for(int x=0; x < w; x++) {
            double old = g[y * w + x];
            int v = (int)floor(old + 0.5);
            if (v < 0) v = 0;
            if (v > levels - 1) v = levels - 1;
            level[y * w + x] = v;
            double e = (old - v) / 8;
            for(int a=0; a < 6; a++) {
                int nx = x + AHEAD[a][0], ny = y + AHEAD[a][1];
                if (nx >= 0 && nx < w && ny < h) g[ny * w + nx] += e;
            }
        }
    }
    free(g);
    return(level);
}

// The picture drawn: a pixel is white if its level is more than the mask's
// threshold, cut to levels - 1.
static void draw(const int* level, int lw, int scale, int levels, const mask* m, uint8_t* black) {
    for(int y=0; y < ph; y++) {
        for(int x=0; x < pw; x++) {
            int v = level[(y / scale) * lw + x / scale];
            int t = (int)((long)m->at[(y % m->n) * m->n + x % m->n] * (levels - 1) / m->of);
            black[y * pw + x] = !(v > t);
        }
    }
}

// The same with the levels between the samples made smooth: each pixel's
// is the four nearest samples', weighted by how near each is (bilinear),
// and is white if that is more than the mask's threshold, not cut.
static void draw_smooth(const int* level, int lw, int lh, int scale, int levels, const mask* m, uint8_t* black) {
    for(int y=0; y < ph; y++) {
        double sy = (y + 0.5) / scale - 0.5;
        if (sy < 0) sy = 0;
        if (sy > lh - 1) sy = lh - 1;
        int y0 = (int)sy, y1 = y0 + 1 < lh ? y0 + 1 : y0;
        double fy = sy - y0;
        for(int x=0; x < pw; x++) {
            double sx = (x + 0.5) / scale - 0.5;
            if (sx < 0) sx = 0;
            if (sx > lw - 1) sx = lw - 1;
            int x0 = (int)sx, x1 = x0 + 1 < lw ? x0 + 1 : x0;
            double fx = sx - x0;
            double v = (level[y0 * lw + x0] * (1 - fx) + level[y0 * lw + x1] * fx) * (1 - fy) +
                       (level[y1 * lw + x0] * (1 - fx) + level[y1 * lw + x1] * fx) * fy;
            double t = (m->at[(y % m->n) * m->n + x % m->n] + 0.5) * (levels - 1) / m->of;
            black[y * pw + x] = !(v > t);
        }
    }
}

static double blurred(const uint8_t* black) {
    double sum = 0;
    for(int y=0; y < ph; y++) {
        for(int x=0; x < pw; x++) {
            double g = 0, b = 0;
            int n = 0;
            for(int dy=-2; dy <= 2; dy++) for(int dx=-2; dx <= 2; dx++) {
                int px = x + dx, py = y + dy;
                if (px < 0 || px >= pw || py < 0 || py >= ph) continue;
                g += gray[py * pw + px], b += black[py * pw + px] ? 0 : 255, n++;
            }
            sum += fabs(g - b) / n;
        }
    }
    return(sum / ((double)pw * ph));
}

// How much contrast a picture has after a 5 by 5 blur: how far its pixels
// are from its mean, as a root mean square, of 255.  Of a bitmap if `black`,
// a pixel being 0 or 255; of the grays if not.
static double contrast(const uint8_t* black) {
    double* b = room((size_t)pw * ph, sizeof(double));
    double mean = 0;
    for(int y=0; y < ph; y++) {
        for(int x=0; x < pw; x++) {
            double sum = 0;
            int n = 0;
            for(int dy=-2; dy <= 2; dy++) for(int dx=-2; dx <= 2; dx++) {
                int px = x + dx, py = y + dy;
                if (px < 0 || px >= pw || py < 0 || py >= ph) continue;
                sum += black ? (black[py * pw + px] ? 0 : 255) : gray[py * pw + px], n++;
            }
            b[y * pw + x] = sum / n;
            mean += sum / n;
        }
    }
    mean /= (double)pw * ph;
    double var = 0;
    for(long i=0; i < (long)pw * ph; i++) var += (b[i] - mean) * (b[i] - mean);
    free(b);
    return(sqrt(var / ((double)pw * ph)));
}

// How much of a bitmap is black, of 100.
static double dark(const uint8_t* black) {
    long n = 0;
    for(long i=0; i < (long)pw * ph; i++) n += black[i];
    return(100.0 * n / ((double)pw * ph));
}

static void pbm(const char* dir, const char* name, const uint8_t* black) {
    if (!dir) return;
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s.pbm", dir, name);
    FILE* out = fopen(path, "wb");
    if (!out) { perror(path); exit(EXIT_FAILURE); }
    fprintf(out, "P4\n%d %d\n", pw, ph);
    for(int y=0; y < ph; y++) {
        for(int x=0; x < pw; x += 8) {
            int byte = 0;
            for(int b=0; b < 8; b++) byte |= (x + b < pw && black[y * pw + x + b]) << (7 - b);
            fputc(byte, out);
        }
    }
    fclose(out);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "usage: masks PICTURE.tiff own|half [OUT-DIRECTORY]\n");
        return(1);
    }
    const char* dir = argc > 3 ? argv[3] : NULL;
    tiff picture = read_tiff(argv[1]);
    gray = grays_of(picture.bytes, picture.w, picture.h, picture.samples, !strcmp(argv[2], "half"), &pw, &ph);
    uint8_t* atk = room((size_t)pw * ph, 1);
    dither(gray, pw, ph, atk);
    long bitmap = (long)pw * ph / 8;
    printf("%s at %s size: %d by %d, a bitmap of %ld bytes\n\n", argv[1], strcmp(argv[2], "half") ? "its own" : "half",
           pw, ph, bitmap);
    double should = 0;
    for(long i=0; i < (long)pw * ph; i++) should += 255 - gray[i];
    should = 100.0 * should / 255.0 / ((double)pw * ph);
    double own_contrast = contrast(NULL);
    printf("of the picture's grays, %.1f%% is black\n\n", should);
    printf("%-8s %4s %-6s %-18s %8s %8s %9s %7s %9s\n", "gray at", "bits", "mask", "tone", "bytes", "smaller", "blurred",
           "black", "contrast");
    printf("%-8s %4s %-6s %-18s %8ld %7s  %9.2f %6.1f%% %8.0f%%\n", "dither", "1", "-", "-", bitmap, "1.0x",
           blurred(atk), dark(atk), 100.0 * contrast(atk) / own_contrast);
    pbm(dir, "dither", atk);

    mask masks[] = { bayer(), blue(64, "blue"), blue(32, "blue32"), blue(16, "blue16") };
    int n_masks = sizeof(masks) / sizeof(masks[0]);
    static const struct { const char* name; int scale, bits; } KEPT[] = {
        {"full", 1, 1}, {"full", 1, 2}, {"full", 1, 4},
        {"half", 2, 2}, {"half", 2, 3}, {"half", 2, 4},
        {"quarter", 4, 2}, {"quarter", 4, 4} };
    // The tones each is kept in: as it is, darker in the middle, sharpened,
    // and both; with each mask.
    static const struct { const char* name; int sharp; double amount, gamma; } TONES[] = {
        {"plain", 0, 0, 1.0}, {"g1.3", 0, 0, 1.3}, {"sharp", 1, 1.0, 1.0}, {"sharp-g1.3", 1, 1.0, 1.3},
        {"wide", 2, 1.5, 1.0}, {"wide-g1.3", 2, 1.5, 1.3} };
    int tones = sizeof(TONES) / sizeof(TONES[0]);
    double* kept[sizeof(TONES) / sizeof(TONES[0])];
    for(int t=0; t < tones; t++) kept[t] = toned(TONES[t].sharp, TONES[t].amount, TONES[t].gamma);
    uint8_t* black = room((size_t)pw * ph, 1);
    for(int k=0; k < (int)(sizeof(KEPT) / sizeof(KEPT[0])); k++) {
        int levels = 1 << KEPT[k].bits;
        for(int m=0; m < n_masks; m++) {
            for(int t=0; t < tones; t++) {
                int lw, lh;
                int* level = levels_of(kept[t], KEPT[k].scale, levels, &lw, &lh);
                long bytes = ((long)lw * lh * KEPT[k].bits + 7) / 8;
                // Drawn as each sample's block, and, at less than the
                // picture's resolution, smooth between them.
                for(int smooth=0; smooth < (KEPT[k].scale > 1 ? 2 : 1); smooth++) {
                    if (smooth) draw_smooth(level, lw, lh, KEPT[k].scale, levels, &masks[m], black);
                    else draw(level, lw, KEPT[k].scale, levels, &masks[m], black);
                    char tone[32];
                    snprintf(tone, sizeof(tone), "%s%s", TONES[t].name, smooth ? "-smooth" : "");
                    printf("%-8s %4d %-6s %-18s %8ld %7.2fx  %9.2f %6.1f%% %8.0f%%\n", KEPT[k].name, KEPT[k].bits,
                           masks[m].name, tone, bytes, (double)bitmap / bytes, blurred(black), dark(black),
                           100.0 * contrast(black) / own_contrast);
                    char name[96];
                    if (t || smooth) {
                        snprintf(name, sizeof(name), "%s-%d-%s-%s", KEPT[k].name, KEPT[k].bits, masks[m].name, tone);
                    } else {
                        snprintf(name, sizeof(name), "%s-%d-%s", KEPT[k].name, KEPT[k].bits, masks[m].name);
                    }
                    pbm(dir, name, black);
                    fflush(stdout);
                }
                free(level);
            }
        }
    }
    printf("\nsmaller: the bitmap's bytes over the gray's.  blurred: how far from the grays, both blurred over\n"
           "5 by 5, of 255; it measures tone and not detail.  contrast: how much it varies after the same blur,\n"
           "against how much the picture's grays do: under 100%% is flatter than the picture, over is punchier.\n");
    return(0);
}
