//
// source_test IMAGE.hex DISK.img
//
// Checks how the machine reads source: comments are skipped, at the prompt
// and in a block (load), as standard Forth defines them.  A block is read
// as a single piece of text.  A ( comment ends at the next ), whether or
// not that is on a later line, and \ is the word that knows a block has
// lines.  Typed input is one line, and both kinds of comment end with it.
//
// The disk is one that mkdisk made with test/source.fs as its source
// block, so the code that packs a file's lines into a block is tested
// together with the code that reads them.  That file is words with
// comments around them.  Each comment would undo its word, or leave a
// later word undefined, if it were read as code or did not end where it
// should.  The file also has another block loaded into the buffer while it
// is being read, and loads a block itself.  The values each word must
// leave are given here, not in that file.
//
// The machine is the nucleus on vm.c (nucleus.h).  Before each check a
// known number is pushed, under whatever the check leaves.  If the machine
// met a word it did not know, it emptied its stack, and that number is
// gone.
//
// It prints how many checks it made and how many cells it compared.  It
// fails if it compared none, because then it tested nothing.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "nucleus.h"

#define UNDER 12345             // the number pushed under whatever a check leaves
#define THROWS 3                // the block the test adds, after test/source.fs and the empty block
                                // that file loads; it contains a word the machine does not know
#define SAID(x) #x
#define NUMBER(x) SAID(x)       // a named number, as the text that is typed

typedef struct {
    const char* label;
    const char* typed;          // the lines to type; for a block check they are typed after the block is loaded
    int count;                  // the number of cells they leave
    int64_t leaves[4];          // those cells, the top of the stack last
} check;

// The checks of typed input.
static check PROMPT[] = {
    {"a comment to the end of the line", "7 \\ 8 9", 1, {7}},
    {"a comment to the end of the line, and the line after it", "7 \\ 8\n9", 2, {7, 9}},
    {"a comment that is all of its line", "\\\n7", 1, {7}},
    {"a comment in a line", "( 8 9 ) 7", 1, {7}},
    {"a comment in a line, between what is read", "7 ( 8 ) 9", 2, {7, 9}},
    {"both in a line", "7 ( 8 ) 9 \\ 10 ( 11 )", 2, {7, 9}},
    {"a comment in a line that has no end ends with the line", "7 ( 8\n9", 2, {7, 9}},
    {"comments in a definition, which is over two lines",
     ": more ( a -- b ) 1+ \\ ; 99\n( 1+ ) ;\n6 more", 1, {7}},
    {""}};

// The checks that run after test/source.fs has been loaded from its block.
static check BLOCK[] = {
    {"a word with a comment after it that would throw", "seven", 1, {7}},
    {"a word after a comment that would make that word again", "seven nine", 2, {7, 9}},
    {"a definition over three lines of a block, a comment in each", "both", 2, {7, 9}},
    {"a comment in brackets over two lines, with a \\ in it", "open shut", 2, {3, 4}},
    {"a line after a comment that is the last of its line", "after full", 2, {5, 6}},
    {"a line after a comment in the last column of its line", "full last", 2, {6, 8}},
    {"a word after another block was in the buffer", "back", 1, {10}},
    {"a word after a block was loaded from this one", "inner", 1, {11}},
    {"a comment in brackets with no end ends with the block", "end", 1, {12}},
    {"when the block is done no block is being read", "blk @ 7 \\ 8", 2, {0, 7}},
    // The machine does not know the word in that block, and empties its
    // stack.
    {"nor when a block has thrown",
     NUMBER(THROWS) " load\n" NUMBER(UNDER) " blk @ 7 \\ 8", 2, {0, 7}},
    {""}};

static FILE* disk;              // the disk, for the block checks
static long disk_blocks;        // its size in blocks
static int checks, compared, failed;

// Makes the disk the checks use: a copy of the one mkdisk made from
// test/source.fs, with one more block, THROWS, which contains a word the
// machine does not know.  (Loading a block that is missing or all zeros
// throws no error.  The check of a block that throws once loaded the
// picture's block on a disk that had no picture, and so nothing threw.)
static void disk_is(const char* path) {
    static const char unknown[] = "no-such-word";
    FILE* made = fopen(path, "rb");
    if (made == NULL) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    fseek(made, 0, SEEK_END);
    long len = ftell(made);
    disk_blocks = len / DISK_BLOCK;
    if (disk_blocks <= THROWS) {
        fprintf(stderr, "%s: %ld blocks, which has no block %d\n", path, disk_blocks, THROWS);
        exit(EXIT_FAILURE);
    }
    uint8_t* bytes = malloc(len);
    rewind(made);
    if (fread(bytes, 1, len, made) != (size_t)len) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    fclose(made);
    memset(bytes + THROWS * DISK_BLOCK, ' ', DISK_BLOCK);
    memcpy(bytes + THROWS * DISK_BLOCK, unknown, strlen(unknown));
    disk = tmpfile();
    if (disk == NULL || fwrite(bytes, 1, len, disk) != (size_t)len) {
        perror("a copy of the disk");
        exit(EXIT_FAILURE);
    }
    free(bytes);
}

static void run(const check* test, bool block) {
    start();
    line("%d", UNDER);
    if (block) {
        machine.DISKS[0] = disk;    // the machine finds this disk when it starts
        machine.DISKBLOCKS[0] = disk_blocks;
        line("%d load", DISK_SOURCE_BLOCK);
    }
    line("%s", test->typed);
    type();

    int differ = 0;
    if (machine.SP != test->count + 1 || left(test->count) != UNDER) {
        differ = test->count;
    } else {
        for(int idx=0; idx < test->count; idx++) {
            if (left(test->count - 1 - idx) != (uint64_t)test->leaves[idx]) differ++;
        }
    }
    checks++;
    compared += test->count;
    if (differ) failed++;
    printf("%s %s, %s: %d cells left, %d not as they should be\n", differ ? "FAIL" : "ok  ",
           block ? "in a block" : "at the prompt", test->label, test->count, differ);
    if (differ) printf("     the machine said:\n%s\n", said);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s IMAGE.hex DISK.img\n", argv[0]);
        return(1);
    }
    alarm(600);                 // kill the test if the machine never stops
    nucleus_is(argv[1]);
    disk_is(argv[2]);

    for(check* test = PROMPT; strlen(test->label); test++) run(test, false);
    for(check* test = BLOCK; strlen(test->label); test++) run(test, true);
    printf("source, cells of %d bits: %d checks at the prompt and in a block, %d failed; %d cells compared\n",
           CELL_BITS, checks, failed, compared);
    return(failed || !compared);
}
