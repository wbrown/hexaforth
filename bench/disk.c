//
// disk PICTURE.tiff DISK.img OUT.img BLOCKS.fs
//
// The benchmarks' disk: a disk of the system's (util/mkdisk.c), which has
// the picture on it as the TIFF it is, with the same picture's grays as a
// TIFF of one sample put after it.  The grays are the dither's own
// (test/dither.c), so the two files are to make the same bitmap.
//
// BLOCKS.fs is what says, to whoever types at the machine, the block each
// of the two starts at: two constants, colour-picture and gray-picture.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../vm_constants.h"
#include "../test/dither.h"
#include "tiff.h"

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s PICTURE.tiff DISK.img OUT.img BLOCKS.fs\n", argv[0]);
        return(1);
    }
    tiff picture = read_tiff(argv[1]);
    int w, h;
    int* gray = grays_of(picture.bytes, picture.w, picture.h, picture.samples, false, &w, &h);
    uint8_t* grays = malloc((size_t)w * h);
    for(int idx=0; idx < w * h; idx++) grays[idx] = gray[idx];
    size_t len;
    uint8_t* gray_file = gray_tiff(grays, w, h, &len);

    FILE* in = fopen(argv[2], "rb");
    if (in == NULL) {
        perror(argv[2]);
        return(1);
    }
    fseek(in, 0, SEEK_END);
    size_t was = ftell(in);
    fseek(in, 0, SEEK_SET);
    // The grays go in the first block after the disk as it is.
    int gray_block = (was + DISK_BLOCK - 1) / DISK_BLOCK;
    size_t blocks = gray_block + (len + DISK_BLOCK - 1) / DISK_BLOCK;
    uint8_t* disk = calloc(blocks, DISK_BLOCK);
    if (fread(disk, 1, was, in) != was) {
        perror(argv[2]);
        return(1);
    }
    fclose(in);
    memcpy(disk + (size_t)gray_block * DISK_BLOCK, gray_file, len);

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
    fclose(out);
    printf("%s: %d blocks, %s at block %d and its grays at block %d\n",
           argv[3], (int)blocks, argv[1], DISK_PICTURE_BLOCK, gray_block);
    return(0);
}
