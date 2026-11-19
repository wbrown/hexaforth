//
// boot_test IMAGE.hex DISK.img
//
// A machine started from its ROM (doc/boot.md).  The disk is one that
// mkdisk made of the same image (mkdisk --boot): block 0 its index
// (doc/disk.md), block 1 test/boot.fs, and the image's memory above the
// ROM in binary runs.  With nothing in memory but the ROM, the machine is
// to read the rest of the system from the disk, load block 1, and be at
// its prompt with what that block made.  A machine whose image is in
// memory already is to load block 1 from such a disk too, and not to load
// anything from a disk that has no index, or with no disk.
//
// What is typed is a word block 1 makes, over a number put there first.
// A machine that did not load the block does not know the word: it
// empties its stack, and the number is gone.  What the word is to leave
// is said here and not there.
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

#define UNDER  12345            // what is under what the word leaves
#define BOOTED 42               // what the word block 1 makes leaves

static uint8_t* disk_bytes;     // the disk, as mkdisk made it,
static long disk_len;           // how many bytes it is,
static uint8_t* no_index;       // and the same with block 0 nothing
static int checks, compared, failed;

// A disk as the machine reads it: from memory, so that the test changes
// no file.
static FILE* disk_of(uint8_t* bytes) {
    FILE* disk = fmemopen(bytes, disk_len, "r+");
    if (disk == NULL) {
        perror("fmemopen");
        exit(EXIT_FAILURE);
    }
    return(disk);
}

static void run(const char* label, bool rom_only, uint8_t* disk_is, bool loads) {
    start();
    if (rom_only) {
        memset((uint8_t*)machine.memory + KROM_BYTES, 0, sizeof(machine.memory) - KROM_BYTES);
    }
    FILE* disk = disk_is ? disk_of(disk_is) : NULL;
    machine.DISK = disk;
    machine.DISKBLOCKS = disk ? disk_len / DISK_BLOCK : 0;
    line("%d", UNDER);
    line("booted");
    type();
    if (disk) fclose(disk);

    int count = loads ? 2 : 0;          // what is to be left: the number and what the word leaves, or nothing
    int differ = 0;
    if (machine.SP != count) {
        differ = count ? count : 1;
    } else if (loads) {
        differ = (left(1) != UNDER) + (left(0) != BOOTED);
    }
    checks++;
    compared += count;
    if (differ) failed++;
    printf("%s %s: block 1 %s, %d cells not as they should be\n", differ ? "FAIL" : "ok  ",
           label, loads ? "loaded" : "not loaded", differ);
    if (differ) printf("     the machine said:\n%s\n", said);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s IMAGE.hex DISK.img\n", argv[0]);
        return(1);
    }
    alarm(600);                 // a machine that does not stop is stopped
    nucleus_is(argv[1]);
    FILE* file = fopen(argv[2], "rb");
    if (file == NULL) {
        perror(argv[2]);
        return(1);
    }
    fseek(file, 0, SEEK_END);
    disk_len = ftell(file);
    fseek(file, 0, SEEK_SET);
    disk_bytes = malloc(disk_len);
    no_index = malloc(disk_len);
    if (disk_len < 2 * DISK_BLOCK || fread(disk_bytes, 1, disk_len, file) != (size_t)disk_len) {
        fprintf(stderr, "%s is not a disk that has an index and a block 1\n", argv[2]);
        return(1);
    }
    fclose(file);
    memcpy(no_index, disk_bytes, disk_len);
    memset(no_index, 0, DISK_BLOCK);

    run("the ROM alone, and the disk", true, disk_bytes, true);
    run("the image in memory, and the disk", false, disk_bytes, true);
    run("the image in memory, and the disk with no index", false, no_index, false);
    run("the image in memory, and no disk", false, NULL, false);
    printf("boot, cells of %d bits: %d checks, %d failed; %d cells compared\n",
           CELL_BITS, checks, failed, compared);
    return(failed || !compared);
}
