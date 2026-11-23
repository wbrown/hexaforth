//
// profile IMAGE.hex IMAGE.tables IMAGE.sizes DISK.img PICTURES.img PICTURE.tiff FORTH OUT
//
// Reports where the desktop's instructions go.
//
// The machine has two disks, as it has in the desktop's test
// (test/deskvm.h): DISK.img, and PICTURES.img, which has PICTURE.tiff
// under its label.
//
// The desktop runs on the reference VM that counts how many times the
// instruction at each address runs (counting.c).  It is driven through a
// few things a person would do (DONE, below).  Each is written in the
// notation the desktop's checks use for their actions (test/deskcheck.c).
// After each one the counts are read, and the increase since the reading
// before is that activity's.
//
// The desktop never stops: its event loop keeps turning whether or not
// anything is happening.  So one reading is taken while nothing happens,
// to learn what one turn of the loop costs.  That cost is subtracted from
// each activity once for every turn the loop made while the activity ran.
// The remainder is the work the activity caused.  On this machine the
// number of turns is exact: the loop makes one turn for each thing it is
// sent (test/deskvm.h).
//
// For each activity it prints the work and where the work went: by source
// file, by word, and by kind of instruction.  If the environment variable
// DETAIL is set, each activity whose name contains DETAIL's value also has
// its words listed: each word's instructions, the number of times it was
// called, and the instructions per call.  At the end it prints the cells
// of each stack that were ever used: every task has a part of each stack.
//
// It writes OUT/desktop.counts: the work of all the activities by address,
// and what one turn of the loop runs when nothing is happening (work.h
// describes the file).
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../test/deskvm.h"
#include "../test/nucleus.h"
#include "../test/tables.h"
#include "../util/image.h"
#include "../util/words.h"
#include "counting.h"
#include "files.h"
#include "work.h"

static const struct { const char* name; const char* does; } DONE[] = {
    {"typing two lines in the Forth window", "'2 3 + .\r' ': sq dup * ; 12 sq .\r'"},
    {"printing 40 lines, which scroll", "': p 40 0 do i . cr loop ; p\r'"},
    {"the Forth window: a page back, and forward again", "pgup pgdn"},
    {"raising the calculator", "front:calc"},
    {"the calculator: keys and buttons", "'12\r34+' click:calc+17+108 click:calc+119+132"},
    {"bringing windows to the front in turn, until it is the Words window", "cycle:words"},
    {"the Words window: finding dither-row, and choosing it",
     "click:words+10+8 away down find:dither-row name:dither-row"},
    // The Words window's definition starts 112 pixels from the left of the
    // window's content, and a character is 8 by 16 pixels.  The pointer is
    // put on every other character of the definition's second and third
    // lines.
    {"the Words window: the pointer over the definition, from one word to the next",
     "at:words+116+18 at:words+132+18 at:words+148+18 at:words+164+18 at:words+180+18 at:words+196+18 "
     "at:words+212+18 at:words+228+18 at:words+244+18 at:words+260+18 at:words+276+18 at:words+292+18 "
     "at:words+308+18 at:words+324+18 "
     "at:words+116+34 at:words+132+34 at:words+148+34 at:words+164+34 at:words+180+34 at:words+196+34 "
     "at:words+212+34 at:words+228+34 at:words+244+34 at:words+260+34 at:words+276+34 at:words+292+34 "
     "at:words+308+34 at:words+324+34"},
    {"the Words window: choosing a word", "click:words+10+72"},
    {"the Words window: a long definition, a page down and back",
     "find:tiff-open name:tiff-open pgdn pgup"},
    {"the Words window: six words down, and a page", "down down down down down down right"},
    {"raising the Forth window, and dragging it away and back", "front:forth drag:forth+60+50 drag:forth-60-50"},
    {"raising the editor", "front:editor"},
    {"typing eleven characters in the editor", "'hello there'"},
    {"moving the pointer across the screen",
     "point:100,160 point:140,164 point:180,168 point:220,172 point:260,176 point:300,180 point:340,184 "
     "point:380,188 point:420,192 point:460,196 point:500,200 point:540,204 point:580,208 point:620,212 "
     "point:660,216"},
    {NULL}};

// ---- the word, the file and the kind of an address

static word* WORDS;             // the image's words (words.h)
static int words;
static long image_places;       // the number of instructions in the image as it was built
static span* SPANS;             // the source files, and the code each one has (files.h)
static int spans;

// Returns the name of the word an address is in.  An address past the end
// of the image as it was built is in code that was defined at the prompt,
// and the text returned says so.
static const char* word_of(long at) {
    if (at >= image_places) return("(defined at the prompt)");
    return(word_at(WORDS, words, at));
}

// Returns the source file the code at an address came from.  A file that
// includes other files has their code inside its own, so the file is the
// smallest one that has the address.  Code that is in none of the system's
// files is the nucleus's own (nuc.fs).
static const char* file_of(long at) {
    const char* is = file_at(SPANS, spans, at);
    return(is ? is : at >= image_places ? "(defined at the prompt)" : "nuc.fs");
}

// Returns the kind of the instruction at an address.  A literal fetches or
// stores a variable if its shift field has the value that vm_opcodes.h
// names "var".
static const char* kind_of(long at) {
    instruction ins = *(instruction*)&machine.memory[at];
    bool variable = ins.lit.lit_f && !strcmp(LIT_SHIFT_REPR[ins.lit.lit_shifts], "var");
    if (ins.lit.lit_f) return(variable ? "variable fetch or store" : ins.lit.lit_add ? "literal add" : "literal push");
    return(ins.jmp.op_type == OP_TYPE_JMP ? "jump" : ins.jmp.op_type == OP_TYPE_CJMP ? "conditional branch" :
           ins.jmp.op_type == OP_TYPE_CALL ? "call" : "ALU");
}

// ---- the counts, totalled by file, word or kind

typedef struct { const char* of; double much, calls; } share;  // a name, the instructions charged to it, and its calls
static share* SHARES;           // the list grows as names are found (room.h)
static long shares, shares_room;

// Adds instructions and calls to the share with that name, creating the
// share if there is none.
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

static long places;             // the number of addresses that have counts: RAN_TO, which grows as more code runs (counting.h)

// Totals the counts by file, word or kind, whichever `of` returns for an
// address, and prints the `most` largest totals.  If `each` is not 0, a
// total is printed divided by `each`.  Otherwise it is printed as a
// percentage of `whole`.
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

// Each array below has one entry for each address that has counts.  More
// addresses run as the work goes on, and AS_MANY() extends the arrays with
// zeros.
static uint64_t* last;          // the counts at the last reading
static uint64_t* counts;        // the increase in the counts since the last reading
static double* net;             // that increase, less what the loop's turns would have run with nothing happening
static double* work;            // the sum of `net` over all the activities so far
static double* a_turn;          // what one turn of the loop runs when nothing is happening
static long last_turns;         // the number of turns the loop had made at the last reading

// Takes a reading.  It sets `counts` to the increase since the last
// reading, and returns the number of turns the loop made in that time.
static long reading(void) {
    static long room[5];
    desk_screen();                      // let the machine take everything that was sent to it
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

// Prints the cells of a stack that were ever used.  The machine starts
// with every stack cell 0, and it does not clear a cell when the cell is
// dropped, so a cell that is not 0 was used.  One or two cells of 0
// between such cells do not end a range.
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
    if (argc != 9) {
        fprintf(stderr, "Usage: %s IMAGE.hex IMAGE.tables IMAGE.sizes DISK.img PICTURES.img PICTURE.tiff FORTH OUT\n",
                argv[0]);
        return(1);
    }
    alarm(1800);                // kill the program if the machine never stops
    uint8_t* image;
    size_t len = load_image(argv[1], &image);
    image_places = len / 2;
    words = words_of(image, len, argv[2], &WORDS);
    spans = files_of(argv[3], &SPANS);
    tables_are(argv[7]);

    desk on = deskvm_start(argv[1], argv[4], argv[5], argv[6], argv[8], true, false);
    if (!desk_begins(&on)) {
        fprintf(stderr, "the desktop did not come up with its windows where forth/desk.fs puts them\n");
        return(1);
    }
    long turns = reading();
    double all = 0;
    for(long at=0; at < places; at++) all += counts[at];
    printf("booting, with the picture dithered: %.0f instructions, %ld turns of the loop\n", all, turns);

    // Measure what one turn of the loop runs when nothing is happening.
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
        if (detail && strstr(DONE[n].name, detail)) {   // list each word with its instructions and its calls
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
    char* path = room_for(strlen(argv[8]) + sizeof(file), 1);
    sprintf(path, "%s%s", argv[8], file);
    return(work_written(path, work, a_turn, places) ? 0 : 1);
}
