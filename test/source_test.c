//
// source_test IMAGE.hex DISK.img
//
// Source as the machine reads it: what is a comment is not read, at the
// prompt and in a block (load), as Forth has it.  A block is read as one
// thing: a comment in brackets ends at the next bracket, lines on or not,
// and \ is what knows that a block has lines.  What is typed is a line,
// and both comments end with it.
//
// The disk is one that mkdisk made with test/source.fs as its block of
// source, so what lays a file's lines down as a block's is tried with what
// reads them.  That file is words with comments about them, each comment
// one that would undo its word, or leave a later word undefined, were it
// read or did it not end where it should.  It has another block put in
// the buffer while it is read, and loads one.  What each word is to leave
// is said here and not there.
//
// The machine is the nucleus on vm.c, typed at (nucleus.h).  Under what a
// check leaves is a number put there first: if the machine did not know a
// word it has emptied its stack, and the number is gone.
//
// It says how many checks it made and how many cells it compared, and
// fails if that is none: then it found nothing to test.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "nucleus.h"

#define UNDER 12345             // what is under what a check leaves
#define SAID(x) #x
#define NUMBER(x) SAID(x)       // a number that has a name, as it is typed

typedef struct {
    const char* label;
    const char* typed;          // lines, typed after the block is loaded if it is one for the block
    int count;                  // how many cells they leave,
    int64_t leaves[4];          // and what those are, the top last
} check;

// What is typed at the prompt.
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

// What is typed when test/source.fs has been loaded.
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
    // The picture is not source: the machine does not know its first word,
    // and empties its stack.
    {"nor when a block has thrown",
     NUMBER(DISK_PICTURE_BLOCK) " load\n" NUMBER(UNDER) " blk @ 7 \\ 8", 2, {0, 7}},
    {""}};

static FILE* disk;              // the disk, if the checks being made are of the block,
static long disk_blocks;        // and how many blocks it is
static int checks, compared, failed;

static void run(const check* test, bool block) {
    start();
    line("%d", UNDER);
    if (block) {
        machine.DISK = disk;    // which the machine finds when it starts
        machine.DISKBLOCKS = disk_blocks;
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
    alarm(600);                 // a machine that does not stop is stopped
    nucleus_is(argv[1]);
    disk = fopen(argv[2], "rb");
    if (disk == NULL) {
        perror(argv[2]);
        return(1);
    }
    fseek(disk, 0, SEEK_END);
    disk_blocks = ftell(disk) / DISK_BLOCK;

    for(check* test = PROMPT; strlen(test->label); test++) run(test, false);
    for(check* test = BLOCK; strlen(test->label); test++) run(test, true);
    printf("source, cells of %d bits: %d checks at the prompt and in a block, %d failed; %d cells compared\n",
           CELL_BITS, checks, failed, compared);
    return(failed || !compared);
}
