//
// mkdisk PICTURE.tiff OUT.img [SOURCE.fs [NAMES]]
// mkdisk --pack SOURCE.fs...
// mkdisk --boot BITS IMAGE.hex SOURCE.fs OUT.img
//
// A disk for the system: each file from the block that vm_constants.h says
// is its own.  The Forth source is blocks, as `load` reads them and the
// editor shows them (pack, below), up to the picture's.  The picture is a
// TIFF as it is.  NAMES is the names the build made to go with the image
// (build/nuc.tables).  After them are the blocks kept for HexaPaint's
// canvas, which are nothing until it is drawn in, and then the threshold
// masks that a picture kept as gray is drawn through: blue noise of 64 by
// 64 and of 32 by 32, and Bayer's of 8 by 8, as planes of bits
// (util/bluenoise.c, mask_planes).  Last are the blocks the viewer writes
// its dithered bitmap to, which start out empty.
//
// With --pack it makes no disk: it lays each file as blocks, and says how
// many blocks it took or why it does not go into blocks.
//
// With --boot it makes a disk that a machine of cells of BITS bits starts
// from with its ROM alone (doc/boot.md): block 0 is its index
// (doc/disk.md), the source is from block 1, which is what the machine
// loads, and after it is the image's memory above the ROM, which the ROM
// reads into place.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../vm_constants.h"
#include "bluenoise.c"
#include "image.h"
#include "lines.c"

#define BLOCK_LINES (DISK_BLOCK / DISK_LINE)

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

// Source as blocks have it, from the block `block` and not reaching the
// block `limit`: a line of the text is a line of a block, DISK_LINE
// characters with spaces to its end, which is what \ takes a line of a
// block to be.  Whoever writes the source keeps to that width, and the
// blocks are this one's doing:
//
// - A run of lines with no line of nothing in it, that has code in it, is
//   laid in one block, so that nothing that runs over lines is cut by the
//   end of one.  If it does not fit in what is left of a block it starts
//   the next, and if it is longer than a block it is said.
// - A run of nothing but comments goes on into the next block as it is.
// - A line of nothing is laid as it is, but at the top of a block.
//
// A block that is laid in is spaces to its end.  It says how many blocks
// the source took, or what does not fit and then -1.
int pack(const char* what, const uint8_t* text, size_t len, uint8_t* disk, int block, int limit) {
    char** lines = NULL;
    long n = 0, room = 0;
    for(size_t from=0, idx=0; idx <= len; idx++) {
        if (idx < len && text[idx] != '\n') continue;
        if (idx == len && idx == from) break;
        ONE_MORE(lines, n, room);
        char* line = lines[n++] = room_for(idx - from + 1, 1);
        size_t at = 0;
        for(size_t k=from; k < idx; k++) if (text[k] != '\r') line[at++] = text[k] == '\t' ? ' ' : text[k];
        from = idx + 1;
    }
    // Which lines start in a comment in brackets, and which are nothing
    // but comments.
    char* starts_in = room_for(n + 1, 1);
    char* only = room_for(n + 1, 1);
    piece* pieces = NULL;
    long had = 0;
    int brackets = 0;
    for(long i=0; i < n; i++) {
        starts_in[i] = brackets;
        only[i] = only_comments(lines[i], brackets);
        pieces_of(lines[i], &brackets, &pieces, &had);
    }
    free(pieces);

    int at = block, line = 0, last = block, fails = 0;
    long before = -1;                           // the line of the text laid just above, in this block
    memset(disk + (size_t)block * DISK_BLOCK, ' ', DISK_BLOCK);
    for(long i=0; i < n && !fails; ) {
        if (!line_length(lines[i])) {           // a line of nothing
            if (line) line++, before = -1;
            if (line == BLOCK_LINES) at++, line = 0;
            i++;
            continue;
        }
        long end = i;
        int code = 0;
        while (end < n && line_length(lines[end])) code |= !only[end++];
        if (code && end - i > BLOCK_LINES) {
            fprintf(stderr, "mkdisk: %s: lines %ld to %ld have code in them and no line of nothing between them, "
                    "and are more than the %d lines of a block\n", what, i + 1, end, BLOCK_LINES);
            fails = 1;
            break;
        }
        if (code && line + (end - i) > BLOCK_LINES) at++, line = 0, before = -1;
        for(; i < end && !fails; i++) {
            if (line == BLOCK_LINES) at++, line = 0, before = -1;
            if (at >= limit) {
                fprintf(stderr, "mkdisk: %s: line %ld runs into block %d\n", what, i + 1, limit);
                fails = 1;
                break;
            }
            int wide = line_length(lines[i]);
            if (wide > DISK_LINE) {
                fprintf(stderr, "mkdisk: %s: line %ld is %d characters, and a block's line is %d\n",
                        what, i + 1, wide, DISK_LINE);
                fails = 1;
                break;
            }
            // A block has nothing between its lines.  The last word of a
            // line that is full and the first of the next, if that starts
            // in its first column, are one word to whatever reads the
            // block, and two in the file; unless the first is in a comment
            // to the end of its line.
            if (before >= 0 && line_length(lines[before]) == DISK_LINE && lines[i][0] != ' ' &&
                !ends_in_comment(lines[before], DISK_LINE, starts_in[before])) {
                fprintf(stderr, "mkdisk: %s: line %ld is all %d characters of a block's line, and line %ld starts in "
                        "its first column: in a block they run on\n", what, before + 1, DISK_LINE, i + 1);
                fails = 1;
                break;
            }
            uint8_t* to = disk + (size_t)at * DISK_BLOCK;
            if (at > last) {
                memset(to, ' ', DISK_BLOCK);
                last = at;
            }
            memcpy(to + line * DISK_LINE, lines[i], wide);
            before = i;
            line++;
        }
    }
    for(long i=0; i < n; i++) free(lines[i]);
    free(lines);
    free(starts_in);
    free(only);
    return(fails ? -1 : last - block + 1);
}

// Each file laid as blocks somewhere of its own, and what it took said.
int pack_only(int files, char* paths[]) {
    if (!files) {
        fprintf(stderr, "mkdisk --pack: no files were given to lay as blocks\n");
        return(1);
    }
    int failed = 0;
    for(int f=0; f < files; f++) {
        uint8_t* bytes;
        size_t len = read_file(paths[f], &bytes);
        long lines = len && bytes[len - 1] != '\n';         // a block for each line is room enough
        for(size_t idx=0; idx < len; idx++) lines += bytes[idx] == '\n';
        uint8_t* blocks = room_for(lines + 1, DISK_BLOCK);
        int took = pack(paths[f], bytes, len, blocks, 0, (int)lines + 1);
        if (took < 0) failed = 1;
        else printf("%s: %ld lines in %d blocks\n", paths[f], lines, took);
        free(blocks);
        free(bytes);
    }
    return(failed);
}

// A number of an entry of the index, two bytes or four, the lowest first.
static void put(uint8_t* at, long x, int bytes) {
    for(int idx=0; idx < bytes; idx++) at[idx] = (uint8_t)(x >> (8 * idx));
}

// Entry n of the index in block 0: its label, its type, its blocks, where
// it goes in memory if it is binary, and how long it is.  A number of two
// bytes is to fit in them.
static void entry(int n, const char* label, int type, long first, long blocks, long where, long length) {
    size_t wide = strlen(label);
    if (wide >= DISK_LABEL) {
        fprintf(stderr, "mkdisk: the label %s is more than %d characters\n", label, DISK_LABEL - 1);
        exit(EXIT_FAILURE);
    }
    if (first > 0xffff || blocks > 0xffff || where > 0xffff) {
        fprintf(stderr, "mkdisk: %s is blocks %ld to %ld, for kilobyte %ld: more than two bytes say\n",
                label, first, first + blocks - 1, where);
        exit(EXIT_FAILURE);
    }
    uint8_t* at = disk + (size_t)n * DISK_ENTRY;
    at[0] = (uint8_t)wide;
    memcpy(at + 1, label, wide);
    put(at + DISK_ENTRY_TYPE, type, 2);
    put(at + DISK_ENTRY_FIRST, first, 2);
    put(at + DISK_ENTRY_BLOCKS, blocks, 2);
    put(at + DISK_ENTRY_WHERE, where, 2);
    put(at + DISK_ENTRY_LENGTH, length, 4);
}

// The disk a machine starts from with its ROM alone.  Its index is block 0,
// and its last slot is kept for an entry that goes on to more of the index
// (doc/disk.md).  The image's memory above the ROM is a run for each stretch
// of kilobytes that has something in it: what is between is nothing in the
// image, and is left as memory has it.
int boot_disk(int bits, const char* image_path, const char* source_path, const char* out_path) {
    uint8_t* image;
    size_t image_len = load_image(image_path, &image);
    uint8_t* text;
    size_t text_len = read_file(source_path, &text);
    long lines = text_len && text[text_len - 1] != '\n';    // a block for each line is room enough
    for(size_t idx=0; idx < text_len; idx++) lines += text[idx] == '\n';
    long kilobytes = (long)(image_len + DISK_BLOCK - 1) / DISK_BLOCK;
    long room = DISK_SOURCE_BLOCK + lines + 1 + kilobytes;
    while (room % DISK_ROUND) room++;
    disk = calloc(room, DISK_BLOCK);
    uint8_t* whole = calloc(kilobytes, DISK_BLOCK);         // the image, to a whole number of kilobytes
    memcpy(whole, image, image_len);

    int took = pack(source_path, text, text_len, disk, DISK_SOURCE_BLOCK, DISK_SOURCE_BLOCK + (int)lines + 1);
    if (took < 0) return(1);
    const char* name = strrchr(source_path, '/');
    int slots = DISK_BLOCK / DISK_ENTRY - 1, n = 1;
    entry(n++, name ? name + 1 : source_path, DISK_SOURCE, DISK_SOURCE_BLOCK, took, 0, lines);

    long at = DISK_SOURCE_BLOCK + took;
    int runs = 0;
    for(long k = KROM_BYTES / DISK_BLOCK; k < kilobytes; ) {
        const uint8_t* from = whole + k * DISK_BLOCK;
        int nothing = 1;
        for(int idx=0; idx < DISK_BLOCK && nothing; idx++) nothing = !from[idx];
        if (nothing) {
            k++;
            continue;
        }
        long start = k;
        for(k++; k < kilobytes; k++) {
            const uint8_t* next = whole + k * DISK_BLOCK;
            int more = 0;
            for(int idx=0; idx < DISK_BLOCK && !more; idx++) more = next[idx] != 0;
            if (!more) break;
        }
        if (n == slots) {
            fprintf(stderr, "mkdisk: the image is more runs than block 0 has slots for\n");
            return(1);
        }
        char label[DISK_LABEL];
        snprintf(label, sizeof(label), "image %ld", start);
        entry(n++, label, DISK_BINARY, at, k - start, start, (k - start) * DISK_BLOCK);
        memcpy(disk + (size_t)at * DISK_BLOCK, from, (size_t)(k - start) * DISK_BLOCK);
        at += k - start;
        runs++;
    }
    long blocks = at;
    while (blocks % DISK_ROUND) blocks++;
    char label[DISK_LABEL];
    snprintf(label, sizeof(label), "hexaforth-%d", bits);
    entry(0, label, DISK_INDEX, 0, 1, bits, blocks);

    FILE* out = fopen(out_path, "wb");
    if (out == NULL) {
        perror(out_path);
        return(1);
    }
    fwrite(disk, DISK_BLOCK, blocks, out);
    fclose(out);
    printf("%s: %ld blocks: %s from block %d, %d blocks; the image in %d runs, from block %d\n",
           out_path, blocks, source_path, DISK_SOURCE_BLOCK, took, runs, DISK_SOURCE_BLOCK + took);
    return(0);
}

int main(int argc, char *argv[]) {
    if (argc > 1 && !strcmp(argv[1], "--pack")) return(pack_only(argc - 2, argv + 2));
    if (argc == 6 && !strcmp(argv[1], "--boot")) return(boot_disk(atoi(argv[2]), argv[3], argv[4], argv[5]));
    if (argc < 3) {
        fprintf(stderr, "Usage: %s PICTURE.tiff OUT.img [SOURCE.fs [NAMES]]\n"
                        "       %s --pack SOURCE.fs...\n"
                        "       %s --boot BITS IMAGE.hex SOURCE.fs OUT.img\n", argv[0], argv[0], argv[0]);
        return(1);
    }
    _Static_assert(DISK_MASKS_BLOCK + DISK_MASKS_BLOCKS <= DISK_VIEW_BLOCK, "the viewer's bitmap is after the masks");
    int blocks = DISK_VIEW_BLOCK + DISK_VIEW_BLOCKS;
    while (blocks % DISK_ROUND) blocks++;
    disk = calloc(blocks, DISK_BLOCK);

    // Each mask, a row of its planes being so many bytes, up to the block of the next.
    #define MASK_BYTES(n) ((n) * DISK_MASK_PLANES * DISK_MASK_WIDTH / 8)
    _Static_assert(DISK_CANVAS_BLOCK + DISK_CANVAS_BLOCKS <= DISK_MASKS_BLOCK, "the masks are after the canvas");
    _Static_assert(DISK_MASKS_BLOCK * DISK_BLOCK + MASK_BYTES(64) <= DISK_MASK32_BLOCK * DISK_BLOCK,
                   "the mask of 64 by 64 is before the one of 32 by 32");
    _Static_assert(DISK_MASK32_BLOCK * DISK_BLOCK + MASK_BYTES(32) <= DISK_BAYER_BLOCK * DISK_BLOCK,
                   "and that before Bayer's");
    _Static_assert(DISK_BAYER_BLOCK * DISK_BLOCK + MASK_BYTES(8) <= (DISK_MASKS_BLOCK + DISK_MASKS_BLOCKS) * DISK_BLOCK,
                   "the masks are in their blocks");
    uint8_t thresholds[64 * 64];
    blue_bytes(64, thresholds);
    mask_planes(thresholds, 64, disk + (size_t)DISK_MASKS_BLOCK * DISK_BLOCK);
    blue_bytes(32, thresholds);
    mask_planes(thresholds, 32, disk + (size_t)DISK_MASK32_BLOCK * DISK_BLOCK);
    bayer_bytes(thresholds);
    mask_planes(thresholds, 8, disk + (size_t)DISK_BAYER_BLOCK * DISK_BLOCK);

    uint8_t* bytes;
    size_t len;

    memset(disk + DISK_SOURCE_BLOCK * DISK_BLOCK, ' ', DISK_BLOCK);
    if (argc > 3) {
        len = read_file(argv[3], &bytes);
        if (pack(argv[3], bytes, len, disk, DISK_SOURCE_BLOCK, DISK_PICTURE_BLOCK) < 0) exit(EXIT_FAILURE);
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
