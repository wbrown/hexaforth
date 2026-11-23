//
// boot_test IMAGE.hex DISK.img IPL.bin ERROR.img FORTH APPS.img SECOND.img
//
// Tests a machine that starts from its ROM (doc/boot.md).
//
// mkdisk made the disks from the same image (mkdisk --boot): block 0 is
// the index (doc/disk.md), the sources start at block 1, and the image's
// memory above the ROM follows as binary runs.
//
//   DISK.img     Block 1 is test/boot.fs, which prints that it has been
//                read and loads test/boot-more.fs by its label.
//   ERROR.img    Block 1 is test/boot-error.fs, which has an undefined
//                word on its third line.
//   IPL.bin      What mkdisk wrote for the console along with DISK.img:
//                the runs, as records of cells in hex.
//   APPS.img     The disk of the boot image (BOOT=on), which is the system
//                without the files forth/apps.fs lists.  Block 1 is
//                apps.fs, which makes the boot compile those files from
//                their source on the disk.  The test then runs the desktop
//                they define, which must have opened its windows by the
//                time the test leaves it.
//   SECOND.img   A second disk, which some checks give the machine along
//                with DISK.img (mkdisk --disk, from test/second/).  It has
//                one source the first disk lacks, and one under a label
//                the first disk also has.  A block's number says which
//                disk it is on, and a label is looked up on the first disk
//                first (doc/disk.md, Several disks).
//
// An included file has names of its own.  test/boot-more.fs has a word
// with no header, which must not be found once the file has been read.  It
// also has a definition that a header gives another name, which must be
// found by the header's name and not by the definition's.
//
// What the checks require:
//
//   With nothing in memory but the ROM, the machine reads the rest of the
//   system from the disk, loads block 1, and is at its prompt with the
//   words that block defined.
//
//   A machine whose image is already in memory also loads block 1 from
//   such a disk.  It loads nothing from a disk that has no index, or when
//   there is no disk.
//
//   With nothing in memory but the ROM and no disk, the machine says so
//   once, reads the image from the console as records, ignoring anything
//   sent before them that is not a record, and is at its prompt.
//
//   An error in a block is reported once, with where it happened, and the
//   rest of the block is not read.
//
//   Where the machine loads block 1, the screen is its console too.  The
//   text block 1 prints, followed by the label of the file it includes
//   (which `include` prints as the file starts), must be drawn at the top
//   of the screen in the system's plain font, mono.  The glyphs to compare
//   with are read from FORTH/mono.fs (tables.h).
//
// Before the typed text of each check a known number is pushed.  A machine
// that meets a word it does not know empties its stack, and the number is
// gone.  What each check must leave and what the machine must print are
// given here, not in the sources.
//
// The machine is the nucleus on vm.c (nucleus.h).  It stops at a store
// into the ROM (ROM_BYTES), which is where a machine whose kernel really
// is a ROM would fail.  It prints how many checks it made and how many
// cells it compared, and fails if it compared none, because then it tested
// nothing.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "nucleus.h"
#include "tables.h"

#define UNDER  12345            // the number pushed under whatever the typed text leaves
#define BOOTED 42               // the value left by the word test/boot.fs defines
#define MORE   8                // the value left by the word test/boot-more.fs defines
#define WINDOWS 6               // the number of windows the desktop opens when there is a disk (desk.fs, windows)
#define CLOSE_ALL    "\x11\x11\x11\x11\x11\x11"                 // one ^Q for each window: each closes the window in front
#define CLOSE_OTHERS "\x17\x11\x17\x11\x17\x11\x17\x11\x17\x11" // ^W ^Q for each window except the Forth window, which stays in front
#define READ    4               // the glyph of the disk lights' font (spin.fs) that is the arrow for a read
#define WRITTEN 5               // the glyph that is the arrow for a write; the floppy's four rotations come before these two
#define NO_DISK_SAID "no disk, ready to receive"
#define BLOCK1_SAID  "block 1 is read"
// What `include` prints after that for the file block 1 includes: its
// label, its number of blocks, and a dot for each block.
#define FILE_SAID    "boot-more.fs 1 ."
#define BLOCK1_SHOWN BLOCK1_SAID " " FILE_SAID
#define ERROR_SAID   "error -13 in block 1 line 2 at nonesuch\r\n" \
                     ": before ( -- n ) 1 ;  nonesuch  : after ( -- n ) 2 ;"
#define IN_A_BLOCK   " in block "       // text that `load` prints for any error in a block (blocks.fs, said-where)

enum { NO_DISK, DISK, NO_INDEX, ERROR_DISK, APPS_DISK };        // which disk a check gives the machine
enum { NOTHING, IPL, NOTHING_THEN_IPL };        // what a check sends to the console before anything is typed

typedef struct {
    const char* label;
    bool rom_only;              // true if the machine starts with nothing in memory but the ROM
    int disk;
    int sent;
    const char* typed;          // the text typed, after UNDER has been pushed
    bool known;                 // true if the machine knows every word of it; if not, it empties its stack
    int64_t leaves;             // the value the text leaves, if the machine knows it
    const char* says;           // text the machine must print exactly once, or NULL
    int no_disk;                // the number of times the machine must say it has no disk
    const char* shows;          // text the screen must show starting at its top left, or NULL
    bool second;                // true if the machine also has the second disk, SECOND.img
    int lights;                 // the number of disk lights the bar must show when the desktop is left; 0 if the desktop is not run
    int arrow;                  // the arrow one light must have, READ or WRITTEN; 0 if the arrows are not checked
    int arrow_on;               // the disk whose light has the arrow; no other light may have one
} check;

static check CHECKS[] = {
    {"the ROM alone, and the disk", true, DISK, NOTHING, "booted more +", true, BOOTED + MORE,
     BLOCK1_SAID, 0, BLOCK1_SHOWN},
    {"the image in memory, and the disk", false, DISK, NOTHING, "booted more +", true, BOOTED + MORE,
     BLOCK1_SAID, 0, BLOCK1_SHOWN},
    // `include` announces a file once, as it starts: its label, its number
    // of blocks, a dot as each block starts, and then a newline.
    {"the ROM alone, and the disk: the file block 1 includes is said", true, DISK, NOTHING, "booted", true, BOOTED,
     FILE_SAID "\r\n", 0, NULL},
    // A word in an included file is private to the file unless a header
    // precedes it.  A definition that a header names stays available under
    // the header's name.
    {"the ROM alone, and the disk: a word of the file it includes that has no header", true, DISK, NOTHING,
     "eight", false, 0, BLOCK1_SAID, 0, NULL},
    {"the ROM alone, and the disk: the definition a header gave another name", true, DISK, NOTHING,
     "more-of", false, 0, BLOCK1_SAID, 0, NULL},
    {"the image in memory, and the disk: a word of the file it includes that has no header", false, DISK, NOTHING,
     "eight", false, 0, BLOCK1_SAID, 0, NULL},
    // A private cell of the file, and a word the file defines between
    // `meta` and `target`, are placed in the scratch (placement.fs,
    // files.fs).  Once the file has been read, the words that used them
    // still work, the scratch holds no code, and neither name is found.
    {"the ROM alone, and the disk: a word that used a cell of the file's own and a word of its scratch", true, DISK,
     NOTHING, "five-twice", true, 5 + 6, BLOCK1_SAID, 0, NULL},
    {"the image in memory, and the disk: the same", false, DISK, NOTHING, "five-twice", true, 5 + 6, BLOCK1_SAID, 0,
     NULL},
    {"the ROM alone, and the disk: the scratch when the file is read", true, DISK, NOTHING, "scratch @ i@", true, 0,
     BLOCK1_SAID, 0, NULL},
    {"the ROM alone, and the disk: the cell's name", true, DISK, NOTHING, "own-five", false, 0, BLOCK1_SAID, 0, NULL},
    {"the ROM alone, and the disk: the name of the word of the scratch", true, DISK, NOTHING, "twice", false, 0,
     BLOCK1_SAID, 0, NULL},
    // After such a file, the words that could be found before it can still
    // be found: removing the file's private names loses nothing else from
    // the dictionary.
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
    // Run the desktop the boot compiled.  It opens its windows and paints
    // them.  The event loop reads ^Q from the console: each one closes the
    // window in front, and one more with no window open leaves the desktop.
    // The text typed after that pushes the number of windows that were
    // made.  A private word of desk.fs is not found, because the file has
    // been read.  The bar must then show a light for the one disk, at its
    // right end, and nothing where a second disk's light would be.
    {"the ROM alone, and the disk of the boot image, whose block 1 compiles the desktop", true, APPS_DISK,
     NOTHING, "windows\n" CLOSE_ALL "\x11" "nawin @", true, WINDOWS, "desktop", 0, NULL, false, 1},
    // With a second disk the bar has a light for each disk.  The second
    // disk's two glyphs are to the left of the first disk's.
    {"the ROM alone, the disk of the boot image and a second disk: the desktop's lights", true, APPS_DISK,
     NOTHING, "windows\n" CLOSE_ALL "\x11" "nawin @", true, WINDOWS, "desktop", 0, NULL, true, 2},
    // The arrow is beside the light of the disk the last block transfer
    // used.  It points up for a read and down for a write.  Every window
    // except the Forth window is closed first, each brought to the front
    // with ^W and closed with ^Q, because a window may read or write blocks
    // as it comes to the front or leaves it.  The Forth window then gets
    // the typed text, which reads a block of the second disk or writes a
    // block of the first.  The desktop is left a few turns of its loop
    // later, while the arrow is still showing.
    {"the desktop with two disks: a read of the second has its arrow at the second's light", true, APPS_DISK,
     NOTHING, "windows\n" CLOSE_OTHERS "4097 block drop\r  \x11\x11" "nawin @", true, WINDOWS, "desktop", 0, NULL,
     true, 2, READ, 1},
    {"the desktop with two disks: a write of the first has its arrow at the first's light", true, APPS_DISK,
     NOTHING, "windows\n" CLOSE_OTHERS "1 block drop update flush\r  \x11\x11" "nawin @", true, WINDOWS, "desktop",
     0, NULL, true, 2, WRITTEN, 0},
    {"the ROM alone, and the disk of the boot image: a word of desk.fs with no header", true, APPS_DISK,
     NOTHING, "4 place", false, 0, NULL, 0, NULL},
    // A machine with two disks.  A block's number says which disk it is
    // on: 4097 is block 1 of disk 1, which is the second disk's first
    // source, and that source starts with the \ of its first line.
    {"the ROM alone, and two disks: a block of the second, by its number", true, DISK, NOTHING,
     "4097 block c@", true, '\\', BLOCK1_SAID, 0, NULL, true},
    // A label is looked up on every disk that has an index, the first disk
    // first.  A source that only the second disk has is found there.
    {"the ROM alone, and two disks: a source only the second has", true, DISK, NOTHING,
     "include second.fs\nsecond", true, 5, "second.fs 1 .\r\n", 0, NULL, true},
    // A label that both disks have resolves to the first disk's file, both
    // when the boot's block 1 includes it and when it is included again by
    // hand.
    {"the ROM alone, and two disks: a label both have, as block 1 includes it", true, DISK, NOTHING,
     "booted more +", true, BOOTED + MORE, BLOCK1_SAID, 0, NULL, true},
    {"the ROM alone, and two disks: a label both have, included again", true, DISK, NOTHING,
     "include boot-more.fs\nmore", true, MORE, BLOCK1_SAID, 0, NULL, true},
    // With one disk, a block of a disk that is not present reads as zeros.
    {"the ROM alone, and the disk: a block of a disk that is not there", true, DISK, NOTHING,
     "4097 block c@", true, 0, BLOCK1_SAID, 0, NULL},
    {""}};

static font mono;               // the system's plain font, which the console draws with
static long pixels;             // the number of screen pixels compared

// Returns the number of pixels where the screen differs from the text
// drawn in mono starting at the screen's top left.
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

// The disk lights are at the right end of the desktop's bar (wm.fs).
// There is a floppy for each disk present: disk 0's is at the right, and
// each later disk's is one light's width further left.  That width is the
// first number of disk-corner.  A light is its arrow followed by its
// floppy, which is one of the first four glyphs of the font spin.  The
// floppy is drawn ten pixels in from the light's left edge and two pixels
// down, as draw-light draws it.
static font spin;               // the font of the lights
static int light_wide;          // the width of one disk's light
#define FLOPPY_IN   10
#define FLOPPY_DOWN 2
#define TURNS       4           // the number of floppy glyphs, each a quarter turn from the last

// Returns the number of pixels where the screen differs from a glyph of
// spin drawn with its top left at x, y.
static int glyph_wrong(int code, int x, int y) {
    int width, wrong = 0;
    int start = glyph_of(&spin, code, &width);
    for(int row=0; row < spin.rows; row++) {
        for(int i=0; i < width; i++) wrong += strike_pixel(&spin, start + i, row) != black_at(x + i, y + row);
    }
    pixels += (long)width * spin.rows;
    return(wrong);
}

// Returns true if a disk's light is correct.  If the disk is present, the
// light must be a floppy at any of its rotations.  If it is not, no pixel
// may be black where the floppy would be.
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

// Returns true if a disk's arrow is correct.  With `arrow` nonzero, that
// glyph must be at the left of the disk's light.  With `arrow` 0, no pixel
// may be black where an arrow would be.
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
static disk_is disks[5];        // by the enum: none, DISK.img, DISK.img with block 0 zeroed, ERROR.img, APPS.img
static disk_is second;          // the second disk, for the checks that give the machine two
static disk_is ipl;             // the records mkdisk wrote for the console
static int checks, compared, failed;

// Reads a whole disk file, or the file of records, into memory.
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

// Returns a disk for the machine to use: a copy in a temporary file, which
// is deleted when it is closed.  So the test never changes a disk the
// build made, and every check gets the disk as it was built, whatever an
// earlier check wrote.  (The disk used to be its bytes in memory, opened
// as a file.  The library function that opens memory as a file writes a
// zero byte after anything written to it, so a check that wrote a block
// zeroed the first byte of the next block, for itself and for every later
// check.)
static FILE* disk_of(disk_is d) {
    FILE* disk = tmpfile();
    if (disk == NULL || fwrite(d.bytes, 1, d.len, disk) != (size_t)d.len) {
        perror("a copy of a disk");
        exit(EXIT_FAILURE);
    }
    rewind(disk);
    return(disk);
}

// Returns the number of times the machine printed a text.
static int times_said(const char* what) {
    int n = 0;
    for(const char* at = strstr(said, what); at; at = strstr(at + 1, what)) n++;
    return(n);
}

static void run(const check* test) {
    start();
    if (test->rom_only) {
        memset((uint8_t*)machine.memory + KROM_BYTES, 0, MEM_BYTES - KROM_BYTES);
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

    int count = test->known ? 2 : 0;    // the cells that must be left: UNDER and the typed text's value, or none
    int differ = 0;
    if (machine.SP != count) {
        differ = count ? count : 1;
    } else if (test->known) {
        differ = (left(1) != UNDER) + (left(0) != (uint64_t)test->leaves);
    }
    int no_disk = times_said(NO_DISK_SAID);
    if (no_disk != test->no_disk) differ++;
    if (test->says && times_said(test->says) != 1) differ++;
    if (times_said("error: -1 ")) differ++;    // a -1 rethrown out of a block must not be reported
    // An error in a block may be reported only in the checks that put one
    // there.  A boot that reports one did not compile what follows the
    // error, even if the words a check types were all defined before it.
    if (times_said(IN_A_BLOCK) != (test->says && strstr(test->says, IN_A_BLOCK) ? 1 : 0)) differ++;
    int wrong = test->shows ? not_shown(test->shows) : 0;
    if (wrong) differ++;
    // The lights: one for each disk the bar must show, and none in the next position.
    for(int d=0; test->lights && d <= test->lights; d++) {
        if (!light_right(d, d < test->lights)) differ++;
    }
    // The arrow: at the light of the disk that must have it, and at no other light.
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
    alarm(600);                 // kill the test if the machine never stops
    setvbuf(stdout, NULL, _IOLBF, 0);   // line-buffer stdout, so everything printed before that has been written
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
