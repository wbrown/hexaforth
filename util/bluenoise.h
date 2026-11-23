//
// bluenoise.h - blue-noise and Bayer threshold masks, for drawing a
// picture that is stored as gray.  bench/masks.c uses them to measure how
// the result looks.  util/mkdisk.c writes them to the disk the machine
// reads them from, in the layout mask_planes() produces.  A tool uses this
// file by including it.
//
// A mask is an n by n grid.  Each position has a rank from 0 to n * n - 1,
// assigned by Ulichney's void-and-cluster method on a torus:
//
//   A position's energy is the sum, over every set position, of a Gaussian
//   of the distance between the two.
//
//   The starting pattern sets a tenth of the positions, chosen by a fixed
//   pseudo-random sequence.  It is evened out by repeatedly moving the set
//   position in the tightest cluster to the largest void, until the two
//   are the same position.
//
//   The ranks below the starting count are assigned by removing the
//   tightest cluster one position at a time.  The ranks from the starting
//   count up are assigned by filling the largest void one position at a
//   time.
//
// So the positions at or below any rank are spread as evenly as possible.
// Drawing through the mask then looks like error diffusion, where an
// ordered mask shows a grid.  The same n always gives the same mask.
//

#ifndef HEXAFORTH_BLUENOISE_H
#define HEXAFORTH_BLUENOISE_H

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

// Returns the ranks of an n by n blue-noise mask, row by row, in an
// allocated array.  The caller frees it.
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

// Writes the n by n blue-noise mask to `out` as bytes, row by row.  Each
// byte is the position's rank scaled to 0-255 (rank * 256 / (n * n)), so
// every byte value occurs equally often.
static void blue_bytes(int n, uint8_t* out) {
    int* rank = blue_noise(n);
    for(int p=0; p < n * n; p++) out[p] = (uint8_t)((long)rank[p] * 256 / (n * n));
    free(rank);
}

// Writes the 8 by 8 Bayer mask to `out` as bytes, row by row.  Each byte
// is the low three bits of x and of (y xor x), interleaved lowest bit
// first, times four.
static void bayer_bytes(uint8_t* out) {
    for(int y=0; y < 8; y++) {
        for(int x=0; x < 8; x++) {
            int v = 0, xx = x, yy = y ^ x;
            for(int b=0; b < 3; b++) v = (v << 2) | ((yy >> b & 1) << 1) | (xx >> b & 1);
            out[y * 8 + x] = (uint8_t)(v * 4);
        }
    }
}

// Converts an n by n mask of bytes to the layout it has on the disk
// (vm_constants.h), and writes it to `out`.  Each threshold is scaled to
// one of sixteen levels (byte * 15 / 256).  Each row of the mask becomes
// the four bit planes of its levels, lowest plane first.  A plane is
// DISK_MASK_WIDTH pixels wide, with the mask repeated across it, one bit
// per pixel, eight pixels to a byte, and the leftmost pixel in the lowest
// bit of the first byte.  A plane row is therefore whole cells of a bitmap
// whose leftmost pixel is the lowest bit, at any cell size, so the machine
// loads it by copying cells.  `out` receives n rows of
// DISK_MASK_PLANES * DISK_MASK_WIDTH / 8 bytes each.
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

#endif // HEXAFORTH_BLUENOISE_H
