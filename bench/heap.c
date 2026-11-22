//
// heap IMAGE.hex [DISK.img [MEMORY.out]]
//
// The reference VM, watching what is given out of the heap: the memory
// above the image that halloc gives out (machine.fs).  It is main.c and
// vm.c as they are, with what vm.c tells of an instruction about to be run
// (RUNS), of a cell about to be stored to (STORES) and of cells read into
// from the disk (FILLS) taken up here.
//
// What is given out is seen where the nucleus keeps it.  himem moves up
// when halloc gives memory out, and back when memory is given back by
// putting it where it was; the run that hfree keeps (hole) is had from by
// hreuse; and hfree is told what is given back.  Nothing here says how
// much any of them gives: a piece is what the pointer moved over.
//
// Who asked is the word that the call or the jump to halloc or to hreuse
// was in; and who asked that is the nearest return on the return stack
// that is to another word.  How much of a piece was stored to is how many
// of its cells an instruction stored to, or the disk was read into, while
// the piece was held: so a piece that is given more than it ever uses
// shows.
//
// The stacks are the machine's other room, and are not memory: how many
// cells each has had at the most is watched too, with the word that was
// running when the return stack had its most, what that stack held then
// (held_said), and how many files were being read (own-names and own-done,
// files.fs): a file that is read keeps what it has of the dictionary on the
// return stack until it is done.  The most a stack has had is the highest
// cell that was used, and not how deep one task went: a task has a part of
// each stack, from where spawn is told (nuc.fs).
//
// When the machine stops, what was found is written to the file that the
// environment's HEAP names, if it names one (heap_said): how far the heap
// reached and who asked then; what of it is held, kept to be had again and
// neither when the machine stopped; the most each stack had; and a line
// for each who asked, with how often, how big a piece, how much is held
// and was at most, and the most of any one piece that was stored to.  HEAP_NAMES names the file of
// names that goes with the image (nuc.tables), for the words that have no
// header.  The machine is watched from when it last started at instruction
// 0, so a test that starts it more than once says what the last did.
//
// A test that has the VM in it is built with this in place of vm.c, and
// with WATCHING_A_TEST said: then this is the VM alone, and the test has
// what was found written when it is done.
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

#define CELLS   ((uint64_t)1 << ADDR_BITS)     // the cells of the machine's memory
#define DEEP    8                               // how far down the return stack who asked is looked for

static context* MACHINE;
static word* WORDS;             // the image's words, by place
static int NWORDS;
static uint64_t HIMEM, HOLE;    // the cells the nucleus keeps them in, or nothing
static long HALLOC = -1, HREUSE = -1, HFREE = -1;      // where the three words are, or less than nothing

// A piece of the heap that is held.
typedef struct {
    uint64_t at, cells;
    long asker;                 // who asked for it: which of ASKERS
} piece;
static piece* PIECES;           // in the order they are in memory
static long npieces, pieces_room;

// Who asked, and what they have had.
typedef struct {
    int who, of;                // the word, and the word that asked it: less than nothing if neither is a word
    long times;
    uint64_t least, most;       // the cells of a piece: the fewest and the most
    uint64_t held, most_held;   // the cells held now, and the most there were at once
    uint64_t stored;            // the most cells of any one piece that were stored to
} asker;
static asker* ASKERS;
static long naskers, askers_room;

static uint8_t* TOUCHED;        // of each cell of memory: stored to since it was given out
static uint64_t FROM;           // where the heap starts: himem as the image has it
static uint64_t FURTHEST;       // the furthest himem has been,
static long FURTHEST_FOR = -1;  // and who had asked then
static int64_t LEFT = -1;       // where the last jump or call was,
static int64_t ASKING = -1;     // and where the one to halloc or hreuse was
static long given_out;          // how many pieces there have been
static uint64_t held_now;       // the cells held now

static int DEEPEST, DEEPEST_RETURN;     // the most cells each stack has had: the data stack, and the return stack
static int64_t DEEPEST_AT = -1;         // the instruction about to be run when the return stack had that many,
static int64_t DEEPEST_HELD[STACK_CELLS];       // and what it held then, the oldest first
static long OWN_NAMES = -1, OWN_DONE = -1;      // the words a file that is read begins and is done with, or less than nothing
static int reading, most_reading, reading_then; // files being read: now, at the most, and when the return stack had its most

// The word that the instruction at a place is in, or less than nothing.
static int word_holding(int64_t at) {
    int lo = 0, hi = NWORDS;                    // the first word that starts after it
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (WORDS[mid].at <= at) lo = mid + 1; else hi = mid;
    }
    for(int idx=lo - 1; idx >= 0 && WORDS[idx].at + PAGE > at; idx--) {
        if (WORDS[idx].size) return(at < WORDS[idx].at + WORDS[idx].size ? idx : -1);
    }
    return(-1);
}

static int inside(long in, int64_t at) {
    return(in >= 0 && at >= WORDS[in].at && at < WORDS[in].at + WORDS[in].size);
}

static uint64_t cell_now(uint64_t cell) {
    return(cell < CELLS ? cell_of((const uint8_t*)MACHINE->memory, cell) : 0);
}

// The cell a variable of the nucleus is, as util/map.c finds one.
static uint64_t variable(const char* tables, const char* name) {
    uint64_t at = data_called(tables, name);
    if (at) return(at);
    long place = place_of(WORDS, NWORDS, name);
    return(place >= 0 ? pushed_by((const uint8_t*)MACHINE->memory, place) : 0);
}

static long word_called(const char* name) {
    long place = place_of(WORDS, NWORDS, name);
    return(place >= 0 ? word_holding(place) : -1);
}

// The machine starts: so does the watch.  The words are the image's, which
// is what is in memory then.
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

// How many cells of a piece have been stored to.
static uint64_t stored_to(const piece* p) {
    uint64_t n = 0;
    for(uint64_t cell=p->at; cell < p->at + p->cells && cell < CELLS; cell++) n += TOUCHED[cell];
    return(n);
}

// A piece is no longer held, or only its first cells are.
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

// Is the instruction before a place a call?  Then the place is where one
// returns to.
static int returns_to(context* ctx, uint64_t at) {
    if (!at || at > MEM_BYTES / sizeof(instruction)) return(0);
    instruction ins = *(instruction*)&ctx->memory[at - 1];
    return(!ins.lit.lit_f && ins.jmp.op_type == OP_TYPE_CALL);
}

// Cells have been given out: to whoever the call or the jump to halloc or
// hreuse was in, or if there was none, to whoever moved the pointer.
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

// Cells have been given back: the pieces that start in them are let go.
static void given_back(uint64_t at, uint64_t cells) {
    for(long idx=npieces - 1; idx >= 0; idx--) {
        if (PIECES[idx].at >= at && PIECES[idx].at < at + cells) let_go(idx, 0);
    }
}

// The pointer has been put back: what is past it is let go.
static void back_to(uint64_t at) {
    for(long idx=npieces - 1; idx >= 0; idx--) {
        if (PIECES[idx].at >= at) let_go(idx, 0);
        else if (PIECES[idx].at + PIECES[idx].cells > at) let_go(idx, at - PIECES[idx].at);
    }
}

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
        if (!inside(HREUSE, LEFT)) ASKING = LEFT;       // halloc asked by hreuse is for whoever asked hreuse
    } else if (HFREE >= 0 && at == WORDS[HFREE].at && sp >= 2) {
        given_back(ucell(ctx->DSTACK[sp - 2]), ucell(t));
    }
    instruction ins = *(instruction*)&ctx->memory[at];
    if (!ins.lit.lit_f && (ins.jmp.op_type == OP_TYPE_JMP || ins.jmp.op_type == OP_TYPE_CALL)) LEFT = at;
}

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

static void filled(context* ctx, uint64_t cell, uint64_t cells) {
    if (!HIMEM) return;
    for(uint64_t idx=cell; idx < cell + cells && idx < CELLS; idx++) TOUCHED[idx] = 1;
}

static const char* named(int which) {
    return(which >= 0 ? WORDS[which].name : "?");
}

// What the return stack held when it had its most, the oldest first, as far
// as that says what the machine was at.  A cell that is where a call
// returns to is said by the word it goes back into; one that is where a
// word starts, by that word with a tick before it, which is how a task's
// word and a caught word are there; and the cells between, by how many they
// are.  A number that is kept there and is also such a place is said as
// one.  A task has a part of the stack (spawn, nuc.fs), so the cells under
// the part of the task that was running are other tasks'.
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
        for(int which=0; which < 2; which++) {          // the cells that were neither, and then the word
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
