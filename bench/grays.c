//
// grays PICTURE.tiff GRAYS.tiff SMALL.tiff
//
// Writes a picture's grays as a one-sample TIFF (GRAYS.tiff), and its
// grays at half size (SMALL.tiff).  The grays are computed the way the
// reference dither computes them (test/dither.c).  So the colour file and
// the gray file dither to the same bitmap, and the half-size grays dither
// to the same bitmap as the picture dithered at half size.
//
// The benchmarks dither all three pictures.  Each is on a disk the machine
// is given, under a label.  The Mandrill is on the pictures' disk, where
// the desktop has it.  These two are on a disk of their own, which
// cmake/bench.cmake makes with mkdisk --disk, because a disk holds no more
// than a floppy and the three pictures together are larger than that.
// bench/pictures.fs, typed at the machine, finds each picture by its
// label.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../test/dither.h"
#include "tiff.h"

// Returns a picture's grays as a one-sample TIFF file in memory, at full
// size or half size, and sets `*len` to its length.  The caller frees it.
static uint8_t* grays_file(const tiff* picture, bool half, size_t* len) {
    int w, h;
    int* gray = grays_of(picture->bytes, picture->w, picture->h, picture->samples, half, &w, &h);
    uint8_t* grays = malloc((size_t)w * h);
    for(int idx=0; idx < w * h; idx++) grays[idx] = gray[idx];
    uint8_t* file = gray_tiff(grays, w, h, len);
    free(gray), free(grays);
    return(file);
}

static void write_file(const char* path, const uint8_t* bytes, size_t len) {
    FILE* out = fopen(path, "wb");
    if (out == NULL || fwrite(bytes, 1, len, out) != len) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    fclose(out);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s PICTURE.tiff GRAYS.tiff SMALL.tiff\n", argv[0]);
        return(1);
    }
    tiff picture = read_tiff(argv[1]);
    size_t len, small_len;
    uint8_t* gray_file = grays_file(&picture, false, &len);
    uint8_t* small_file = grays_file(&picture, true, &small_len);
    write_file(argv[2], gray_file, len);
    write_file(argv[3], small_file, small_len);
    printf("%s: its grays, %d bytes, in %s, and its grays at half its size, %d bytes, in %s\n",
           argv[1], (int)len, argv[2], (int)small_len, argv[3]);
    return(0);
}
