//
// fuse.h - finds shorter equivalents of instruction sequences by asking
// the machine:
//
//   fuse_one    the single instruction equivalent to a sequence of two or
//               three
//   fuse_two    the two instructions equivalent to a sequence of three
//   fuse_lit    a shorter run equivalent to a run that contains literals
//               and variable accesses
//   fuse_few    the one instruction, or the two, equivalent to a run of
//               ALU instructions and literals
//
// Nothing in this file describes what an instruction does.  The machine's
// step function is written once, in vm_step.h, and is compiled here twice:
// in vm.c, where it runs on cells, and below, where it runs on terms.
//
// Running on terms is symbolic execution.  A term names a value instead of
// holding a number: the value a cell had when the sequence started, or the
// result of an operation on two terms.  Stepping a sequence on terms gives
// the term in every cell when the sequence ends.  Two sequences that end
// with the same terms end in the same state from every starting state, and
// that is how an answer is found.  This is sound because the step function
// is the machine's own, and because of the facts about the arithmetic that
// vm.c states beside it:
//
//   which operations are commutative
//   which are associative, and the identity element of each
//   which operation is its own inverse
//   which operations return a flag
//   that two cells are equal exactly when their xor is 0, and that a
//     cell's xor with itself is 0
//   which operations return the cell when both inputs are that cell
//   that no cell is less than itself
//
// try_facts() tests each of these against the arithmetic at startup.  Two
// sequences that are equivalent only because of some other property of
// numbers are not found.
//
// Running on cells is an independent check of the terms.  Every answer
// the terms give is run on vm.c beside the sequence it replaces, from a
// number of starting states, and the final states are compared: the stack
// depths, the stacks, every data cell, and the next instruction (same()).
// If they differ the build stops: the step on terms is wrong, or a stated
// fact about the arithmetic is wrong, or the states are.
//
// A state is a set of cells holding the kinds of number a program has:
// data addresses, small numbers, flags, and arbitrary values.  Random
// states alone are a weak test, for three reasons, and each has a remedy.
//
//   A random state never contains a value the sequence itself computes.
//   The xor of two cells appears nowhere else in a state, so `xor =` would
//   never return true, and would pass for an instruction that always gives
//   0.  So the two sequences are run again from states built for them:
//   each value they compute from a state is put, one value and one place
//   at a time, where they may read it next (same_from()).
//
//   Two sequences that test different conditions both return false from
//   nearly every random state.  So the two are also run from every
//   assignment of a few fixed numbers to the cells they read
//   (same_from_few()).
//
//   A state may lead a sequence to fetch the cell the sequence itself is
//   stored in.  So both sequences are in memory while either runs.
//
// Running was once the only check, and it gave a wrong answer.  It is a
// test, and no number of states makes it a proof.
//
// The machine that runs is the strict one (STRICT_ALIGN, ROM_BYTES).  It
// stops at a fetch, a store or a return outside memory, and at a store
// into its ROM, because those are undefined (MODEL.md).  A state that
// stops a sequence shows only that it stopped, and if too few states run a
// sequence to its end the test says nothing about it.  The machine runs
// exactly the sequence's instructions (strict_steps), so a sequence that
// returns is run like any other, and its return address is part of its
// final state.
//
// Memory is 256 cells, small enough to compare all of it after every run.
// The code is in a ROM in the middle of memory.  That leaves the low
// cells, whose addresses are flags and small numbers, as ordinary data, so
// a sequence that fetches or stores through such a value still runs.  A
// variable is a cell far from those, and it is the same cell whether a
// sequence reaches it by the variable instruction or by its address as a
// literal.  So the machine is given one extra cell for each address the
// two sequences contain as a literal (FAR).  A sequence that fetches or
// stores at a computed address where the machine has no cell stops from
// every state.  The test says nothing about such a sequence, and the terms
// are the only evidence for it.
//
// Not handled: instructions that do IO, which the step on terms does not
// model, and a sequence that returns before its last instruction, since
// the rest of it never runs.
//
// The cell size is that of the machine the program is run for
// (THIS_MACHINE, vm_constants.h): the machine of the image a tool is
// given, or the machine cross.fs is building for, which cross.fs passes to
// this file through gforth's C interface.  get_ready() sets everything up
// once, for the cell size in effect at that moment.
//

#ifndef HEXAFORTH_FUSE_H
#define HEXAFORTH_FUSE_H

// The machine compiled into this file has the memory sizes defined below
// and no display, whatever its cell size.  A file that included vm.h before
// this one would get one set of sizes for both machines, so that is an
// error.
#ifdef HEXAFORTH_VM_H
#error "fuse.h has the machine in it, of its own sizes: it is had before anything else that has vm.h"
#endif

#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../vm_constants.h"
#undef ADDR_BITS
#define ADDR_BITS   8
#undef DISP_W
#define DISP_W      0                               // no display
#undef MEM_MOST
#define MEM_MOST    ((CELL_MOST / 8) << ADDR_BITS)  // the size of its memory in bytes, with the widest cell
// Cells 0 to LOW-1 and the cells from DATA up are data.  The low cells are
// the ones a false flag or a small number addresses, and the high cells
// are what the states use as addresses.  The cells between are the ROM,
// which holds the code.
#define LOW         64
#define DATA        ((1 << ADDR_BITS) - 64)
#define DATA_CELLS  (LOW + (1 << ADDR_BITS) - DATA)
#define WHERE(nth)  ((nth) < LOW ? (nth) : DATA + (nth) - LOW)  // the address of the nth data cell
#define SEQ         3               // the longest sequence fuse_one and fuse_two take
#define RUN         32              // the longest run cross.fs asks fuse_lit or fuse_few about (fl-room)
#define LEADS       8               // cells for each sequence's lead-in code, at the start of the ROM
#define FIRST_CELL  (LOW + 2 * LEADS)                   // the first cell after the two lead-ins
#define SEQ_CELLS   ((DATA - FIRST_CELL) / 2)           // cells of ROM for each of the two sequences
#define LAID        (SEQ_CELLS * (CELL_BYTES / 2) - 1)  // the longest sequence that fits: a zero instruction follows it
#define LAID_LEAST  (SEQ_CELLS - 1)                     // LAID when a cell holds one instruction
#define LAID_MOST   (SEQ_CELLS * (CELL_MOST / 16) - 1)  // LAID with the widest cell
#define AT(which)   ((FIRST_CELL + SEQ_CELLS * (which)) * (CELL_BYTES / 2))     // the instruction address of a sequence
#define LEAD(which) ((LOW + LEADS * (which)) * (CELL_BYTES / 2))    // the instruction address of its lead-in
#define DEPTH       8               // the depth of each stack when a sequence starts
#define FILLED      16              // cells of each stack that a state fills
#define STATES      96              // the number of random states
#define ENOUGH      8               // a sequence must run to its end from at least this many states
#define TRIED       16              // states that same_from() builds more states from
#define MADE        6               // computed values put into each of those
#define PLACES      10              // places each value is put

#define STRICT_ALIGN
#define ROM_FROM    (LOW * CELL_BYTES)
#define ROM_BYTES   (DATA * CELL_BYTES)

// vm.c is included below as the machine.  When the machine stops it calls
// exit(), which is redefined here to longjmp back to the caller.  It also
// prints why it stopped, so stderr is pointed at /dev/null while vm.c is
// compiled.  stderr is restored afterwards with pop_macro and not just
// undefined: in Apple's C library stderr is only a macro, and the files
// included after this one use it.
//
// The machine has few cells, and a run can refer to cells outside them: a
// variable, reached by its instruction or by its address as a literal, is
// a cell at VAR_BASE or above.  So the machine is given one extra cell for
// each address that the two runs being compared contain as a literal
// (FAR), and a variable is the same cell however it is reached.
#define FARS 8
static struct {
    int n;
    uint64_t at[FARS];              // the addresses
    uint8_t cell[FARS][8];          // the cell at each address
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

// Sizes taken from the machine's own definitions.  The number of possible
// instruction words comes from the width of an instruction
// (vm_instruction.h), and the number of values a field can have is the
// number of names vm_opcodes.h gives it.
#define COUNT(of)   ((int)(sizeof(of) / sizeof((of)[0])))
#define EVERY_WORD  (1 << (8 * sizeof(instruction)))
#define INPUTS      COUNT(INPUT_MUX_REPR)
#define OPS         COUNT(ALU_OPS_REPR)
#define OUTPUTS     COUNT(OUTPUT_MUX_REPR)

#include "named.h"                  // looks up a word of vm_opcodes.h by name
#include "room.h"                   // growable lists and buffers

typedef struct {
    int64_t dstack[FILLED];
    int64_t rstack[FILLED];
    int64_t data[DATA_CELLS];
    int64_t far[FARS];              // the contents of the extra cells (FAR)
} state;

static state STATE[STATES];
static context machine;

// The random states are the same on every run: the generator starts from a
// fixed seed.
static uint64_t seed = 0x9e3779b97f4a7c15ull;
static uint64_t chance(void) {
    seed ^= seed << 13;
    seed ^= seed >> 7;
    seed ^= seed << 17;
    return(seed);
}

// The kinds of number a state's cell can hold:
//
//   address    the address of a data cell
//   small      a small number; an address plus a small number is usually
//              still an address
//   flag       TRUE or FALSE
//   below      the complement of an address or of a small number.  This is
//              how the machine makes a negative number, and inverting it
//              gives an address again
//   anything   any value
static int64_t address(void)  { return(DATA + chance() % (DATA_CELLS - LOW)); }
static int64_t small(void)    { return(chance() % 16); }
static int64_t flag(void)     { return(chance() & 1 ? cell(TRUE) : cell(FALSE)); }
static int64_t below(void)    { return(cell(~(chance() & 1 ? address() : small()))); }
static int64_t anything(void) { return(cell(chance())); }

// A sequence runs to its end only from a state whose cells hold the kinds
// of number it needs: a sequence that fetches needs an address, and one
// that inverts and then fetches needs the complement of an address.  If
// every state mixed all the kinds, each sequence would run from few of
// them.  So each state uses one of the mixes below, taken in turn.  The
// first few states, which are tried first, use the first few mixes, and
// none of those contains an arbitrary value.
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

// Returns a value for one cell of a state.  Half the time it is one of
// four values chosen for that state, so that cells are often equal to each
// other.
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

// Memory has room for two sequences, each with a lead-in that sets the two
// stack depths and jumps to the sequence.  Two sequences being compared
// are both in memory while either runs, so that a fetch of a cell that
// holds one of them reads the same value whichever is running.
static int lead;                    // the number of instructions in a lead-in
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

// Copies a sequence into memory as sequence 0 or sequence 1.
_Static_assert(LAID_LEAST >= RUN, "fuse.h: a run cross.fs asks about is longer than there is room to lay it");
static int after[2];                // the address of the instruction after each sequence
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

// Runs the first `len` instructions of sequence `which` from a state, and
// stops there.  The result is left in the machine.  Returns 0 if the
// machine stopped early.
static int run(int which, int len, const state* from) {
    memcpy(machine.DSTACK, from->dstack, sizeof(from->dstack));
    memcpy(machine.RSTACK, from->rstack, sizeof(from->rstack));
    for(int idx=0; idx < DATA_CELLS; idx++) cell_to(cell_at(&machine, WHERE(idx)), from->data[idx]);
    for(int idx=0; idx < far.n; idx++) cell_to(far.cell[idx], from->far[idx]);
    machine.EIP = LEAD(which);
    strict_steps = lead + len;
    if (setjmp(stopped)) return(0);
    vm(&machine);
    return(1);
}

// Runs sequence `which` from a state and returns a hash of the final
// state, or STOPPED if the machine stopped early.  `ran` counts the runs
// that reached the end.  The hash includes the next instruction: the one
// after the sequence, or the address the sequence returned to.  The two
// sequences are at different addresses, so falling off the end leads to a
// different address for each.  The address after either sequence is
// therefore hashed as one value for both.  Otherwise a state in which a
// sequence returned to the address just after the other sequence would
// make the two differ for that reason alone.
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

// Returns 1 if a value was already present before the sequence ran: on a
// stack, in the data, or in the ROM.
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

// Collects the values that sequence `which` computes from a state: every
// value present after each prefix of the sequence that was in no cell of
// the starting state.  They are added to the `n` values already in `made`.
// Returns the new count.
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

// Puts a value in one of the places a sequence may read: the top cells of
// the two stacks, and the data cells that those top cells address.
// Returns 0 if the place does not exist.
static int put(state* in, int place, int64_t v) {
    if (place < 4) { in->dstack[DEPTH - 1 - place] = v; return(1); }
    if (place < 6) { in->rstack[DEPTH - 1 - (place - 4)] = v; return(1); }
    int64_t at = place < 9 ? in->dstack[DEPTH - 1 - (place - 6)] : in->rstack[DEPTH - 1];
    for(int idx=0; idx < far.n; idx++) if (far.at[idx] == ucell(at)) { in->far[idx] = v; return(1); }
    if (at < 0 || at >= (1 << ADDR_BITS) || (at >= LOW && at < DATA)) return(0);
    in->data[at < LOW ? at : LOW + at - DATA] = v;
    return(1);
}

// When a sequence runs to its end from too few of the random states, the
// states lack the numbers it needs in the cells it reads.  A sequence that
// shifts and then fetches, for example, needs two numbers whose shift is
// an address, and no random state was built for that.  So states that it
// does run from are searched for: a random state with one or two of the
// cells a sequence reads first changed to another number.  GIVE is the
// numbers tried, and READ is the places, numbered as put() numbers them.
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

// Copies a state to `with`, changes one cell to another number (and a
// second cell, if `other` is not negative), and runs sequence `which` from
// it.  Returns 1 if the sequence ran to its end.
static int runs_with(int which, int len, const state* from, int one, int other, state* with) {
    *with = *from;
    put(with, READ[one / gives], GIVE[one % gives]);
    if (other >= 0) put(with, READ[other / gives], GIVE[other % gives]);
    return(run(which, len, with));
}

// Searches for states that sequence `which` runs from, and puts them in
// FOUND.  The search starts from a point derived from the sequence's
// instructions and the state's number, so it is repeatable but does not
// try the same change on every state.
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

// Returns 1 if the two sequences in memory end in the same state from
// every state built from `from` by putting one of the values that either
// sequence computes in one of the places.
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

// A comparison is rarely true by chance: two cells of a random state are
// rarely equal, and two pairs of them at once almost never.  So two
// sequences that compute different flags both give false from nearly every
// random state.  `= swap r> = and` and a pair of instructions that tested
// something else once passed as equivalent from every state there was.
//
// same_from_few() therefore runs the two sequences in memory from every
// assignment of three numbers (0, 1 and TRUE) to the cells they read
// first.  That covers every way those cells can be equal or unequal to
// each other, and greater or less than each other.  The cells are the top
// five of the data stack, the top two of the return stack, and data cells
// 0 and 1, which the numbers 0 and 1 address.
//
// There are NUMBERS to the power GIVEN assignments, and everything outside
// those cells is the same in each.  So the data is not written to memory a
// cell at a time for every run, and the result is not hashed.  The first
// state's memory is saved as raw bytes and copied back whole before each
// run, and a run's result is saved as raw bytes and compared whole.
#define NUMBERS 3
#define GIVEN   9
#define LOW_BYTES   (LOW * CELL_BYTES)                  // bytes of data below the ROM
#define HIGH_BYTES  ((DATA_CELLS - LOW) * CELL_BYTES)   // bytes of data above it
#define HIGH_AT     ((uint8_t*)machine.memory + DATA * CELL_BYTES)  // where the data above it starts

typedef struct {
    int sp, rsp, next;
    int64_t dstack[FILLED], rstack[FILLED];
    uint8_t low[LOW * (CELL_MOST / 8)], high[(DATA_CELLS - LOW) * (CELL_MOST / 8)];    // room for the widest cell
    uint8_t far[FARS][8];
} leavings;

// Runs sequence `which` from the given stacks, and from data saved as raw
// bytes.  Only the data is copied into memory; the ROM is left as it is.
// Returns 0 if the machine stopped early.
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

static int left_is(const leavings* l) {         // returns 1 if the machine's state equals the saved one
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
    run(0, 0, &with);                   // loads the first state's data into memory
    keep_left(&data);
    for(int way=0; way < ways; way++) {
        int w = way;
        for(int idx=0; idx < 5; idx++, w /= NUMBERS) with.dstack[DEPTH - 1 - idx] = number[w % NUMBERS];
        for(int idx=0; idx < 2; idx++, w /= NUMBERS) with.rstack[DEPTH - 1 - idx] = number[w % NUMBERS];
        for(int idx=0; idx < 2; idx++, w /= NUMBERS) {
            cell_to(data.low + idx * CELL_BYTES, number[w % NUMBERS]);
        }
        int ran = run_few(0, alen, &with, &data);
        if (ran) keep_left(&first);
        if (run_few(1, blen, &with, &data) != ran) return(0);
        if (ran && !left_is(&first)) return(0);
    }
    return(1);
}

// Tests whether two sequences are equivalent, as far as running them can
// tell.  Returns 1 if every run agreed, 0 if any run differed, and -1 if
// the first sequence ran to its end from too few states to tell.
//
// Both sequences are put in memory and run from every random state, and
// their final states must match.  If the first sequence ran to its end
// from fewer than ENOUGH states, find_states() searches for states it does
// run from, and the two must match from those as well.  Then the two are
// run from states built for them (same_from()), and from every assignment
// of a few numbers to the cells they read (same_from_few()).
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
// A term names the value in a cell.  The kinds of term are:
//
//   IN_DATA, IN_RETURN   the value a stack cell had when the sequence
//                        started
//   A_NUMBER             a known number
//   BY_ALU               the result of an operation on terms
//   FETCHED              the value fetched from memory at the address a
//                        term gives
//   MEMORY_WAS, STORED   memory itself: as it was at the start, or with a
//                        term stored at the address a term gives
//
// A term stands for the same value whatever the starting state, so two
// sequences that end with the same terms end in the same state from every
// starting state.  Each distinct term is stored once in KNOWN and found
// again by its contents, so two terms are the same exactly when they have
// the same index.
//
// One assumption is made about a starting state: every cell of a stack and
// of memory already holds a value of the cell's width, so truncating it to
// a cell changes nothing.  The machine never stores anything wider: the
// step does not, and a call truncates its return address to a cell (vm.c).
enum { IN_DATA, IN_RETURN, A_NUMBER, BY_ALU, FETCHED, MEMORY_WAS, STORED };

#define OPERANDS 8                  // the most operands a term has
#define SLOTS    32                 // cells of each stack that have a term
#define DEEP     (SLOTS / 2)        // the stack depth a sequence starts at
// The fixed terms, which always exist: the cells of the data stack as they
// were at the start, then the cells of the return stack, then memory as it
// was at the start.
#define DATA_TERM(idx)   (idx)
#define RETURN_TERM(idx) (SLOTS + (idx))
#define CELL_TERMS       (2 * SLOTS)        // the number of fixed terms that are stack cells
#define OF_DATA(term)    ((term) < SLOTS)   // true if a stack cell's term is of the data stack
#define CELL_OF(term)    ((term) % SLOTS)   // which cell of its stack it is
#define MEMORY_TERM      CELL_TERMS
#define FIXED            (MEMORY_TERM + 1)
#define ROOM     512
#define FETCHES  16

typedef struct {
    int kind, op, n;
    int of[OPERANDS];               // its operands, as indexes into KNOWN
    int64_t number;                 // the value of a number; or which cell a stack cell is
    uint64_t is;                    // a hash of the whole term, the same wherever it is built
} term;

static term KNOWN[ROOM];
static int known;
static int lost;                    // set when a table overflowed: the result is then unknown

static int term_of(int kind, int op, int64_t number, int n, const int* of) {
    uint64_t is = mix(mix(mix(mix(0x7465726d, kind), op), number), n);
    for(int idx=0; idx < n; idx++) is = mix(is, KNOWN[of[idx]].is);
    // the fixed terms come first, and only they have these kinds
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

// name_cells() creates the fixed terms, in order.  forget() discards every
// other term.
static void name_cells(void) {
    for(int idx=0; idx < SLOTS; idx++) term_of(IN_DATA, 0, idx, 0, NULL);
    for(int idx=0; idx < SLOTS; idx++) term_of(IN_RETURN, 0, idx, 0, NULL);
    term_of(MEMORY_WAS, 0, 0, 0, NULL);
}

static void forget(void) {
    known = FIXED;
    lost = 0;
}

static void in_order(int* of, int n) {      // sorts terms by hash, which gives the same order on every run
    for(int i=1; i < n; i++) {
        for(int j=i; j > 0 && KNOWN[of[j]].is < KNOWN[of[j - 1]].is; j--) {
            int t = of[j]; of[j] = of[j - 1]; of[j - 1] = t;
        }
    }
}

static int t_number(int64_t n) {
    return(term_of(A_NUMBER, 0, cell(n), 0, NULL));
}

// Returns the term for a literal's number.  While `any_number` is set, it
// returns instead a term for an unknown value: one more starting cell,
// belonging to neither stack.  Two sequences found equivalent that way are
// equivalent whatever the number is (util/joins.c).
static int any_number;

static int t_in_instruction(int64_t n) {
    if (any_number) return(term_of(IN_DATA, 0, SLOTS, 0, NULL));
    return(t_number(n));
}

static int t_whole(int x) {         // a term already has the width of a cell
    return(x);
}

// Tests whether a term is a flag, or a flag with the self-inverse
// operation applied to it.  Returns the flag's term, or -1 if it is
// neither.  `undone_by` is set to that operation, or to -1 if it was not
// applied.
static int flag_in(int t, int* undone_by) {
    *undone_by = -1;
    if (KNOWN[t].kind == BY_ALU && ALU_UNDOES(KNOWN[t].op)) {
        *undone_by = KNOWN[t].op;
        t = KNOWN[t].of[0];
    }
    return(KNOWN[t].kind == BY_ALU && ALU_FLAGS(KNOWN[t].op) ? t : -1);
}

// Returns the value of a term that is a number, a flag or an inverted
// flag, given the flag's value.
static int64_t when(int t, int64_t flag) {
    int undone_by;
    if (KNOWN[t].kind == A_NUMBER) return(KNOWN[t].number);
    flag_in(t, &undone_by);
    return(undone_by < 0 ? cell(flag) : cell(alu(undone_by, cell(flag), cell(flag))));
}

static int t_made(int op, int a, int b);

// Simplifies an operation on a flag and a number, or on a flag and itself,
// where either operand may be inverted.  A flag is TRUE or FALSE, so the
// operation is computed for both values with the machine's arithmetic.  If
// both give the same number, the result is that number.  If they give the
// flag back, or the inverted flag, the result is that term.  Returns -1 if
// the operands are not of this form, or the result is none of those.
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

// Returns the term for an operation applied to two terms.  The facts vm.c
// states about the arithmetic are used to give equal values the same term:
//
//   two numbers are computed with the machine's arithmetic
//   the operands of a commutative operation are put in a fixed order
//   a chain of an associative operation is flattened into one term, with
//     its numbers combined into one
//   a self-inverse operation applied twice gives its operand back
//   an operation on a flag is simplified (of_flag())
//   an equality test becomes a test on the xor of its two operands; a
//     cell's xor with itself is 0, so a cell compared with itself is TRUE,
//     which is a number
//   an idempotent operation (and, or) on a cell and itself gives the cell
//   a less-than comparison of a cell with itself is FALSE
static int t_made(int op, int a, int b) {
    const term* x = &KNOWN[a];
    const term* y = &KNOWN[b];
    if (x->kind == A_NUMBER && y->kind == A_NUMBER) return(t_number(alu(op, x->number, y->number)));
    if (ALU_UNDOES(op) && x->kind == BY_ALU && x->op == op) return(t_whole(x->of[0]));
    int flag = of_flag(op, a, b);
    if (flag >= 0) return(flag);
    // Two cells are equal exactly when their xor is 0.  So the xor of the
    // two operands is built first, which flattens it into any xor that
    // either operand already was.  The equality is then a test of that
    // xor's two operands against each other, or of the whole xor against 0
    // if it does not have exactly two.
    for(int apart=0; ALU_SAME(op) && apart < OPS; apart++) {
        if (!ALU_DIFFERS(apart) || !ALU_REGROUPS(apart)) continue;
        int told = t_made(apart, a, b);
        const term* t = &KNOWN[told];
        if (t->kind == A_NUMBER) return(t_number(alu(op, t->number, 0)));   // the xor is a known number
        int two[2] = { told, 0 };
        if (t->kind == BY_ALU && t->op == apart && t->n == 2) { two[0] = t->of[0]; two[1] = t->of[1]; }
        else two[1] = t_number(0);
        in_order(two, 2);
        flag = of_flag(op, two[0], two[1]);
        return(flag >= 0 ? flag : term_of(BY_ALU, op, 0, 2, two));
    }
    if (ALU_ORDERS(op) && a == b) return(t_number(FALSE));      // no cell is less than itself
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
        // An operation that gives 0 for two equal cells, and whose identity
        // is 0, cancels a term that appears twice among its operands.  That
        // operation is xor.
        if (ALU_DIFFERS(op) && cell(ALU_NOTHING(op)) == 0) {
            int kept = 0;
            in_order(of, n);
            for(int idx=0; idx < n; idx++) {
                if (idx + 1 < n && of[idx] == of[idx + 1]) idx++;
                else of[kept++] = of[idx];
            }
            n = kept;
        }
        // An idempotent operation (and, or) gives the cell for a cell and
        // itself, so a repeated operand is kept once.
        if (ALU_KEEPS(op)) {
            int kept = 0;
            in_order(of, n);
            for(int idx=0; idx < n; idx++) if (idx + 1 == n || of[idx] != of[idx + 1]) of[kept++] = of[idx];
            n = kept;
        }
        if (numbers && (number != cell(ALU_NOTHING(op)) || !n)) of[n++] = t_number(number);
        if (!n) return(t_number(ALU_NOTHING(op)));
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

// The machine's state, as terms.  The stacks are laid out as vm.c's are: T
// and R hold the top cells, and DS and RS hold the cells under them, up to
// the depths.
typedef struct {
    int DS[SLOTS], RS[SLOTS], T, R, SP, RSP;
    int M;                          // memory
    int next;                       // the address it returned to, or -1 if it did not return
    int fetched[FETCHES], fetches;  // the addresses it fetched from
    int unsaid;                     // set if it did something the step on terms does not model: IO, a jump, a call
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

// Returns the term for a fetch from an address.  A fetch from the address
// of the most recent store gives the stored term.  Each address fetched
// from is recorded once, however often it is fetched: only whether it was
// fetched matters (fetched_from()).
static int t_fetch(named* m, int at) {
    at = t_whole(at);
    int had = 0;
    for(int idx=0; idx < m->fetches; idx++) had |= m->fetched[idx] == at;
    if (!had) {
        if (m->fetches == FETCHES) { lost = 1; return(0); }
        m->fetched[m->fetches++] = at;
    }
    const term* mem = &KNOWN[m->M];
    if (mem->kind == STORED && mem->of[1] == at) return(mem->of[2]);
    int of[2] = { m->M, at };
    return(term_of(FETCHED, 0, 0, 2, of));
}

// Records a store.  A store to the address of the most recent store
// replaces it.
static void t_store(named* m, int at, int x) {
    at = t_whole(at);
    const term* mem = &KNOWN[m->M];
    int of[3] = { mem->kind == STORED && mem->of[1] == at ? mem->of[0] : m->M, at, t_whole(x) };
    m->M = term_of(STORED, 0, 0, 3, of);
}

// Steps one instruction on terms, using the machine's step function
// (vm_step.h).
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

// Lists the addresses a sequence fetched from, sorted, and returns how
// many.  An address it also stored to is left out: if the store there does
// not stop the machine, the fetch does not either.
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

// Returns a hash of a sequence's final state: the stack depths, the stack
// cells, memory, the return address, and the addresses fetched from.  The
// fetch addresses are included so that two sequences with the same hash
// also stop from the same starting states.  Returns 0 if the state is
// unknown.
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

// Returns 1 if two final states have the same terms.  Both must have been
// computed with no forget() between them.
static int same_named(const named* a, const named* b) {
    if (a->unsaid || b->unsaid || lost) return(0);
    if (a->SP != b->SP || a->RSP != b->RSP || a->T != b->T || a->R != b->R || a->M != b->M || a->next != b->next) return(0);
    if (memcmp(a->DS, b->DS, (a->SP - 1) * sizeof(int)) || memcmp(a->RS, b->RS, (a->RSP - 1) * sizeof(int))) return(0);
    int x[FETCHES], y[FETCHES];
    int n = fetched_from(a, x);
    return(n == fetched_from(b, y) && !memcmp(x, y, n * sizeof(int)));
}

// Steps a sequence on terms from the starting state, into `m`, and returns
// the hash of its final state.
static uint64_t names(const uint16_t* seq, int len, named* m) {
    start(m);
    for(int idx=0; idx < len; idx++) {
        step_named(m, seq[idx]);
        if (m->next >= 0 && idx < len - 1) m->unsaid = 1;      // it returned, and the rest did not run
    }
    return(left_named(m));
}

// Classifies an instruction as a variable fetch, a variable store, or
// neither.  The step decides.  A variable instruction is one that vm.c's
// loop decodes as a literal and that, stepped alone, fetches from a cell
// whose address is a known number, or stores to such a cell.  The term
// table is left as it was.
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

// Gives the machine an extra cell at an address, if the address is a known
// number and the machine has no cell there.
static void give_cell(int address) {
    if (KNOWN[address].kind != A_NUMBER) return;
    uint64_t at = ucell(KNOWN[address].number);
    if (!(at >> ADDR_BITS) || far_cell(at) || far.n == FARS) return;
    far.at[far.n++] = at;
}

// Runs two sequences that the terms found equivalent on vm.c, and ends the
// program with a message if they differ.  The terms are right for every
// state only if the step and the stated facts about the arithmetic are
// right.  Running is an independent check of that: if the machine
// disagrees, one of the two is wrong, and neither is used.
static void tried(const uint16_t* a, int alen, const uint16_t* b, int blen) {
    // Give the machine a cell for each literal address the two sequences
    // fetch from or store to, as the step on terms reports them.  The term
    // table is left as it was.
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
    fprintf(stderr, "\nfuse.h: by their terms");
    for(int idx=0; idx < alen; idx++) fprintf(stderr, " %04x", a[idx]);
    fprintf(stderr, " and");
    for(int idx=0; idx < blen; idx++) fprintf(stderr, " %04x", b[idx]);
    fprintf(stderr, " do the same, and run on vm.c they do not\n");
    exit(1);
}

// ALU lists the ALU instructions whose final state the step can give,
// which are those that do no IO.  Each entry has the hash of the
// instruction's final state, and the representative of all the
// instructions with that hash (plainer(), below).
typedef struct {
    uint16_t ins;
    uint64_t names;                 // the hash of its final state
    uint16_t stands;                // its representative
    int8_t d, r;                    // how far it moves each stack
    int8_t stores, fetches;         // whether it stores, and whether it fetches
} single;

static single ALU[EVERY_WORD];
static int alus;
static int alu_at[EVERY_WORD];      // an instruction's index in ALU, or -1
static int by_names[EVERY_WORD];    // indexes into ALU, sorted by hash

// Fills ALU.  Every possible instruction word is stepped on terms.  The
// ones kept are those that vm.c's loop decodes as ALU instructions and
// whose final state the step can give.  How each moves the stacks is read
// from that final state.  Nothing here decodes the fields.
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

// Returns 1 if instruction `a` is a better representative than `b`.  Among
// instructions with the same final state, the representative is the one
// whose operation has the lowest value in the operation field
// (vm_instruction.h), where passing the input through is the lowest; and
// among those, the lowest instruction word.  Having a name does not make
// an instruction the representative.  Everything that produces an
// instruction must arrive at the same one from the fields alone.  The
// nucleus's peephole does, by trying the operations in that order
// (forth/compose.fs), and util/joins.c checks that every name in
// vm_opcodes.h is given to a representative.
static int plainer(uint16_t a, uint16_t b) {
    int x = ((instruction*)&a)->alu.alu_op, y = ((instruction*)&b)->alu.alu_op;
    return(x != y ? x < y : a < b);
}

// Sorts by_names by hash, for binary search, and sets the representative
// of each group of instructions with the same hash.
static void sort_alu(void) {
    for(int idx=0; idx < alus; idx++) by_names[idx] = idx;
    qsort(by_names, alus, sizeof(int), names_order);
    for(int from=0; from < alus; ) {
        int to = from, stands = by_names[from];
        while (to < alus && ALU[by_names[to]].names == ALU[by_names[from]].names) to++;
        for(int idx=from; idx < to; idx++) {
            if (plainer(ALU[by_names[idx]].ins, ALU[stands].ins)) stands = by_names[idx];
        }
        for(int idx=from; idx < to; idx++) ALU[by_names[idx]].stands = ALU[stands].ins;
        from = to;
    }
}

// Returns the ALU index of an instruction whose final state has this hash,
// or -1 if there is none.
static int find(uint64_t names) {
    int lo = 0, hi = alus;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (ALU[by_names[mid]].names < names) lo = mid + 1; else hi = mid;
    }
    return(lo < alus && ALU[by_names[lo]].names == names ? by_names[lo] : -1);
}

// Finding an answer is slow, and the answer stays the same until the
// machine or this file changes.  So every answer is cached in a file in
// build/.  The file's name contains a hash of the text of this file and of
// the machine's source files (TEXTS), so a change to any of them starts a
// new cache file.  A line of the file is the question (the sequence, with
// a bit that says whether one instruction or two were asked for) and the
// answer, which is 0 if none was found.
static const char* TEXTS[] = {
    "fuse.h", "named.h", "room.h", "stack.c", "../vm.c", "../vm_loop.h", "../vm_step.h", "../vm.h", "../vm_opcodes.c",
    "../vm_opcodes.h", "../vm_instruction.h", "../vm_constants.h",
};
static uint64_t written;            // the hash of the texts
static int texts_read = 1;          // 0 if any of them could not be read

// Opens a file by its path relative to this file's directory.
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

#define FOR_ONE (1ull << 48)        // set in a question when one instruction was asked for
static FILE* kept_file;

// The cache in memory: a hash table of questions and their answers.  It is
// kept at most half full, so a lookup takes a probe or two.
typedef struct { uint64_t asked; long found; } answer_kept;
static answer_kept* KEPT_ANSWER;
static int kept_bits;               // the table has 2^kept_bits slots
static long kept_count;

static answer_kept* kept_at(uint64_t was_asked) {
    long at = mix(0, was_asked) & ((1L << kept_bits) - 1);
    while (KEPT_ANSWER[at].asked && KEPT_ANSWER[at].asked != was_asked) at = (at + 1) & ((1L << kept_bits) - 1);
    return(&KEPT_ANSWER[at]);
}

// Returns a pointer to the cached answer for a question.  If there is no
// entry yet, one is added with the answer -1, which means unknown.  The
// pointer is valid only until the next call, which may grow the table.
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

// fuse_lit's answers are cached too.  An entry is a hash of the run, the
// length of the shorter run (or -1 if none was found), and the shorter
// run itself.  Its line in the file begins with an L.
typedef struct { uint64_t asked; int len; uint16_t* run; } lit_answer;
static lit_answer* LIT;
static int lit_bits;
static long lit_count;

static lit_answer* lit_at(uint64_t asked) {
    long at = mix(1, asked) & ((1L << lit_bits) - 1);
    while (LIT[at].asked && LIT[at].asked != asked) at = (at + 1) & ((1L << lit_bits) - 1);
    return(&LIT[at]);
}

// Returns the cache entry for a run, or the empty slot where it will go
// (its `asked` is 0).  The pointer is valid only until the next call,
// which may grow the table.
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

// Caches the answer for a run: its length, or -1 if none was found, and
// its instructions.
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
    if (!texts_read) return;        // with no hash of the texts there is no cache
    static const char form[] = "../build/fuse-%d-%016llx.kept";
    char name[sizeof(form) + 3 * sizeof(int)];          // the cell size in decimal is at most three digits per byte of an int
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

// Tests every fact vm.c states about the arithmetic against the arithmetic
// itself, for every operation and a range of cells, before anything is
// derived from those facts.  A fact that fails ends the program.  The
// cells are numbers at both ends of each cell width, and the complement of
// each.
static void try_facts(void) {
    static const int64_t of[] = { 0, 1, 2, 3, 7, 8, 15, 16, 255, 256, 4095, 4096, 0x7fff, 0x8000, 0xffff, 0x10000,
                                  0x7fffffff, 0x80000000ll, 0xffffffffll, 0x123456789abcdefll, 0x7fffffffffffffffll };
    enum { FACT_CELLS = 2 * (sizeof(of) / sizeof(of[0])) };
    int64_t given[FACT_CELLS];
    for(int idx=0; idx < FACT_CELLS / 2; idx++) given[2 * idx] = cell(of[idx]), given[2 * idx + 1] = cell(~of[idx]);
    for(int op=0; op < OPS; op++) {
        for(int i=0; i < FACT_CELLS; i++) for(int j=0; j < FACT_CELLS; j++) {
            const int64_t a = given[i], b = given[j], made = cell(alu(op, a, b));
            const char* not_so = NULL;
            if (ALU_EITHER_WAY(op) && made != cell(alu(op, b, a))) not_so = "it takes its inputs either way";
            if (ALU_REGROUPS(op) && cell(alu(op, a, cell(ALU_NOTHING(op)))) != a) {
                not_so = "there is a number that, as one input, leaves the other as it was";
            }
            for(int k=0; ALU_REGROUPS(op) && k < FACT_CELLS && !not_so; k++) {
                if (cell(alu(op, made, given[k])) != cell(alu(op, a, cell(alu(op, b, given[k]))))) not_so = "a run of it may be regrouped";
            }
            if (ALU_UNDOES(op) && cell(alu(op, cell(alu(op, a, a)), cell(alu(op, a, a)))) != a) {
                not_so = "done twice it gives back the cell it was given";
            }
            if (ALU_FLAGS(op) && made != cell(TRUE) && made != cell(FALSE)) not_so = "it leaves a flag";
            if (ALU_SAME(op) && made != cell(a == b ? TRUE : FALSE)) not_so = "its flag is whether two cells are the same";
            if (ALU_DIFFERS(op) && (made == 0) != (a == b)) {
                not_so = "it makes nothing of two cells when they are the same, and of no others";
            }
            if (ALU_KEEPS(op) && cell(alu(op, a, a)) != a) not_so = "it gives a cell when both inputs are that cell";
            if (ALU_ORDERS(op) && cell(alu(op, a, a)) != cell(FALSE)) not_so = "no cell is less than itself";
            if (!not_so) continue;
            fprintf(stderr, "fuse.h: vm.c says of the operation %s that %s, and of %llx and %llx it is not so\n",
                    ALU_OPS_REPR[op], not_so, (unsigned long long)ucell(a), (unsigned long long)ucell(b));
            exit(EXIT_FAILURE);
        }
    }
}

// Sets everything up, once.  The states and tables are built for the cell
// size in effect at the first call, and a later call with another cell
// size ends the program.
static int ready = 0;               // the cell size it was set up for, in bits
static void get_ready(void) {
    if (ready == CELL_BITS) return;
    if (ready) {
        fprintf(stderr, "fuse.h: it was made ready for cells of %d bits, and is asked of cells of %d\n", ready, CELL_BITS);
        exit(EXIT_FAILURE);
    }
    init_opcodes(FORTH_WORDS);
    nowhere = fopen("/dev/null", "w");
    try_facts();
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
    ready = CELL_BITS;
}

// Returns 1 if every instruction of a sequence is in ALU and none but the
// last returns.
static int asked(const uint16_t* seq, int len) {
    for(int idx=0; idx < len; idx++) {
        if (alu_at[seq[idx]] < 0) return(0);
        if (idx < len - 1 && returns(seq[idx])) return(0);
    }
    return(1);
}

// Returns the single instruction equivalent to a sequence of two or three
// instructions, or 0 if none is found.  For a sequence of two, `first` is
// 0.
long fuse_one(long first, long second, long third) {
    get_ready();
    uint16_t seq[SEQ];
    int len = 0;
    if (first) seq[len++] = first;
    seq[len++] = second;
    seq[len++] = third;
    if (!asked(seq, len)) return(0);
    // The lookup by terms is cheap and is repeated on every call.  What is
    // cached is that the answer has been run on the machine.
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

// The table of pairs, for fuse_two: every pair of representative
// instructions that do not return, with the hash of the pair's final
// state, sorted by that hash.  It is built the first time it is needed.
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

// Returns the index in PAIRS of the first pair whose final state has this
// hash.  The pairs with the same hash follow it.
static long pairs_leaving(uint64_t is) {
    long lo = 0, hi = pairs;
    while (lo < hi) {
        long mid = (lo + hi) / 2;
        if (PAIRS[mid].names < is) lo = mid + 1; else hi = mid;
    }
    return(lo);
}

// Returns how far a pair moves the stacks.  A move of the return stack
// counts as four moves of the data stack.
#define MOST_MOVED 20                   // the largest result: both instructions move both stacks as far as an instruction can
static int moved_by(const pair* p) {
    int moved = 0;
    for(int idx=0; idx < 2; idx++) {
        const single* one = &ALU[alu_at[p->seq[idx]]];
        moved += abs(one->d) + 4 * abs(one->r);
    }
    return(moved);
}

// Finds the two instructions equivalent to a run, and writes them to
// `best`.  Returns 1 if there are two, and 0 if there are not.  The run may
// contain ALU instructions and literals.  If the run's last instruction
// returns, the second of the two must return too.  So the pairs are looked
// up by the final state of the run without the return, and each candidate
// is then compared with the return added.  Among the pairs with the same
// final terms, the one that moves the stacks least is chosen.  This
// function discards every term (forget()).
static int pair_for(const uint16_t* run, int len, uint16_t* best) {
    if (!PAIRS) list_pairs();
    int ret = alu_at[run[len - 1]] >= 0 && returns(run[len - 1]);
    uint16_t plain[RUN];
    memcpy(plain, run, len * 2);
    if (ret) plain[len - 1] = returning(run[len - 1], 0);
    named whole, pair_of;
    forget();
    uint64_t is = names(plain, len, &whole);
    if (!is) return(0);
    int least = 0, found = 0;
    for(long at=pairs_leaving(is); at < pairs && PAIRS[at].names == is; at++) {
        const pair* p = &PAIRS[at];
        int moved = moved_by(p);
        if (found && moved >= least) continue;
        uint16_t two[2] = { p->seq[0], returning(p->seq[1], ret) };
        forget();
        names(run, len, &whole);
        names(two, 2, &pair_of);
        if (!same_named(&whole, &pair_of)) continue;
        best[0] = two[0];
        best[1] = two[1];
        least = moved;
        found = 1;
    }
    return(found);
}

// Returns the two instructions equivalent to a sequence of three, packed
// with the first in the upper sixteen bits, or 0 if none is found.
long fuse_two(long first, long second, long third) {
    get_ready();
    uint16_t seq[SEQ] = { first, second, third };
    if (!asked(seq, SEQ)) return(0);
    uint64_t was_asked = (uint64_t)seq[0] << 32 | (uint64_t)seq[1] << 16 | seq[2];
    long two = *kept(was_asked);
    if (two >= 0) return(two);
    uint16_t best[2];
    if (!pair_for(seq, SEQ, best)) return(keep(was_asked, 0));
    tried(seq, SEQ, best, 2);
    return(keep(was_asked, (long)best[0] << 16 | best[1]));
}

// Returns 1 if two runs are equivalent.  Each may be up to RUN
// instructions long, and may contain literals and variable fetches and
// stores.  It returns 1 only for two runs with the same final terms, and
// those are then run on the machine, as every answer is.
long fuse_same(const uint16_t* a, long alen, const uint16_t* b, long blen) {
    get_ready();
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
// cross.fs asks about the last few things it compiled together with the
// instruction it is compiling: a run of ALU instructions, literals pushed
// and added, and variable accesses.  fuse_lit searches for a shorter run
// with the same final terms.  The candidates are:
//
//   the empty run
//   one ALU instruction, or two
//   either of those with one number pushed or added before, between or
//     after the instructions
//   any of those with the run's variable instructions, each used once
//
// The number is one that appears in the run's final terms.  When a
// candidate has two ALU instructions, both are basewords: trying every
// instruction with every other would take too long.
//
// A candidate is only a guess until its terms are compared with the
// run's.  So the search may miss answers, but it cannot return a wrong
// one.

// How a number is written as literal instructions is learned from the
// step.  Every literal instruction whose value field is 1 is stepped
// alone.  If it leaves the stack one deeper with a number on top, it is a
// push, and that number is what one unit of its value field is worth.  If
// it leaves the top with a number added to it, it is an add.
typedef struct { uint16_t ins; int64_t worth; } literal;
static literal* PUSHES;             // the push literals; both lists grow as literals are found
static literal* ADDS;               // the add literals
static long pushes_room, adds_room;
static int pushes, adds;
static int64_t most;                // the largest value a literal's value field holds
static uint16_t INVERTS, IS_SAME;   // the basewords `invert` and `=`

static void more_worth(literal* of, int n) {     // sorts by worth, largest first
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

// Writes a number as literal instructions into `out`: pushed, or added to
// the top if `add` is set.  A push is the largest literal that fits the
// number, followed by adds for the remainder.  A negative number is pushed
// as its complement followed by `invert`.  Returns the number of
// instructions, or -1 if the number cannot be written this way.
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

// BASE lists the ALU instructions that are basewords and do not return.
// REPS lists the representatives that do not return.  Both hold indexes
// into ALU.
static int BASE[COUNT(FORTH_WORDS)], bases;    // at most one for each word of FORTH_WORDS
static int REPS[EVERY_WORD], reps;

static void list_tried(void) {
    for(int at=0; at < alus; at++) {
        if (returns(ALU[at].ins)) continue;
        if (is_word(ALU[at].ins)) BASE[bases++] = at;
        if (ALU[at].stands == ALU[at].ins) REPS[reps++] = at;
    }
}

// numbers_in() adds the numbers that appear in a term to `out`, skipping
// any already there, and returns the new count.  numbers_left() collects
// the numbers in a whole final state.
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

// The state of one search: the target, and the best candidate so far.
static struct {
    named want;                     // the run's final state, which a candidate must match
    int after_want;                 // the number of terms `want` uses; terms above this are scratch
    uint16_t tail[4];               // instructions appended to the run and to every candidate: `0 =` before a branch
    int tail_len;
    const uint16_t* run;            // the run being replaced
    int len;
    uint16_t best[RUN];             // the best candidate found
    int best_len;
    int as_it_stood;                // how many of the run's leading instructions `best` keeps unchanged
} looked;

static void follow(named* m, const uint16_t* run, int len) {
    for(int idx=0; idx < len; idx++) step_named(m, run[idx]);
}

static int rise(const uint16_t* run, int len) {     // returns how much deeper a run leaves the data stack
    int had = known, was_lost = lost;
    named m;
    start(&m);
    follow(&m, run, len);
    known = had;
    lost = was_lost;
    return(m.SP - DEEP);
}

// Offers a candidate whose final state is `m`.  If it matches the target,
// it replaces the best so far when it is shorter, or when it is the same
// length and keeps more of the run's leading instructions unchanged.  A
// candidate that keeps more of the run changes less of what the compiler
// has already compiled, so later joins find what they would have found.
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

// Tries every candidate with one arrangement of the non-ALU parts: `pre`
// before the first ALU instruction, `mid` between the two, and `suf`
// after.  `alus_in` is the number of ALU instructions, 0, 1 or 2.  Every
// instruction that moves the stacks by the required amount is tried in
// each position.
static void place(const uint16_t* pre, int plen, const uint16_t* mid, int mlen, const uint16_t* suf, int slen, int alus_in) {
    // a candidate must be shorter than the run, and at most as long as the best so far
    if (plen + mlen + slen + alus_in >= looked.len || plen + mlen + slen + alus_in > looked.best_len) return;
    // If the run returns, the candidate must return too, in its last
    // instruction.  So that must be an ALU instruction, with the return in
    // it.
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
    // Skip an instruction that cannot be part of an answer: one that moves
    // the stacks so that no second instruction (or none) can bring them to
    // the target depths, one that stores when the target leaves memory
    // unchanged, or one that fetches when the target fetches nothing.
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

// Tries every order of the parts of a candidate: the variable
// instructions, the number if there is one, and the one or two ALU
// instructions, where x always comes before y.
enum { AN_X = -1, A_Y = -2 };
static void orders(const uint16_t* things[], const int* lens, int count, int alus_in) {
    int item[5], items = 0;
    for(int idx=0; idx < count; idx++) item[items++] = idx;
    if (alus_in >= 1) item[items++] = AN_X;
    if (alus_in == 2) item[items++] = A_Y;
    int order[5];
    // enumerate the permutations by counting through them
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

// Offers every pair of instructions whose final state is the run's.  The
// two need not be basewords, because the pairs are looked up in PAIRS.
// fuse_lit builds PAIRS before it computes the target, since building it
// discards every term.  The pairs are offered in order of how far they
// move the stacks, least first.  So among the pairs that keep the same
// number of the run's leading instructions, offer() keeps the one that
// moves the stacks least.  If the run returns, the second instruction of
// each pair returns.  Nothing is offered before a conditional branch,
// because there the target is not the run's own final state.
static void offer_pairs(void) {
    if (looked.tail_len || looked.len < 3 || !PAIRS) return;
    int ends = looked.want.next >= 0;
    uint16_t plain[RUN];
    memcpy(plain, looked.run, looked.len * 2);
    if (ends) plain[looked.len - 1] = returning(plain[looked.len - 1], 0);
    named left;
    known = looked.after_want;
    lost = 0;
    uint64_t is = names(plain, looked.len, &left);
    if (!is) return;
    long from = pairs_leaving(is);
    for(int moved=0; moved <= MOST_MOVED; moved++) {
        for(long at=from; at < pairs && PAIRS[at].names == is; at++) {
            if (moved_by(&PAIRS[at]) != moved) continue;
            uint16_t two[2] = { PAIRS[at].seq[0], returning(PAIRS[at].seq[1], ends) };
            named with;
            known = looked.after_want;
            lost = 0;
            start(&with);
            follow(&with, two, 2);
            offer(&with, two, 2);
        }
    }
}

// Finds a shorter run equivalent to a run with numbers in it.  Returns its
// length and writes it to `out`, or returns -1 if none is found.  If
// `branch` is set, a conditional branch follows the run.  The replacement
// then only has to leave the same state apart from the top of the stack,
// and a top that is zero exactly when the run's top is zero.
static void look_for_run(const uint16_t* run, int len);

long fuse_lit(const uint16_t* run, long len, long branch, uint16_t* out) {
    get_ready();
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
        named again;                // a cached answer is used only if its terms still match
        memcpy(tried_run, answer->run, answer->len * 2);
        memcpy(tried_run + answer->len, looked.tail, looked.tail_len * 2);
        names(tried_run, answer->len + looked.tail_len, &again);
        if (same_named(&again, &looked.want)) {
            memcpy(out, answer->run, answer->len * 2);
            return(answer->len);
        }
    }

    // offer_pairs() needs the table of pairs.  Building it discards every
    // term, so it is built here, before the search, and the target is
    // computed again afterwards.
    if (!PAIRS) {
        list_pairs();
        forget();
        names(whole, len + looked.tail_len, &looked.want);
        looked.after_want = known;
    }

    // Any shorter run that is found fits in `out`: cross.fs gave `out`
    // room for the run itself.
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

// Searches for a shorter run with the final state looked.want, which the
// caller has computed.  The result is in looked.best and looked.best_len.
// best_len equals `len` if nothing was found.
static void look_for_run(const uint16_t* run, int len) {
    // the run's variable instructions, fetches and stores
    const uint16_t* variable[2];
    int variables = 0;
    for(int idx=0; idx < len; idx++) {
        if (of_variable(run[idx]) == NO_VARIABLE) continue;
        if (variables == 2) { variables = -1; break; }
        variable[variables++] = &run[idx];
    }
    // The numbers to try: those in the run's final state; those that were
    // on top of the stack along the way, since the final state may use one
    // of them twice; the result of the self-inverse operation on each of
    // those, since an instruction may apply it; and 0, since a number may
    // be pushed only to make room for a cell.
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
    // Candidates are tried by the number of parts in them, fewest first.
    // The parts are: any of the run's variable instructions, each
    // unchanged and used once (one that is left out may be replaced by a
    // number pushed and an instruction that fetches from it or stores to
    // it); one number, or none; and the ALU instructions.
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
        if (count == 2 && looked.best_len == len) offer_pairs();
    }
}

// ---- The one instruction, or the two, equivalent to a run
//
// cross.fs calls fuse_few for a run of ALU instructions and literals, up
// to RUN long: a 0 pushed together with the instructions before it, and a
// run of four or more ALU instructions.  Nothing is searched.  The run is
// stepped on terms, and its final state is looked up among the single
// instructions (find()) and then among the pairs (pair_for()).
// util/survey.c finds the runs of an image that fewer instructions would
// do in the same way.  Returns the number of instructions found, which
// are written to `out`, or -1 if there are none.  The answers are cached
// with fuse_lit's, under keys that start from a different number.
#define FOR_FEW 4                       // fuse_lit's keys start from 2 and 3
long fuse_few(const uint16_t* run, long len, uint16_t* out) {
    get_ready();
    if (len < 2 || len > RUN) return(-1);
    named want, got;
    forget();
    uint64_t is = names(run, len, &want);
    if (!is) return(-1);
    uint64_t asked = FOR_FEW;
    for(int idx=0; idx < len; idx++) asked = mix(asked, run[idx]);
    asked |= 1;
    const lit_answer* answer = lit_kept(asked);
    if (answer->asked) {
        if (answer->len < 0) return(-1);
        names(answer->run, answer->len, &got);  // a cached answer is used only if its terms still match
        if (same_named(&got, &want)) {
            memcpy(out, answer->run, answer->len * 2);
            return(answer->len);
        }
    }
    uint16_t best[2] = { 0, 0 };
    int found = -1, one = find(is);
    if (one >= 0) {
        best[0] = ALU[one].stands;
        names(best, 1, &got);
        if (same_named(&want, &got)) found = 1;
    }
    if (found < 0 && len > 2 && pair_for(run, len, best)) found = 2;
    if (found > 0) {
        tried(run, len, best, found);
        memcpy(out, best, found * 2);
    }
    lit_keep(asked, found, best);
    if (kept_file) {
        fprintf(kept_file, "L %llx %d", (unsigned long long)asked, found);
        for(int idx=0; idx < found; idx++) fprintf(kept_file, " %x", best[idx]);
        fprintf(kept_file, "\n");
        fflush(kept_file);
    }
    return(found);
}

#endif // HEXAFORTH_FUSE_H
