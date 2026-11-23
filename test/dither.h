//
// Atkinson's dither, one pixel at a time, written directly from the
// description of the algorithm.
//
// It is the reference that the dither in forth/atkinson.fs is compared
// with, by its test (dither_test.c) and by the benchmarks (bench/).  It
// was written independently of the machine's dither, which works on a cell
// of lanes at a time, and nothing in the machine is built from it.
//

#ifndef HEXAFORTH_DITHER_H
#define HEXAFORTH_DITHER_H

#include <stdbool.h>
#include <stdint.h>

// Returns a pixel's gray level.  For a pixel with one sample, that is the
// sample.  For an RGB pixel it is
// (77 * red + 150 * green + 29 * blue) / 256.
int gray_of(const uint8_t* pixel, int samples);

// Returns the gray levels of a picture of w by h pixels.  With `half`, it
// returns them for the picture at half size: each gray is the average of
// four pixels, rounded to the nearest, and an odd last column or row is
// dropped.  The result is *gw by *gh values in allocated memory, which the
// caller frees.
int* grays_of(const uint8_t* bytes, int w, int h, int samples, bool half, int* gw, int* gh);

// Dithers a picture.  A pixel is white if its gray plus the error passed
// to it is more than 127, and black otherwise.  black[] receives one byte
// per pixel, 1 for black.
void dither(const int* gray, int w, int h, uint8_t* black);

// Returns the cells that a bitmap of part of the dithered picture should
// contain: the pw by ph pixels starting at px, py of the w by h picture.
// A row is *stride whole cells.  The pixels within a cell have the bit
// order and the polarity the display uses (vm_constants.h: DISP_LEFT_LOW,
// DISP_INK_BLACK).  The part of a row's last cell that lies past the
// picture is not black.  The cells are in allocated memory, which the
// caller frees.
uint64_t* cells_of(const uint8_t* black, int w, int px, int py, int pw, int ph,
                   int cell_bits, bool left_low, bool ink_black, int* stride);

#endif //HEXAFORTH_DITHER_H
