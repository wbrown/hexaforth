//
// boot_test IMAGE.hex DISK.img IPL.bin ERROR.img FORTH APPS.img SECOND.img
//
// SECOND.img is a second disk, which some checks give the machine besides
// DISK.img (mkdisk --disk, of test/second/): it has a source the first
// has not, and one under a label the first has too.  A block's number
// says its disk, and a label is looked for on the first disk first
// (doc/disk.md, Several disks).
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
#define READ    4               // the glyphs of the disk lights' font (spin.fs) that are the arrow for a read,
#define WRITTEN 5               // and for a write: the floppy's four turns are before them
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
    bool second;                // the machine has a second disk as well, SECOND.img
    int lights;                 // how many disks' lights the bar is to show when the desktop is left, if it is run
    int arrow;                  // and the arrow one of them is to have: READ or WRITTEN, or nothing for none looked at,
    int arrow_on;               // and which disk's light has it: no other light is to have one
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
    // The bar then has a light for the one disk, at its right, and none
    // where a second disk's would be.
    {"the ROM alone, and the disk of the boot image, whose block 1 compiles the desktop", true, APPS_DISK,
     NOTHING, "windows\n\x11nwin @", true, WINDOWS, "desktop", 0, NULL, false, 1},
    // With a second disk it has a light for each, the second disk's two
    // glyphs to the left of the first's.
    {"the ROM alone, the disk of the boot image and a second disk: the desktop's lights", true, APPS_DISK,
     NOTHING, "windows\n\x11nwin @", true, WINDOWS, "desktop", 0, NULL, true, 2},
    // The arrow is beside the light of the disk the last block moved was
    // of, and is up for a read and down for a write.  The Forth window is
    // in front, and takes what is typed: a block of the second disk is
    // read there, or a block of the first is written, and the desktop is
    // left a few turns of its loop after, while the arrow is still shown.
    {"the desktop with two disks: a read of the second has its arrow at the second's light", true, APPS_DISK,
     NOTHING, "windows\n4097 block drop\r  \x11nwin @", true, WINDOWS, "desktop", 0, NULL, true, 2, READ, 1},
    {"the desktop with two disks: a write of the first has its arrow at the first's light", true, APPS_DISK,
     NOTHING, "windows\n1 block drop update flush\r  \x11nwin @", true, WINDOWS, "desktop", 0, NULL, true, 2,
     WRITTEN, 0},
    {"the ROM alone, and the disk of the boot image: a word of desk.fs with no header", true, APPS_DISK,
     NOTHING, "4 place", false, 0, NULL, 0, NULL},
    // A machine with two disks.  A block's number says its disk: 4097 is
    // block 1 of disk 1, which is the second disk's first source, and
    // that starts with the \ of its first line.
    {"the ROM alone, and two disks: a block of the second, by its number", true, DISK, NOTHING,
     "4097 block c@", true, '\\', BLOCK1_SAID, 0, NULL, true},
    // A label is looked for on every disk that has an index, the first
    // disk first: a source only the second has is found there,
    {"the ROM alone, and two disks: a source only the second has", true, DISK, NOTHING,
     "include second.fs\nsecond", true, 5, "second.fs 1 .\r\n", 0, NULL, true},
    // and a label both have is the first disk's, when the boot's block 1
    // includes it and when it is included again by hand.
    {"the ROM alone, and two disks: a label both have, as block 1 includes it", true, DISK, NOTHING,
     "booted more +", true, BOOTED + MORE, BLOCK1_SAID, 0, NULL, true},
    {"the ROM alone, and two disks: a label both have, included again", true, DISK, NOTHING,
     "include boot-more.fs\nmore", true, MORE, BLOCK1_SAID, 0, NULL, true},
    // With one disk, a block of a disk that is not there is nothing.
    {"the ROM alone, and the disk: a block of a disk that is not there", true, DISK, NOTHING,
     "4097 block c@", true, 0, BLOCK1_SAID, 0, NULL},
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

// The disk lights, at the right of the desktop's bar (wm.fs): a floppy for
// each disk there is, disk 0's at the right and each disk after it one
// light's width to the left, which is the first number of disk-corner.  A
// light is its arrow and then its floppy, which is one of the first four
// glyphs of the font spin: the floppy is ten pixels in from the light's
// left and two down, as draw-light has it.
static font spin;               // the lights' font
static int light_wide;          // how wide one disk's light is
#define FLOPPY_IN   10
#define FLOPPY_DOWN 2
#define TURNS       4           // the floppy's glyphs, a quarter turn apart

// How many pixels of a glyph of spin, its top left at a place on the
// screen, are not as the screen has them.
static int glyph_wrong(int code, int x, int y) {
    int width, wrong = 0;
    int start = glyph_of(&spin, code, &width);
    for(int row=0; row < spin.rows; row++) {
        for(int i=0; i < width; i++) wrong += strike_pixel(&spin, start + i, row) != black_at(x + i, y + row);
    }
    pixels += (long)width * spin.rows;
    return(wrong);
}

// Is a disk's light as it is to be: a floppy, at any of its turns, if the
// disk is there, and no pixel of one if it is not?
static bool light_right(int disk, bool there) {
    int x = DISP_W - (disk + 1) * light_wide + FLOPPY_IN;
    if (there) {
        for(int turn=0; turn < TURNS; turn++) if (!glyph_wrong(turn, x, FLOPPY_DOWN)) return(true);
        return(false);
    }
    int width, black = 0;
    glyph_of(&spin, 0, &width);
    for(int row=0; row < spin.rows; row++) {
        for(int i=0; i < width; i++) black += black_at(x + i, FLOPPY_DOWN + row);
    }
    pixels += (long)width * spin.rows;
    return(!black);
}

// Is a disk's arrow as it is to be: the glyph it is to be, at the left of
// its light, or with `arrow` nothing, no pixel of one?
static bool arrow_right(int disk, int arrow) {
    int x = DISP_W - (disk + 1) * light_wide;
    if (arrow) return(!glyph_wrong(arrow, x, FLOPPY_DOWN));
    int width, black = 0;
    glyph_of(&spin, READ, &width);
    for(int row=0; row < spin.rows; row++) {
        for(int i=0; i < width; i++) black += black_at(x + i, FLOPPY_DOWN + row);
    }
    pixels += (long)width * spin.rows;
    return(!black);
}

typedef struct { uint8_t* bytes; long len; } disk_is;
static disk_is disks[5];        // none, the disk, it with block 0 nothing, the disk with the error, the apps'
static disk_is second;          // the second disk, of the checks that give a machine two
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

// A disk as the machine has it: a copy, in a file of the test's own that
// is gone when it is closed.  So the test changes no disk that was made,
// and each check has the disk as it was made, whatever a check before it
// wrote.  (It was the disk's bytes where they are in memory, as a file:
// what makes memory a file puts a byte of nothing after whatever is
// written to it, so a check that wrote a block made nothing of the first
// byte of the block after, for itself and for every check that followed.)
static FILE* disk_of(disk_is d) {
    FILE* disk = tmpfile();
    if (disk == NULL || fwrite(d.bytes, 1, d.len, disk) != (size_t)d.len) {
        perror("a copy of a disk");
        exit(EXIT_FAILURE);
    }
    rewind(disk);
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
    machine.DISKS[0] = disk;
    machine.DISKBLOCKS[0] = disk ? disks[test->disk].len / DISK_BLOCK : 0;
    FILE* other = test->second ? disk_of(second) : NULL;
    machine.DISKS[1] = other;
    machine.DISKBLOCKS[1] = other ? second.len / DISK_BLOCK : 0;
    if (test->sent == NOTHING_THEN_IPL) {
        uint8_t nothing[DISK_BLOCK] = {0};
        keys(nothing, sizeof(nothing));
    }
    if (test->sent != NOTHING) keys(ipl.bytes, ipl.len);
    line("%d", UNDER);
    line("%s", test->typed);
    type();
    if (disk) fclose(disk);
    if (other) fclose(other);

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
    // The lights: one for each disk the bar is to have, and none for the next.
    for(int d=0; test->lights && d <= test->lights; d++) {
        if (!light_right(d, d < test->lights)) differ++;
    }
    // The arrow: at the light of the disk it is to be at, and at no other.
    for(int d=0; test->arrow && d < test->lights; d++) {
        if (!arrow_right(d, d == test->arrow_on ? test->arrow : 0)) differ++;
    }
    checks++;
    compared += count + 2 + (test->says != NULL);
    if (differ) failed++;
    printf("%s %s: %d cells left, said it has no disk %d times, %d pixels not shown; %d not as they should be\n",
           differ ? "FAIL" : "ok  ", test->label, machine.SP, no_disk, wrong, differ);
    if (differ) printf("     the machine said:\n%s\n", said);
}

int main(int argc, char *argv[]) {
    if (argc < 8) {
        fprintf(stderr, "Usage: %s IMAGE.hex DISK.img IPL.bin ERROR.img FORTH APPS.img SECOND.img\n", argv[0]);
        return(1);
    }
    alarm(600);                 // a machine that does not stop is stopped
    setvbuf(stdout, NULL, _IOLBF, 0);   // and what was said of it until then has been written
    tables_are(argv[5]);
    mono = font_named("mono");
    spin = font_named("spin");
    int64_t* corner;
    table("wm.fs", "disk-corner", &corner, 2);
    light_wide = (int)corner[0];
    nucleus_is(argv[1]);
    disks[DISK] = whole(argv[2]);
    ipl = whole(argv[3]);
    disks[ERROR_DISK] = whole(argv[4]);
    disks[NO_INDEX].len = disks[DISK].len;
    disks[NO_INDEX].bytes = malloc(disks[DISK].len);
    memcpy(disks[NO_INDEX].bytes, disks[DISK].bytes, disks[DISK].len);
    memset(disks[NO_INDEX].bytes, 0, DISK_BLOCK);
    disks[APPS_DISK] = whole(argv[6]);
    second = whole(argv[7]);

    for(check* test = CHECKS; strlen(test->label); test++) run(test);
    printf("boot, cells of %d bits: %d checks, %d failed; %d cells and %ld pixels compared\n",
           CELL_BITS, checks, failed, compared, pixels);
    return(failed || !compared || !pixels);
}
