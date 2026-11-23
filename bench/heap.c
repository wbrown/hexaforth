//
// heap IMAGE.hex [DISK.img [MEMORY.out]]
//
// The reference VM, recording how the heap is used.  The heap is the
// memory above the image that halloc allocates (machine.fs).  This is
// main.c and vm.c unchanged.  vm.c reports three events through macros,
// and this file defines them: an instruction about to run (RUNS), a cell
// about to be stored to (STORES), and cells that a disk read fills
// (FILLS).
//
// Allocations are detected from the nucleus's own variables.  himem moves
// up when halloc allocates memory, and moves back when memory is freed by
// setting himem to an earlier value.  hreuse allocates from the run of
// cells that hfree keeps (hole).  hfree takes the memory being freed as
// its arguments.  This program does not read a size from any of those
// words: a piece is the cells the pointer moved over.
//
// The word that asked for a piece is the word that contains the call or
// jump to halloc or hreuse.  The word that called it is found from the
// nearest return address on the return stack that is in a different word.
// The number of cells of a piece that were stored to is the number that
// an instruction stored to, or a disk read filled, while the piece was
// allocated.  So a piece that is allocated larger than it ever needs
// shows in the report.
//
// The stacks are the machine's other storage, and they are not part of
// memory.  The program also records the most cells each stack held, the
// word that was running when the return stack was deepest, what that stack
// held then (held_said), and how many files were being read then
// (own-names and own-done, files.fs).  A file that is being read keeps its
// part of the dictionary on the return stack until it is finished.  The
// most a stack held is the highest cell that was used, which is not how
// deep one task went: each task has its own part of each stack, starting
// at the bases passed to spawn (nuc.fs).
//
// When the machine stops, the results are written to the file named by the
// environment variable HEAP, if it is set (heap_said):
//
//   - how far the heap reached, and which word had asked then
//   - how much of the heap was allocated, how much was kept for reuse and
//     how much was neither when the machine stopped
//   - the most cells each stack held
//   - one line for each word that asked: the number of pieces, the size
//     of a piece, the amount it has allocated now and had at most, and
//     the most cells of any one piece that were stored to
//
// HEAP_NAMES names the file of names that goes with the image
// (nuc.tables), which has the words with no header.  Recording starts
// again each time the machine starts at instruction 0, so a test that
// starts the machine more than once reports its last run.
//
// A test that has the VM in it can be built with this file in place of
// vm.c, with WATCHING_A_TEST defined.  This file is then the VM alone, and
// the test calls heap_said() when it is done.
//

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../vm.h"

static void ran(context* ctx, int64_t at, int sp, int rsp, int64_t t, int64_t r);
static void stored(context* ctx, int64_t addr, int64_t x, int64_t next, int rsp, int64_t r);
static void filled(context* ctx, uint64_t cell, uint64_t cells);

#define RUNS(at)            ran(ctx, at, SP, RSP, T, R)
#define STORES(addr, x)     stored(ctx, addr, x, EIP, RSP, R)
#define FILLS(cell, cells)  filled(ctx, cell, cells)
#include "../vm.c"
#include "../util/layout.h"

#define CELLS   ((uint64_t)1 << ADDR_BITS)     // the number of cells in the machine's memory
#define DEEP    8                               // the number of return-stack cells searched for the word that called the one that asked

static context* MACHINE;
static word* WORDS;             // the image's words, in order of address
static int NWORDS;
static uint64_t HIMEM, HOLE;    // the addresses of the nucleus's variables himem and hole, or 0 if the image has none
static long HALLOC = -1, HREUSE = -1, HFREE = -1;      // the index in WORDS of each of the three words, or -1

// An allocated piece of the heap.
typedef struct {
    uint64_t at, cells;         // its first cell, and its length in cells
    long asker;                 // the index in ASKERS of the word that asked for it
} piece;
static piece* PIECES;           // sorted by address
static long npieces, pieces_room;

// A word that asked for memory, and what it was given.
typedef struct {
    int who, of;                // the index in WORDS of the word that asked, and of the word that called it; -1 for one that is not in a word
    long times;                 // the number of pieces it was given
    uint64_t least, most;       // the smallest and the largest piece, in cells
    uint64_t held, most_held;   // the cells it has allocated now, and the most it had at one time
    uint64_t stored;            // the most cells of any one of its pieces that were stored to
} asker;
static asker* ASKERS;
static long naskers, askers_room;

static uint8_t* TOUCHED;        // for each cell of memory: 1 if it was stored to since it was allocated
static uint64_t FROM;           // the heap's first cell: the value of himem in the image
static uint64_t FURTHEST;       // the end of the highest piece allocated so far
static long FURTHEST_FOR = -1;  // the index in ASKERS of the word whose piece reached it, or -1
static int64_t LEFT = -1;       // the address of the last jump or call that ran
static int64_t ASKING = -1;     // the address of the jump or call that went to halloc or hreuse, or -1
static long given_out;          // the number of pieces allocated so far
static uint64_t held_now;       // the cells allocated now

static int DEEPEST, DEEPEST_RETURN;     // the most cells the data stack has held, and the most the return stack has held
static int64_t DEEPEST_AT = -1;         // the address of the instruction about to run when the return stack was deepest
static int64_t DEEPEST_HELD[STACK_CELLS];       // what the return stack held then, oldest first
static long OWN_NAMES = -1, OWN_DONE = -1;      // the index in WORDS of the word that runs when a file starts being read, and of the word that runs when it is finished; -1 if the image has none
static int reading, most_reading, reading_then; // the number of files being read: now, at most, and when the return stack was deepest

// Returns the index of the word that contains the instruction at an
// address, or -1 if no word does.
static int word_holding(int64_t at) {
    int lo = 0, hi = NWORDS;                    // search for the first word that starts after the address
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (WORDS[mid].at <= at) lo = mid + 1; else hi = mid;
    }
    for(int idx=lo - 1; idx >= 0 && WORDS[idx].at + PAGE > at; idx--) {
        if (WORDS[idx].size) return(at < WORDS[idx].at + WORDS[idx].size ? idx : -1);
    }
    return(-1);
}

// Returns true if the address is inside the word with index `in`.
static int inside(long in, int64_t at) {
    return(in >= 0 && at >= WORDS[in].at && at < WORDS[in].at + WORDS[in].size);
}

// Returns the value a cell of the machine's memory has now, or 0 if the
// address is outside memory.
static uint64_t cell_now(uint64_t cell) {
    return(cell < CELLS ? cell_of((const uint8_t*)MACHINE->memory, cell) : 0);
}

// Returns the address of one of the nucleus's variables, found the way
// util/map.c finds it: from the labels for data, or else from the number
// that the word of that name pushes.  Returns 0 if neither is found.
static uint64_t variable(const char* tables, const char* name) {
    uint64_t at = data_called(tables, name);
    if (at) return(at);
    long place = place_of(WORDS, NWORDS, name);
    return(place >= 0 ? pushed_by((const uint8_t*)MACHINE->memory, place) : 0);
}

// Returns the index in WORDS of the word with that name, or -1.
static long word_called(const char* name) {
    long place = place_of(WORDS, NWORDS, name);
    return(place >= 0 ? word_holding(place) : -1);
}

// Starts recording.  It is called when the machine starts.  The first
// time, it reads the image's words from the machine's memory, which holds
// the image unchanged at that time.
static void watch(context* ctx) {
    MACHINE = ctx;
    if (!WORDS) {
        const char* tables = getenv("HEAP_NAMES");
        NWORDS = words_of((const uint8_t*)ctx->memory, MEM_BYTES, tables, &WORDS);
        HIMEM = variable(tables, "himem");
        HOLE = variable(tables, "hole");
        HALLOC = word_called("halloc");
        HREUSE = word_called("hreuse");
        HFREE = word_called("hfree");
        OWN_NAMES = word_called("own-names");
        OWN_DONE = word_called("own-done");
        TOUCHED = room_for(CELLS, 1);
    }
    npieces = naskers = given_out = 0;
    held_now = 0;
    DEEPEST = DEEPEST_RETURN = reading = most_reading = reading_then = 0;
    DEEPEST_AT = -1;
    FROM = FURTHEST = HIMEM ? cell_now(HIMEM) : 0;
    FURTHEST_FOR = -1;
    LEFT = ASKING = -1;
}

// Returns the number of cells of a piece that have been stored to.
static uint64_t stored_to(const piece* p) {
    uint64_t n = 0;
    for(uint64_t cell=p->at; cell < p->at + p->cells && cell < CELLS; cell++) n += TOUCHED[cell];
    return(n);
}

// Records that a piece was freed, or that all of it past its first
// `keeping` cells was.
static void let_go(long which, uint64_t keeping) {
    piece* p = &PIECES[which];
    asker* a = &ASKERS[p->asker];
    uint64_t was = stored_to(p);
    if (was > a->stored) a->stored = was;
    a->held -= p->cells - keeping;
    held_now -= p->cells - keeping;
    if (keeping) {
        p->cells = keeping;
        return;
    }
    for(long idx=which; idx + 1 < npieces; idx++) PIECES[idx] = PIECES[idx + 1];
    npieces--;
}

// Returns true if the instruction before an address is a call, which
// makes the address a return address.
static int returns_to(context* ctx, uint64_t at) {
    if (!at || at > MEM_BYTES / sizeof(instruction)) return(0);
    instruction ins = *(instruction*)&ctx->memory[at - 1];
    return(!ins.lit.lit_f && ins.jmp.op_type == OP_TYPE_CALL);
}

// Records that `cells` cells at `at` were allocated.  The word that asked
// is the one that contains the call or jump to halloc or hreuse.  If
// there was no such call, it is the word that contains the instruction at
// `by`, which moved the pointer.  `rsp` and `r` are the return stack's
// pointer and its top: the return stack is searched for the word that
// called the one that asked.
static void given(context* ctx, uint64_t at, uint64_t cells, int64_t by, int rsp, int64_t r) {
    int who = word_holding(ASKING >= 0 ? ASKING : by), of = -1;
    for(int down=0; down < DEEP && of < 0 && rsp - 1 - down >= 0; down++) {
        uint64_t back = ucell(down ? ctx->RSTACK[rsp - 1 - down] : r);
        int in = returns_to(ctx, back) ? word_holding((int64_t)back - 1) : -1;
        if (in >= 0 && in != who && !inside(HALLOC, (int64_t)back - 1) && !inside(HREUSE, (int64_t)back - 1)) of = in;
    }
    long which = 0;
    while (which < naskers && (ASKERS[which].who != who || ASKERS[which].of != of)) which++;
    if (which == naskers) {
        ONE_MORE(ASKERS, naskers, askers_room);
        ASKERS[naskers++] = (asker){ who, of, 0, cells, cells, 0, 0, 0 };
    }
    asker* a = &ASKERS[which];
    a->times++;
    if (cells < a->least) a->least = cells;
    if (cells > a->most) a->most = cells;
    a->held += cells;
    if (a->held > a->most_held) a->most_held = a->held;
    held_now += cells;
    given_out++;

    ONE_MORE(PIECES, npieces, pieces_room);
    long to = npieces++;
    while (to > 0 && PIECES[to - 1].at > at) PIECES[to] = PIECES[to - 1], to--;
    PIECES[to] = (piece){ at, cells, which };
    for(uint64_t cell=at; cell < at + cells && cell < CELLS; cell++) TOUCHED[cell] = 0;
    if (at + cells > FURTHEST) FURTHEST = at + cells, FURTHEST_FOR = which;
    ASKING = -1;
}

// Records that `cells` cells at `at` were freed: every piece that starts
// inside them is freed.
static void given_back(uint64_t at, uint64_t cells) {
    for(long idx=npieces - 1; idx >= 0; idx--) {
        if (PIECES[idx].at >= at && PIECES[idx].at < at + cells) let_go(idx, 0);
    }
}

// Records that himem moved back to `at`.  Every piece that starts at or
// past `at` is freed, and a piece that crosses `at` keeps only its cells
// before it.
static void back_to(uint64_t at) {
    for(long idx=npieces - 1; idx >= 0; idx--) {
        if (PIECES[idx].at >= at) let_go(idx, 0);
        else if (PIECES[idx].at + PIECES[idx].cells > at) let_go(idx, at - PIECES[idx].at);
    }
}

// vm.c calls this before it runs each instruction.  `at` is the
// instruction's address, `sp` and `rsp` are the two stack pointers, and
// `t` and `r` are the tops of the data stack and the return stack.
static void ran(context* ctx, int64_t at, int sp, int rsp, int64_t t, int64_t r) {
    if (at == 0 || !MACHINE) watch(ctx);
    if (sp > DEEPEST) DEEPEST = sp;
    if (OWN_NAMES >= 0 && at == WORDS[OWN_NAMES].at && ++reading > most_reading) most_reading = reading;
    if (OWN_DONE >= 0 && at == WORDS[OWN_DONE].at && reading) reading--;
    if (rsp > DEEPEST_RETURN) {
        DEEPEST_RETURN = rsp, DEEPEST_AT = at, reading_then = reading;
        for(int idx=0; idx < rsp && idx < STACK_CELLS; idx++) DEEPEST_HELD[idx] = idx == rsp - 1 ? r : ctx->RSTACK[idx];
    }
    if (!HIMEM) return;
    if ((HALLOC >= 0 && at == WORDS[HALLOC].at) || (HREUSE >= 0 && at == WORDS[HREUSE].at)) {
        if (!inside(HREUSE, LEFT)) ASKING = LEFT;       // when hreuse calls halloc, the word that asked is still the one that called hreuse
    } else if (HFREE >= 0 && at == WORDS[HFREE].at && sp >= 2) {
        given_back(ucell(ctx->DSTACK[sp - 2]), ucell(t));
    }
    instruction ins = *(instruction*)&ctx->memory[at];
    if (!ins.lit.lit_f && (ins.jmp.op_type == OP_TYPE_JMP || ins.jmp.op_type == OP_TYPE_CALL)) LEFT = at;
}

// vm.c calls this before a store.  `addr` is the cell stored to, `x` is
// the value, and `next` is the address of the next instruction.
static void stored(context* ctx, int64_t addr, int64_t x, int64_t next, int rsp, int64_t r) {
    if (!HIMEM) return;
    uint64_t cell = ucell(addr), was, is = ucell(x);
    if (cell == HIMEM) {
        was = cell_now(HIMEM);
        if (is > was) given(ctx, was, is - was, next - 1, rsp, r);
        else if (is < was) back_to(is);
    } else if (HOLE && cell == HOLE && inside(HREUSE, next - 1)) {
        was = cell_now(HOLE);
        if (is > was) given(ctx, was, is - was, next - 1, rsp, r);
    } else if (cell < CELLS) {
        TOUCHED[cell] = 1;
    }
}

// vm.c calls this when a disk read fills `cells` cells starting at
// `cell`.
static void filled(context* ctx, uint64_t cell, uint64_t cells) {
    if (!HIMEM) return;
    for(uint64_t idx=cell; idx < cell + cells && idx < CELLS; idx++) TOUCHED[idx] = 1;
}

// Returns the name of the word with that index, or "?" for -1.
static const char* named(int which) {
    return(which >= 0 ? WORDS[which].name : "?");
}

// Prints what the return stack held when it was deepest, oldest first, to
// show what the machine was doing.  A cell that is a return address is
// printed as the name of the word it returns into.  A cell that is the
// address of a word's first instruction is printed as that word's name
// with a tick before it: a task's word and a word run by catch are on the
// stack in that form.  The cells between those are printed as a count.  A
// number that was put on the stack and happens to equal such an address is
// printed as the word too.  Each task has its own part of the stack
// (spawn, nuc.fs), so the cells under the running task's part belong to
// other tasks.
static void held_said(FILE* out) {
    int col = 0, others = 0;
    fprintf(out, "  what it held then, the oldest first.  A word: a call returns into it.  'word: the word starts there,\n"
                 "  or a number kept there is that.  A number: so many cells that are neither.  [n]: the cell the line\n"
                 "  starts at.  A task has a part of the stack: the cells under the running task's are other tasks'\n");
    for(int idx=0; idx <= DEEPEST_RETURN && idx <= STACK_CELLS; idx++) {
        char said[NAME_ROOM + 2] = "";
        if (idx < DEEPEST_RETURN && idx < STACK_CELLS) {
            const uint64_t x = ucell(DEEPEST_HELD[idx]);
            int in = x && returns_to(MACHINE, x) ? word_holding((int64_t)x - 1) : -1;
            if (in >= 0) {
                snprintf(said, sizeof(said), "%s", WORDS[in].name);
            } else if (x && (in = word_holding((int64_t)x)) >= 0 && WORDS[in].at == (long)x) {
                snprintf(said, sizeof(said), "'%s", WORDS[in].name);
            } else {
                others++;
                continue;
            }
        }
        for(int which=0; which < 2; which++) {          // first the count of the cells that were neither, and then the word
            char token[NAME_ROOM + 2];
            if (which == 0 && !others) continue;
            if (which == 1 && !*said) continue;
            if (which == 0) snprintf(token, sizeof(token), "%d", others);
            else snprintf(token, sizeof(token), "%s", said);
            const int first = which == 0 ? idx - others : idx;
            if (!col || col + 1 + (int)strlen(token) > 100) col = fprintf(out, "%s  [%3d]", col ? "\n" : "", first) - (col ? 1 : 0);
            col += fprintf(out, " %s", token);
            if (which == 0) others = 0;
        }
    }
    fprintf(out, "\n");
}

static int by_most_held(const void* a, const void* b) {
    const asker* x = a;
    const asker* y = b;
    if (x->most_held != y->most_held) return(x->most_held > y->most_held ? -1 : 1);
    return(x->times > y->times ? -1 : x->times < y->times);
}

// Writes the results to the file HEAP names.
void heap_said(void) {
    const char* path = getenv("HEAP");
    if (!path) return;
    FILE* out = fopen(path, "w");
    if (!out) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    if (!MACHINE || !HIMEM) {
        fprintf(out, "the heap: the image has no himem, and nothing was watched\n");
        fclose(out);
        return;
    }
    const uint64_t bytes = CELL_BYTES;
    for(long idx=0; idx < npieces; idx++) {
        asker* a = &ASKERS[PIECES[idx].asker];
        uint64_t was = stored_to(&PIECES[idx]);
        if (was > a->stored) a->stored = was;
    }
    uint64_t now = cell_now(HIMEM), kept = 0;
    if (HOLE && cell_now(HOLE) >= FROM && cell_now(HOLE) + cell_now(HOLE + 1) <= now) kept = cell_now(HOLE + 1);
    uint64_t extent = now > FROM ? now - FROM : 0;

    fprintf(out, "the heap, as it was given out while the machine ran: from byte $%llx, cell $%llx\n",
            (unsigned long long)(FROM * bytes), (unsigned long long)FROM);
    fprintf(out, "  pieces given out: %ld.  The furthest it reached: byte $%llx, %llu bytes from its start",
            given_out, (unsigned long long)(FURTHEST * bytes), (unsigned long long)((FURTHEST - FROM) * bytes));
    if (FURTHEST_FOR >= 0) {
        fprintf(out, ", for %s (%s)", named(ASKERS[FURTHEST_FOR].who), named(ASKERS[FURTHEST_FOR].of));
    }
    fprintf(out, "\n  when the machine stopped: %llu bytes from its start to byte $%llx; %llu held in %ld pieces, "
            "%llu kept to be had again, %llu neither\n",
            (unsigned long long)(extent * bytes), (unsigned long long)(now * bytes),
            (unsigned long long)(held_now * bytes), npieces, (unsigned long long)(kept * bytes),
            (unsigned long long)((extent > held_now + kept ? extent - held_now - kept : 0) * bytes));
    fprintf(out, "  the stacks, %d cells each: the data stack had %d at the most, and the return stack %d, in %s",
            STACK_CELLS, DEEPEST, DEEPEST_RETURN, named(DEEPEST_AT >= 0 ? word_holding(DEEPEST_AT) : -1));
    if (OWN_NAMES >= 0 && OWN_DONE >= 0) {
        fprintf(out, ";\n  files being read then: %d, and the most at once: %d", reading_then, most_reading);
    }
    fprintf(out, "\n");
    held_said(out);
    fprintf(out, "\n");

    qsort(ASKERS, naskers, sizeof(asker), by_most_held);
    fprintf(out, "  who asked (and who asked that)             times     bytes a piece        held   most held  most stored to\n");
    for(long idx=0; idx < naskers; idx++) {
        const asker* a = &ASKERS[idx];
        char who[2 * NAME_ROOM + 8], each[48];
        snprintf(who, sizeof(who), "%s (%s)", named(a->who), named(a->of));
        if (a->least == a->most) snprintf(each, sizeof(each), "%llu", (unsigned long long)(a->most * bytes));
        else snprintf(each, sizeof(each), "%llu-%llu", (unsigned long long)(a->least * bytes), (unsigned long long)(a->most * bytes));
        fprintf(out, "  %-40s %7ld %17s %11llu %11llu %15llu\n", who, a->times, each,
                (unsigned long long)(a->held * bytes), (unsigned long long)(a->most_held * bytes),
                (unsigned long long)(a->stored * bytes));
    }
    if (fclose(out)) {
        perror(path);
        exit(EXIT_FAILURE);
    }
}

#ifndef WATCHING_A_TEST
#define main run_image
#include "../main.c"
#undef main

int main(int argc, char *argv[]) {
    run_image(argc, argv);
    heap_said();
    return(0);
}
#endif
