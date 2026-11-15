//
// fuse.c - which instruction does the work of two or of three, and which
// two do the work of three, asked of the machine.
//
// A sequence of instructions is run on vm.c from a number of states, and
// what it leaves is noted: the depths, the stacks, and every cell of data.
// Two sequences that leave the same from every state are taken to do the
// same.  Nothing here says what an instruction does: the machine is run.
//
// That is no proof, and running is trusted only so far.  A state is cells
// of numbers taken by chance, and what an instruction makes of them that
// is new (a sum, a flag) is like nothing else in the state: `xor =` leaves
// what an instruction that makes nothing would, since no state has a cell
// that is the xor of two others.  What chance does show is where things
// go.  So of the instructions of a sequence at most one may make anything
// new, and the rest only move what is there, which is also found by
// running them (only_moves).  Then all the sequence does is put cells in
// other places and do one thing to them, and the states tell which cells
// and which thing.
//
// The machine is the strict one (STRICT_ALIGN, ROM_BYTES), which stops at
// a fetch or a store outside memory and at a store into its ROM: what
// those do is not defined (MODEL.md).  A state that stops a sequence says
// nothing of it but that it stopped, and a sequence that few states run to
// the end of is not known well enough to be given an answer.  Memory is 256
// cells, so that all of it can be looked at after every run; the code is in
// the ROM.
//
// Not asked: anything that returns or does IO, of which nothing is said.
// cross.fs folds a return in for itself.
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
#define DATA_CELLS  64                              // the cells that are not ROM,
#define DATA        ((1 << ADDR_BITS) - DATA_CELLS) // and the first of them
#define SEQ         3               // the most instructions in a sequence
#define CODE_CELL   160             // where the sequence is.  No cell of the code
                                    // is one that an AND, an OR or an XOR of two
                                    // addresses of data is the address of
#define DEPTH       8               // of each stack, when the sequence starts
#define FILLED      16              // cells of each stack that a state gives
#define STATES      96
#define FEW         4               // the states tried first
#define ENOUGH      8               // a sequence that runs to its end from fewer
                                    // states than this is not known well enough

#define STRICT_ALIGN
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
// data; a small number, which added to an address is mostly still one; and
// anything.
static int64_t address(void) { return(DATA + chance() % DATA_CELLS); }
static int64_t small(void)   { return(chance() % 16); }
static int64_t anything(void) { return(cell(chance())); }

// A value for a cell of a state: half of them one of a few the state has,
// so that cells are often the same as each other, and the rest of the
// three kinds alike.  A plain state has no cell of just anything, so that
// what fetches and stores mostly runs from it to its end.
static int64_t value(const int64_t* few, int plain) {
    if (chance() & 1) return(few[chance() % 4]);
    switch (chance() % 3) {
        case 0:  return(address());
        case 1:  return(small());
        default: return(plain ? address() : anything());
    }
}

// The first few states are plain: they are the ones tried first.
static void make_states(void) {
    for(int s=0; s < STATES; s++) {
        int plain = s < FEW;
        int64_t few[4] = { address(), address(), small(), plain ? small() : anything() };
        for(int idx=0; idx < FILLED; idx++) STATE[s].dstack[idx] = value(few, plain);
        for(int idx=0; idx < FILLED; idx++) STATE[s].rstack[idx] = value(few, plain);
        for(int idx=0; idx < DATA_CELLS; idx++) STATE[s].data[idx] = value(few, plain);
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

// What sets the two depths and goes to the sequence: the same for all.
static void lay_prefix(void) {
    instruction io_store;
    lookup_word(FORTH_WORDS, "io!", &io_store);
    uint16_t* m = machine.memory;
    *m++ = push(DEPTH);  *m++ = push(IO_RSP);  *m++ = *(uint16_t*)&io_store;
    *m++ = push(DEPTH);  *m++ = push(IO_SP);   *m++ = *(uint16_t*)&io_store;
    *m++ = jump(CODE_CELL * (CELL_BYTES / 2));
}

static uint64_t mix(uint64_t h, uint64_t x) {
    h ^= x;
    h *= 0x100000001b3ull;
    h ^= h >> 29;
    return(h);
}

#define STOPPED 1

// Run a sequence from a state.  What it left is in the machine; false if
// the machine stopped.
static int run(const uint16_t* seq, int len, int s) {
    uint16_t* code = machine.memory + CODE_CELL * (CELL_BYTES / 2);
    memset(code, 0, (SEQ + 1) * 2);
    memcpy(code, seq, len * 2);
    memcpy(machine.DSTACK, STATE[s].dstack, sizeof(STATE[s].dstack));
    memcpy(machine.RSTACK, STATE[s].rstack, sizeof(STATE[s].rstack));
    for(int idx=0; idx < DATA_CELLS; idx++) {
        memcpy(cell_at(&machine, DATA + idx), &STATE[s].data[idx], CELL_BYTES);
    }
    machine.EIP = 0;
    if (setjmp(stopped)) return(0);
    vm(&machine);
    return(1);
}

// A number for what a sequence leaves from a state.  `ran` counts those
// that ran to the end.
static int ran;
static uint64_t left(const uint16_t* seq, int len, int s) {
    if (!run(seq, len, s)) return(STOPPED);
    ran++;
    uint64_t h = mix(mix(0xcbf29ce484222325ull, machine.SP), machine.RSP);
    for(int idx=0; idx < machine.SP; idx++) h = mix(h, machine.DSTACK[idx]);
    for(int idx=0; idx < machine.RSP; idx++) h = mix(h, machine.RSTACK[idx]);
    for(int idx=0; idx < DATA_CELLS; idx++) h = mix(h, mem_load(&machine, DATA + idx));
    return(h | 2);
}

// And from the states `from` up to `to`, carrying on from h.
static uint64_t leaves(const uint16_t* seq, int len, int from, int to, uint64_t h) {
    for(int s=from; s < to; s++) h = mix(h, left(seq, len, s));
    return(h);
}

// Was a value there before the sequence ran: in a stack, in the data, or
// in the ROM?
static int was_there(int64_t v, int s) {
    if (v == DEPTH) return(1);
    for(int idx=0; idx < FILLED; idx++) {
        if (STATE[s].dstack[idx] == v || STATE[s].rstack[idx] == v) return(1);
    }
    for(int idx=0; idx < DATA_CELLS; idx++) if (STATE[s].data[idx] == v) return(1);
    for(int idx=0; idx < DATA; idx++) if (mem_load(&machine, idx) == v) return(1);
    return(0);
}

// Does an instruction only move what is there, and make nothing new?  From
// every state it runs from, every cell it leaves holds what some cell held.
static int only_moves(uint16_t ins) {
    int ran = 0;
    for(int s=0; s < STATES; s++) {
        if (!run(&ins, 1, s)) continue;
        ran++;
        for(int idx=0; idx < machine.SP; idx++) if (!was_there(machine.DSTACK[idx], s)) return(0);
        for(int idx=0; idx < machine.RSP; idx++) if (!was_there(machine.RSTACK[idx], s)) return(0);
        for(int idx=0; idx < DATA_CELLS; idx++) {
            if (!was_there(mem_load(&machine, DATA + idx), s)) return(0);
        }
    }
    return(ran > 0);
}

// The instructions asked about: those of the ALU that do no IO and do not
// return.  Each has what it leaves, from the first few states and from all
// of them; whether it only moves; and the one that stands for all that do
// the same, which is a baseword if any of them is.
typedef struct {
    uint16_t ins;
    uint64_t few, all;
    int moves;
    uint16_t stands;
} single;

static single ALU[4096];
static int alus;
static int alu_at[65536];           // where an instruction is in ALU, or -1
static int by_few[4096], by_all[4096];

static void list_alu(void) {
    memset(alu_at, -1, sizeof(alu_at));
    for(int in=0; in < 4; in++)
    for(int op=0; op < 16; op++)
    for(int out=0; out < 4; out++)
    for(int d=-2; d < 2; d++)
    for(int r=-2; r < 2; r++) {
        if (op == ALU_IO_READ || out == OUTPUT_IO_T) continue;
        instruction ins = {0};
        ins.alu.op_type = OP_TYPE_ALU;
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
        ALU[alus].moves = only_moves(x);
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

static int ready = 0;
static void get_ready(void) {
    init_opcodes(FORTH_WORDS);
    make_states();
    lay_prefix();
    nowhere = fopen("/dev/null", "w");
    list_alu();
    sort_alu();
    ready = 1;
}

// The instruction that does the work of a sequence of two or three (the
// first is nothing for two), or nothing if none is known to.
long fuse_one(long first, long second, long third) {
    if (!ready) get_ready();
    uint16_t seq[SEQ];
    int len = 0, makes = 0;
    if (first) seq[len++] = first;
    seq[len++] = second;
    seq[len++] = third;
    for(int idx=0; idx < len; idx++) {
        if (alu_at[seq[idx]] < 0) return(0);
        makes += !ALU[alu_at[seq[idx]]].moves;
    }
    if (makes > 1) return(0);
    ran = 0;
    uint64_t h = leaves(seq, len, 0, FEW, 0);
    if (find(by_few, 0, h) < 0) return(0);
    int one = find(by_all, 1, leaves(seq, len, FEW, STATES, h));
    return(one < 0 || ran < ENOUGH ? 0 : ALU[one].stands);
}

// The pairs that may do the work of three: of the instructions that stand
// for the rest, every two of which at most one makes anything new, with
// what the pair leaves from the first few states, and in order of that.
// They are made the first time they are asked for.
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

static void list_pairs(void) {
    PAIRS = malloc((size_t)alus * alus * sizeof(pair));
    for(int a=0; a < alus; a++) {
        if (ALU[a].stands != ALU[a].ins) continue;
        for(int b=0; b < alus; b++) {
            if (ALU[b].stands != ALU[b].ins) continue;
            if (!ALU[a].moves && !ALU[b].moves) continue;
            pair* p = &PAIRS[pairs++];
            p->seq[0] = ALU[a].ins;
            p->seq[1] = ALU[b].ins;
            p->known = 0;
            ran = 0;
            p->few = leaves(p->seq, 2, 0, FEW, 0);
            if (!ran) pairs--;              // it stopped every time: nothing is known of it
        }
    }
    qsort(PAIRS, pairs, sizeof(pair), pair_order);
}

// The two instructions that do the work of three, the first in the upper
// sixteen bits, or nothing if none are known to.
long fuse_two(long first, long second, long third) {
    if (!ready) get_ready();
    uint16_t seq[SEQ] = { first, second, third };
    int makes = 0;
    for(int idx=0; idx < SEQ; idx++) {
        if (alu_at[seq[idx]] < 0) return(0);
        makes += !ALU[alu_at[seq[idx]]].moves;
    }
    if (makes > 1) return(0);
    if (!PAIRS) list_pairs();

    ran = 0;
    uint64_t few = leaves(seq, SEQ, 0, FEW, 0);
    long lo = 0, hi = pairs;
    while (lo < hi) {
        long mid = (lo + hi) / 2;
        if (PAIRS[mid].few < few) lo = mid + 1; else hi = mid;
    }
    if (lo == pairs || PAIRS[lo].few != few) return(0);
    uint64_t all = leaves(seq, SEQ, FEW, STATES, few);
    if (ran < ENOUGH) return(0);
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
        int moved = 0;
        for(int idx=0; idx < 2; idx++) {
            instruction ins = *(instruction*)&p->seq[idx];
            moved += abs(ins.alu.dstack) + 4 * abs(ins.alu.rstack);
        }
        if (!best || moved < least) {
            best = (long)p->seq[0] << 16 | p->seq[1];
            least = moved;
        }
    }
    return(best);
}
