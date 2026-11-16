//
// profile IMAGE.hex IMAGE.tables IMAGE.sizes DISK.img PICTURE.tiff FORTH OUT
//
// Where the desktop's instructions go.
//
// The desktop is run on the reference VM that counts how often the
// instruction at each place is run (counting.c), and worked through a few
// things a person would do (DONE, below), each said in the words the
// desktop's checks say what they do in (test/deskcheck.c).  After each,
// the counts are read, and what they have grown by is that activity's.
//
// The desktop never stops: its event loop goes round whether anything is
// happening or not.  So one reading is taken with nothing happening, to
// learn what a turn of the loop costs, and that much is taken off each
// activity for every turn made while it ran.  What is left is the work
// the activity caused.  On this machine a turn is exact: the loop takes
// one for each thing it is sent (test/deskvm.h).
//
// For each activity it prints the work, and where it went: by source
// file, by word, and by kind of instruction.  With DETAIL in the
// environment, an activity whose name has that in it has each of its
// words printed: its instructions, how often it was called, and the
// instructions to a call.  Then, of each stack, the cells that were ever
// used: every task has a part of each.
//
// It leaves OUT/desktop.counts: for each of the first CODE places, the
// work of all the activities, and then what a turn with nothing happening
// runs, each a number of 64 bits with a fraction.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../test/deskvm.h"
#include "../test/nucleus.h"
#include "../test/tables.h"
#include "../util/image.h"
#include "../util/words.c"
#include "counting.h"

#define CODE 0x10000            // the places the counts are kept of

static const struct { const char* name; const char* does; } DONE[] = {
    {"typing two lines in the Forth window", "'2 3 + .\r' ': sq dup * ; 12 sq .\r'"},
    {"printing 40 lines, which scroll", "': p 40 0 do i . cr loop ; p\r'"},
    {"raising the calculator", "front:calc"},
    {"the calculator: keys and buttons", "'12\r34+' click:calc+17+108 click:calc+119+132"},
    {"bringing windows to the front in turn, until it is the Words window", "cycle:words"},
    {"the Words window: choosing a word", "click:words+10+72"},
    {"the Words window: six words down, and a page", "down down down down down down pgdn"},
    {"raising the Forth window, and dragging it away and back", "front:forth drag:forth+60+50 drag:forth-60-50"},
    {"raising the editor", "front:editor"},
    {"typing eleven characters in the editor", "'hello there'"},
    {"moving the pointer across the screen",
     "point:100,160 point:140,164 point:180,168 point:220,172 point:260,176 point:300,180 point:340,184 "
     "point:380,188 point:420,192 point:460,196 point:500,200 point:540,204 point:580,208 point:620,212 "
     "point:660,216"},
    {NULL}};

// ---- what a place is in

static word WORDS[1 << 14];
static int words;
static long image_places;       // the instructions the image was built with
static struct { long from, to; char file[64]; } SPANS[256];
static int spans;

// The word a place is in; or that it is in what was defined at the
// prompt, which is past what the image was built with.
static const char* word_of(long at) {
    if (at >= image_places) return("(defined at the prompt)");
    return(word_at(WORDS, words, at));
}

// The source file a place's code is of.  A file that includes others has
// their code in its own, so it is the smallest file's that has the place;
// and what no file of the system's has is the nucleus's own.
static const char* file_of(long at) {
    const char* is = at >= image_places ? "(defined at the prompt)" : "nuc.fs";
    long least = -1;
    for(int n=0; n < spans; n++) {
        long size = SPANS[n].to - SPANS[n].from;
        if (at >= SPANS[n].from && at < SPANS[n].to && (least < 0 || size < least)) least = size, is = SPANS[n].file;
    }
    return(is);
}

// The image's .sizes: a line a file, with the bytes of code it compiled
// second and the instruction its code starts at sixth.
static void files_of(const char* path) {
    FILE* in = fopen(path, "r");
    if (in == NULL) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    char row[256];
    while (fgets(row, sizeof(row), in) && spans < 256) {
        long bytes = 0, a, b, c, from = 0;
        if (sscanf(row, "%63s %ld %ld %ld %ld %ld", SPANS[spans].file, &bytes, &a, &b, &c, &from) != 6) continue;
        if (SPANS[spans].file[0] == '(' || !bytes) continue;
        SPANS[spans].from = from, SPANS[spans].to = from + bytes / 2;
        spans++;
    }
    fclose(in);
}

static const char* kind_of(long at) {
    instruction ins = *(instruction*)&machine.memory[at];
    if (ins.lit.lit_f) return(ins.lit.lit_shifts == 3 ? "variable fetch or store" : ins.lit.lit_add ? "literal add" : "literal push");
    return(ins.jmp.op_type == OP_TYPE_JMP ? "jump" : ins.jmp.op_type == OP_TYPE_CJMP ? "conditional branch" :
           ins.jmp.op_type == OP_TYPE_CALL ? "call" : "ALU");
}

// ---- what is counted, by what it is of

typedef struct { const char* of; double much, calls; } share;
static share SHARES[1 << 14];
static int shares;

static void shared(const char* of, double much, double calls) {
    int n = 0;
    while (n < shares && strcmp(SHARES[n].of, of)) n++;
    if (n == shares) SHARES[shares++] = (share){ of, 0, 0 };
    SHARES[n].much += much;
    SHARES[n].calls += calls;
}

static int by_most(const void* a, const void* b) {
    double x = ((const share*)a)->much, y = ((const share*)b)->much;
    return(x < y ? 1 : x > y ? -1 : 0);
}

// The places' counts by file, word or kind, and the most of them printed.
static void by(const char* what, const double* counts, const char* (*of)(long), int most, double whole, double each) {
    shares = 0;
    for(long at=0; at < CODE; at++) {
        if (counts[at] > 0) shared(of(at), counts[at], 0);
    }
    qsort(SHARES, shares, sizeof(share), by_most);
    printf("  %s", what);
    for(int n=0; n < shares && n < most; n++) {
        if (each) printf("%s %s %.0f", n ? "," : "", SHARES[n].of, SHARES[n].much / each);
        else printf("%s %s %.0f%%", n ? "," : "", SHARES[n].of, 100 * SHARES[n].much / whole);
    }
    printf("\n");
}

// ---- the readings

static uint64_t last[CODE];
static long last_turns;

// What has been run since the last reading, and the turns the loop took.
static long reading(uint64_t* counts) {
    desk_screen();                      // the machine takes everything that was sent
    for(long at=0; at < CODE; at++) counts[at] = RAN[at] - last[at], last[at] = RAN[at];
    long turns = deskvm_turns - last_turns;
    last_turns = deskvm_turns;
    return(turns);
}

// The cells of a stack that were ever used.  The machine starts with
// nothing in them and nothing clears a cell that is dropped, so a cell
// that is not nothing was used; two that are nothing among them are not
// the end of a run.
static void used(const char* stack, const int64_t* cells) {
    printf("  the %s stack:", stack);
    for(int at=0; at < STACK_CELLS; at++) {
        if (!cells[at]) continue;
        int end = at;
        for(int next=at + 1; next < STACK_CELLS && next <= end + 3; next++) if (cells[next]) end = next;
        printf(" cells %d to %d,", at, end);
        at = end;
    }
    printf(" of %d\n", STACK_CELLS);
}

int main(int argc, char *argv[]) {
    if (argc != 8) {
        fprintf(stderr, "Usage: %s IMAGE.hex IMAGE.tables IMAGE.sizes DISK.img PICTURE.tiff FORTH OUT\n", argv[0]);
        return(1);
    }
    alarm(1800);                // a machine that does not stop is stopped
    uint8_t* image;
    size_t len = load_image(argv[1], &image);
    image_places = len / 2;
    words = words_of(image, len, argv[2], WORDS, 1 << 14);
    files_of(argv[3]);
    tables_are(argv[6]);

    desk on = deskvm_start(argv[1], argv[4], argv[5], argv[7], true);
    if (!desk_begins(&on)) {
        fprintf(stderr, "the desktop did not come up with its windows where forth/desk.fs puts them\n");
        return(1);
    }
    static uint64_t counts[CODE];
    static double net[CODE], work[CODE], a_turn[CODE];
    long turns = reading(counts);
    double all = 0;
    for(long at=0; at < CODE; at++) all += counts[at];
    printf("booting, with the picture dithered: %.0f instructions, %ld turns of the loop\n", all, turns);

    // Nothing happening: what a turn of the loop runs.
    desk_turns(200);
    turns = reading(counts);
    all = 0;
    for(long at=0; at < CODE; at++) a_turn[at] = (double)counts[at] / turns, all += a_turn[at];
    printf("\na turn of the event loop with nothing happening: %.0f instructions\n", all);
    by("by word: ", a_turn, word_of, 9, all, 1);

    const char* detail = getenv("DETAIL");
    for(int n=0; DONE[n].name; n++) {
        desk_does(DONE[n].does);
        desk_turns(12);
        turns = reading(counts);
        double total = 0, ran = 0;
        for(long at=0; at < CODE; at++) {
            net[at] = counts[at] - turns * a_turn[at];
            if (net[at] < 0) net[at] = 0;
            work[at] += net[at];
            total += net[at];
            ran += counts[at];
        }
        printf("\n%s: %.0f instructions of work (%.0f run, %ld turns of the loop)\n", DONE[n].name, total, ran, turns);
        if (total <= 0) continue;
        by("by file: ", net, file_of, 6, total, 0);
        by("by word: ", net, word_of, 9, total, 0);
        by("by kind: ", net, kind_of, 9, total, 0);
        if (detail && strstr(DONE[n].name, detail)) {   // each word: its instructions, and its calls
            shares = 0;
            for(long at=0; at < CODE; at++) {
                bool first = at < image_places && place_of(WORDS, words, word_of(at)) == at;
                if (net[at] > 0) shared(word_of(at), net[at], first ? net[at] : 0);
            }
            qsort(SHARES, shares, sizeof(share), by_most);
            for(int w=0; w < shares && w < 40; w++) {
                printf("    %-20s %12.0f %10.0f calls %8.1f each\n", SHARES[w].of, SHARES[w].much, SHARES[w].calls,
                       SHARES[w].much / (SHARES[w].calls > 1 ? SHARES[w].calls : 1));
            }
        }
    }

    printf("\nthe cells of each stack that were used:\n");
    used("data", machine.DSTACK);
    used("return", machine.RSTACK);

    char path[512];
    snprintf(path, sizeof(path), "%s/desktop.counts", argv[7]);
    FILE* out = fopen(path, "wb");
    if (out == NULL) {
        perror(path);
        return(1);
    }
    fwrite(work, sizeof(double), CODE, out);
    fwrite(a_turn, sizeof(double), CODE, out);
    fclose(out);
    return(0);
}
