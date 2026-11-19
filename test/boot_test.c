//
// boot_test IMAGE.hex DISK.img IPL.bin ERROR.img FORTH APPS.img
//
// A machine started from its ROM (doc/boot.md).  The disks are ones that
// mkdisk made of the same image (mkdisk --boot): block 0 the index
// (doc/disk.md), the sources from block 1, and the image's memory above
// the ROM in binary runs.  On DISK.img block 1 is test/boot.fs, which
// says it is read and loads test/boot-more.fs by its label; on ERROR.img
// it is test/boot-error.fs, which has a word none knows on its third line.
// IPL.bin is what mkdisk wrote for the console with DISK.img: the runs, as
// records of cells in hex.  APPS.img is the disk of the image a machine boots
// from (BOOT=on), which is the system without what forth/apps.fs lists:
// block 1 is apps.fs, which has the boot compile those files from their
// source on the disk.  The desktop they make is then run, and is to have
// opened its windows when it is left.
//
// A file that is included has names of its own: test/boot-more.fs has a
// word with no header, which is not to be found once the file is read, and
// one that a header names otherwise, which is to be found by the header's
// name and not the definition's.
//
// With nothing in memory but the ROM, the machine is to read the rest of
// the system from the disk, load block 1, and be at its prompt with what
// that block made.  A machine whose image is in memory already is to load
// block 1 from such a disk too, and not to load anything from a disk that
// has no index, or with no disk.  With nothing in memory but the ROM and
// no disk, it is to say so once, take the image from the console as
// records, whatever is sent before them that is not one, and be at its
// prompt.  An error in a block is to be said once, where it
// happened, and what is after it not read.  Where the machine loads block
// 1, the screen is its console too: what block 1 says, and after it the
// label of the file it includes, which include says as the file starts,
// is to be drawn at the top of the screen, in the system's plain font,
// mono, whose glyphs are read from FORTH/mono.fs (tables.h).
//
// What is typed is put over a number put there first.  A machine that does
// not know a word of it empties its stack, and the number is gone.  What
// each check is to leave, and what the machine is to say, is said here and
// not in the sources.
//
// The machine is the nucleus on vm.c, typed at (nucleus.h), and it stops
// at a store into the ROM (ROM_BYTES), where a machine whose kernel is a
// ROM would go wrong.  It says how many checks it made and how many cells
// it compared, and fails if that is none: then it found nothing to test.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "nucleus.h"
#include "tables.h"

#define UNDER  12345            // what is under what is typed leaves
#define BOOTED 42               // what the words test/boot.fs and test/boot-more.fs make leave
#define MORE   8
#define WINDOWS 6               // the windows the desktop opens where there is a disk (desk.fs, windows)
#define NO_DISK_SAID "no disk, ready to receive"
#define BLOCK1_SAID  "block 1 is read"
// and after it the file it includes, as include says it: its label, the blocks it is, and a dot for each
#define FILE_SAID    "boot-more.fs 1 ."
#define BLOCK1_SHOWN BLOCK1_SAID " " FILE_SAID
#define ERROR_SAID   "error -13 in block 1 line 2 at nonesuch\r\n" \
                     ": before ( -- n ) 1 ;  nonesuch  : after ( -- n ) 2 ;"
#define IN_A_BLOCK   " in block "       // what load says of any error in a block (blocks.fs, said-where)

enum { NO_DISK, DISK, NO_INDEX, ERROR_DISK, APPS_DISK };        // the disk a check gives the machine
enum { NOTHING, IPL, NOTHING_THEN_IPL };        // what it sends to the console before anything is typed

typedef struct {
    const char* label;
    bool rom_only;              // nothing in memory but the ROM
    int disk;
    int sent;
    const char* typed;          // what is typed, over UNDER
    bool known;                 // whether the machine knows it: if not, it empties its stack,
    int64_t leaves;             // and if so, what it leaves
    const char* says;           // what the machine is to say once, if anything
    int no_disk;                // how many times the machine is to say it has no disk
    const char* shows;          // what the screen is to show from its top left, if anything
} check;

static check CHECKS[] = {
    {"the ROM alone, and the disk", true, DISK, NOTHING, "booted more +", true, BOOTED + MORE,
     BLOCK1_SAID, 0, BLOCK1_SHOWN},
    {"the image in memory, and the disk", false, DISK, NOTHING, "booted more +", true, BOOTED + MORE,
     BLOCK1_SAID, 0, BLOCK1_SHOWN},
    // A file that is included is said as it starts, once: its label, how
    // many blocks it is, a dot as each starts, and the end of the line.
    {"the ROM alone, and the disk: the file block 1 includes is said", true, DISK, NOTHING, "booted", true, BOOTED,
     FILE_SAID "\r\n", 0, NULL},
    // A word of a file that is included is the file's own unless a header
    // came before it, and what the header names stays under that name.
    {"the ROM alone, and the disk: a word of the file it includes that has no header", true, DISK, NOTHING,
     "eight", false, 0, BLOCK1_SAID, 0, NULL},
    {"the ROM alone, and the disk: the definition a header gave another name", true, DISK, NOTHING,
     "more-of", false, 0, BLOCK1_SAID, 0, NULL},
    {"the image in memory, and the disk: a word of the file it includes that has no header", false, DISK, NOTHING,
     "eight", false, 0, BLOCK1_SAID, 0, NULL},
    // What is found again after such a file is what was found before it:
    // nothing of the dictionary is lost with the file's own names.
    {"the ROM alone, and the disk: a word of the kernel's after the file", true, DISK, NOTHING,
     "more 2 +", true, MORE + 2, BLOCK1_SAID, 0, NULL},
    {"the image in memory, and the disk with no index", false, NO_INDEX, NOTHING, "booted", false, 0, NULL, 0, NULL},
    {"the image in memory, and no disk", false, NO_DISK, NOTHING, "booted", false, 0, NULL, 0, NULL},
    {"the ROM alone and no disk, and the image sent to the console", true, NO_DISK, IPL, "1 2 +", true, 3,
     NULL, 1, NULL},
    {"the ROM alone and no disk, and a block of nothing sent before the records", true, NO_DISK, NOTHING_THEN_IPL,
     "1 2 +", true, 3, NULL, 1, NULL},
    {"the ROM alone and no disk, which has no block 1 to load", true, NO_DISK, IPL, "booted", false, 0, NULL, 1,
     NULL},
    {"the ROM alone, and a block 1 with an error in it, what is before the error", true, ERROR_DISK, NOTHING,
     "before", true, 1, ERROR_SAID, 0, "error -13 in block 1 line 2 at nonesuch"},
    {"the ROM alone, and a block 1 with an error in it, what is after the error", true, ERROR_DISK, NOTHING,
     "after", false, 0, ERROR_SAID, 0, NULL},
    // The desktop that the boot compiled is run: it opens its windows,
    // paints them, and is left with ^Q, which the event loop takes from
    // the console.  What is typed after that is how many windows there
    // are.  A word of desk.fs's own is not found: the file has been read.
    {"the ROM alone, and the disk of the boot image, whose block 1 compiles the desktop", true, APPS_DISK,
     NOTHING, "windows\n\x11nwin @", true, WINDOWS, "desktop", 0, NULL},
    {"the ROM alone, and the disk of the boot image: a word of desk.fs with no header", true, APPS_DISK,
     NOTHING, "4 place", false, 0, NULL, 0, NULL},
    {""}};

static font mono;               // the system's plain font, which the console draws in
static long pixels;             // how many pixels of the screen were compared

// How many pixels of the text, drawn in mono from the top left of the
// screen, are not as the screen has them.
static int not_shown(const char* text) {
    int wrong = 0, x = 0;
    for(const char* c = text; *c; c++) {
        int width;
        int start = glyph_of(&mono, *c, &width);
        for(int row=0; row < mono.rows; row++) {
            for(int i=0; i < width; i++) wrong += strike_pixel(&mono, start + i, row) != black_at(x + i, row);
        }
        pixels += (long)width * mono.rows;
        x += width;
    }
    return(wrong);
}

typedef struct { uint8_t* bytes; long len; } disk_is;
static disk_is disks[5];        // none, the disk, it with block 0 nothing, the disk with the error, the apps'
static disk_is ipl;             // what mkdisk wrote for the console
static int checks, compared, failed;

static disk_is whole(const char* path) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    disk_is d;
    fseek(file, 0, SEEK_END);
    d.len = ftell(file);
    fseek(file, 0, SEEK_SET);
    d.bytes = malloc(d.len);
    if (d.len < 2 * DISK_BLOCK || fread(d.bytes, 1, d.len, file) != (size_t)d.len) {
        fprintf(stderr, "%s is not a disk, or what is sent for one\n", path);
        exit(EXIT_FAILURE);
    }
    fclose(file);
    return(d);
}

// A disk as the machine reads it: from memory, so that the test changes
// no file.
static FILE* disk_of(disk_is d) {
    FILE* disk = fmemopen(d.bytes, d.len, "r+");
    if (disk == NULL) {
        perror("fmemopen");
        exit(EXIT_FAILURE);
    }
    return(disk);
}

// How many times the machine said it.
static int times_said(const char* what) {
    int n = 0;
    for(const char* at = strstr(said, what); at; at = strstr(at + 1, what)) n++;
    return(n);
}

static void run(const check* test) {
    start();
    if (test->rom_only) {
        memset((uint8_t*)machine.memory + KROM_BYTES, 0, sizeof(machine.memory) - KROM_BYTES);
    }
    FILE* disk = test->disk == NO_DISK ? NULL : disk_of(disks[test->disk]);
    machine.DISK = disk;
    machine.DISKBLOCKS = disk ? disks[test->disk].len / DISK_BLOCK : 0;
    if (test->sent == NOTHING_THEN_IPL) {
        uint8_t nothing[DISK_BLOCK] = {0};
        keys(nothing, sizeof(nothing));
    }
    if (test->sent != NOTHING) keys(ipl.bytes, ipl.len);
    line("%d", UNDER);
    line("%s", test->typed);
    type();
    if (disk) fclose(disk);

    int count = test->known ? 2 : 0;    // what is to be left: the number and what is typed leaves, or nothing
    int differ = 0;
    if (machine.SP != count) {
        differ = count ? count : 1;
    } else if (test->known) {
        differ = (left(1) != UNDER) + (left(0) != (uint64_t)test->leaves);
    }
    int no_disk = times_said(NO_DISK_SAID);
    if (no_disk != test->no_disk) differ++;
    if (test->says && times_said(test->says) != 1) differ++;
    if (times_said("error: -1 ")) differ++;    // -1 is thrown on from a block, and says nothing
    // An error in a block is said only where one is put there: a boot
    // that says one has not compiled what follows it, though what was
    // before it may be all a check types.
    if (times_said(IN_A_BLOCK) != (test->says && strstr(test->says, IN_A_BLOCK) ? 1 : 0)) differ++;
    int wrong = test->shows ? not_shown(test->shows) : 0;
    if (wrong) differ++;
    checks++;
    compared += count + 2 + (test->says != NULL);
    if (differ) failed++;
    printf("%s %s: %d cells left, said it has no disk %d times, %d pixels not shown; %d not as they should be\n",
           differ ? "FAIL" : "ok  ", test->label, machine.SP, no_disk, wrong, differ);
    if (differ) printf("     the machine said:\n%s\n", said);
}

int main(int argc, char *argv[]) {
    if (argc < 7) {
        fprintf(stderr, "Usage: %s IMAGE.hex DISK.img IPL.bin ERROR.img FORTH APPS.img\n", argv[0]);
        return(1);
    }
    alarm(600);                 // a machine that does not stop is stopped
    tables_are(argv[5]);
    mono = font_named("mono");
    nucleus_is(argv[1]);
    disks[DISK] = whole(argv[2]);
    ipl = whole(argv[3]);
    disks[ERROR_DISK] = whole(argv[4]);
    disks[NO_INDEX].len = disks[DISK].len;
    disks[NO_INDEX].bytes = malloc(disks[DISK].len);
    memcpy(disks[NO_INDEX].bytes, disks[DISK].bytes, disks[DISK].len);
    memset(disks[NO_INDEX].bytes, 0, DISK_BLOCK);
    disks[APPS_DISK] = whole(argv[6]);

    for(check* test = CHECKS; strlen(test->label); test++) run(test);
    printf("boot, cells of %d bits: %d checks, %d failed; %d cells and %ld pixels compared\n",
           CELL_BITS, checks, failed, compared, pixels);
    return(failed || !compared || !pixels);
}
