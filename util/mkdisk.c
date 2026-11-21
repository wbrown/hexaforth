//
// mkdisk OUT.img [SOURCE.fs [NAMES]]
// mkdisk --pack SOURCE.fs...
// mkdisk --disk OUT.img FILE...
// mkdisk --boot BITS IMAGE.hex OUT.img IPL.bin SOURCE.fs...
// mkdisk --boot-desk BITS IMAGE.hex OUT.img IPL.bin NAMES SOURCE.fs...
//
// A disk is no more than DISK_FLOPPY blocks, what an 8-inch floppy holds,
// and one that would be more is not made.
//
// A disk for the system: each thing from the block that vm_constants.h says
// is its own.  The Forth source is first, as blocks, as `load` reads them
// and the editor shows them (pack, below).  NAMES is the names the build
// made to go with the image (build/nuc.tables).  After them are the blocks
// kept for HexaPaint's canvas, which are nothing until it is drawn in, and
// then the threshold masks that a picture kept as gray is drawn through:
// blue noise of 64 by 64 and of 32 by 32, and Bayer's of 8 by 8, as planes
// of bits (util/bluenoise.h, mask_planes).  Last are the blocks the viewer
// writes its dithered bitmap to, and those it writes the picture kept as
// gray to, which start out empty.  The picture itself is not on this
// disk: the pictures are on a disk of their own (--disk), and the viewer
// finds one by its label.
//
// With --pack it makes no disk: it lays each file as blocks, and says how
// many blocks it took or why it does not go into blocks.
//
// With --disk it makes a disk that is an index and the files it is given,
// source or data by their names, each found by its label, which is its
// name or what is said before = in front of it (LABEL=FILE): a disk a
// machine has besides the one it starts from (files_disk, below).
//
// With --boot it makes a disk that a machine of cells of BITS bits starts
// from with its ROM alone (doc/boot.md): block 0 is its index
// (doc/disk.md), the sources are from block 1, the first of them being
// what the machine loads, with each file a source includes after them
// (with_included, below), and after them is the image's memory above the
// ROM, which the ROM reads into place.  IPL.bin is what the ROM takes from
// the console where there is no disk: the same runs, as records of cells
// in hex (records, below).
//
// With --boot-desk the disk a machine starts from is the desktop's too: the
// sources are from block 1, one after another, the first being what the
// machine loads; the names, the canvas, the masks and the viewer's bitmap
// are at their own blocks after the source's, as on the first kind of
// disk; after them are blocks kept for the names of the words the boot
// compiles, which the index has an entry for (own-names); and the image is
// after all of those, where the index says.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../vm_constants.h"
#include "bluenoise.h"
#include "image.h"
#include "lines.h"

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
    // Which lines start in a comment in brackets, which are nothing but
    // comments, and which start inside a conditional ([if] with no [then]
    // yet).
    char* starts_in = room_for(n + 1, 1);
    char* only = room_for(n + 1, 1);
    char* skipped = room_for(n + 1, 1);
    piece* pieces = NULL;
    long had = 0;
    where is = {0};
    for(long i=0; i < n; i++) {
        starts_in[i] = is.brackets;
        skipped[i] = is.conditions > 0;
        only[i] = only_comments(lines[i], is.brackets);
        read_line(lines[i], &is, &pieces, &had);
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
            // What skips a conditional stops at the end of the block it is
            // in, and the next block is read as if nothing were skipped:
            // so a conditional is all in one block.
            if (!line && skipped[i]) {
                fprintf(stderr, "mkdisk: %s: line %ld is the first of a block, and is in a conditional that began "
                        "before it: [if] to [then] is to be lines with none of nothing between, in one block\n",
                        what, i + 1);
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
    free(skipped);
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

// What the ROM takes from the console where there is no disk (nuc.fs,
// ipl): the cells of an image from byte `from` to byte `to`, as records in
// hex, RECORD cells to a record.  A record is the cell its first goes in
// and @, each cell and a space, the sum of that cell and those as a cell
// of the machine has it and #, and a line's end.  A cell is its bytes, the
// lowest first.
#define RECORD 8

static void records(FILE* out, const uint8_t* bytes, long from, long to, int bits) {
    int cell = bits / 8;
    uint64_t mask = bits == 64 ? ~0ull : (1ull << bits) - 1;
    for(long at=from; at < to; at += RECORD * cell) {
        uint64_t sum = (uint64_t)(at / cell);
        fprintf(out, "%llx@", (unsigned long long)sum);
        for(long c=at; c < to && c < at + RECORD * cell; c += cell) {
            uint64_t x = 0;
            for(int b=cell - 1; b >= 0; b--) x = x << 8 | bytes[c + b];
            fprintf(out, "%llx ", (unsigned long long)x);
            sum = (sum + x) & mask;
        }
        fprintf(out, "%llx#\n", (unsigned long long)sum);
    }
}

// The disk a machine starts from with its ROM alone.  Its index is block 0,
// and its last slot is kept for an entry that goes on to more of the index
// (doc/disk.md).  The image's memory above the ROM is a run for each stretch
// of kilobytes that has something in it: what is between is nothing in the
// image, and is left as memory has it.  The runs are laid down one after
// another, in the order of their entries; the console is sent the same
// runs, as records, and then ! to end them.
static void write_out(const char* path, const uint8_t* bytes, long blocks) {
    FILE* out = fopen(path, "wb");
    if (out == NULL) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    fwrite(bytes, DISK_BLOCK, blocks, out);
    fclose(out);
}

// What the desktop keeps on the disk a machine starts from, each at the
// block vm_constants.h says is its own: the threshold masks, and the names
// that go with an image if there are any.  The blocks between and after
// them are as they were.  The picture is not here: it is on another disk
// (mkdisk --disk), where it is found by its label.
static void desk_parts(const char* names_path) {
    // Each mask, a row of its planes being so many bytes, up to the block of the next.
    #define MASK_BYTES(n) ((n) * DISK_MASK_PLANES * DISK_MASK_WIDTH / 8)
    _Static_assert(DISK_MASKS_BLOCK + DISK_MASKS_BLOCKS <= DISK_VIEW_BLOCK, "the viewer's bitmap is after the masks");
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

    if (names_path) {
        uint8_t* bytes;
        size_t len = read_file(names_path, &bytes);
        place("the names", DISK_NAMES_BLOCK, DISK_CANVAS_BLOCK, bytes, len);
        free(bytes);
    }
}

// A disk is no more blocks than a floppy holds: one that would be is not
// made, and what is too much for it goes on another (mkdisk --disk).
static void fits_a_floppy(const char* path, long blocks) {
    if (blocks > DISK_FLOPPY) {
        fprintf(stderr, "mkdisk: %s is %ld blocks, and a disk is %d\n", path, blocks, DISK_FLOPPY);
        exit(EXIT_FAILURE);
    }
}

// The blocks the desktop's things take, from the start of a disk: the last
// of them are those kept for the viewer's bitmap and for its gray, which
// are nothing until the viewer makes them.
_Static_assert(DISK_VIEW_BLOCK + DISK_VIEW_BLOCKS <= DISK_GRAY_BLOCK, "the viewer's gray is after its bitmap");
#define DESK_BLOCKS (DISK_GRAY_BLOCK + DISK_GRAY_BLOCKS)

// If it is the desktop's disk as well (desk), the sources are from block 1
// and stop where the names start, the desktop's things are where they are
// on any disk of the desktop's, and the image, which the index says the
// place of, is after them all.
int boot_disk(int bits, const char* image_path, const char* out_path, const char* ipl_path,
              int desk, const char* names_path, int sources, char* source_paths[]) {
    uint8_t* image;
    size_t image_len = load_image(image_path, &image);
    uint8_t** texts = calloc(sources, sizeof(uint8_t*));
    size_t* text_lens = calloc(sources, sizeof(size_t));
    long* lines = calloc(sources, sizeof(long));
    long all_lines = 0;
    for(int f=0; f < sources; f++) {
        text_lens[f] = read_file(source_paths[f], &texts[f]);
        lines[f] = text_lens[f] && texts[f][text_lens[f] - 1] != '\n';  // a block for each line is room enough
        for(size_t idx=0; idx < text_lens[f]; idx++) lines[f] += texts[f][idx] == '\n';
        all_lines += lines[f] + 1;
    }
    long kilobytes = (long)(image_len + DISK_BLOCK - 1) / DISK_BLOCK;
    long room = DISK_SOURCE_BLOCK + (desk ? DESK_BLOCKS + DISK_OWN_BLOCKS : 0) + all_lines + kilobytes;
    while (room % DISK_ROUND) room++;
    disk = calloc(room, DISK_BLOCK);
    uint8_t* whole = calloc(kilobytes, DISK_BLOCK);         // the image, to a whole number of kilobytes
    memcpy(whole, image, image_len);
    if (desk) desk_parts(names_path);

    // Each source after the one before, the first at block 1, each an
    // entry of the index labelled with its file's name.  On a disk that is
    // the desktop's too they are all before the names: the source is
    // first, and whoever reads on from block 1 reads source.
    int slots = DISK_BLOCK / DISK_ENTRY - 1, n = 1;
    long at = DISK_SOURCE_BLOCK;
    for(int f=0; f < sources; f++) {
        long limit = desk ? DISK_NAMES_BLOCK : at + lines[f] + 1;
        int took = pack(source_paths[f], texts[f], text_lens[f], disk, (int)at, (int)limit);
        if (took < 0) return(1);
        if (n == slots) {
            fprintf(stderr, "mkdisk: the sources are more than block 0 has slots for\n");
            return(1);
        }
        const char* name = strrchr(source_paths[f], '/');
        entry(n++, name ? name + 1 : source_paths[f], DISK_SOURCE, at, took, 0, lines[f]);
        printf("%s: block %ld, %d blocks\n", source_paths[f], at, took);
        at += took;
    }

    if (desk && at < DESK_BLOCKS) at = DESK_BLOCKS;
    // The machine that boots from it compiles the files of its block 1, and
    // the names that are those files' own are gone from its dictionary when
    // each is read: it keeps them in these blocks, which are nothing until
    // it does, for the decompiler to show a word by (forth/files.fs).
    if (desk) {
        if (n == slots) {
            fprintf(stderr, "mkdisk: block 0 has no slot left for the blocks kept for names\n");
            return(1);
        }
        entry(n++, "own-names", DISK_DATA, at, DISK_OWN_BLOCKS, 0, 0);
        at += DISK_OWN_BLOCKS;
    }
    long runs_from = at;
    int runs = 0;
    FILE* ipl = fopen(ipl_path, "w");
    if (ipl == NULL) {
        perror(ipl_path);
        exit(EXIT_FAILURE);
    }
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
        records(ipl, whole, start * DISK_BLOCK, k * DISK_BLOCK, bits);
        at += k - start;
        runs++;
    }
    fprintf(ipl, "!\n");
    long sent = ftell(ipl);
    fclose(ipl);
    long blocks = at;
    while (blocks % DISK_ROUND) blocks++;
    fits_a_floppy(out_path, blocks);
    char label[DISK_LABEL];
    snprintf(label, sizeof(label), "hexaforth-%d", bits);
    entry(0, label, DISK_INDEX, 0, 1, bits, blocks);

    write_out(out_path, disk, blocks);
    printf("%s: %ld blocks; the image in %d runs, from block %ld\n"
           "%s: the runs' %ld blocks as records, %ld bytes\n",
           out_path, blocks, runs, runs_from, ipl_path, at - runs_from, sent);
    return(0);
}

// A disk that is an index and the files it is given, one after another
// from block 1: a file whose name ends in .fs is source, laid as blocks,
// and any other is data, as it is.  Each has an entry labelled with its
// name.  There is no binary on it, so its own entry says no cell: it is
// not a disk a machine starts from, but one it finds things on by their
// labels, after the disks before it (doc/disk.md, Several disks).
static int is_source(const char* path) {
    size_t len = strlen(path);
    return(len > 3 && !strcmp(path + len - 3, ".fs"));
}

static int files_disk(const char* out_path, int files, char* given[]) {
    uint8_t** bytes = calloc(files, sizeof(uint8_t*));
    size_t* lens = calloc(files, sizeof(size_t));
    long* lines = calloc(files, sizeof(long));
    // A file is said as its path, and is labelled with its name; or as
    // LABEL=PATH, where it is to have another label than its name.
    char** paths = calloc(files, sizeof(char*));
    char** labels = calloc(files, sizeof(char*));
    for(int f=0; f < files; f++) {
        char* is = strchr(given[f], '=');
        const char* name = strrchr(given[f], '/');
        paths[f] = is ? is + 1 : given[f];
        labels[f] = is ? strndup(given[f], (size_t)(is - given[f])) : strdup(name ? name + 1 : given[f]);
    }
    long room = 1;
    for(int f=0; f < files; f++) {
        lens[f] = read_file(paths[f], &bytes[f]);
        lines[f] = lens[f] && bytes[f][lens[f] - 1] != '\n';
        for(size_t idx=0; idx < lens[f]; idx++) lines[f] += bytes[f][idx] == '\n';
        room += is_source(paths[f]) ? lines[f] + 1 : (long)(lens[f] + DISK_BLOCK - 1) / DISK_BLOCK;     // a block a line is room enough
    }
    while (room % DISK_ROUND) room++;
    disk = calloc(room, DISK_BLOCK);

    int slots = DISK_BLOCK / DISK_ENTRY - 1;
    long at = 1;
    for(int f=0; f < files; f++) {
        if (f + 1 == slots) {
            fprintf(stderr, "mkdisk: the files are more than block 0 has slots for\n");
            return(1);
        }
        const char* name = labels[f];
        long took;
        if (is_source(paths[f])) {
            took = pack(paths[f], bytes[f], lens[f], disk, (int)at, (int)room);
            if (took < 0) return(1);
            entry(f + 1, name, DISK_SOURCE, at, took, 0, lines[f]);
        } else {
            took = (long)(lens[f] + DISK_BLOCK - 1) / DISK_BLOCK;
            memcpy(disk + (size_t)at * DISK_BLOCK, bytes[f], lens[f]);
            entry(f + 1, name, DISK_DATA, at, took, 0, (long)lens[f]);
        }
        printf("%s: block %ld, %ld blocks\n", paths[f], at, took);
        at += took;
    }
    long blocks = at;
    while (blocks % DISK_ROUND) blocks++;
    fits_a_floppy(out_path, blocks);
    entry(0, "hexaforth", DISK_INDEX, 0, 1, 0, blocks);
    write_out(out_path, disk, blocks);
    printf("%s: %ld blocks\n", out_path, blocks);
    return(0);
}

// The sources given, and after them each file that one of them includes,
// once: wherever a line says `include NAME`, before any comment it ends
// with, the file NAME from the same directory.  So what the boot compiles
// is said once, in the list the first source is (forth/apps.fs), and the
// disk has each file it names.
static char** with_included(int* sources, char* given[]) {
    int n = *sources, most = n + 8;
    char** paths = malloc(sizeof(char*) * most);
    memcpy(paths, given, sizeof(char*) * n);
    for(int f=0; f < n; f++) {                  // n grows: an included file is looked in too
        uint8_t* text;
        size_t len = read_file(paths[f], &text);
        text[len] = '\0';
        for(char* line = (char*)text; line; ) {
            char* next = strchr(line, '\n');
            size_t line_len = next ? (size_t)(next - line) : strlen(line);
            int including = 0;                  // the word before was include
            for(size_t at = strspn(line, " \t\r"); at < line_len; ) {
                const char* word = line + at;
                size_t word_len = strcspn(word, " \t\r\n");
                at += word_len;
                at += strspn(line + at, " \t\r");
                if (!including) {
                    if (word_len == 1 && *word == '\\') break;      // the rest of the line is a comment
                    including = word_len == 7 && !strncmp(word, "include", 7);
                    continue;
                }
                including = 0;
                int seen = 0;                   // a file of that name is on the disk: its label is the name
                for(int k=0; k < n; k++) {
                    const char* base = strrchr(paths[k], '/');
                    base = base ? base + 1 : paths[k];
                    seen = seen || (strlen(base) == word_len && !strncmp(base, word, word_len));
                }
                if (seen) continue;
                const char* slash = strrchr(paths[f], '/');
                size_t dir = slash ? (size_t)(slash - paths[f] + 1) : 0;
                char* path = malloc(dir + word_len + 1);
                memcpy(path, paths[f], dir);
                memcpy(path + dir, word, word_len);
                path[dir + word_len] = '\0';
                if (n == most) paths = realloc(paths, sizeof(char*) * (most *= 2));
                paths[n++] = path;
            }
            line = next ? next + 1 : NULL;
        }
        free(text);
    }
    *sources = n;
    return(paths);
}

int main(int argc, char *argv[]) {
    if (argc > 1 && !strcmp(argv[1], "--pack")) return(pack_only(argc - 2, argv + 2));
    if (argc > 3 && !strcmp(argv[1], "--disk")) return(files_disk(argv[2], argc - 3, argv + 3));
    if (argc > 6 && !strcmp(argv[1], "--boot")) {
        int sources = argc - 6;
        char** paths = with_included(&sources, argv + 6);
        return(boot_disk(atoi(argv[2]), argv[3], argv[4], argv[5], 0, NULL, sources, paths));
    }
    if (argc > 7 && !strcmp(argv[1], "--boot-desk")) {
        int sources = argc - 7;
        char** paths = with_included(&sources, argv + 7);
        return(boot_disk(atoi(argv[2]), argv[3], argv[4], argv[5], 1, argv[6], sources, paths));
    }
    if (argc < 2 || argv[1][0] == '-') {
        fprintf(stderr, "Usage: %s OUT.img [SOURCE.fs [NAMES]]\n"
                        "       %s --pack SOURCE.fs...\n"
                        "       %s --disk OUT.img FILE...\n"
                        "       %s --boot BITS IMAGE.hex OUT.img IPL.bin SOURCE.fs...\n"
                        "       %s --boot-desk BITS IMAGE.hex OUT.img IPL.bin NAMES SOURCE.fs...\n",
                argv[0], argv[0], argv[0], argv[0], argv[0]);
        return(1);
    }
    int blocks = DESK_BLOCKS;
    while (blocks % DISK_ROUND) blocks++;
    fits_a_floppy(argv[1], blocks);
    disk = calloc(blocks, DISK_BLOCK);

    memset(disk + DISK_SOURCE_BLOCK * DISK_BLOCK, ' ', DISK_BLOCK);
    if (argc > 2) {
        uint8_t* bytes;
        size_t len = read_file(argv[2], &bytes);
        if (pack(argv[2], bytes, len, disk, DISK_SOURCE_BLOCK, DISK_NAMES_BLOCK) < 0) exit(EXIT_FAILURE);
        free(bytes);
    }
    printf("%s: %d blocks\n", argv[1], blocks);
    desk_parts(argc > 3 ? argv[3] : NULL);

    FILE* out = fopen(argv[1], "wb");
    if (out == NULL) {
        perror(argv[1]);
        exit(EXIT_FAILURE);
    }
    fwrite(disk, DISK_BLOCK, blocks, out);
    fclose(out);
    return(0);
}
