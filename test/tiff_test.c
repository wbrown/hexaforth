//
// tiff_test IMAGE.hex [SEED]
//
// Tests the TIFF reader in forth/tiff.fs, which reads a file from the disk
// a row at a time, against the file's bytes read here.
//
// The disk is filled with random bytes, the same every time for a given
// seed.  A file on it is only a header and a directory written over those
// bytes: its pixels are whatever the disk holds where its strips are said
// to be.  That gives a file of any size, with strips wherever the test
// wants them.
//
// A check is a file: its size, its samples per pixel, how its rows are
// divided into strips and where the strips are, and how its directory
// records all that.  The files cover both byte orders; one, two and three
// samples; strips that start on block and cell boundaries and strips that
// do not; strips that are adjacent, apart, and out of order; strip offsets
// stored as LONGs and as SHORTs, inside the directory entry and outside
// it; and one file whose directory spans the end of a block.  The machine
// is the nucleus on vm.c (nucleus.h) with that disk.  Each file is opened
// and dithered on the machine (tiff-open, atkinson), and the bitmap is
// read out of the machine's memory.
//
// It is a contrastive test.  The grays are computed here from the same
// bytes, following what TIFF defines each field to mean, and are dithered
// one pixel at a time (dither.c).  Nothing is built from either
// implementation.
//
// After the single files, it dithers one file twice; dithers two files a
// row at a time in turn; opens all the files before dithering any; and
// dithers one file of 512 by 342 pixels with three samples, which is
// 525KB, on a machine that has 128KB when its cells are 16 bits.  After
// that it allocates memory until none is left.  The request that cannot be
// met must be refused with an error, not granted.
//
// It prints how many files and how many bitmap cells it compared.  It
// fails if it compared none, because then it tested nothing.
//

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "nucleus.h"
#include "dither.h"

#define SPACING     64          // each file starts this many blocks after the one before it
#define FILES       16          // the most files in one check
#define BIG         640         // the blocks reserved after them for the last file, which is big
#define DISK_BLOCKS ((FILES + 1) * SPACING + BIG)
#define STRIPS      64          // the most strips a file in this test has
#define UNDER       12345       // the number pushed under whatever a check leaves
#define REFUSED     (-8)        // the error thrown when a memory request cannot be met

// The description of one test file.
typedef struct {
    const char* label;
    int w, h, samples;
    int rows;                   // rows per strip; if 0, the file is one strip, and has no RowsPerStrip unless `says` is set
    long says;                  // with `rows` 0: the RowsPerStrip the file records, which is more rows than the file has
    int first;                  // the offset in the file of its first strip
    int further;                // if nonzero, a random amount below this is added to that offset
    int gap;                    // the bytes between one strip and the next
    bool big_endian;
    bool white_is_zero;         // for one sample: 0 is white, not black
    bool shorts;                // the strip offsets are SHORTs, not LONGs
    bool backwards;             // the strips are in the file in the reverse order of their rows
    bool odd;                   // its first strip starts at an odd byte
    int directory;              // the offset of its directory; if 0, it follows the header
} tiff_file;

static tiff_file TESTS[] = {
    {.label = "strips on blocks and cells", .w = 64, .h = 20, .samples = 3, .rows = 4, .first = 1024},
    {.label = "strips at odd bytes", .w = 64, .h = 12, .samples = 3, .rows = 5,
     .first = 257, .further = 742, .odd = true, .gap = 7},
    {.label = "the first strip on a cell, the rest not", .w = 64, .h = 12, .samples = 3, .rows = 4,
     .first = 1024, .gap = 5},
    {.label = "big-endian, white is zero, a strip a row", .w = 37, .h = 11, .samples = 1, .rows = 1,
     .first = 300, .further = 40, .gap = 3, .big_endian = true, .white_is_zero = true},
    {.label = "one strip and no RowsPerStrip, its place a SHORT", .w = 50, .h = 9, .samples = 1,
     .first = 2048, .further = 64, .shorts = true},
    {.label = "big-endian, places an array of SHORTs", .w = 33, .h = 7, .samples = 3, .rows = 3,
     .first = 700, .further = 99, .big_endian = true, .shorts = true},
    {.label = "two places, SHORTs in the entry", .w = 40, .h = 6, .samples = 1, .rows = 3,
     .first = 600, .further = 99, .gap = 1, .shorts = true},
    {.label = "two samples", .w = 24, .h = 5, .samples = 2, .rows = 2, .first = 512},
    {.label = "the directory across the end of a block", .w = 48, .h = 8, .samples = 3, .rows = 8,
     .first = 4096, .directory = 1017},
    {.label = "strips out of order", .w = 48, .h = 9, .samples = 3, .rows = 3,
     .first = 1100, .gap = 2000 - 432, .backwards = true},
    {.label = "one sample, a strip of two rows, strips apart", .w = 37, .h = 20, .samples = 1, .rows = 2,
     .first = 900, .gap = 11},
    // RowsPerStrip set to its default for a file that omits it: the
    // largest value a LONG can hold.
    {.label = "one strip, and more rows to a strip than it has", .w = 50, .h = 9, .samples = 1,
     .says = 0xffffffff, .first = 1500, .further = 64},
    {.label = "one strip of three samples, and a row more to a strip than it has", .w = 40, .h = 6,
     .samples = 3, .says = 7, .first = 3000, .further = 64},
    {.label = ""}};

// The two files that are dithered a row at a time in turn, which have the
// same number of rows; the file that is dithered twice; and the big file.
#define TURN_A  0
#define TURN_B  10
#define TWICE   1
static tiff_file LARGE = {.label = "an image bigger than a small machine's memory",
                          .w = 512, .h = 342, .samples = 3, .rows = 64, .first = 1024, .further = 4};

// ---- random numbers

static uint64_t seed;
static uint32_t chance(void) {
    seed ^= seed << 13;
    seed ^= seed >> 7;
    seed ^= seed << 17;
    return((uint32_t)(seed >> 32));
}

// ---- the disk, and a file on it

static uint8_t* bytes;          // the disk's random bytes
static uint8_t* disk;           // the disk: those bytes, with the test's files written over them

// A file as it was written to the disk.
typedef struct {
    const tiff_file* file;
    long block;                 // the block it starts at
    int strips;                 // its number of strips
    long place[STRIPS];         // the offset of each strip in the file
    long length[STRIPS];        // the length of each strip in bytes
} laid;

static bool big_endian;         // the byte order of the file being written
static uint8_t* writing;        // the address where that file starts

static void put2(long at, unsigned v) {
    writing[at + (big_endian ? 0 : 1)] = v >> 8;
    writing[at + (big_endian ? 1 : 0)] = v;
}

static void put4(long at, unsigned long v) {
    put2(at + (big_endian ? 0 : 2), v >> 16);
    put2(at + (big_endian ? 2 : 0), v & 0xffff);
}

// Writes one directory entry: a tag, the type (SHORT or LONG), the count,
// and the values.  Values that fit in four bytes go in the entry itself.
// Otherwise they go after the directory, at the offset the entry records.
// `*after` is the offset where the next such values go.
static void entry(long at, int tag, bool shorts, int count, const long* values, long* after) {
    int size = shorts ? 2 : 4;
    long to = at + 8;
    put2(at, tag);
    put2(at + 2, shorts ? 3 : 4);
    put4(at + 4, count);
    put4(to, 0);
    if (size * count > 4) {
        put4(to, *after);
        to = *after;
        *after += size * count + (size * count & 1);
    }
    for(int idx=0; idx < count; idx++) {
        if (shorts) put2(to + 2 * idx, values[idx]); else put4(to + 4 * idx, values[idx]);
    }
}

// Writes a file's header and directory over the disk, starting at a
// block.  Returns the file's block and the offsets of its strips.
static laid lay(const tiff_file* f, long block) {
    laid l = { f, block };
    long row = (long)f->w * f->samples;
    int rows = f->rows ? f->rows : f->h;
    l.strips = (f->h + rows - 1) / rows;
    if (l.strips > STRIPS) {
        fprintf(stderr, "tiff_test: %s: more strips than %d\n", f->label, STRIPS);
        exit(EXIT_FAILURE);
    }
    long at = f->first + (f->further ? chance() % f->further : 0);
    if (f->odd) at |= 1;
    for(int idx=0; idx < l.strips; idx++) {
        int n = f->backwards ? l.strips - 1 - idx : idx;        // the strip at this position in the file
        int left = f->h - n * rows;
        l.place[n] = at;
        l.length[n] = row * (left < rows ? left : rows);
        at += l.length[n] + f->gap;
    }

    writing = disk + block * DISK_BLOCK;
    big_endian = f->big_endian;
    long dir = f->directory ? f->directory : 8;
    writing[0] = writing[1] = big_endian ? 'M' : 'I';
    put2(2, 42);
    put4(4, dir);
    int entries = f->rows || f->says ? 9 : 8;
    long next = dir + 2, after = dir + 2 + 12 * entries + 4;
    long eights[3] = { 8, 8, 8 };
    long w = f->w, h = f->h, samples = f->samples, plain = 1, said_rows = f->says ? f->says : f->rows;
    long white = f->white_is_zero ? 0 : f->samples == 3 ? 2 : 1;
    put2(dir, entries);
    entry(next, 256, false, 1, &w, &after), next += 12;
    entry(next, 257, true, 1, &h, &after), next += 12;
    entry(next, 258, true, f->samples, eights, &after), next += 12;
    entry(next, 259, true, 1, &plain, &after), next += 12;
    entry(next, 262, true, 1, &white, &after), next += 12;
    entry(next, 273, f->shorts, l.strips, l.place, &after), next += 12;
    entry(next, 277, true, 1, &samples, &after), next += 12;
    if (said_rows) entry(next, 278, false, 1, &said_rows, &after), next += 12;
    entry(next, 279, false, l.strips, l.length, &after), next += 12;
    put4(next, 0);

    for(int idx=0; idx < l.strips; idx++) {
        if (l.place[idx] < after) {
            fprintf(stderr, "tiff_test: %s: a strip is in the directory\n", f->label);
            exit(EXIT_FAILURE);
        }
        if ((block * DISK_BLOCK + l.place[idx] + l.length[idx]) > (long)DISK_BLOCKS * DISK_BLOCK) {
            fprintf(stderr, "tiff_test: %s: a strip is past the end of the disk\n", f->label);
            exit(EXIT_FAILURE);
        }
    }
    return(l);
}

// Computes the cells the file's bitmap should contain: its grays,
// computed from its bytes as they are on the disk, dithered one pixel at a
// time.  A pixel with three samples is red, green and blue.  With one or
// two samples, the first sample is the gray, or 255 minus the gray if 0 is
// white in the file.
static int expected(const laid* l, uint64_t** cells, int* stride) {
    const tiff_file* f = l->file;
    const uint8_t* file = disk + l->block * DISK_BLOCK;
    int rows = f->rows ? f->rows : f->h;
    int* gray = malloc(sizeof(int) * f->w * f->h);
    for(int y=0; y < f->h; y++) {
        const uint8_t* row = file + l->place[y / rows] + (long)(y % rows) * f->w * f->samples;
        for(int x=0; x < f->w; x++) {
            int g = gray_of(row + x * f->samples, f->samples == 3 ? 3 : 1);
            gray[y * f->w + x] = f->white_is_zero ? 255 - g : g;
        }
    }
    uint8_t* black = calloc((size_t)f->w * f->h + 1, 1);
    dither(gray, f->w, f->h, black);
    *cells = cells_of(black, f->w, 0, 0, f->w, f->h, CELL_BITS, DISP_LEFT_LOW, DISP_INK_BLACK, stride);
    free(gray);
    free(black);
    return(*stride * f->h);
}

// ---- the machine

static int files, compared, failed;

// Restores the disk to its random bytes, with no files on it.
static void disk_start(void) {
    memcpy(disk, bytes, (size_t)DISK_BLOCKS * DISK_BLOCK);
}

// Starts the machine with the disk attached.  The machine finds the disk
// when it starts.
static FILE* machine_start(void) {
    start();
    machine.DISKS[0] = fmemopen(disk, (size_t)DISK_BLOCKS * DISK_BLOCK, "rb");
    machine.DISKBLOCKS[0] = DISK_BLOCKS;
    return(machine.DISKS[0]);
}

// Compares a bitmap in the machine's memory with the bitmap a file should
// give.  `bitmap` is its address, and `row` is its number of cells per
// row.
static void look(const char* how, const laid* l, uint64_t bitmap, uint64_t row) {
    uint64_t* want;
    int stride;
    int total = expected(l, &want, &stride);
    int differ = 0, first = -1;
    if (row != (uint64_t)stride || !in_memory(bitmap, total)) {
        differ = total;
    } else {
        for(int idx=0; idx < total; idx++) {
            if (fetch(bitmap + idx) != want[idx]) {
                if (first < 0) first = idx;
                differ++;
            }
        }
    }
    files++;
    compared += total;
    if (differ) failed++;
    printf("%s %s%s: %d by %d, %d to a pixel, %d strips: %d cells of bitmap, %d differ\n",
           differ ? "FAIL" : "ok  ", how, l->file->label, l->file->w, l->file->h, l->file->samples,
           l->strips, total, differ);
    if (first >= 0) {
        printf("     cell %d of row %d should be %llx and is %llx\n", first % stride, first / stride,
               (unsigned long long)want[first], (unsigned long long)fetch(bitmap + first));
    } else if (differ) {
        printf("     a row of the bitmap is %llu cells at %llx, and should be %d\n",
               (unsigned long long)row, (unsigned long long)bitmap, stride);
    }
    free(want);
}

// Returns true if the typed lines left what they should: UNDER, and above
// it the address and the cells per row of each of `count` bitmaps.
static bool left_bitmaps(const char* what, int count) {
    if (machine.SP == 1 + 2 * count && left(2 * count) == UNDER) return(true);
    printf("FAIL %s: the machine did not do as it was asked.  It said:\n%s\n", what, said);
    files += count;
    failed += count;
    return(false);
}

// Tests one file on its own: opens it and dithers it, and with `twice`
// dithers it a second time into another bitmap.
static void alone(const tiff_file* f, bool twice) {
    disk_start();
    laid l = lay(f, SPACING);
    FILE* d = machine_start();
    line("create b0 0 , 0 ,  create b1 0 , 0 ,");
    line("%ld tiff-open constant i0", l.block);
    line("b0 i0 atkinson");
    if (twice) line("b1 i0 atkinson");
    line("%d  b0 @ b0 1+ @", UNDER);
    if (twice) line("b1 @ b1 1+ @");
    type();
    fclose(d);
    if (!left_bitmaps(f->label, twice ? 2 : 1)) return;
    if (twice) {
        look("", &l, left(3), left(2));
        look("dithered a second time: ", &l, left(1), left(0));
    } else {
        look("", &l, left(1), left(0));
    }
}

// Tests two files at once, dithered a row at a time in turn.  Each has
// its own window.
static void in_turn(const tiff_file* a, const tiff_file* b) {
    disk_start();
    laid la = lay(a, SPACING), lb = lay(b, 2 * SPACING);
    FILE* d = machine_start();
    line("create b0 0 , 0 ,  create b1 0 , 0 ,");
    line("%ld tiff-open constant i0  %ld tiff-open constant i1", la.block, lb.block);
    line("b0 i0 dither-start constant d0  b1 i1 dither-start constant d1");
    line(": rows %d 0 do d0 dither-row d1 dither-row loop ; rows", a->h);
    line("%d  b0 @ b0 1+ @  b1 @ b1 1+ @", UNDER);
    type();
    fclose(d);
    if (!left_bitmaps("two at once", 2)) return;
    look("two at once, the first: ", &la, left(3), left(2));
    look("two at once, the second: ", &lb, left(1), left(0));
}

// Opens all the files, and then dithers each, the last one opened first.
// An image is a record, and the state of one image must not be shared
// with another.
static void together(void) {
    laid l[FILES];
    int count = 0;
    disk_start();
    for(const tiff_file* f = TESTS; strlen(f->label); f++, count++) l[count] = lay(f, (count + 1) * SPACING);
    FILE* d = machine_start();
    for(int n=0; n < count; n++) {
        line("create b%d 0 , 0 ,  %ld tiff-open constant i%d", n, l[n].block, n);
    }
    for(int n=count - 1; n >= 0; n--) line("b%d i%d atkinson", n, n);
    line("%d", UNDER);
    for(int n=0; n < count; n++) line("b%d @ b%d 1+ @", n, n);
    type();
    fclose(d);
    if (!left_bitmaps("all opened, then each dithered", count)) return;
    for(int n=0; n < count; n++) {
        int at = 2 * (count - 1 - n);
        look("opened with the rest: ", &l[n], left(at + 1), left(at));
    }
}

// Tests the big file.  Then it allocates memory a block at a time until
// none is left, which must throw an error that the test catches.  Returns
// true if that error was thrown.
static bool large(void) {
    disk_start();
    laid l = lay(&LARGE, (FILES + 1) * SPACING);
    FILE* d = machine_start();
    line("create b0 0 , 0 ,  %ld tiff-open constant i0", l.block);
    line("b0 i0 atkinson");
    line(": all begin %d halloc drop again ;", DISK_BLOCK);
    line("%d  b0 @ b0 1+ @  ' all catch", UNDER);
    type();
    fclose(d);
    if (machine.SP != 4 || left(3) != UNDER) {
        printf("FAIL %s: the machine did not do as it was asked.  It said:\n%s\n", LARGE.label, said);
        files++;
        failed++;
        return(false);
    }
    look("", &l, left(2), left(1));
    bool refused = left(0) == as_cell(REFUSED);
    printf("%s memory asked for until there is none: it is %s\n", refused ? "ok  " : "FAIL",
           refused ? "refused" : "not refused as it should be");
    return(refused);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s IMAGE.hex [SEED]\n", argv[0]);
        return(1);
    }
    int given = argc > 2 ? atoi(argv[2]) : 1;
    seed = 0x9e3779b97f4a7c15ull * (uint64_t)(given + 1);
    if (!seed) seed = 1;        // a zero seed would produce only zeros
    alarm(1200);                // kill the test if the machine never stops

    nucleus_is(argv[1]);
    size_t len = (size_t)DISK_BLOCKS * DISK_BLOCK;
    bytes = malloc(len);
    disk = malloc(len);
    for(size_t idx=0; idx < len; idx++) bytes[idx] = chance() >> 8;

    for(const tiff_file* f = TESTS; strlen(f->label); f++) alone(f, false);
    alone(&TESTS[TWICE], true);
    in_turn(&TESTS[TURN_A], &TESTS[TURN_B]);
    together();
    bool refused = large();

    printf("the TIFF reader, cells of %d bits, seed %d: %d files, %d not as their bytes dithered a pixel at "
           "a time; %d cells of bitmap compared; memory that is not there %s\n",
           CELL_BITS, given, files, failed, compared, refused ? "refused" : "NOT refused");
    return(failed || !compared || !refused);
}
