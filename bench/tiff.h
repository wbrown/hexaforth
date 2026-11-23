//
// Reads a whole picture from a TIFF, for the benchmarks' own programs.  It
// reads an uncompressed file with one or three 8-bit samples per pixel, in
// either byte order, with any number of strips.  The machine's reader is
// forth/tiff.fs, and this code was not derived from it.
//

#ifndef HEXAFORTH_BENCH_TIFF_H
#define HEXAFORTH_BENCH_TIFF_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int w, h;                   // width and height in pixels
    int samples;                // samples per pixel: one or three
    uint8_t* bytes;             // the samples, row after row
} tiff;

// Reads a picture.  If it cannot, it prints the reason and exits.
tiff read_tiff(const char* path);

// Returns a one-sample picture as a TIFF file in memory, in which 0 is
// black.  The file is a header block of 1,024 bytes followed by the grays.
// The caller frees the result, and `*len` receives its length.
uint8_t* gray_tiff(const uint8_t* grays, int w, int h, size_t* len);

#endif //HEXAFORTH_BENCH_TIFF_H
