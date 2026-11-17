//
// tiff_test IMAGE.hex [SEED]
//
// The TIFF reader of forth/tiff.fs, which reads a file from the disk a row
// at a time, against the file's bytes read here.
//
// The disk is bytes taken by chance, the same every time for a seed, and a
// file on it is only its header and its directory, written over them: its
// pixels are whatever the disk holds where its strips say they are.  So a
// file of any size is had, with strips wherever the test says.
//
// A check is a file: how big it is, how many samples a pixel has, how its
// rows are cut into strips and where those are, and how its directory says
// so.  The files are of either byte order, of one sample, two and three,
// with strips that are and are not where blocks and cells start, next to
// each other, apart and out of order, their places LONGs and SHORTs, in
// the entry and out of it, and one has its directory across the end of a
// block.  The machine is the nucleus on vm.c, typed at (nucleus.h), with
// that disk: each file is opened and dithered there (tiff-open, atkinson),
// and the bitmap is read out of the machine's memory.
//
// It is a contrastive test: the grays are made here of the same bytes, by
// what TIFF says a file's fields mean, and dithered a pixel at a time
// (dither.c).  Nothing is built from either.
//
// Then one file is dithered twice; two are dithered a row of each in
// turn; all of them are opened before any is dithered; and one of 512 by
// 342 of three samples is dithered, which is 525KB, on a machine that
// with cells of 16 bits has 128KB.  When that is done memory is asked for
// until there is none, which is to be refused and not given.
//
// It says how many files and how many cells of bitmap it compared, and
// fails if that is none: then it found nothing to test.
//

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "nucleus.h"
#include "dither.h"

#define SPACING     64          // a file is this many blocks of the disk after the one before it
#define FILES       16          // the most files a check has, and
#define BIG         640         // the blocks kept after them for the last, which is big
#define DISK_BLOCKS ((FILES + 1) * SPACING + BIG)
#define STRIPS      64          // the most strips a file here has
#define UNDER       12345       // what is under what a check leaves
#define REFUSED     (-8)        // what is thrown when there is not the memory asked for

// A file.
typedef struct {
    const char* label;
    int w, h, samples;
    int rows;                   // rows to a strip; with none, it is one strip and the file does not say,
    long says;                  // or says this, which is more rows than it has
    int first;                  // where in the file its first strip is,
    int further;                // or up to this much further on, by chance,
    int gap;                    // and the bytes between one strip and the next
    bool big_endian;
    bool white_is_zero;         // of one sample: nothing is white, and not black
    bool shorts;                // the strips' places are SHORTs, and not LONGs
    bool backwards;             // the strips are in the file in the other order from the rows they are
    bool odd;                   // its first strip is at an odd byte
    int directory;              // where its directory is: after the header, if it is not said
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
    // What a file that does not say is taken to say: the most a LONG can.
    {.label = "one strip, and more rows to a strip than it has", .w = 50, .h = 9, .samples = 1,
     .says = 0xffffffff, .first = 1500, .further = 64},
    {.label = "one strip of three samples, and a row more to a strip than it has", .w = 40, .h = 6,
     .samples = 3, .says = 7, .first = 3000, .further = 64},
    {.label = ""}};

// The two that are dithered a row of each in turn, which are as many rows;
// the one dithered twice; and the big one.
#define TURN_A  0
#define TURN_B  10
#define TWICE   1
static tiff_file LARGE = {.label = "an image bigger than a small machine's memory",
                          .w = 512, .h = 342, .samples = 3, .rows = 64, .first = 1024, .further = 4};

// ---- chance

static uint64_t seed;
static uint32_t chance(void) {
    seed ^= seed << 13;
    seed ^= seed >> 7;
    seed ^= seed << 17;
    return((uint32_t)(seed >> 32));
}

// ---- the disk, and a file on it

static uint8_t* bytes;          // the disk's bytes as chance had them,
static uint8_t* disk;           // and the disk: those, with the files that are on it

// A file as it is on the disk.
typedef struct {
    const tiff_file* file;
    long block;                 // the block it starts at,
    int strips;                 // its strips,
    long place[STRIPS];         // where in the file each is,
    long length[STRIPS];        // and how many bytes
} laid;

static bool big_endian;         // of the file being written
static uint8_t* writing;        // where it starts

static void put2(long at, unsigned v) {
    writing[at + (big_endian ? 0 : 1)] = v >> 8;
    writing[at + (big_endian ? 1 : 0)] = v;
}

static void put4(long at, unsigned long v) {
    put2(at + (big_endian ? 0 : 2), v >> 16);
    put2(at + (big_endian ? 2 : 0), v & 0xffff);
}

// An entry of the directory: a tag, SHORTs or LONGs, and their values.
// Values that fit in four bytes are in the entry, and more are after the
// directory, where the entry says; *after is where the next such go.
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

// The file's header and directory, written over the disk from a block.
// Returns where it is and where its strips are.
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
        int n = f->backwards ? l.strips - 1 - idx : idx;        // the strip that is here in the file
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

// The cells the file's bitmap should be: its grays, made of its bytes as
// they are on the disk, dithered a pixel at a time.  A pixel of three
// samples is red, green and blue; of one or two its first sample is its
// gray, or what is left of white if nothing is white in the file.
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

// The disk as chance had it, with nothing on it.
static void disk_start(void) {
    memcpy(disk, bytes, (size_t)DISK_BLOCKS * DISK_BLOCK);
}

// The machine started, with the disk, and told that its disk is this VM's.
static FILE* machine_start(void) {
    start();
    machine.DISK = fmemopen(disk, (size_t)DISK_BLOCKS * DISK_BLOCK, "rb");
    machine.DISKBLOCKS = DISK_BLOCKS;
    line("vmdisk-init");
    return(machine.DISK);
}

// A bitmap in the machine's memory, at a place and of so many cells to a
// row, against what a file's should be.
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

// What was typed left, over the number that is under it, a bitmap's place
// and its cells to a row for each of `count` bitmaps: are they there?
static bool left_bitmaps(const char* what, int count) {
    if (machine.SP == 1 + 2 * count && left(2 * count) == UNDER) return(true);
    printf("FAIL %s: the machine did not do as it was asked.  It said:\n%s\n", what, said);
    files += count;
    failed += count;
    return(false);
}

// A file by itself: opened and dithered, and with `twice` dithered again
// to another bitmap.
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

// Two at once, a row of each in turn: each has its own window.
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

// All of them opened, and then each dithered, the last opened first: an
// image is a record, and what one has in hand is not another's.
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

// The big one; and then memory is taken a block at a time until there is
// none, which is thrown and caught.  Returns whether it was.
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
    if (!seed) seed = 1;        // nothing comes of nothing
    alarm(1200);                // a machine that does not stop is stopped

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
