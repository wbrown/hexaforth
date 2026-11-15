//
// fuse.c - which instruction does the work of two or of three, and which
// two do the work of three, asked of the machine.
//
// A sequence of instructions is run on vm.c from a number of states, and
// what it leaves is noted: the depths, the stacks, every cell of data, and
// where it goes next.  Two sequences that leave the same from every state
// are taken to do the same.  Nothing here says what an instruction does:
// the machine is run.
//
// A state is cells of numbers of the kinds a program has: addresses of
// data, small numbers, flags, and anything.  Taken by chance alone they
// would not tell what a sequence does with what it has made itself: the
// xor of two cells is like nothing else in a state, so `xor =` would never
// be seen to say yes, and would pass for an instruction that leaves
// nothing.  So an answer found from the states is tried again from states
// made for it: what the sequences make from a state is put, a value and a
// place at a time, where they may next look for it (same_from).  Nor would
// they tell two sequences apart that ask different things of the cells
// they are given, when the answer to both is nearly always no: so the two
// are also run from every way of giving those cells a few numbers
// (same_from_few).  A state may also lead a sequence to fetch the cell it
// is itself in: so two that are compared are both in memory while either
// runs.
//
// This is a test and not a proof: two sequences are said to do the same
// when nothing here could tell them apart.  What it says is checked by
// what is made otherwise, the comparison of an image built with the
// peephole and one built without (CLAUDE.md, Testing), and a wrong answer
// found there is a kind of state that is missing here.
//
// The machine is the strict one (STRICT_ALIGN, ROM_BYTES), which stops at
// a fetch, a store or a return outside memory and at a store into its ROM:
// what those do is not defined (MODEL.md).  A state that stops a sequence
// says nothing of it but that it stopped, and a sequence that few states
// run to the end of is not known well enough to be given an answer.  The
// machine runs the sequence and no more (strict_steps), so one that
// returns is run as any other is, and where it returns to is part of what
// it leaves.  Memory is 256 cells, so that all of it can be looked at
// after every run.  The code is in a ROM in the middle of it, so that the
// cells a flag and a small number are the addresses of are data like any
// other, and what fetches or stores through one runs.
//
// Not asked: what does IO, of which nothing is said; and a sequence that
// returns before its last instruction, which does not run the rest.
//
// It is built for the cell the image is for (CELL_BITS).  cross.fs has it
// through gforth's C interface.
//

#include <setjmp.h>
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
#define SEQ         3               // the most instructions in a sequence
#define FIRST_CELL  165             // where the first of two sequences is; each has
                                    // four cells
#define AT(which)   ((FIRST_CELL + 4 * (which)) * (CELL_BYTES / 2))     // as an instruction
#define LEAD(which) ((LOW + 8 * (which)) * (CELL_BYTES / 2))    // where what leads to each starts
#define DEPTH       8               // of each stack, when the sequence starts
#define FILLED      16              // cells of each stack that a state gives
#define STATES      96
#define FEW         4               // the states tried first
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
static jmp_buf stopped;
static FILE* nowhere;
#define exit(code) longjmp(stopped, 1)
#pragma push_macro("stderr")
#undef stderr
#define stderr nowhere
#include "../vm.c"
#pragma pop_macro("stderr")
#undef exit
#include "../vm_opcodes.c"
#include "stack.c"

typedef struct {
    int64_t dstack[FILLED];
    int64_t rstack[FILLED];
    int64_t data[DATA_CELLS];
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
    instruction io_store;
    lookup_word(FORTH_WORDS, "io!", &io_store);
    for(int which=0; which < 2; which++) {
        uint16_t* m = machine.memory + LEAD(which);
        *m++ = push(DEPTH);  *m++ = push(IO_RSP);  *m++ = *(uint16_t*)&io_store;
        *m++ = push(DEPTH);  *m++ = push(IO_SP);   *m++ = *(uint16_t*)&io_store;
        *m++ = jump(AT(which));
        lead = m - (machine.memory + LEAD(which));
    }
}

// Put a sequence in memory, as the first or the second.
static int after[2];                // the instruction after each
static void lay(int which, const uint16_t* seq, int len) {
    uint16_t* code = machine.memory + AT(which);
    memset(code, 0, (SEQ + 1) * 2);
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
    int on = machine.EIP == after[0] || machine.EIP == after[1];
    h = mix(h, on ? -1 : machine.EIP);
    return(h | 2);
}

// A number for what a sequence leaves from the states `from` up to `to`,
// carrying on from h, with no other sequence in memory.  The sequences are
// kept in order of this, to find those that may do the same: it is `same`
// that says whether they do.
static uint64_t leaves(const uint16_t* seq, int len, int from, int to, uint64_t h) {
    lay(0, seq, len);
    lay(1, NULL, 0);
    for(int s=from; s < to; s++) h = mix(h, left(0, len, &STATE[s]));
    return(h);
}

// Was a value there before the sequence ran: in a stack, in the data, or
// in the ROM?
static int was_there(int64_t v, const state* in) {
    if (v == DEPTH) return(1);
    for(int idx=0; idx < FILLED; idx++) {
        if (in->dstack[idx] == v || in->rstack[idx] == v) return(1);
    }
    for(int idx=0; idx < DATA_CELLS; idx++) if (in->data[idx] == v) return(1);
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
#define MEMORY  (MEM_BYTES / 2)     // of the machine, in instructions

typedef struct {
    int ran, sp, rsp, next;
    int64_t dstack[FILLED], rstack[FILLED];
    uint16_t memory[MEMORY];
} leavings;

static void run_few(int which, int len, const state* stacks, const uint16_t* memory, leavings* l) {
    memcpy(machine.DSTACK, stacks->dstack, sizeof(stacks->dstack));
    memcpy(machine.RSTACK, stacks->rstack, sizeof(stacks->rstack));
    memcpy(machine.memory, memory, sizeof(l->memory));
    machine.EIP = LEAD(which);
    strict_steps = lead + len;
    l->ran = 0;
    if (setjmp(stopped)) return;
    vm(&machine);
    l->ran = 1;
    l->sp = machine.SP;
    l->rsp = machine.RSP;
    l->next = machine.EIP == after[0] || machine.EIP == after[1] ? -1 : machine.EIP;
    memcpy(l->dstack, machine.DSTACK, sizeof(l->dstack));
    memcpy(l->rstack, machine.RSTACK, sizeof(l->rstack));
    memcpy(l->memory, machine.memory, sizeof(l->memory));
}

static int alike(const leavings* a, const leavings* b) {
    if (!a->ran || !b->ran) return(a->ran == b->ran);
    return(a->sp == b->sp && a->rsp == b->rsp && a->next == b->next &&
           !memcmp(a->dstack, b->dstack, a->sp * sizeof(a->dstack[0])) &&
           !memcmp(a->rstack, b->rstack, a->rsp * sizeof(a->rstack[0])) &&
           !memcmp(a->memory, b->memory, sizeof(a->memory)));
}

static int same_from_few(int alen, int blen) {
    const int64_t number[NUMBERS] = { 0, 1, cell(TRUE) };
    static uint16_t memory[MEMORY];
    static leavings first, second;
    int ways = 1;
    for(int idx=0; idx < GIVEN; idx++) ways *= NUMBERS;
    state with = STATE[0];
    run(0, 0, &with);                   // the first state's data, with the code, as it is in memory
    memcpy(memory, machine.memory, sizeof(memory));
    for(int way=0; way < ways; way++) {
        int w = way;
        for(int idx=0; idx < 5; idx++, w /= NUMBERS) with.dstack[DEPTH - 1 - idx] = number[w % NUMBERS];
        for(int idx=0; idx < 2; idx++, w /= NUMBERS) with.rstack[DEPTH - 1 - idx] = number[w % NUMBERS];
        for(int idx=0; idx < 2; idx++, w /= NUMBERS) {
            memcpy((uint8_t*)memory + idx * CELL_BYTES, &number[w % NUMBERS], CELL_BYTES);
        }
        run_few(0, alen, &with, memory, &first);
        run_few(1, blen, &with, memory, &second);
        if (!alike(&first, &second)) return(0);
    }
    return(1);
}

// Do two sequences do the same?  Both are put in memory, and each is run
// from every state: they must leave the same.  If the first runs to its
// end from too few, states it does run from are looked for, and they must
// leave the same from those; and if there are still too few, nothing is
// known.  Then they are run from states made for them (same_from), and
// from every way of giving the cells they read a few numbers
// (same_from_few).
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
        if (ran < ENOUGH) return(0);
    }
    for(int s=0; s < TRIED; s++) if (!same_from(alen, blen, &STATE[s])) return(0);
    for(int idx=0; idx < founds; idx++) if (!same_from(alen, blen, &FOUND[idx])) return(0);
    return(same_from_few(alen, blen));
}

// The instructions asked about: those of the ALU that do no IO.  Each has
// what it leaves, from the first few states and from all of them, and the
// one that stands for all that do the same, which is a baseword if any of
// them is.
typedef struct {
    uint16_t ins;
    uint64_t few, all;
    uint16_t stands;
} single;

static single ALU[8192];
static int alus;
static int alu_at[65536];           // where an instruction is in ALU, or -1
static int by_few[8192], by_all[8192];

static void list_alu(void) {
    memset(alu_at, -1, sizeof(alu_at));
    for(int ret=0; ret < 2; ret++)
    for(int in=0; in < 4; in++)
    for(int op=0; op < 16; op++)
    for(int out=0; out < 4; out++)
    for(int d=-2; d < 2; d++)
    for(int r=-2; r < 2; r++) {
        if (op == ALU_IO_READ || out == OUTPUT_IO_T) continue;
        instruction ins = {0};
        ins.alu.op_type = OP_TYPE_ALU;
        ins.alu.r_eip = ret;
        ins.alu.in_mux = in;
        ins.alu.alu_op = op;
        ins.alu.out_mux = out;
        ins.alu.dstack = d;
        ins.alu.rstack = r;
        uint16_t x = *(uint16_t*)&ins;
        alu_at[x] = alus;
        ALU[alus].ins = x;
        ALU[alus].few = leaves(&x, 1, 0, FEW, 0);
        ALU[alus].all = leaves(&x, 1, FEW, STATES, ALU[alus].few);
        alus++;
    }
}

static int few_order(const void* a, const void* b) {
    uint64_t x = ALU[*(const int*)a].few, y = ALU[*(const int*)b].few;
    return(x < y ? -1 : x > y);
}

static int all_order(const void* a, const void* b) {
    uint64_t x = ALU[*(const int*)a].all, y = ALU[*(const int*)b].all;
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
    for(int idx=0; idx < alus; idx++) by_few[idx] = by_all[idx] = idx;
    qsort(by_few, alus, sizeof(int), few_order);
    qsort(by_all, alus, sizeof(int), all_order);
    for(int from=0; from < alus; ) {
        int to = from, stands = by_all[from];
        while (to < alus && ALU[by_all[to]].all == ALU[by_all[from]].all) to++;
        for(int idx=to - 1; idx >= from; idx--) {
            if (is_word(ALU[by_all[idx]].ins)) stands = by_all[idx];
        }
        for(int idx=from; idx < to; idx++) ALU[by_all[idx]].stands = ALU[stands].ins;
        from = to;
    }
}

static int find(const int* order, int all, uint64_t h) {
    int lo = 0, hi = alus;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        uint64_t at = all ? ALU[order[mid]].all : ALU[order[mid]].few;
        if (at < h) lo = mid + 1; else hi = mid;
    }
    if (lo == alus) return(-1);
    return((all ? ALU[order[lo]].all : ALU[order[lo]].few) == h ? order[lo] : -1);
}

// Finding an answer is slow, and it is the same answer until the machine,
// or this, is written otherwise.  So what each sequence was found to be is
// kept in a file beside the images, which has in its name a number for the
// text of this file and of the machine's: change any of them and it is
// another file.  A line is the sequence, with a bit to say whether one
// instruction was asked for or two, and what was found, or nothing.
static const char* TEXTS[] = {
    "fuse.c", "stack.c", "../vm.c", "../vm.h", "../vm_opcodes.c", "../vm_opcodes.h",
    "../vm_instruction.h", "../vm_constants.h",
};
static uint64_t written;            // the number for the texts,
static int texts_read = 1;          // and whether every one could be read

// A file by where it is from this one.
static FILE* beside(const char* name, const char* mode) {
    const char* slash = strrchr(__FILE__, '/');
    char path[1024];
    snprintf(path, sizeof(path), "%.*s%s", slash ? (int)(slash - __FILE__ + 1) : 0, __FILE__, name);
    return(fopen(path, mode));
}

static void read_texts(void) {
    for(int idx=0; idx < sizeof(TEXTS) / sizeof(TEXTS[0]); idx++) {
        FILE* in = beside(TEXTS[idx], "r");
        if (in == NULL) { texts_read = 0; continue; }
        for(int c; (c = fgetc(in)) != EOF; ) written = mix(written, c);
        fclose(in);
    }
}

#define KEPT (1 << 18)
#define FOR_ONE (1ull << 48)        // in what was asked: one instruction was asked for
static uint64_t kept_asked[KEPT];
static long kept_found[KEPT];
static FILE* kept_file;

// Where what was found for a question is, or will be: less than nothing
// until it is known.
static long* kept(uint64_t was_asked) {
    long at = mix(0, was_asked) & (KEPT - 1);
    while (kept_asked[at] && kept_asked[at] != was_asked) at = (at + 1) & (KEPT - 1);
    kept_asked[at] = was_asked;
    return(&kept_found[at]);
}

static void read_kept(void) {
    for(long at=0; at < KEPT; at++) kept_found[at] = -1;
    if (!texts_read) return;        // nothing is kept that cannot be said to be of these texts
    char name[80];
    snprintf(name, sizeof(name), "../build/fuse-%d-%016llx.kept", CELL_BITS, (unsigned long long)written);
    FILE* in = beside(name, "r");
    unsigned long long was_asked;
    long found;
    while (in && fscanf(in, "%llx %lx", &was_asked, &found) == 2) *kept(was_asked) = found;
    if (in) fclose(in);
    kept_file = beside(name, "a");
}

static long keep(long* where, uint64_t was_asked, long found) {
    *where = found;
    if (kept_file) {
        fprintf(kept_file, "%llx %lx\n", (unsigned long long)was_asked, found);
        fflush(kept_file);
    }
    return(found);
}

static int ready = 0;
static void get_ready(void) {
    init_opcodes(FORTH_WORDS);
    nowhere = fopen("/dev/null", "w");
    make_states();
    list_give();
    lay_leads();
    list_alu();
    sort_alu();
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
    uint64_t h = leaves(seq, len, 0, FEW, 0);
    if (find(by_few, 0, h) < 0) return(0);
    int found = find(by_all, 1, leaves(seq, len, FEW, STATES, h));
    if (found < 0) return(0);
    uint64_t was_asked = FOR_ONE | (uint64_t)first << 32 | (uint64_t)second << 16 | third;
    long* answer = kept(was_asked);
    if (*answer >= 0) return(*answer);
    uint16_t one = ALU[found].stands;
    return(keep(answer, was_asked, same(seq, len, &one, 1) ? one : 0));
}

// The pairs that may do the work of three: every two of the instructions
// that do not return and stand for the rest, with what the pair leaves
// from the first few states, and in order of that.  They are made the
// first time they are asked for.
typedef struct {
    uint64_t few;
    uint64_t all;                   // from every state, once it has been wanted
    uint16_t seq[2];
    uint16_t known;
} pair;

static pair* PAIRS;
static long pairs;

static int pair_order(const void* a, const void* b) {
    const pair* x = a;
    const pair* y = b;
    if (x->few != y->few) return(x->few < y->few ? -1 : 1);
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
        for(int b=0; b < alus; b++) {
            if (!stands_alone(b)) continue;
            pair* p = &PAIRS[pairs++];
            p->seq[0] = ALU[a].ins;
            p->seq[1] = ALU[b].ins;
            p->known = 0;
            p->few = leaves(p->seq, 2, 0, FEW, 0);
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
    long* two = kept(was_asked);
    if (*two >= 0) return(*two);
    return(keep(two, was_asked, look_for_two(seq)));
}

// If the third returns, so does the second of the two: the pairs are
// looked for that may do the work of the three without the return, and
// each is then tried with it.
static long look_for_two(const uint16_t* seq) {
    if (!PAIRS) list_pairs();
    int ret = returns(seq[SEQ - 1]);
    uint16_t plain[SEQ] = { seq[0], seq[1], returning(seq[2], 0) };

    ran = 0;
    uint64_t few = leaves(plain, SEQ, 0, FEW, 0);
    long lo = 0, hi = pairs;
    while (lo < hi) {
        long mid = (lo + hi) / 2;
        if (PAIRS[mid].few < few) lo = mid + 1; else hi = mid;
    }
    if (lo == pairs || PAIRS[lo].few != few) return(0);
    uint64_t all = leaves(plain, SEQ, FEW, STATES, few);

    // If the three run from too few of the states, a great many pairs leave
    // the same from them, having stopped as often.  States the three do run
    // from are looked for, and a pair is one that leaves the same from
    // those too.
    static state more[TRIED];
    int mores = 0;
    uint64_t from_more = 0;
    if (ran < ENOUGH) {
        find_states(0, SEQ);
        if (!founds) return(0);
        mores = founds;
        memcpy(more, FOUND, sizeof(more));
        for(int idx=0; idx < mores; idx++) from_more = mix(from_more, left(0, SEQ, &more[idx]));
    }

    // Of the pairs that do the same, the one that moves the stacks least.
    long best = 0;
    int least = 0;
    for(; lo < pairs && PAIRS[lo].few == few; lo++) {
        pair* p = &PAIRS[lo];
        if (!p->known) {
            p->all = leaves(p->seq, 2, FEW, STATES, few);
            p->known = 1;
        }
        if (p->all != all) continue;
        if (mores) {
            uint64_t h = 0;
            lay(0, p->seq, 2);
            lay(1, NULL, 0);
            for(int idx=0; idx < mores; idx++) h = mix(h, left(0, 2, &more[idx]));
            if (h != from_more) continue;
        }
        int moved = 0;
        for(int idx=0; idx < 2; idx++) {
            instruction ins = *(instruction*)&p->seq[idx];
            moved += abs(ins.alu.dstack) + 4 * abs(ins.alu.rstack);
        }
        if (best && moved >= least) continue;
        uint16_t two[2] = { p->seq[0], returning(p->seq[1], ret) };
        if (!same(seq, SEQ, two, 2)) continue;
        best = (long)two[0] << 16 | two[1];
        least = moved;
    }
    return(best);
}
