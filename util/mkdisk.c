//
// mkdisk PICTURE.tiff OUT.img [SOURCE.fs [NAMES]]
//
// A disk for the system: each file from the block that vm_constants.h says
// is its own.  The Forth source is one block, as `load` reads it and the
// editor shows it: a line of the file is a line of the block, DISK_LINE
// characters with spaces to its end, which is what \ takes a line of a
// block to be.  The picture is a TIFF as it is.  NAMES is the names the
// build made to go with the image
// (build/nuc.tables).  After them are the blocks kept for HexaPaint's
// canvas, which are nothing until it is drawn in.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../vm_constants.h"

// A disk is a whole number of these, as a disk that is a file is made.
#define DISK_ROUND 64

static uint8_t* disk;

// Read a file whole, and return how many bytes it is.
size_t read_file(const char* path, uint8_t** bytes) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    fseek(file, 0, SEEK_END);
    size_t len = ftell(file);
    fseek(file, 0, SEEK_SET);
    *bytes = malloc(len + 1);
    if (fread(*bytes, 1, len, file) != len) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    fclose(file);
    return(len);
}

// Lay bytes down from a block.  They are not to reach the block `limit`,
// which is something else's.
void place(const char* what, int block, int limit, const uint8_t* bytes, size_t len) {
    if (len > (size_t)(limit - block) * DISK_BLOCK) {
        fprintf(stderr, "mkdisk: %s is %d bytes, and runs into block %d\n",
                what, (int)len, limit);
        exit(EXIT_FAILURE);
    }
    memcpy(disk + (size_t)block * DISK_BLOCK, bytes, len);
}

// Source as a block has it: each line of the text is a line of the block,
// which is spaces already.  A line too long for a block's, or more lines
// than a block has, is said and nothing is made.
void lines_of(const char* what, const uint8_t* text, size_t len, uint8_t* block) {
    size_t line = 0, column = 0;
    for(size_t idx=0; idx < len; idx++) {
        if (text[idx] == '\n') {
            line++;
            column = 0;
        } else if (text[idx] != '\r') {
            if (column >= DISK_LINE || line >= DISK_BLOCK / DISK_LINE) {
                fprintf(stderr, "mkdisk: %s: line %d does not fit a block, which is %d lines of %d characters\n",
                        what, (int)line + 1, DISK_BLOCK / DISK_LINE, DISK_LINE);
                exit(EXIT_FAILURE);
            }
            block[line * DISK_LINE + column++] = text[idx] == '\t' ? ' ' : text[idx];
        }
    }
    // A block has nothing between its lines.  The last word of a line that
    // is full and the first of the next, if that starts in its first
    // column, are one word to whatever reads the block, and two in the file.
    for(line = 1; line < DISK_BLOCK / DISK_LINE; line++) {
        if (block[line * DISK_LINE - 1] != ' ' && block[line * DISK_LINE] != ' ') {
            fprintf(stderr, "mkdisk: %s: line %d is all %d characters of a block's line, and line %d starts in "
                    "its first column: in a block they run on\n", what, (int)line, DISK_LINE, (int)line + 1);
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s PICTURE.tiff OUT.img [SOURCE.fs [NAMES]]\n", argv[0]);
        return(1);
    }
    int blocks = DISK_CANVAS_BLOCK + DISK_CANVAS_BLOCKS;
    while (blocks % DISK_ROUND) blocks++;
    disk = calloc(blocks, DISK_BLOCK);

    uint8_t* bytes;
    size_t len;

    memset(disk + DISK_SOURCE_BLOCK * DISK_BLOCK, ' ', DISK_BLOCK);
    if (argc > 3) {
        len = read_file(argv[3], &bytes);
        lines_of(argv[3], bytes, len, disk + DISK_SOURCE_BLOCK * DISK_BLOCK);
        free(bytes);
    }

    len = read_file(argv[1], &bytes);
    place("the picture", DISK_PICTURE_BLOCK, DISK_NAMES_BLOCK, bytes, len);
    free(bytes);
    printf("%s: %d blocks, the picture at block %d (%d bytes)\n",
           argv[2], blocks, DISK_PICTURE_BLOCK, (int)len);

    if (argc > 4) {
        len = read_file(argv[4], &bytes);
        place("the names", DISK_NAMES_BLOCK, DISK_CANVAS_BLOCK, bytes, len);
        free(bytes);
    }

    FILE* out = fopen(argv[2], "wb");
    if (out == NULL) {
        perror(argv[2]);
        exit(EXIT_FAILURE);
    }
    fwrite(disk, DISK_BLOCK, blocks, out);
    fclose(out);
    return(0);
}
