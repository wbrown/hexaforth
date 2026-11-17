//
// fuse.c - which instruction does the work of two or of three, which two
// do the work of three, and what shorter run does the work of a run that
// has numbers and variables in it, asked of the machine.
//
// Nothing here says what an instruction does.  The machine's step is said
// once (vm_step.h), and it is had here twice: in vm.c, which runs it on
// cells, and on terms.
//
// On terms it says what a sequence leaves in every cell, as the name of
// what is there and not a number: what was in some cell when it started,
// or what an operation made of two such.  Two sequences that leave the
// same terms leave the same from every state there is, and that is how an
// answer is found (fuse_one, fuse_two, fuse_lit).  It is so because the
// step is the machine's own, and because of what is said beside the
// arithmetic in vm.c: which operations take their inputs in either order,
// which may be regrouped and what number leaves the other input as it was,
// which is undone by being done again, which leave a flag, and that two
// cells are the same when their difference is nothing.  Two sequences that
// do the same only by something else that is so of numbers are not found.
//
// On cells it is a test of that, made otherwise.  Every answer the terms
// give is run on vm.c against what it is the answer to, from a number of
// states, and what each leaves is compared: the depths, the stacks, every
// cell of data, and where it goes next (same).  If they leave something
// else the build stops: the step on terms is wrong, or what is said of the
// arithmetic is, or the states are.
//
// A state is cells of numbers of the kinds a program has: addresses of
// data, small numbers, flags, and anything.  Taken by chance alone they
// would not tell what a sequence does with what it has made itself: the
// xor of two cells is like nothing else in a state, so `xor =` would never
// be seen to say yes, and would pass for an instruction that leaves
// nothing.  So the two are run again from states made for them: what they
// make from a state is put, a value and a place at a time, where they may
// next look for it (same_from).  Nor would they tell two sequences apart
// that ask different things of the cells they are given, when the answer
// to both is nearly always no: so the two are also run from every way of
// giving those cells a few numbers (same_from_few).  A state may also lead
// a sequence to fetch the cell it is itself in: so the two are both in
// memory while either runs.  This was once all there was, and it gave a
// wrong answer: running is a test, and no number of states makes it more.
//
// The machine that is run is the strict one (STRICT_ALIGN, ROM_BYTES),
// which stops at a fetch, a store or a return outside memory and at a
// store into its ROM: what those do is not defined (MODEL.md).  A state
// that stops a sequence says nothing of it but that it stopped, and of a
// sequence that few states run to the end of the test says nothing.  The
// machine runs the sequence and no more (strict_steps), so one that
// returns is run as any other is, and where it returns to is part of what
// it leaves.  Memory is 256 cells, so that all of it can be looked at
// after every run.  The code is in a ROM in the middle of it, so that the
// cells a flag and a small number are the addresses of are data like any
// other, and what fetches or stores through one runs.  A variable is a
// cell far from those, and it is one cell whether a sequence comes to it
// by its instruction or by its address as a number: so the machine is
// given a cell for each address the two sequences name as a number (FAR).
// A sequence that fetches or stores at an address it works out, where the
// machine has no cell, stops from every state, and of that one the test
// says nothing: the terms are all there is for it.
//
// Not asked: what does IO, of which nothing is said; and a sequence that
// returns before its last instruction, which does not run the rest.
//
// It is built for the cell the image is for (CELL_BITS).  cross.fs has it
// through gforth's C interface.
//

#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#undef ADDR_BITS
#define ADDR_BITS   8
#undef DISP_W
#define DISP_W      0                               // no display
// The cells from nothing up to LOW and from DATA up are data: the first are
// what a flag or a small number is the address of, and the rest are what
// the states call addresses.  The cells between are the ROM, and the code
// is there.
#define LOW         64
#define DATA        ((1 << ADDR_BITS) - 64)
#define DATA_CELLS  (LOW + (1 << ADDR_BITS) - DATA)
#define WHERE(nth)  ((nth) < LOW ? (nth) : DATA + (nth) - LOW)  // the cell that the nth of the data is
#define SEQ         3               // the most instructions in a sequence that is joined
#define RUN         32              // the most in a run cross.fs asks about (fl-room)
#define LEADS       8               // the cells of what leads to each of two sequences, at the start of the ROM;
#define FIRST_CELL  (LOW + 2 * LEADS)                   // and after them the two, which have the rest of it,
#define SEQ_CELLS   ((DATA - FIRST_CELL) / 2)           // so many cells each
#define LAID        (SEQ_CELLS * (CELL_BYTES / 2) - 1)  // the most instructions in one: one that does nothing is after it
#define AT(which)   ((FIRST_CELL + SEQ_CELLS * (which)) * (CELL_BYTES / 2))     // as an instruction
#define LEAD(which) ((LOW + LEADS * (which)) * (CELL_BYTES / 2))    // where what leads to each starts
#define DEPTH       8               // of each stack, when the sequence starts
#define FILLED      16              // cells of each stack that a state gives
#define STATES      96
#define ENOUGH      8               // a sequence that runs to its end from fewer
                                    // states than this is not known well enough
#define TRIED       16              // the states that more are made from,
#define MADE        6               // the values put into each,
#define PLACES      10              // and the places each is put

#define STRICT_ALIGN
#define ROM_FROM    (LOW * CELL_BYTES)
#define ROM_BYTES   (DATA * CELL_BYTES)

// The machine, which stops by coming back here.  It says why it stopped
// each time, which is not wanted.  What stderr was is put back and not
// only taken away: where the C library is Apple's it is a macro and
// nothing else, and what is included after this uses it.
//
// The machine that is run has few cells, and a run names cells that are
// not among them: a variable, by its instruction and by its address as a
// number, is a cell from VAR_BASE.  So the machine is given a cell for
// each address the two runs being compared name as a number (FAR), and a
// variable is the same cell however it is come to.
#define FARS 8
static struct {
    int n;
    uint64_t at[FARS];              // the addresses,
    uint8_t cell[FARS][8];          // and their cells
} far;

static uint8_t* far_cell(uint64_t addr) {
    for(int idx=0; idx < far.n; idx++) if (far.at[idx] == addr) return(far.cell[idx]);
    return(NULL);
}

static jmp_buf stopped;
static FILE* nowhere;
#define exit(code) longjmp(stopped, 1)
#define FAR_CELL(addr) far_cell(ucell(addr))
#pragma push_macro("stderr")
#undef stderr
#define stderr nowhere
#include "../vm.c"
#pragma pop_macro("stderr")
#undef exit
#include "../vm_opcodes.c"
#include "stack.c"

// The machine's sizes, from where it says them: every word an instruction
// can be is as many as an instruction has bits for (vm_instruction.h), and
// a field has as many values as vm_opcodes.h has names for it.
#define COUNT(of)   ((int)(sizeof(of) / sizeof((of)[0])))
#define EVERY_WORD  (1 << (8 * sizeof(instruction)))
#define INPUTS      COUNT(INPUT_MUX_REPR)
#define OPS         COUNT(ALU_OPS_REPR)
#define OUTPUTS     COUNT(OUTPUT_MUX_REPR)

#include "named.c"                  // a word of vm_opcodes.h by its name
#include "room.c"                   // room for as many of a thing as there are

typedef struct {
    int64_t dstack[FILLED];
    int64_t rstack[FILLED];
    int64_t data[DATA_CELLS];
    int64_t far[FARS];              // what is in the cells the machine is given besides
} state;

static state STATE[STATES];
static context machine;

// The states are the same every time.
static uint64_t seed = 0x9e3779b97f4a7c15ull;
static uint64_t chance(void) {
    seed ^= seed << 13;
    seed ^= seed >> 7;
    seed ^= seed << 17;
    return(seed);
}

// The kinds of number a cell of a state holds: the address of a cell of
// data; a small number, which added to an address is mostly still one; a
// flag; the complement of an address or of a small number, which is how a
// number below nothing is made here, and is an address again when it has
// been inverted; and anything.
static int64_t address(void)  { return(DATA + chance() % (DATA_CELLS - LOW)); }
static int64_t small(void)    { return(chance() % 16); }
static int64_t flag(void)     { return(chance() & 1 ? cell(TRUE) : cell(FALSE)); }
static int64_t below(void)    { return(cell(~(chance() & 1 ? address() : small()))); }
static int64_t anything(void) { return(cell(chance())); }

// A sequence runs to its end only from a state whose cells are of the
// kinds it wants: one that fetches wants an address, one that inverts and
// then fetches the complement of one.  So the states are not all of every
// kind mixed, which would give each sequence few to run from: each is of
// one of these mixes, and they are taken in turn.  The first few states,
// which are the ones tried first, are of the first few mixes, and none of
// those has a cell of just anything.
#define KINDS 5
#define MIXES 6
static const int MIX[MIXES][KINDS] = {
    // address, small, flag, below, anything
    { 1, 0, 0, 0, 0 },
    { 3, 2, 0, 0, 0 },
    { 2, 1, 1, 0, 0 },
    { 1, 0, 0, 1, 0 },
    { 1, 1, 0, 0, 1 },
    { 1, 1, 1, 1, 1 },
};

static int64_t of_mix(const int* mix) {
    int all = 0;
    for(int kind=0; kind < KINDS; kind++) all += mix[kind];
    int which = chance() % all;
    if ((which -= mix[0]) < 0) return(address());
    if ((which -= mix[1]) < 0) return(small());
    if ((which -= mix[2]) < 0) return(flag());
    if ((which -= mix[3]) < 0) return(below());
    return(anything());
}

// A value for a cell of a state: half of them one of a few the state has,
// so that cells are often the same as each other.
static int64_t value(const int64_t* few, const int* mix) {
    return(chance() & 1 ? few[chance() % 4] : of_mix(mix));
}

static void make_states(void) {
    for(int s=0; s < STATES; s++) {
        const int* mix = MIX[s % MIXES];
        int64_t few[4] = { of_mix(mix), of_mix(mix), of_mix(mix), of_mix(mix) };
        for(int idx=0; idx < FILLED; idx++) STATE[s].dstack[idx] = value(few, mix);
        for(int idx=0; idx < FILLED; idx++) STATE[s].rstack[idx] = value(few, mix);
        for(int idx=0; idx < DATA_CELLS; idx++) STATE[s].data[idx] = value(few, mix);
        for(int idx=0; idx < FARS; idx++) STATE[s].far[idx] = value(few, mix);
    }
}

static uint16_t push(int n) {
    instruction ins = {0};
    ins.lit.lit_f = true;
    ins.lit.lit_v = n;
    return(*(uint16_t*)&ins);
}

static uint16_t jump(int target) {
    instruction ins = {0};
    ins.jmp.op_type = OP_TYPE_JMP;
    ins.jmp.target = target;
    return(*(uint16_t*)&ins);
}

// Memory has room for two sequences, and what leads to each: it sets the
// two depths and goes to the sequence.  Two that are to be compared are
// both there while either is run, so that a fetch of a cell a sequence is
// in, which a state can lead to, fetches the same whichever is run.
static int lead;                    // how many instructions lead to a sequence
static void lay_leads(void) {
    uint16_t io_store = ins_named("io!");
    for(int which=0; which < 2; which++) {
        uint16_t* m = machine.memory + LEAD(which);
        *m++ = push(DEPTH);  *m++ = push(IO_RSP);  *m++ = io_store;
        *m++ = push(DEPTH);  *m++ = push(IO_SP);   *m++ = io_store;
        *m++ = jump(AT(which));
        lead = m - (machine.memory + LEAD(which));
    }
}

// Put a sequence in memory, as the first or the second.
_Static_assert(LAID >= RUN, "fuse.c: a run cross.fs asks about is longer than there is room to lay it");
static int after[2];                // the instruction after each
static void lay(int which, const uint16_t* seq, int len) {
    uint16_t* code = machine.memory + AT(which);
    memset(code, 0, (LAID + 1) * 2);
    memcpy(code, seq, len * 2);
    after[which] = AT(which) + len;
}

static int returns(uint16_t x) {
    return(((instruction*)&x)->alu.r_eip);
}

static uint16_t returning(uint16_t x, int does) {
    ((instruction*)&x)->alu.r_eip = does;
    return(x);
}

static uint64_t mix(uint64_t h, uint64_t x) {
    h ^= x;
    h *= 0x100000001b3ull;
    h ^= h >> 29;
    return(h);
}

#define STOPPED 1

// Run the first `len` instructions of one of the two sequences in memory
// from a state, and no further.  What it left is in the machine; false if
// the machine stopped.
static int run(int which, int len, const state* from) {
    memcpy(machine.DSTACK, from->dstack, sizeof(from->dstack));
    memcpy(machine.RSTACK, from->rstack, sizeof(from->rstack));
    for(int idx=0; idx < DATA_CELLS; idx++) {
        memcpy(cell_at(&machine, WHERE(idx)), &from->data[idx], CELL_BYTES);
    }
    for(int idx=0; idx < far.n; idx++) memcpy(far.cell[idx], &from->far[idx], CELL_BYTES);
    machine.EIP = LEAD(which);
    strict_steps = lead + len;
    if (setjmp(stopped)) return(0);
    vm(&machine);
    return(1);
}

// A number for what one of the two sequences leaves from a state.  `ran`
// counts those that ran to the end.  Where it goes next is the instruction
// after it, or where it returned to.  The two are in different places, so
// to go on is to go to different instructions: the instruction after
// either counts as going on, for both, or a state that had one of the two
// return to just after the other would tell them apart for that alone.
static int ran;
static uint64_t left(int which, int len, const state* from) {
    if (!run(which, len, from)) return(STOPPED);
    ran++;
    uint64_t h = mix(mix(0xcbf29ce484222325ull, machine.SP), machine.RSP);
    for(int idx=0; idx < machine.SP; idx++) h = mix(h, machine.DSTACK[idx]);
    for(int idx=0; idx < machine.RSP; idx++) h = mix(h, machine.RSTACK[idx]);
    for(int idx=0; idx < DATA_CELLS; idx++) h = mix(h, mem_load(&machine, WHERE(idx)));
    for(int idx=0; idx < far.n; idx++) h = mix(h, mem_load(&machine, far.at[idx]));
    int on = machine.EIP == after[0] || machine.EIP == after[1];
    h = mix(h, on ? -1 : machine.EIP);
    return(h | 2);
}

// Was a value there before the sequence ran: in a stack, in the data, or
// in the ROM?
static int was_there(int64_t v, const state* in) {
    if (v == DEPTH) return(1);
    for(int idx=0; idx < FILLED; idx++) {
        if (in->dstack[idx] == v || in->rstack[idx] == v) return(1);
    }
    for(int idx=0; idx < DATA_CELLS; idx++) if (in->data[idx] == v) return(1);
    for(int idx=0; idx < far.n; idx++) if (in->far[idx] == v) return(1);
    for(int idx=LOW; idx < DATA; idx++) if (mem_load(&machine, idx) == v) return(1);
    return(0);
}

static int note(int64_t v, const state* in, int64_t* made, int n) {
    if (n == MADE || was_there(v, in)) return(n);
    for(int idx=0; idx < n; idx++) if (made[idx] == v) return(n);
    made[n] = v;
    return(n + 1);
}

// What one of the two sequences makes from a state: the values in what
// each of its first instructions leave that were in no cell of the state.
// They are added to the n already noted.
static int makes(int which, int len, const state* from, int64_t* made, int n) {
    for(int upto=1; upto <= len; upto++) {
        if (!run(which, upto, from)) break;
        for(int idx=0; idx < machine.SP; idx++) n = note(machine.DSTACK[idx], from, made, n);
        for(int idx=0; idx < machine.RSP; idx++) n = note(machine.RSTACK[idx], from, made, n);
        for(int idx=0; idx < DATA_CELLS; idx++) n = note(mem_load(&machine, WHERE(idx)), from, made, n);
        for(int idx=0; idx < far.n; idx++) n = note(mem_load(&machine, far.at[idx]), from, made, n);
    }
    return(n);
}

// Put a value in one of the places a sequence may look: the top cells of
// the stacks, and the cells of data that the top cells are the addresses
// of.  False if the place is not there.
static int put(state* in, int place, int64_t v) {
    if (place < 4) { in->dstack[DEPTH - 1 - place] = v; return(1); }
    if (place < 6) { in->rstack[DEPTH - 1 - (place - 4)] = v; return(1); }
    int64_t at = place < 9 ? in->dstack[DEPTH - 1 - (place - 6)] : in->rstack[DEPTH - 1];
    for(int idx=0; idx < far.n; idx++) if (far.at[idx] == ucell(at)) { in->far[idx] = v; return(1); }
    if (at < 0 || at >= (1 << ADDR_BITS) || (at >= LOW && at < DATA)) return(0);
    in->data[at < LOW ? at : LOW + at - DATA] = v;
    return(1);
}

// A sequence that runs to its end from too few of the states has not been
// shown to stop always: the states have not, in the cells it reads, the
// numbers it wants.  One that shifts and then fetches wants two numbers
// that shifted are an address, and no state was made with that in mind.
// So states it does run from are looked for: a state, with one or two of
// the cells a sequence reads first given another number.  These are the
// numbers given, and the places, as `put` counts them.
static int64_t GIVE[64];
static int gives;
static const int READ[] = { 0, 1, 2, 4 };
#define READS 4

static void list_give(void) {
    for(int v=0; v < 16; v++) GIVE[gives++] = v;
    for(int idx=0; idx < 8; idx++) GIVE[gives++] = DATA + idx * ((DATA_CELLS - LOW) / 8) + idx;
    for(int idx=0; idx < 16; idx += 3) GIVE[gives++] = cell(~GIVE[idx]);
    for(int idx=16; idx < 24; idx += 2) GIVE[gives++] = cell(~GIVE[idx]);
    GIVE[gives++] = cell(TRUE);
}

static state FOUND[TRIED];
static int founds;

// Does one of the two sequences run from a state with one cell, or two,
// given another number?  The state is left in `with`.
static int runs_with(int which, int len, const state* from, int one, int other, state* with) {
    *with = *from;
    put(with, READ[one / gives], GIVE[one % gives]);
    if (other >= 0) put(with, READ[other / gives], GIVE[other % gives]);
    return(run(which, len, with));
}

// Look for states that one of the two sequences runs from.  Where the
// looking starts is taken from the sequence, so that it finds the same
// each time and not the same for every state.
static void find_states(int which, int len) {
    uint64_t start = 0;
    for(int idx=0; idx < len; idx++) start = mix(start, machine.memory[AT(which) + idx]);
    int choices = READS * gives;
    founds = 0;
    for(int s=0; s < 2 * TRIED && founds < TRIED; s++) {
        uint64_t h = mix(start, s);
        int found = 0;
        for(int i=0; (s & 1) && i < choices && !found; i++) {
            found = runs_with(which, len, &STATE[s], (h + i) % choices, -1, &FOUND[founds]);
        }
        for(int i=0; i < choices && !found; i++) {
            int one = (h + i) % choices;
            uint64_t h2 = mix(h, one);
            for(int j=0; j < choices && !found; j++) {
                int other = (h2 + j) % choices;
                if (one / gives == other / gives) continue;
                found = runs_with(which, len, &STATE[s], one, other, &FOUND[founds]);
            }
        }
        founds += found;
    }
}

// Do the two sequences in memory leave the same from a state, and from
// the states made of it by putting each value either makes in each place?
static int same_from(int alen, int blen, const state* from) {
    int64_t made[MADE];
    int n = makes(0, alen, from, made, 0);
    n = makes(1, blen, from, made, n);
    for(int idx=0; idx < n; idx++) {
        for(int place=0; place < PLACES; place++) {
            state with = *from;
            if (!put(&with, place, made[idx])) continue;
            if (left(0, alen, &with) != left(1, blen, &with)) return(0);
        }
    }
    return(1);
}

// A flag is seldom yes by chance: two cells of a state are seldom the same,
// and two pairs of them at once hardly ever.  So two sequences that leave
// different flags leave the same, no, from nearly every state: `= swap r>
// = and` and two instructions that asked something else were taken for the
// same, from every state there was.  The two sequences in memory are
// therefore run from every way of giving the cells they read first one of
// three numbers, which are nothing, one and the flag that says yes: then
// every way those cells can be the same as each other or not, and above or
// below each other, is there.  The cells are the top five of the data
// stack, the top two of the return stack, and the two cells of data that
// nothing and one are the addresses of.
//
// There are a great many ways, and all but those cells are the same in
// each: so the data is not put in memory a cell at a time for every run,
// nor a number made of all that is left.  The memory of the first state is
// kept as it is in the machine, and put back whole; and what a run left is
// kept as it is in the machine, and compared whole.
#define NUMBERS 3
#define GIVEN   9
#define LOW_BYTES   (LOW * CELL_BYTES)                  // the data below the ROM,
#define HIGH_BYTES  ((DATA_CELLS - LOW) * CELL_BYTES)   // the data above it,
#define HIGH_AT     ((uint8_t*)machine.memory + DATA * CELL_BYTES)  // and where that is

typedef struct {
    int sp, rsp, next;
    int64_t dstack[FILLED], rstack[FILLED];
    uint8_t low[LOW_BYTES], high[HIGH_BYTES];
    uint8_t far[FARS][8];
} leavings;

// Run one of the two from stacks and data as they are in the machine.
// Only the data is put in memory: the ROM is as it was.
static int run_few(int which, int len, const state* stacks, const leavings* data) {
    memcpy(machine.DSTACK, stacks->dstack, sizeof(stacks->dstack));
    memcpy(machine.RSTACK, stacks->rstack, sizeof(stacks->rstack));
    memcpy(machine.memory, data->low, LOW_BYTES);
    memcpy(HIGH_AT, data->high, HIGH_BYTES);
    memcpy(far.cell, data->far, sizeof(far.cell));
    machine.EIP = LEAD(which);
    strict_steps = lead + len;
    if (setjmp(stopped)) return(0);
    vm(&machine);
    return(1);
}

static int goes_next(void) {
    return(machine.EIP == after[0] || machine.EIP == after[1] ? -1 : machine.EIP);
}

static void keep_left(leavings* l) {
    l->sp = machine.SP;
    l->rsp = machine.RSP;
    l->next = goes_next();
    memcpy(l->dstack, machine.DSTACK, sizeof(l->dstack));
    memcpy(l->rstack, machine.RSTACK, sizeof(l->rstack));
    memcpy(l->low, machine.memory, LOW_BYTES);
    memcpy(l->high, HIGH_AT, HIGH_BYTES);
    memcpy(l->far, far.cell, sizeof(far.cell));
}

static int left_is(const leavings* l) {         // is what the machine has what was kept?
    return(l->sp == machine.SP && l->rsp == machine.RSP && l->next == goes_next() &&
           !memcmp(l->dstack, machine.DSTACK, l->sp * sizeof(l->dstack[0])) &&
           !memcmp(l->rstack, machine.RSTACK, l->rsp * sizeof(l->rstack[0])) &&
           !memcmp(l->low, machine.memory, LOW_BYTES) && !memcmp(l->high, HIGH_AT, HIGH_BYTES) &&
           !memcmp(l->far, far.cell, sizeof(far.cell)));
}

static int same_from_few(int alen, int blen) {
    const int64_t number[NUMBERS] = { 0, 1, cell(TRUE) };
    static leavings data, first;
    int ways = 1;
    for(int idx=0; idx < GIVEN; idx++) ways *= NUMBERS;
    state with = STATE[0];
    run(0, 0, &with);                   // the first state's data, as it is in memory
    keep_left(&data);
    for(int way=0; way < ways; way++) {
        int w = way;
        for(int idx=0; idx < 5; idx++, w /= NUMBERS) with.dstack[DEPTH - 1 - idx] = number[w % NUMBERS];
        for(int idx=0; idx < 2; idx++, w /= NUMBERS) with.rstack[DEPTH - 1 - idx] = number[w % NUMBERS];
        for(int idx=0; idx < 2; idx++, w /= NUMBERS) {
            memcpy(data.low + idx * CELL_BYTES, &number[w % NUMBERS], CELL_BYTES);
        }
        int ran = run_few(0, alen, &with, &data);
        if (ran) keep_left(&first);
        if (run_few(1, blen, &with, &data) != ran) return(0);
        if (ran && !left_is(&first)) return(0);
    }
    return(1);
}

// Do two sequences do the same, as far as running them can tell?  Both are
// put in memory, and each is run from every state: they must leave the
// same.  If the first runs to its end from too few, states it does run
// from are looked for, and they must leave the same from those; and if
// there are still too few, nothing is known, which is less than nothing
// here.  Then they are run from states made for them (same_from), and from
// every way of giving the cells they read a few numbers (same_from_few).
static int same(const uint16_t* a, int alen, const uint16_t* b, int blen) {
    lay(0, a, alen);
    lay(1, b, blen);
    ran = 0;
    for(int s=0; s < STATES; s++) {
        uint64_t first = left(0, alen, &STATE[s]);
        int so_far = ran;
        if (first != left(1, blen, &STATE[s])) return(0);
        ran = so_far;
    }
    founds = 0;
    if (ran < ENOUGH) {
        find_states(0, alen);
        for(int idx=0; idx < founds; idx++) {
            uint64_t first = left(0, alen, &FOUND[idx]);
            int so_far = ran;
            if (first != left(1, blen, &FOUND[idx])) return(0);
            ran = so_far;
        }
        if (ran < ENOUGH) return(-1);
    }
    for(int s=0; s < TRIED; s++) if (!same_from(alen, blen, &STATE[s])) return(0);
    for(int idx=0; idx < founds; idx++) if (!same_from(alen, blen, &FOUND[idx])) return(0);
    return(same_from_few(alen, blen));
}

// ---- The step on terms
//
// A term is the name of what is in a cell: what was in a cell of a stack
// when the sequence started, a number, what an operation made of two
// terms, what was fetched from memory at a term; and memory is a term too,
// as it was or with a term stored in it at a term.  The same term is the
// same number whatever the state, so two sequences that leave the same
// terms leave the same from every state.  A term is made once and is found
// again by what it is made of, so that to be the same term is to be at the
// same place here.
//
// What is taken to be so of a state: every cell of a stack and of memory
// holds a cell's width and no more, so that to make a cell of what was in
// one changes nothing.  The machine puts nothing else there: not the
// step, and not a call, whose return address is made a cell (vm.c).
enum { IN_DATA, IN_RETURN, A_NUMBER, BY_ALU, FETCHED, MEMORY_WAS, STORED };

#define OPERANDS 8                  // the most terms a term is made of
#define SLOTS    32                 // cells of each stack that have a name,
#define DEEP     (SLOTS / 2)        // and the depth a sequence starts at
// The terms there always are: first the cells of the data stack as they
// were at the start, then those of the return stack, and then memory as it
// was.
#define DATA_TERM(idx)   (idx)
#define RETURN_TERM(idx) (SLOTS + (idx))
#define CELL_TERMS       (2 * SLOTS)        // how many of them are cells',
#define OF_DATA(term)    ((term) < SLOTS)   // whether a cell's is of the data stack,
#define CELL_OF(term)    ((term) % SLOTS)   // and which cell of its stack it is of
#define MEMORY_TERM      CELL_TERMS
#define FIXED            (MEMORY_TERM + 1)
#define ROOM     512
#define FETCHES  16

typedef struct {
    int kind, op, n;
    int of[OPERANDS];               // the terms it is made of
    int64_t number;                 // of a number, and which cell a cell is
    uint64_t is;                    // a number for all of it, the same wherever it is made
} term;

static term KNOWN[ROOM];
static int known;
static int lost;                    // there was no room for a term: nothing is known

static int term_of(int kind, int op, int64_t number, int n, const int* of) {
    uint64_t is = mix(mix(mix(mix(0x7465726d, kind), op), number), n);
    for(int idx=0; idx < n; idx++) is = mix(is, KNOWN[of[idx]].is);
    // the terms there always are are first, and only they are of their kinds
    for(int at=(kind == IN_DATA || kind == IN_RETURN || kind == MEMORY_WAS ? 0 : FIXED); at < known; at++) {
        const term* t = &KNOWN[at];
        if (t->is == is && t->kind == kind && t->op == op && t->number == number && t->n == n &&
            !memcmp(t->of, of, n * sizeof(int))) return(at);
    }
    if (known == ROOM) { lost = 1; return(0); }
    term* t = &KNOWN[known];
    t->kind = kind; t->op = op; t->number = number; t->n = n; t->is = is;
    memcpy(t->of, of, n * sizeof(int));
    return(known++);
}

// The terms there always are, in their places; and back to only those.
static void name_cells(void) {
    for(int idx=0; idx < SLOTS; idx++) term_of(IN_DATA, 0, idx, 0, NULL);
    for(int idx=0; idx < SLOTS; idx++) term_of(IN_RETURN, 0, idx, 0, NULL);
    term_of(MEMORY_WAS, 0, 0, 0, NULL);
}

static void forget(void) {
    known = FIXED;
    lost = 0;
}

static void in_order(int* of, int n) {      // by the number for each, which is the same every time
    for(int i=1; i < n; i++) {
        for(int j=i; j > 0 && KNOWN[of[j]].is < KNOWN[of[j - 1]].is; j--) {
            int t = of[j]; of[j] = of[j - 1]; of[j - 1] = t;
        }
    }
}

static int t_number(int64_t n) {
    return(term_of(A_NUMBER, 0, cell(n), 0, NULL));
}

// A number an instruction has in it.  While any_number is so, it is a
// number of which nothing is known: a cell like one of a stack's, and of
// neither.  What two sequences are then found to leave the same, they
// leave the same whatever the number is (util/joins.c).
static int any_number;

static int t_in_instruction(int64_t n) {
    if (any_number) return(term_of(IN_DATA, 0, SLOTS, 0, NULL));
    return(t_number(n));
}

static int t_whole(int x) {         // a term is a cell already
    return(x);
}

// Is a term a flag, or a flag undone?  The flag, or less than nothing; and
// the operation it was undone by, or none.
static int flag_in(int t, int* undone_by) {
    *undone_by = -1;
    if (KNOWN[t].kind == BY_ALU && ALU_UNDOES(KNOWN[t].op)) {
        *undone_by = KNOWN[t].op;
        t = KNOWN[t].of[0];
    }
    return(KNOWN[t].kind == BY_ALU && ALU_FLAGS(KNOWN[t].op) ? t : -1);
}

// What a term that is a flag, a flag undone or a number is, when the flag
// is this.
static int64_t when(int t, int64_t flag) {
    int undone_by;
    if (KNOWN[t].kind == A_NUMBER) return(KNOWN[t].number);
    flag_in(t, &undone_by);
    return(undone_by < 0 ? cell(flag) : cell(alu(undone_by, cell(flag), cell(flag))));
}

static int t_made(int op, int a, int b);

// What an operation makes of a flag and a number, or of a flag and itself,
// either of them undone or not: a flag is TRUE or FALSE, and the machine's
// arithmetic says what it makes of each.  If that is the same number it is
// that number, and if it is what the flag is, or what the flag undone is,
// it is that.  Less than nothing if the two are not such, or it is none of
// those.
static int of_flag(int op, int a, int b) {
    int undone_a, undone_b;
    int fa = flag_in(a, &undone_a), fb = flag_in(b, &undone_b);
    int flag = fa >= 0 ? fa : fb;
    if (flag < 0) return(-1);
    if (fa < 0 && KNOWN[a].kind != A_NUMBER) return(-1);
    if (fb < 0 && KNOWN[b].kind != A_NUMBER) return(-1);
    if (fa >= 0 && fb >= 0 && fa != fb) return(-1);
    int64_t if_true = cell(alu(op, when(a, TRUE), when(b, TRUE)));
    int64_t if_false = cell(alu(op, when(a, FALSE), when(b, FALSE)));
    if (if_true == if_false) return(t_number(if_true));
    if (if_true == cell(TRUE) && if_false == cell(FALSE)) return(flag);
    for(int undo=0; undo < OPS; undo++) {
        if (ALU_UNDOES(undo) && if_true == cell(alu(undo, cell(TRUE), cell(TRUE))) &&
            if_false == cell(alu(undo, cell(FALSE), cell(FALSE)))) {
            int of[2] = { flag, flag };
            return(term_of(BY_ALU, undo, 0, 2, of));
        }
    }
    return(-1);
}

// What an operation makes of two terms, as a cell.  Of two numbers it is
// the number the machine's arithmetic makes.  What is so of the arithmetic
// (vm.c) gives one term to what is the same cell: the inputs in one order,
// a run of an operation as one term of all its inputs with the numbers
// among them made one, what was done twice undone, what is made of a flag,
// and whether two cells are the same for whether what tells them apart
// made nothing.
static int t_made(int op, int a, int b) {
    const term* x = &KNOWN[a];
    const term* y = &KNOWN[b];
    if (x->kind == A_NUMBER && y->kind == A_NUMBER) return(t_number(alu(op, x->number, y->number)));
    if (ALU_UNDOES(op) && x->kind == BY_ALU && x->op == op) return(t_whole(x->of[0]));
    int flag = of_flag(op, a, b);
    if (flag >= 0) return(flag);
    // Two cells are the same when what tells them apart makes nothing of
    // them.  So it is made of the two, which regroups it with what either
    // already was: and whether they are the same is whether its two inputs
    // are, or whether it is nothing if it has more.
    for(int apart=0; ALU_SAME(op) && apart < OPS; apart++) {
        if (!ALU_DIFFERS(apart) || !ALU_REGROUPS(apart)) continue;
        int told = t_made(apart, a, b);
        const term* t = &KNOWN[told];
        int two[2] = { told, 0 };
        if (t->kind == BY_ALU && t->op == apart && t->n == 2) { two[0] = t->of[0]; two[1] = t->of[1]; }
        else two[1] = t_number(0);
        in_order(two, 2);
        flag = of_flag(op, two[0], two[1]);
        return(flag >= 0 ? flag : term_of(BY_ALU, op, 0, 2, two));
    }
    int of[2 * OPERANDS], n = 0;
    if (ALU_REGROUPS(op)) {
        const int both[2] = { a, b };
        int numbers = 0;
        int64_t number = 0;
        for(int which=0; which < 2; which++) {
            const term* t = &KNOWN[both[which]];
            int run = t->kind == BY_ALU && t->op == op;
            for(int idx=0; idx < (run ? t->n : 1); idx++) {
                int o = run ? t->of[idx] : both[which];
                if (KNOWN[o].kind != A_NUMBER) of[n++] = o;
                else number = cell(numbers++ ? alu(op, number, KNOWN[o].number) : KNOWN[o].number);
            }
        }
        if (numbers && (number != cell(ALU_NOTHING(op)) || !n)) of[n++] = t_number(number);
        if (n == 1) return(of[0]);
        if (n > OPERANDS) { lost = 1; return(0); }
        in_order(of, n);
    } else {
        of[n++] = a;
        of[n++] = b;
        if (ALU_EITHER_WAY(op)) in_order(of, n);
    }
    return(term_of(BY_ALU, op, 0, n, of));
}

// The machine, of terms.  Its stacks are as vm.c's are: T and R are the
// top cells, and the cells of DS and RS below the depths are the rest.
typedef struct {
    int DS[SLOTS], RS[SLOTS], T, R, SP, RSP;
    int M;                          // memory
    int next;                       // where it returned to, or less than nothing: it went on
    int fetched[FETCHES], fetches;  // where it fetched from
    int unsaid;                     // it did what nothing is said of: IO, a jump, a call
} named;

static void start(named* m) {
    for(int idx=0; idx < SLOTS; idx++) { m->DS[idx] = DATA_TERM(idx); m->RS[idx] = RETURN_TERM(idx); }
    m->SP = m->RSP = DEEP;
    m->T = m->DS[m->SP - 1];
    m->R = m->RS[m->RSP - 1];
    m->M = MEMORY_TERM;
    m->next = -1;
    m->fetches = m->unsaid = 0;
}

// A fetch of what the last store put there is what was stored.
static int t_fetch(named* m, int at) {
    at = t_whole(at);
    if (m->fetches == FETCHES) { lost = 1; return(0); }
    m->fetched[m->fetches++] = at;
    const term* mem = &KNOWN[m->M];
    if (mem->kind == STORED && mem->of[1] == at) return(mem->of[2]);
    int of[2] = { m->M, at };
    return(term_of(FETCHED, 0, 0, 2, of));
}

// A store where the last store was takes its place.
static void t_store(named* m, int at, int x) {
    at = t_whole(at);
    const term* mem = &KNOWN[m->M];
    int of[3] = { mem->kind == STORED && mem->of[1] == at ? mem->of[0] : m->M, at, t_whole(x) };
    m->M = term_of(STORED, 0, 0, 3, of);
}

// One instruction: the machine's step (vm_step.h), on terms.
static void step_named(named* m, uint16_t x) {
    instruction ins = *(instruction*)&x;
    if ((!ins.lit.lit_f && ins.alu.op_type != OP_TYPE_ALU) ||
        m->SP < 4 || m->SP > SLOTS - 2 || m->RSP < 4 || m->RSP > SLOTS - 2) {
        m->unsaid = 1;
        return;
    }
    int T = m->T, R = m->R, IN = 0, N = 0, OUT = 0, SP = m->SP, RSP = m->RSP;
#define VALUE           int
#define DS(i)           m->DS[i]
#define RS(i)           m->RS[i]
#define NUMBER(n)       t_in_instruction(n)
#define WHOLE(x)        t_whole(x)
#define ALU(op, a, b)   t_made(op, a, b)
#define FETCH(a)        t_fetch(m, a)
#define STORE(a, x)     t_store(m, a, x)
#define RETURN(r)       m->next = t_whole(r)
#define IO_IN(port)     (m->unsaid = 1, (port))
#define IO_OUT()        m->unsaid = 1; goto resolve_dstack
#define DEPTHS()
#include "../vm_step.h"
#undef VALUE
#undef DS
#undef RS
#undef NUMBER
#undef WHOLE
#undef ALU
#undef FETCH
#undef STORE
#undef RETURN
#undef IO_IN
#undef IO_OUT
#undef DEPTHS
    m->T = T; m->R = R; m->SP = SP; m->RSP = RSP;
}

// Where a sequence fetched from, each place once and in order.  A place it
// also stored to is left out: where the store does not stop the machine,
// the fetch does not.
static int fetched_from(const named* m, int* out) {
    int n = 0;
    for(int idx=0; idx < m->fetches; idx++) {
        int at = m->fetched[idx], has = 0;
        for(int had=0; had < n; had++) has |= out[had] == at;
        for(int mem=m->M; KNOWN[mem].kind == STORED; mem = KNOWN[mem].of[0]) has |= KNOWN[mem].of[1] == at;
        if (!has) out[n++] = at;
    }
    in_order(out, n);
    return(n);
}

// A number for what a sequence left: the depths, the cells of the stacks,
// memory, where it goes next, and where it fetched from, so that two with
// the same number stop from the same states too.  Nothing, if nothing can
// be said of it.
static uint64_t left_named(const named* m) {
    if (m->unsaid || lost) return(0);
    uint64_t h = mix(mix(mix(mix(mix(1, m->SP), m->RSP), KNOWN[m->T].is), KNOWN[m->R].is), KNOWN[m->M].is);
    h = mix(h, m->next < 0 ? 0 : KNOWN[m->next].is);
    for(int idx=0; idx < m->SP - 1; idx++) h = mix(h, KNOWN[m->DS[idx]].is);
    for(int idx=0; idx < m->RSP - 1; idx++) h = mix(h, KNOWN[m->RS[idx]].is);
    int from[FETCHES];
    int n = fetched_from(m, from);
    for(int idx=0; idx < n; idx++) h = mix(h, KNOWN[from[idx]].is);
    return(h | 1);
}

// Did two leave the same terms?  Both were run with no forgetting between.
static int same_named(const named* a, const named* b) {
    if (a->unsaid || b->unsaid || lost) return(0);
    if (a->SP != b->SP || a->RSP != b->RSP || a->T != b->T || a->R != b->R || a->M != b->M || a->next != b->next) return(0);
    if (memcmp(a->DS, b->DS, (a->SP - 1) * sizeof(int)) || memcmp(a->RS, b->RS, (a->RSP - 1) * sizeof(int))) return(0);
    int x[FETCHES], y[FETCHES];
    int n = fetched_from(a, x);
    return(n == fetched_from(b, y) && !memcmp(x, y, n * sizeof(int)));
}

// Run a sequence on terms from the start; the number for what it left.
static uint64_t names(const uint16_t* seq, int len, named* m) {
    start(m);
    for(int idx=0; idx < len; idx++) {
        step_named(m, seq[idx]);
        if (m->next >= 0 && idx < len - 1) m->unsaid = 1;      // it returned, and the rest did not run
    }
    return(left_named(m));
}

// What the terms say is so from every state, if the step and what is said
// of the arithmetic are right.  So it is tried on the machine as well, from
// the states there are here, which is a thing made otherwise: if the
// machine, run, leaves something else, one of the two is wrong, and
// nothing is built on either.
// Is an instruction the fetch of a variable, or the store to one?  That is
// asked of the step too: it is a literal, as vm.c's loop takes one to be,
// and run alone it fetched from a cell whose address is a number, or
// stored to one.  The terms there were are as they were after.
enum { NO_VARIABLE, FETCHES_ONE, STORES_ONE };
static int of_variable(uint16_t x) {
    instruction ins = *(instruction*)&x;
    if (!ins.lit.lit_f) return(NO_VARIABLE);
    int had = known, was_lost = lost, what = NO_VARIABLE;
    named m;
    start(&m);
    step_named(&m, x);
    if (!m.unsaid && !lost) {
        if (m.fetches == 1 && KNOWN[m.fetched[0]].kind == A_NUMBER) what = FETCHES_ONE;
        if (KNOWN[m.M].kind == STORED && KNOWN[KNOWN[m.M].of[1]].kind == A_NUMBER) what = STORES_ONE;
    }
    known = had;
    lost = was_lost;
    return(what);
}

// The machine that is run is given a cell for an address, if the address
// is a number and the machine has no cell there.
static void give_cell(int address) {
    if (KNOWN[address].kind != A_NUMBER) return;
    uint64_t at = ucell(KNOWN[address].number);
    if (!(at >> ADDR_BITS) || far_cell(at) || far.n == FARS) return;
    far.at[far.n++] = at;
}

static void tried(const uint16_t* a, int alen, const uint16_t* b, int blen) {
    // The cells the two name as numbers: what each fetches from and stores
    // to, which the step on terms says.  The terms there were are as they
    // were after.
    const uint16_t* from[2] = { a, b };
    const int len[2] = { alen, blen };
    int had = known, was_lost = lost;
    far.n = 0;
    for(int s=0; s < 2; s++) {
        named m;
        start(&m);
        for(int idx=0; idx < len[s]; idx++) step_named(&m, from[s][idx]);
        for(int idx=0; idx < m.fetches; idx++) give_cell(m.fetched[idx]);
        for(int mem=m.M; KNOWN[mem].kind == STORED; mem = KNOWN[mem].of[0]) give_cell(KNOWN[mem].of[1]);
    }
    known = had;
    lost = was_lost;
    if (same(a, alen, b, blen)) return;
    fprintf(stderr, "\nfuse.c: by their terms");
    for(int idx=0; idx < alen; idx++) fprintf(stderr, " %04x", a[idx]);
    fprintf(stderr, " and");
    for(int idx=0; idx < blen; idx++) fprintf(stderr, " %04x", b[idx]);
    fprintf(stderr, " do the same, and run on vm.c they do not\n");
    exit(1);
}

// The instructions asked about: those of the ALU that do no IO.  Each has
// the number for what it leaves, and the one that stands for all that
// leave the same, which is a baseword if any of them is.
typedef struct {
    uint16_t ins;
    uint64_t names;
    uint16_t stands;
    int8_t d, r;                    // how far it moves each stack,
    int8_t stores, fetches;         // and whether it stores, and fetches
} single;

static single ALU[EVERY_WORD];
static int alus;
static int alu_at[EVERY_WORD];      // where an instruction is in ALU, or -1
static int by_names[EVERY_WORD];

// Which they are is asked of the step, and so is how each moves the
// stacks: every instruction there can be is run, and those are kept that
// are of the ALU, as vm.c's loop takes one to be, and of which the step
// can say what they leave.  Nothing here says what the fields are.
static void list_alu(void) {
    memset(alu_at, -1, sizeof(alu_at));
    for(int word=0; word < EVERY_WORD; word++) {
        uint16_t x = word;
        instruction ins = *(instruction*)&x;
        if (ins.lit.lit_f || ins.alu.op_type != OP_TYPE_ALU) continue;
        named m;
        forget();
        uint64_t is = names(&x, 1, &m);
        if (!is) continue;
        alu_at[x] = alus;
        ALU[alus].ins = x;
        ALU[alus].names = is;
        ALU[alus].d = m.SP - DEEP;
        ALU[alus].r = m.RSP - DEEP;
        ALU[alus].stores = m.M != MEMORY_TERM;
        ALU[alus].fetches = m.fetches > 0;
        alus++;
    }
}

static int names_order(const void* a, const void* b) {
    uint64_t x = ALU[*(const int*)a].names, y = ALU[*(const int*)b].names;
    if (x != y) return(x < y ? -1 : 1);
    return(*(const int*)a - *(const int*)b);
}

static int is_word(uint16_t ins) {
    for(word_node* w = FORTH_WORDS; w->repr && strlen(w->repr); w++) {
        if (w->num_ins == 1 && *(uint16_t*)&w->ins[0] == ins) return(1);
    }
    return(0);
}

// In order of what they leave, to be found by halving; and for those that
// leave the same, the one that stands for them.
static void sort_alu(void) {
    for(int idx=0; idx < alus; idx++) by_names[idx] = idx;
    qsort(by_names, alus, sizeof(int), names_order);
    for(int from=0; from < alus; ) {
        int to = from, stands = by_names[from];
        while (to < alus && ALU[by_names[to]].names == ALU[by_names[from]].names) to++;
        for(int idx=to - 1; idx >= from; idx--) {
            if (is_word(ALU[by_names[idx]].ins)) stands = by_names[idx];
        }
        for(int idx=from; idx < to; idx++) ALU[by_names[idx]].stands = ALU[stands].ins;
        from = to;
    }
}

// An instruction that leaves what has this number, or less than nothing.
static int find(uint64_t names) {
    int lo = 0, hi = alus;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (ALU[by_names[mid]].names < names) lo = mid + 1; else hi = mid;
    }
    return(lo < alus && ALU[by_names[lo]].names == names ? by_names[lo] : -1);
}

// Finding an answer is slow, and it is the same answer until the machine,
// or this, is written otherwise.  So what each sequence was found to be is
// kept in a file beside the images, which has in its name a number for the
// text of this file and of the machine's: change any of them and it is
// another file.  A line is the sequence, with a bit to say whether one
// instruction was asked for or two, and what was found, or nothing.
static const char* TEXTS[] = {
    "fuse.c", "named.c", "room.c", "stack.c", "../vm.c", "../vm_step.h", "../vm.h", "../vm_opcodes.c",
    "../vm_opcodes.h", "../vm_instruction.h", "../vm_constants.h",
};
static uint64_t written;            // the number for the texts,
static int texts_read = 1;          // and whether every one could be read

// A file by where it is from this one.
static FILE* beside(const char* name, const char* mode) {
    const char* slash = strrchr(__FILE__, '/');
    size_t here = slash ? (size_t)(slash - __FILE__ + 1) : 0;
    char* path = room_for(here + strlen(name) + 1, 1);
    memcpy(path, __FILE__, here);
    strcpy(path + here, name);
    FILE* is = fopen(path, mode);
    free(path);
    return(is);
}

static void read_texts(void) {
    for(int idx=0; idx < sizeof(TEXTS) / sizeof(TEXTS[0]); idx++) {
        FILE* in = beside(TEXTS[idx], "r");
        if (in == NULL) { texts_read = 0; continue; }
        for(int c; (c = fgetc(in)) != EOF; ) written = mix(written, c);
        fclose(in);
    }
}

#define FOR_ONE (1ull << 48)        // in what was asked: one instruction was asked for
static FILE* kept_file;

// What was asked, and what was found for it: as many as have been asked,
// with room for twice that, so that one is come to in a step or two.
typedef struct { uint64_t asked; long found; } answer_kept;
static answer_kept* KEPT_ANSWER;
static int kept_bits;               // there is room for two to this many
static long kept_count;

static answer_kept* kept_at(uint64_t was_asked) {
    long at = mix(0, was_asked) & ((1L << kept_bits) - 1);
    while (KEPT_ANSWER[at].asked && KEPT_ANSWER[at].asked != was_asked) at = (at + 1) & ((1L << kept_bits) - 1);
    return(&KEPT_ANSWER[at]);
}

// Where what was found for a question is, or will be: less than nothing
// until it is known.  It is where it is only until the next is asked for.
static long* kept(uint64_t was_asked) {
    if (!KEPT_ANSWER || 2 * (kept_count + 1) > 1L << kept_bits) {
        answer_kept* had = KEPT_ANSWER;
        long was = had ? 1L << kept_bits : 0;
        kept_bits = had ? kept_bits + 1 : 1;
        KEPT_ANSWER = room_for((size_t)1 << kept_bits, sizeof(answer_kept));
        for(long idx=0; idx < was; idx++) if (had[idx].asked) *kept_at(had[idx].asked) = had[idx];
        free(had);
    }
    answer_kept* is = kept_at(was_asked);
    if (!is->asked) {
        is->asked = was_asked;
        is->found = -1;
        kept_count++;
    }
    return(&is->found);
}

// What was found for a run with numbers in it is kept too (fuse_lit): the
// run by a number for it, how long what does the same is, or less than
// nothing, and what that is, which is as long as it is.  Its line begins
// with an L.
typedef struct { uint64_t asked; int len; uint16_t* run; } lit_answer;
static lit_answer* LIT;
static int lit_bits;
static long lit_count;

static lit_answer* lit_at(uint64_t asked) {
    long at = mix(1, asked) & ((1L << lit_bits) - 1);
    while (LIT[at].asked && LIT[at].asked != asked) at = (at + 1) & ((1L << lit_bits) - 1);
    return(&LIT[at]);
}

// What is kept for a run, or where it will be: with nothing asked, until
// it is.  It is where it is only until the next is asked for.
static lit_answer* lit_kept(uint64_t asked) {
    if (!LIT || 2 * (lit_count + 1) > 1L << lit_bits) {
        lit_answer* had = LIT;
        long was = had ? 1L << lit_bits : 0;
        lit_bits = had ? lit_bits + 1 : 1;
        LIT = room_for((size_t)1 << lit_bits, sizeof(lit_answer));
        for(long idx=0; idx < was; idx++) if (had[idx].asked) *lit_at(had[idx].asked) = had[idx];
        free(had);
    }
    return(lit_at(asked));
}

// A run is kept for what was asked: how long it is, or less than nothing
// if none was found, and the run.
static void lit_keep(uint64_t asked, int len, const uint16_t* run) {
    lit_answer* answer = lit_kept(asked);
    if (!answer->asked) lit_count++;
    free(answer->run);
    answer->asked = asked;
    answer->len = len;
    answer->run = len > 0 ? room_for(len, sizeof(uint16_t)) : NULL;
    if (len > 0) memcpy(answer->run, run, len * sizeof(uint16_t));
}

static void read_kept(void) {
    if (!texts_read) return;        // nothing is kept that cannot be said to be of these texts
    static const char form[] = "../build/fuse-%d-%016llx.kept";
    char name[sizeof(form) + 3 * sizeof(int)];          // the cell's size is no more digits than three a byte
    snprintf(name, sizeof(name), form, CELL_BITS, (unsigned long long)written);
    FILE* in = beside(name, "r");
    char* line = NULL;
    long room = 0;
    while (in && line_of(in, &line, &room)) {
        unsigned long long was_asked;
        long found;
        int len, at;
        if (sscanf(line, "L %llx %d%n", &was_asked, &len, &at) == 2) {
            uint16_t* run = room_for(len > 0 ? len : 0, sizeof(uint16_t));
            char* rest = line + at;
            for(int idx=0; idx < len; idx++) run[idx] = strtol(rest, &rest, 16);
            lit_keep(was_asked, len, run);
            free(run);
        } else if (sscanf(line, "%llx %lx", &was_asked, &found) == 2) {
            *kept(was_asked) = found;
        }
    }
    free(line);
    if (in) fclose(in);
    kept_file = beside(name, "a");
}

static long keep(uint64_t was_asked, long found) {
    *kept(was_asked) = found;
    if (kept_file) {
        fprintf(kept_file, "%llx %lx\n", (unsigned long long)was_asked, found);
        fflush(kept_file);
    }
    return(found);
}

static void learn_literals(void);
static void list_tried(void);

static int ready = 0;
static void get_ready(void) {
    init_opcodes(FORTH_WORDS);
    nowhere = fopen("/dev/null", "w");
    make_states();
    list_give();
    lay_leads();
    name_cells();
    list_alu();
    sort_alu();
    learn_literals();
    list_tried();
    read_texts();
    read_kept();
    ready = 1;
}

// Is a sequence one that is asked about?
static int asked(const uint16_t* seq, int len) {
    for(int idx=0; idx < len; idx++) {
        if (alu_at[seq[idx]] < 0) return(0);
        if (idx < len - 1 && returns(seq[idx])) return(0);
    }
    return(1);
}

// The instruction that does the work of a sequence of two or three (the
// first is nothing for two), or nothing if none is known to.
long fuse_one(long first, long second, long third) {
    if (!ready) get_ready();
    uint16_t seq[SEQ];
    int len = 0;
    if (first) seq[len++] = first;
    seq[len++] = second;
    seq[len++] = third;
    if (!asked(seq, len)) return(0);
    // What leaves the same terms is cheap to find, and is found each time;
    // that it was tried on the machine is what is kept.
    named a, b;
    forget();
    int found = find(names(seq, len, &a));
    if (found < 0) return(0);
    uint16_t one = ALU[found].stands;
    names(&one, 1, &b);
    if (!same_named(&a, &b)) return(0);
    uint64_t was_asked = FOR_ONE | (uint64_t)first << 32 | (uint64_t)second << 16 | third;
    if (*kept(was_asked) == one) return(one);
    tried(seq, len, &one, 1);
    return(keep(was_asked, one));
}

// The pairs that may do the work of three: every two of the instructions
// that do not return and stand for the rest, with the number for what the
// pair leaves, and in order of that.  They are made the first time they
// are asked for.
typedef struct {
    uint64_t names;
    uint16_t seq[2];
} pair;

static pair* PAIRS;
static long pairs;

static int pair_order(const void* a, const void* b) {
    const pair* x = a;
    const pair* y = b;
    if (x->names != y->names) return(x->names < y->names ? -1 : 1);
    if (x->seq[0] != y->seq[0]) return((int)x->seq[0] - (int)y->seq[0]);
    return((int)x->seq[1] - (int)y->seq[1]);
}

static int stands_alone(int at) {
    return(ALU[at].stands == ALU[at].ins && !returns(ALU[at].ins));
}

static void list_pairs(void) {
    long each = 0;
    for(int a=0; a < alus; a++) each += stands_alone(a);
    PAIRS = malloc(each * each * sizeof(pair));
    for(int a=0; a < alus; a++) {
        if (!stands_alone(a)) continue;
        named first;
        forget();
        start(&first);
        step_named(&first, ALU[a].ins);
        int after_first = known;
        for(int b=0; b < alus; b++) {
            if (!stands_alone(b)) continue;
            named both = first;
            known = after_first;
            step_named(&both, ALU[b].ins);
            pair* p = &PAIRS[pairs];
            p->seq[0] = ALU[a].ins;
            p->seq[1] = ALU[b].ins;
            p->names = left_named(&both);
            if (p->names) pairs++;
            lost = 0;
        }
    }
    qsort(PAIRS, pairs, sizeof(pair), pair_order);
}

static long look_for_two(const uint16_t* seq);

// The two instructions that do the work of three, the first in the upper
// sixteen bits, or nothing if none are known to.
long fuse_two(long first, long second, long third) {
    if (!ready) get_ready();
    uint16_t seq[SEQ] = { first, second, third };
    if (!asked(seq, SEQ)) return(0);
    uint64_t was_asked = (uint64_t)seq[0] << 32 | (uint64_t)seq[1] << 16 | seq[2];
    long two = *kept(was_asked);
    if (two >= 0) return(two);
    return(keep(was_asked, look_for_two(seq)));
}

// If the third returns, so does the second of the two: the pairs are
// looked for that may do the work of the three without the return, and
// each is then tried with it.
static long look_for_two(const uint16_t* seq) {
    if (!PAIRS) list_pairs();
    int ret = returns(seq[SEQ - 1]);
    uint16_t plain[SEQ] = { seq[0], seq[1], returning(seq[2], 0) };
    named three, pair_of;
    forget();
    uint64_t is = names(plain, SEQ, &three);
    if (!is) return(0);
    long lo = 0, hi = pairs;
    while (lo < hi) {
        long mid = (lo + hi) / 2;
        if (PAIRS[mid].names < is) lo = mid + 1; else hi = mid;
    }

    // Of the pairs that leave the same terms, the one that moves the stacks
    // least.
    uint16_t best[2] = { 0, 0 };
    int least = 0;
    for(; lo < pairs && PAIRS[lo].names == is; lo++) {
        const pair* p = &PAIRS[lo];
        int moved = 0;
        for(int idx=0; idx < 2; idx++) {
            const single* one = &ALU[alu_at[p->seq[idx]]];
            moved += abs(one->d) + 4 * abs(one->r);
        }
        if (best[0] && moved >= least) continue;
        uint16_t two[2] = { p->seq[0], returning(p->seq[1], ret) };
        forget();
        names(seq, SEQ, &three);
        names(two, 2, &pair_of);
        if (!same_named(&three, &pair_of)) continue;
        best[0] = two[0];
        best[1] = two[1];
        least = moved;
    }
    if (!best[0]) return(0);
    tried(seq, SEQ, best, 2);
    return((long)best[0] << 16 | best[1]);
}

// Do two runs of instructions do the same?  They may be of any length up
// to RUN, and have literals in them and the fetch and the store of a
// variable.  Yes is said only of two that leave the same terms, and they
// are then run on the machine as every answer is.
long fuse_same(const uint16_t* a, long alen, const uint16_t* b, long blen) {
    if (!ready) get_ready();
    if (alen > RUN || blen > RUN) return(0);
    named first, second;
    forget();
    names(a, alen, &first);
    names(b, blen, &second);
    if (!same_named(&first, &second)) return(0);
    tried(a, alen, b, blen);
    return(1);
}

// ---- A run with numbers in it
//
// cross.fs asks about the last few things it compiled and the one it is
// compiling: a run of instructions of the ALU, of numbers pushed and
// added, and of variables fetched.  What is looked for is a shorter run
// that leaves the same terms: nothing; one instruction or two; and those
// with one number pushed or added before, between or after them; each
// with the variables the run fetched, fetched once.  The number is one
// that is in what the run leaves.  Where there are two instructions and
// more than those, both are basewords: there are too many of the rest to
// try each with each.
//
// What is found is only a guess until its terms are the same as the
// run's: so how it is looked for can be wrong, or fall short, and what is
// given cannot be other than the same.

// How a number is written is learnt of the step.  Each literal that has
// one in its bits is run alone: one that leaves the stack a cell deeper
// with a number on top pushes, and what it pushed is what one in its bits
// is worth; one that leaves the top with a number added to it adds.
typedef struct { uint16_t ins; int64_t worth; } literal;
static literal* PUSHES;             // as many as the machine has of each: there is room as they are found
static literal* ADDS;
static long pushes_room, adds_room;
static int pushes, adds;
static int64_t most;                // the most the bits of a literal say
static uint16_t INVERTS, IS_SAME;   // the basewords `invert` and `=`

static void more_worth(literal* of, int n) {     // the one worth most first
    for(int i=1; i < n; i++) {
        for(int j=i; j > 0 && of[j].worth > of[j - 1].worth; j--) {
            literal t = of[j]; of[j] = of[j - 1]; of[j - 1] = t;
        }
    }
}

static void learn_literals(void) {
    instruction ins = {0};
    ins.lit.lit_v = ~0;
    most = ins.lit.lit_v;
    for(int word=0; word < EVERY_WORD; word++) {
        uint16_t x = word;
        ins = *(instruction*)&x;
        if (!ins.lit.lit_f || ins.lit.lit_v != 1) continue;
        named m;
        forget();
        start(&m);
        int top = m.T;
        step_named(&m, x);
        if (m.unsaid || lost || m.fetches || m.M != MEMORY_TERM) continue;
        const term* t = &KNOWN[m.T];
        if (m.SP == DEEP + 1 && t->kind == A_NUMBER && t->number > 0 && m.DS[DEEP - 1] == top) {
            ONE_MORE(PUSHES, pushes, pushes_room);
            PUSHES[pushes++] = (literal){ x, t->number };
        }
        if (m.SP == DEEP && t->kind == BY_ALU && t->op == ALU_ADD && t->n == 2) {
            for(int which=0; which < 2; which++) {
                const term* number = &KNOWN[t->of[which]];
                if (t->of[1 - which] == top && number->kind == A_NUMBER && number->number > 0) {
                    ONE_MORE(ADDS, adds, adds_room);
                    ADDS[adds++] = (literal){ x, number->number };
                }
            }
        }
    }
    more_worth(PUSHES, pushes);
    more_worth(ADDS, adds);
    INVERTS = ins_named("invert");
    IS_SAME = ins_named("=");
}

static uint16_t worth(const literal* of, int64_t times) {
    instruction ins = *(instruction*)&of->ins;
    ins.lit.lit_v = times;
    return(*(uint16_t*)&ins);
}

// A number as instructions: pushed, or added to the top.  A push is the
// literal worth most that the number has any of, and adds for the rest; a
// number below nothing is pushed as the number it inverts to, and an
// invert.  How many instructions, or less than nothing if these cannot
// write it.
#define NUMBER_ROOM 8
static int write_number(int64_t v, int add, uint16_t* out) {
    int n = 0, below = v < 0;
    if (below && add) return(-1);
    if (below) v = ~v;
    for(int idx=0; !add && idx < pushes && !n; idx++) {
        if (v < PUSHES[idx].worth && idx < pushes - 1) continue;
        int64_t times = v / PUSHES[idx].worth < most ? v / PUSHES[idx].worth : most;
        out[n++] = worth(&PUSHES[idx], times);
        v -= times * PUSHES[idx].worth;
    }
    if (!add && !n) return(-1);
    for(int idx=0; idx < adds && v && n < NUMBER_ROOM - 1; idx++) {
        int64_t times = v / ADDS[idx].worth < most ? v / ADDS[idx].worth : most;
        if (!times) continue;
        out[n++] = worth(&ADDS[idx], times);
        v -= times * ADDS[idx].worth;
    }
    if (v) return(-1);
    if (below) out[n++] = INVERTS;
    return(n);
}

// The instructions that are basewords and do not return, and those that
// stand for the rest and do not return.
static int BASE[COUNT(FORTH_WORDS)], bases;    // no more of them than there are words
static int REPS[EVERY_WORD], reps;

static void list_tried(void) {
    for(int at=0; at < alus; at++) {
        if (returns(ALU[at].ins)) continue;
        if (is_word(ALU[at].ins)) BASE[bases++] = at;
        if (ALU[at].stands == ALU[at].ins) REPS[reps++] = at;
    }
}

// The numbers in a term, and in all that a run left.
#define NUMBERS_ROOM 16
static int numbers_in(int t, int64_t* out, int n) {
    const term* x = &KNOWN[t];
    if (x->kind == A_NUMBER) {
        for(int idx=0; idx < n; idx++) if (out[idx] == x->number) return(n);
        if (n < NUMBERS_ROOM) out[n++] = x->number;
        return(n);
    }
    for(int idx=0; idx < x->n; idx++) n = numbers_in(x->of[idx], out, n);
    return(n);
}

static int numbers_left(const named* m, int64_t* out) {
    int n = numbers_in(m->T, out, 0);
    n = numbers_in(m->R, out, n);
    n = numbers_in(m->M, out, n);
    if (m->next >= 0) n = numbers_in(m->next, out, n);
    for(int idx=0; idx < m->SP - 1; idx++) n = numbers_in(m->DS[idx], out, n);
    for(int idx=0; idx < m->RSP - 1; idx++) n = numbers_in(m->RS[idx], out, n);
    for(int idx=0; idx < m->fetches; idx++) n = numbers_in(m->fetched[idx], out, n);
    return(n);
}

// What is being looked for, and the shortest found so far.
static struct {
    named want;                     // what the run leaves
    int after_want;                 // the terms that is made of: the rest are tried and forgotten
    uint16_t tail[4];               // what follows the run and anything tried: `0 =` before a branch
    int tail_len;
    const uint16_t* run;            // the run itself,
    int len;
    uint16_t best[RUN];             // the best found for it,
    int best_len;
    int as_it_stood;                // and how much of the run that begins with
} looked;

static void follow(named* m, const uint16_t* run, int len) {
    for(int idx=0; idx < len; idx++) step_named(m, run[idx]);
}

static int rise(const uint16_t* run, int len) {     // how much deeper a run leaves the data stack
    int had = known, was_lost = lost;
    named m;
    start(&m);
    follow(&m, run, len);
    known = had;
    lost = was_lost;
    return(m.SP - DEEP);
}

// A run that was tried has left `m`: is it what is wanted?  Of those that
// are, the shortest is kept; and of those as short, the one that has most
// of the run it is for as it stood, from the first instruction on.  That
// one changes least: what is compiled next finds what it would have found.
static void offer(named* m, const uint16_t* tried_run, int n) {
    follow(m, looked.tail, looked.tail_len);
    if (!same_named(m, &looked.want)) return;
    int as_it_stood = 0;
    while (as_it_stood < n && as_it_stood < looked.len && tried_run[as_it_stood] == looked.run[as_it_stood]) as_it_stood++;
    if (n == looked.best_len && as_it_stood <= looked.as_it_stood) return;
    memcpy(looked.best, tried_run, n * 2);
    looked.best_len = n;
    looked.as_it_stood = as_it_stood;
}

// Try what is not of the ALU where it has been put: before the first
// instruction, between the two, and after; with every instruction that
// moves the stacks as it must.
static void place(const uint16_t* pre, int plen, const uint16_t* mid, int mlen, const uint16_t* suf, int slen, int alus_in) {
    // what is as long as the run is not wanted, nor what is longer than the best there is
    if (plen + mlen + slen + alus_in >= looked.len || plen + mlen + slen + alus_in > looked.best_len) return;
    // If the run returns, so does what is given for it, and by its last
    // instruction: so that is of the ALU, with the return in it.
    int ends = looked.want.next >= 0;
    if (ends && (!alus_in || slen)) return;
    named at;
    uint16_t all[RUN];
    known = looked.after_want;
    lost = 0;
    start(&at);
    follow(&at, pre, plen);
    if (at.unsaid || lost) return;
    int after_pre = known;
    memcpy(all, pre, plen * 2);
    if (!alus_in) {
        offer(&at, all, plen);
        return;
    }
    // An instruction is not tried that cannot be one: that leaves the
    // stacks where no second one, or nothing, brings them to where they
    // are wanted; that stores, when memory is wanted as it was; or that
    // fetches, when nothing is to have been fetched.
    int rise_after = rise(suf, slen), rise_between = rise(mid, mlen);
    int no_store = looked.want.M == MEMORY_TERM, no_fetch = no_store && !looked.want.fetches;
    for(int idx=0; idx < (alus_in == 1 ? reps : bases); idx++) {
        int x = alus_in == 1 ? REPS[idx] : BASE[idx];
        int d = looked.want.SP - at.SP - rise_after - rise_between - ALU[x].d;
        int r = looked.want.RSP - at.RSP - ALU[x].r;
        if ((no_store && ALU[x].stores) || (no_fetch && ALU[x].fetches)) continue;
        if (alus_in == 1 ? d || r : d < -2 || d > 1 || r < -2 || r > 1) continue;
        named first = at;
        known = after_pre;
        lost = 0;
        all[plen] = returning(ALU[x].ins, ends && alus_in == 1);
        step_named(&first, all[plen]);
        if (alus_in == 1) {
            follow(&first, suf, slen);
            memcpy(all + plen + 1, suf, slen * 2);
            offer(&first, all, plen + 1 + slen);
            continue;
        }
        follow(&first, mid, mlen);
        if (first.unsaid || lost) continue;
        int after_mid = known;
        memcpy(all + plen + 1, mid, mlen * 2);
        for(int idy=0; idy < bases; idy++) {
            int y = BASE[idy];
            if (ALU[y].d != d || ALU[y].r != r) continue;
            if ((no_store && ALU[y].stores) || (no_fetch && ALU[y].fetches)) continue;
            named second = first;
            known = after_mid;
            lost = 0;
            all[plen + 1 + mlen] = returning(ALU[y].ins, ends);
            step_named(&second, all[plen + 1 + mlen]);
            follow(&second, suf, slen);
            memcpy(all + plen + 2 + mlen, suf, slen * 2);
            offer(&second, all, plen + mlen + slen + 2);
        }
    }
}

// Every order of what there is to place: the variables, the number if
// there is one, and one or two instructions, which are x and then y.
enum { AN_X = -1, A_Y = -2 };
static void orders(const uint16_t* things[], const int* lens, int count, int alus_in) {
    int item[5], items = 0;
    for(int idx=0; idx < count; idx++) item[items++] = idx;
    if (alus_in >= 1) item[items++] = AN_X;
    if (alus_in == 2) item[items++] = A_Y;
    int order[5];
    // every arrangement, by counting through them
    int total = 1;
    for(int idx=2; idx <= items; idx++) total *= idx;
    for(int which=0; which < total; which++) {
        int left[5], n = items, w = which;
        memcpy(left, item, sizeof(left));
        for(int idx=0; idx < items; idx++) {
            int take = w % n;
            w /= n;
            order[idx] = left[take];
            memmove(left + take, left + take + 1, (n - take - 1) * sizeof(int));
            n--;
        }
        uint16_t part[3][RUN];
        int part_len[3] = { 0, 0, 0 }, stage = 0, y_first = 0;
        for(int idx=0; idx < items; idx++) {
            if (order[idx] == AN_X) { stage = alus_in == 2 ? 1 : 2; continue; }
            if (order[idx] == A_Y) { if (stage == 0) y_first = 1; stage = 2; continue; }
            memcpy(part[stage] + part_len[stage], things[order[idx]], lens[order[idx]] * 2);
            part_len[stage] += lens[order[idx]];
        }
        if (y_first) continue;
        place(part[0], part_len[0], part[1], part_len[1], part[2], part_len[2], alus_in);
    }
}

// The shorter run that does the same as a run with numbers in it: how many
// instructions it is, which are put in `out`, or less than nothing if none
// is known.  Before a conditional branch, the same is to leave the same
// but for the top, and the top nothing when the run leaves it nothing.
static void look_for_run(const uint16_t* run, int len);

long fuse_lit(const uint16_t* run, long len, long branch, uint16_t* out) {
    if (!ready) get_ready();
    if (len < 1 || len > RUN - 4) return(-1);
    uint16_t whole[RUN], tried_run[RUN];
    memcpy(whole, run, len * 2);
    looked.tail_len = 0;
    if (branch) {
        looked.tail_len = write_number(0, 0, looked.tail);
        looked.tail[looked.tail_len++] = IS_SAME;
    }
    memcpy(whole + len, looked.tail, looked.tail_len * 2);
    forget();
    if (!names(whole, len + looked.tail_len, &looked.want)) return(-1);
    looked.after_want = known;

    uint64_t asked = branch + 2;
    for(int idx=0; idx < len; idx++) asked = mix(asked, run[idx]);
    asked |= 1;
    const lit_answer* answer = lit_kept(asked);
    if (answer->asked) {
        if (answer->len < 0) return(-1);
        named again;                // what was kept is what does the same, or it is not used
        memcpy(tried_run, answer->run, answer->len * 2);
        memcpy(tried_run + answer->len, looked.tail, looked.tail_len * 2);
        names(tried_run, answer->len + looked.tail_len, &again);
        if (same_named(&again, &looked.want)) {
            memcpy(out, answer->run, answer->len * 2);
            return(answer->len);
        }
    }

    // A shorter run is what is kept, however long it is: it is shorter
    // than the run, which is as long as cross.fs gave room for.
    look_for_run(run, len);
    int found = -1;
    if (looked.best_len < len) {
        memcpy(tried_run, looked.best, looked.best_len * 2);
        memcpy(tried_run + looked.best_len, looked.tail, looked.tail_len * 2);
        tried(whole, len + looked.tail_len, tried_run, looked.best_len + looked.tail_len);
        found = looked.best_len;
        memcpy(out, looked.best, looked.best_len * 2);
    }
    lit_keep(asked, found, looked.best);
    if (kept_file) {
        fprintf(kept_file, "L %llx %d", (unsigned long long)asked, found);
        for(int idx=0; idx < found; idx++) fprintf(kept_file, " %x", looked.best[idx]);
        fprintf(kept_file, "\n");
        fflush(kept_file);
    }
    return(found);
}

// Look for a shorter run that leaves what is wanted (looked.want, which
// has been made): looked.best, and how long it is, which is as long as the
// run if there is none.
static void look_for_run(const uint16_t* run, int len) {
    // the instructions it has that fetch a variable or store to one
    const uint16_t* variable[2];
    int variables = 0;
    for(int idx=0; idx < len; idx++) {
        if (of_variable(run[idx]) == NO_VARIABLE) continue;
        if (variables == 2) { variables = -1; break; }
        variable[variables++] = &run[idx];
    }
    // The numbers tried: those in what the run leaves; those it had on top
    // on the way, since what it leaves may be made of one twice; what each
    // of those is undone, since an instruction may undo it; and nothing,
    // since a number may be pushed only for the cell it makes room for.
    int64_t number[NUMBERS_ROOM];
    int numbers = numbers_left(&looked.want, number);
    numbers = numbers_in(t_number(0), number, numbers);
    named on_the_way;
    start(&on_the_way);
    for(int idx=0; idx < len; idx++) {
        step_named(&on_the_way, run[idx]);
        numbers = numbers_in(on_the_way.T, number, numbers);
    }
    for(int idx=0, had=numbers; idx < had; idx++) {
        for(int undo=0; undo < OPS; undo++) {
            if (!ALU_UNDOES(undo)) continue;
            int undone = t_number(alu(undo, number[idx], number[idx]));
            numbers = numbers_in(undone, number, numbers);
        }
    }
    looked.after_want = known;
    looked.run = run;
    looked.len = len;
    looked.best_len = len;
    looked.as_it_stood = -1;
    // What is tried has so many things in it, fewest first: any of the
    // variables' instructions, each as it is and once (one that is not
    // there may be a number pushed and an instruction that fetches from it
    // or stores to it); a number or none; and instructions.
    for(int count=0; variables >= 0 && count <= 3 && looked.best_len == len; count++) {
        for(int kept=0; kept < 1 << variables; kept++) {
            const uint16_t* things[4];
            int lens[4], had = 0;
            for(int idx=0; idx < variables; idx++) {
                if (kept >> idx & 1) { things[had] = variable[idx]; lens[had++] = 1; }
            }
            for(int alus_in=0; alus_in <= 2; alus_in++) {
                if (had + alus_in == count) orders(things, lens, had, alus_in);
                if (had + alus_in + 1 != count) continue;
                for(int add=0; add < 2; add++) {
                    for(int idx=0; idx < numbers; idx++) {
                        uint16_t written[NUMBER_ROOM];
                        int n = write_number(number[idx], add, written);
                        if (n < 1) continue;
                        things[had] = written;
                        lens[had] = n;
                        orders(things, lens, had + 1, alus_in);
                    }
                }
            }
        }
    }
}
