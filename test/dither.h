//
// Atkinson's dither, a pixel at a time, as the dither is described.
//
// It is what the dither of forth/atkinson.fs is compared with, by its test
// (dither_test.c) and by the benchmarks (bench/).  It is made apart from
// the machine's, which works a cell of lanes at a time, and nothing of the
// machine's is built from it.
//

#ifndef HEXAFORTH_DITHER_H
#define HEXAFORTH_DITHER_H

#include <stdbool.h>
#include <stdint.h>

// A pixel's gray: its one sample, or of 256 parts 77 of its red, 150 of
// its green and 29 of its blue.
int gray_of(const uint8_t* pixel, int samples);

// The grays of a picture of w by h pixels; or, with `half`, of the picture
// at half its size, where a gray is that of four pixels to the nearest, a
// pixel or a row that is over being left out.  There are *gw by *gh of
// them, and the memory is the caller's to free.
int* grays_of(const uint8_t* bytes, int w, int h, int samples, bool half, int* gw, int* gh);

// The dither.  A pixel is white if its gray and what it is owed come to
// more than 127, and black if not; black[] is a byte a pixel, 1 for black.
void dither(const int* gray, int w, int h, uint8_t* black);

// The cells a bitmap of a part of what was dithered should be: pw by ph
// pixels from px, py of the w by h.  A row is whole cells, *stride of
// them, with the pixels of a cell in the order and the sense the display
// has them (vm_constants.h: DISP_LEFT_LOW, DISP_INK_BLACK), and what of the
// last cell is past the picture is not black.  The memory is the caller's
// to free.
uint64_t* cells_of(const uint8_t* black, int w, int px, int py, int pw, int ph,
                   int cell_bits, bool left_low, bool ink_black, int* stride);

#endif //HEXAFORTH_DITHER_H
