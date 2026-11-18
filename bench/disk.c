//
// disk PICTURE.tiff DISK.img OUT.img BLOCKS.fs GRAYS.tiff SMALL.tiff
//
// The benchmarks' disk: the system's disk (util/mkdisk.c), which has the
// picture as a colour TIFF, then the same picture's grays as a one-sample
// TIFF, then its grays at half size.  The grays are the dither's own
// (test/dither.c), so the colour and gray files dither to the same bitmap,
// and the half-size grays dither to the picture dithered at half size.
// The two gray TIFFs are also written as files (GRAYS.tiff and SMALL.tiff),
// so a desktop disk can be made with either as its picture.
//
// BLOCKS.fs defines three constants for whoever types at the machine:
// the block each picture starts at, colour-picture, gray-picture and
// small-picture.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../vm_constants.h"
#include "../test/dither.h"
#include "tiff.h"

// A picture's grays as a one-sample TIFF, at full size or half size.
// Returns the file, which the caller frees, and sets *len to its length.
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
    if (argc != 7) {
        fprintf(stderr, "Usage: %s PICTURE.tiff DISK.img OUT.img BLOCKS.fs GRAYS.tiff SMALL.tiff\n", argv[0]);
        return(1);
    }
    tiff picture = read_tiff(argv[1]);
    size_t len, small_len;
    uint8_t* gray_file = grays_file(&picture, false, &len);
    uint8_t* small_file = grays_file(&picture, true, &small_len);
    write_file(argv[5], gray_file, len);
    write_file(argv[6], small_file, small_len);

    FILE* in = fopen(argv[2], "rb");
    if (in == NULL) {
        perror(argv[2]);
        return(1);
    }
    fseek(in, 0, SEEK_END);
    size_t was = ftell(in);
    fseek(in, 0, SEEK_SET);
    // The grays start at the first block after the system's disk, and the
    // half-size grays at the first block after them.
    int gray_block = (was + DISK_BLOCK - 1) / DISK_BLOCK;
    int small_block = gray_block + (len + DISK_BLOCK - 1) / DISK_BLOCK;
    size_t blocks = small_block + (small_len + DISK_BLOCK - 1) / DISK_BLOCK;
    uint8_t* disk = calloc(blocks, DISK_BLOCK);
    if (fread(disk, 1, was, in) != was) {
        perror(argv[2]);
        return(1);
    }
    fclose(in);
    memcpy(disk + (size_t)gray_block * DISK_BLOCK, gray_file, len);
    memcpy(disk + (size_t)small_block * DISK_BLOCK, small_file, small_len);

    FILE* out = fopen(argv[3], "wb");
    if (out == NULL) {
        perror(argv[3]);
        return(1);
    }
    fwrite(disk, DISK_BLOCK, blocks, out);
    fclose(out);

    out = fopen(argv[4], "w");
    if (out == NULL) {
        perror(argv[4]);
        return(1);
    }
    fprintf(out, "%d constant colour-picture\n", DISK_PICTURE_BLOCK);
    fprintf(out, "%d constant gray-picture\n", gray_block);
    fprintf(out, "%d constant small-picture\n", small_block);
    fclose(out);
    printf("%s: %d blocks, %s at block %d, its grays at block %d, and its grays at half its size at block %d\n",
           argv[3], (int)blocks, argv[1], DISK_PICTURE_BLOCK, gray_block, small_block);
    return(0);
}
