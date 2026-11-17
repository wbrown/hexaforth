//
// bluenoise.c - threshold masks of blue noise and of Bayer's order, for
// drawing a picture kept as gray through them (bench/masks.c, which
// measures how that looks, and util/mkdisk.c, which puts the masks on the
// disk the machine reads them from, as mask_planes() lays them out).  It
// is had by #include, as the tools here have what they share.
//
// The mask is n by n places, each with a rank from 0 up to n * n - 1, made
// by Ulichney's void and cluster on a torus: a place's energy is the sum,
// over the places that are set, of a Gaussian of how far they are.  A start
// of a tenth of the places, by a fixed sequence, is made even by moving the
// set place in the tightest cluster to the largest void until it is the
// same one.  The ranks below the start are then the tightest taken away one
// at a time, and those above it the largest voids filled.  So the places of
// each rank and under are as far apart as can be, and what is drawn
// through the mask looks like what error diffusion makes, where an ordered
// mask makes a grid.  The same n always makes the same mask.
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

// The ranks of a mask of n by n, a row at a time, which are the caller's to
// free.
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

// The mask as the disk has it: a byte a place, a row at a time, each the
// rank out of 256 rather than out of n * n (rank * 256 / (n * n)).  So each
// byte value is had by as many places as each other.
static void blue_bytes(int n, uint8_t* out) {
    int* rank = blue_noise(n);
    for(int p=0; p < n * n; p++) out[p] = (uint8_t)((long)rank[p] * 256 / (n * n));
    free(rank);
}

// Bayer's mask of 8 by 8, a byte a place, a row at a time: the low three
// bits of x and of y xor x, interleaved with the lowest first, and four
// times that.
static void bayer_bytes(uint8_t* out) {
    for(int y=0; y < 8; y++) {
        for(int x=0; x < 8; x++) {
            int v = 0, xx = x, yy = y ^ x;
            for(int b=0; b < 3; b++) v = (v << 2) | ((yy >> b & 1) << 1) | (xx >> b & 1);
            out[y * 8 + x] = (uint8_t)(v * 4);
        }
    }
}

// A mask of n by n as the disk has it (vm_constants.h): each threshold cut
// to one of sixteen levels (the byte times 15, over 256), and the mask laid
// out a row at a time, each row the four planes of those levels, the
// lowest first, each plane DISK_MASK_WIDTH pixels across with the mask as
// many times across as it goes, a pixel a bit, eight to a byte, the
// leftmost the lowest bit of the first.  So a row of a plane is cells of a
// bitmap whose leftmost pixel is the lowest bit, whatever the cell's size,
// and loading it is moving cells.  out is n rows of DISK_MASK_PLANES *
// DISK_MASK_WIDTH / 8 bytes.
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
