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
// It leaves OUT/desktop.counts: the work of all the activities by place,
// and what a turn with nothing happening runs (work.c says the file).
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
#include "files.c"
#include "work.c"

static const struct { const char* name; const char* does; } DONE[] = {
    {"typing two lines in the Forth window", "'2 3 + .\r' ': sq dup * ; 12 sq .\r'"},
    {"printing 40 lines, which scroll", "': p 40 0 do i . cr loop ; p\r'"},
    {"the Forth window: a page back, and forward again", "pgup pgdn"},
    {"raising the calculator", "front:calc"},
    {"the calculator: keys and buttons", "'12\r34+' click:calc+17+108 click:calc+119+132"},
    {"bringing windows to the front in turn, until it is the Words window", "cycle:words"},
    {"the Words window: finding dither-row, and choosing it",
     "click:words+10+8 away down find:dither-row,dither-start name:dither-row"},
    // The Words window's definition starts 112 across its content, 8 by 16
    // a character: the pointer is put on every other character of the
    // definition's second and third lines.
    {"the Words window: the pointer over the definition, from one word to the next",
     "at:words+116+18 at:words+132+18 at:words+148+18 at:words+164+18 at:words+180+18 at:words+196+18 "
     "at:words+212+18 at:words+228+18 at:words+244+18 at:words+260+18 at:words+276+18 at:words+292+18 "
     "at:words+308+18 at:words+324+18 "
     "at:words+116+34 at:words+132+34 at:words+148+34 at:words+164+34 at:words+180+34 at:words+196+34 "
     "at:words+212+34 at:words+228+34 at:words+244+34 at:words+260+34 at:words+276+34 at:words+292+34 "
     "at:words+308+34 at:words+324+34"},
    {"the Words window: choosing a word", "click:words+10+72"},
    {"the Words window: a long definition, a page down and back",
     "find:tiff-open,tiff-close name:tiff-open pgdn pgup"},
    {"the Words window: six words down, and a page", "down down down down down down right"},
    {"raising the Forth window, and dragging it away and back", "front:forth drag:forth+60+50 drag:forth-60-50"},
    {"raising the editor", "front:editor"},
    {"typing eleven characters in the editor", "'hello there'"},
    {"moving the pointer across the screen",
     "point:100,160 point:140,164 point:180,168 point:220,172 point:260,176 point:300,180 point:340,184 "
     "point:380,188 point:420,192 point:460,196 point:500,200 point:540,204 point:580,208 point:620,212 "
     "point:660,216"},
    {NULL}};

// ---- what a place is in

static word* WORDS;             // the image's words (words.c)
static int words;
static long image_places;       // the instructions the image was built with
static span* SPANS;             // the files its code is of (files.c)
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
    const char* is = file_at(SPANS, spans, at);
    return(is ? is : at >= image_places ? "(defined at the prompt)" : "nuc.fs");
}

// The kind of instruction at a place.  One with a number in it is of a
// variable if its shift is the one vm_opcodes.h names so.
static const char* kind_of(long at) {
    instruction ins = *(instruction*)&machine.memory[at];
    bool variable = ins.lit.lit_f && !strcmp(LIT_SHIFT_REPR[ins.lit.lit_shifts], "var");
    if (ins.lit.lit_f) return(variable ? "variable fetch or store" : ins.lit.lit_add ? "literal add" : "literal push");
    return(ins.jmp.op_type == OP_TYPE_JMP ? "jump" : ins.jmp.op_type == OP_TYPE_CJMP ? "conditional branch" :
           ins.jmp.op_type == OP_TYPE_CALL ? "call" : "ALU");
}

// ---- what is counted, by what it is of

typedef struct { const char* of; double much, calls; } share;
static share* SHARES;           // as many as there are, and room for more as they are found (room.c)
static long shares, shares_room;

static void shared(const char* of, double much, double calls) {
    long n = 0;
    while (n < shares && strcmp(SHARES[n].of, of)) n++;
    ONE_MORE(SHARES, shares, shares_room);
    if (n == shares) SHARES[shares++] = (share){ of, 0, 0 };
    SHARES[n].much += much;
    SHARES[n].calls += calls;
}

static int by_most(const void* a, const void* b) {
    double x = ((const share*)a)->much, y = ((const share*)b)->much;
    return(x < y ? 1 : x > y ? -1 : 0);
}

// The places' counts by file, word or kind, and the most of them printed.
static long places;             // the places the counts are of: as many as have been run (counting.h)

static void by(const char* what, const double* counts, const char* (*of)(long), int most, double whole, double each) {
    shares = 0;
    for(long at=0; at < places; at++) {
        if (counts[at] > 0) shared(of(at), counts[at], 0);
    }
    qsort(SHARES, shares, sizeof(share), by_most);
    printf("  %s", what);
    for(long n=0; n < shares && n < most; n++) {
        if (each) printf("%s %s %.0f", n ? "," : "", SHARES[n].of, SHARES[n].much / each);
        else printf("%s %s %.0f%%", n ? "," : "", SHARES[n].of, 100 * SHARES[n].much / whole);
    }
    printf("\n");
}

// ---- the readings

// Each is of a place, and has room for as many places as have been run:
// more are run as the work goes on, and what is new is nothing.
static uint64_t* last;          // the counts as they were at the last reading,
static uint64_t* counts;        // what has been run since,
static double* net;             // that without what the turns it took run with nothing happening,
static double* work;            // all of that so far,
static double* a_turn;          // and what a turn with nothing happening runs
static long last_turns;

// What has been run since the last reading, and the turns the loop took.
static long reading(void) {
    static long room[5];
    desk_screen();                      // the machine takes everything that was sent
    places = RAN_TO;
    AS_MANY(last, places, room[0]);
    AS_MANY(counts, places, room[1]);
    AS_MANY(net, places, room[2]);
    AS_MANY(work, places, room[3]);
    AS_MANY(a_turn, places, room[4]);
    for(long at=0; at < places; at++) counts[at] = RAN[at] - last[at], last[at] = RAN[at];
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
    words = words_of(image, len, argv[2], &WORDS);
    spans = files_of(argv[3], &SPANS);
    tables_are(argv[6]);

    desk on = deskvm_start(argv[1], argv[4], argv[5], argv[7], true, false);
    if (!desk_begins(&on)) {
        fprintf(stderr, "the desktop did not come up with its windows where forth/desk.fs puts them\n");
        return(1);
    }
    long turns = reading();
    double all = 0;
    for(long at=0; at < places; at++) all += counts[at];
    printf("booting, with the picture dithered: %.0f instructions, %ld turns of the loop\n", all, turns);

    // Nothing happening: what a turn of the loop runs.
    desk_turns(200);
    turns = reading();
    all = 0;
    for(long at=0; at < places; at++) a_turn[at] = (double)counts[at] / turns, all += a_turn[at];
    printf("\na turn of the event loop with nothing happening: %.0f instructions\n", all);
    by("by word: ", a_turn, word_of, 9, all, 1);

    const char* detail = getenv("DETAIL");
    for(int n=0; DONE[n].name; n++) {
        desk_does(DONE[n].does);
        desk_turns(12);
        turns = reading();
        double total = 0, ran = 0;
        for(long at=0; at < places; at++) {
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
            for(long at=0; at < places; at++) {
                bool first = at < image_places && place_of(WORDS, words, word_of(at)) == at;
                if (net[at] > 0) shared(word_of(at), net[at], first ? net[at] : 0);
            }
            qsort(SHARES, shares, sizeof(share), by_most);
            for(long w=0; w < shares && w < 40; w++) {
                printf("    %-20s %12.0f %10.0f calls %8.1f each\n", SHARES[w].of, SHARES[w].much, SHARES[w].calls,
                       SHARES[w].much / (SHARES[w].calls > 1 ? SHARES[w].calls : 1));
            }
        }
    }

    printf("\nthe cells of each stack that were used:\n");
    used("data", machine.DSTACK);
    used("return", machine.RSTACK);

    static const char file[] = "/desktop.counts";
    char* path = room_for(strlen(argv[7]) + sizeof(file), 1);
    sprintf(path, "%s%s", argv[7], file);
    return(work_written(path, work, a_turn, places) ? 0 : 1);
}
