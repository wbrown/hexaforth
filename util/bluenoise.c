//
// bluenoise.c - threshold masks of blue noise and of Bayer's order, for
// drawing a picture kept as gray.  bench/masks.c uses them to measure how
// that looks, and util/mkdisk.c puts them on the disk the machine reads
// them from, in the layout mask_planes() makes.  Tools use it by #include,
// as they do with their other shared code.
//
// A mask is n by n places, each with a rank from 0 to n * n - 1, made by
// Ulichney's void-and-cluster method on a torus.  A place's energy is the
// sum of a Gaussian of its distance to every set place.  A starting set of
// a tenth of the places, from a fixed sequence, is evened out by moving the
// set place in the tightest cluster into the largest void, until those are
// the same place.  Then the ranks below the start come from removing the
// tightest cluster one place at a time, and the ranks above from filling
// the largest void.  So the places at each rank and below are as spread
// out as possible, and drawing through the mask looks like error
// diffusion, where an ordered mask makes a grid.  The same n always makes
// the same mask.
//

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../vm_constants.h"

static void* blue_room(size_t n, size_t size) {
    void* p = calloc(n ? n : 1, size);
    if (p == NULL) {
        fprintf(stderr, "there is no room for %zu of %zu bytes\n", n, size);
        exit(EXIT_FAILURE);
    }
    return(p);
}

// The ranks of an n by n mask, row by row.  The caller frees them.
static int* blue_noise(int n) {
    int cells = n * n;
    double* kernel = blue_room(cells, sizeof(double));
    double sigma = 1.5;
    for(int dy=0; dy < n; dy++) {
        for(int dx=0; dx < n; dx++) {
            int x = dx < n - dx ? dx : n - dx, y = dy < n - dy ? dy : n - dy;
            kernel[dy * n + dx] = exp(-(x * x + y * y) / (2 * sigma * sigma));
        }
    }
    uint8_t* on = blue_room(cells, 1);
    double* energy = blue_room(cells, sizeof(double));
    int* rank = blue_room(cells, sizeof(int));
    #define TOGGLE(p, sign) do { int py_ = (p) / n, px_ = (p) % n; \
        for(int y_=0; y_ < n; y_++) for(int x_=0; x_ < n; x_++) \
            energy[y_ * n + x_] += (sign) * kernel[((y_ - py_ + n) % n) * n + (x_ - px_ + n) % n]; } while (0)
    unsigned seed = 12345;
    int ones = cells / 10;
    for(int i=0; i < ones; ) {
        seed = seed * 1103515245u + 12345u;
        int p = (seed >> 8) % cells;
        if (!on[p]) { on[p] = 1; TOGGLE(p, 1); i++; }
    }
    for(;;) {
        int tight = -1, loose = -1;
        for(int p=0; p < cells; p++) {
            if (on[p] && (tight < 0 || energy[p] > energy[tight])) tight = p;
        }
        on[tight] = 0; TOGGLE(tight, -1);
        for(int p=0; p < cells; p++) {
            if (!on[p] && (loose < 0 || energy[p] < energy[loose])) loose = p;
        }
        on[loose] = 1; TOGGLE(loose, 1);
        if (loose == tight) break;
    }
    uint8_t* start = blue_room(cells, 1);
    double* start_energy = blue_room(cells, sizeof(double));
    memcpy(start, on, cells);
    memcpy(start_energy, energy, cells * sizeof(double));
    for(int r = ones - 1; r >= 0; r--) {
        int tight = -1;
        for(int p=0; p < cells; p++) if (on[p] && (tight < 0 || energy[p] > energy[tight])) tight = p;
        on[tight] = 0; TOGGLE(tight, -1);
        rank[tight] = r;
    }
    memcpy(on, start, cells);
    memcpy(energy, start_energy, cells * sizeof(double));
    for(int r = ones; r < cells; r++) {
        int loose = -1;
        for(int p=0; p < cells; p++) if (!on[p] && (loose < 0 || energy[p] < energy[loose])) loose = p;
        on[loose] = 1; TOGGLE(loose, 1);
        rank[loose] = r;
    }
    #undef TOGGLE
    free(kernel), free(on), free(energy), free(start), free(start_energy);
    return(rank);
}

// The mask as bytes, row by row: each place's rank scaled to 0-255
// (rank * 256 / (n * n)), so every byte value occurs equally often.
static void blue_bytes(int n, uint8_t* out) {
    int* rank = blue_noise(n);
    for(int p=0; p < n * n; p++) out[p] = (uint8_t)((long)rank[p] * 256 / (n * n));
    free(rank);
}

// The 8 by 8 Bayer mask as bytes, row by row: the low three bits of x and
// of y xor x, interleaved lowest first, times four.
static void bayer_bytes(uint8_t* out) {
    for(int y=0; y < 8; y++) {
        for(int x=0; x < 8; x++) {
            int v = 0, xx = x, yy = y ^ x;
            for(int b=0; b < 3; b++) v = (v << 2) | ((yy >> b & 1) << 1) | (xx >> b & 1);
            out[y * 8 + x] = (uint8_t)(v * 4);
        }
    }
}

// An n by n mask in the disk's layout (vm_constants.h).  Each threshold is
// scaled to one of sixteen levels (byte * 15 / 256).  Each row is the four
// bit planes of those levels, lowest first; each plane is DISK_MASK_WIDTH
// pixels wide, with the mask repeated across, one bit per pixel, eight to
// a byte, leftmost pixel in the lowest bit of the first byte.  So a plane
// row is cells of a bitmap whose leftmost pixel is the lowest bit, for any
// cell size, and loading it is copying cells.  out gets n rows of
// DISK_MASK_PLANES * DISK_MASK_WIDTH / 8 bytes.
static void mask_planes(const uint8_t* bytes, int n, uint8_t* out) {
    const int row = DISK_MASK_WIDTH / 8;
    memset(out, 0, (size_t)n * DISK_MASK_PLANES * row);
    for(int y=0; y < n; y++) {
        for(int x=0; x < DISK_MASK_WIDTH; x++) {
            int level = bytes[y * n + x % n] * 15 >> 8;
            for(int b=0; b < DISK_MASK_PLANES; b++) {
                if (level >> b & 1) out[(y * DISK_MASK_PLANES + b) * row + x / 8] |= (uint8_t)(1 << (x % 8));
            }
        }
    }
}
