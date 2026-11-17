//
// A picture read whole from a TIFF, for the benchmarks' own programs: one
// that is not compressed, of one sample or three of eight bits, in either
// byte order and any number of strips.  The machine's reader is
// forth/tiff.fs, and this is not made from it.
//

#ifndef HEXAFORTH_BENCH_TIFF_H
#define HEXAFORTH_BENCH_TIFF_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int w, h;                   // pixels across and down
    int samples;                // of a pixel: one, or three
    uint8_t* bytes;             // the samples, a row after another
} tiff;

// Read a picture, or say why not and stop.
tiff read_tiff(const char* path);

// Write a picture of one sample as a TIFF, black being nothing: a block of
// 1,024 bytes that says what it is, and then its grays.  Returns the bytes
// of the file, which are the caller's to free, and how many in *len.
uint8_t* gray_tiff(const uint8_t* grays, int w, int h, size_t* len);

#endif //HEXAFORTH_BENCH_TIFF_H
