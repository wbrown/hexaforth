//
// loops.h - the loops of an image, for the tools and the tests that have
// this in them.  It has in it cut.h, which cuts an image into its
// straight-line runs and says where a branch lands and where a word starts,
// and kinds.h, which takes the return out of an instruction.
//
// A loop is the instructions from where a branch goes back to, up to the
// branch, within a word.  A jump back to the start of a word, or into
// another word, is a call that will not come back, and no loop.
//
// A turn of a loop is what it does once round.  A loop that tests at the
// top is `begin test while body repeat`: a branch after the test leaves
// the loop when the test left nothing, and a jump at the end goes back.
// One that tests at the bottom is `begin body until`: the branch at the
// end goes back when the body left nothing.  A word the loop calls is put
// in place of the call (placed), if it runs straight to its return and
// leaves alone where it returns to: then what it does in place is what it
// did called, and the turn is the loop with it in place, however long that
// is.  A loop that calls any other word, or branches inside, or returns
// from inside, is neither, and has no turn that is said here.
//

#ifndef HEXAFORTH_LOOPS_H
#define HEXAFORTH_LOOPS_H

#include "kinds.h"                  // first: it has fuse.h, which is had before the rest
#include "cut.h"

typedef struct {
    long head, tail;                    // where it goes back to, and the branch that goes back
} loop;

enum { NO_TURN, TESTS_AT_TOP, TESTS_AT_BOTTOM };       // where a loop tests, if it has a turn

typedef struct {
    int tests_at;                       // one of those
    const uint16_t* test;               // the test, if it is at the top: what is before the branch out
    int tests;
    const uint16_t* body;
    int bodies;
    int placed;                         // the calls the turn makes, each with its word put in place,
    int called;                         // and the instructions that run to call and return, which are not in it
    uint16_t* own;                      // what test and body are in, which whoever took the turn frees
} turn;

// A word put in place of a call to it (put_in_place), at the end of `code`: its
// instructions up to its return, with the return taken out of the last
// (kinds.h, unfolded), and the words it calls put in place too.  A jump
// to a word at the end of it is a call to that word, which returns for it.
// It is to run straight to its return, and to leave alone what the call put
// on the return stack, which is where it returns to: not read it, write
// it, or take it off before its return.  False if it does not.  `in` is the
// words it is being put in place for, which it is not to be one of;
// *called is added to, for each instruction that is not in place.
typedef struct placing { long at; const struct placing* in; } placing;

static int put_in_place(const cut_image* m, long at, const placing* in, uint16_t** code, long* len, long* room, int* called) {
    placing here = { at, in };
    uint16_t exits = ins_named("exit");
    int above = 0;                      // the cells it has put on the return stack, over where it returns to
    for(const placing* p=in; p; p = p->in) if (p->at == at) return(0);
    for(long a=at; a >= 0 && a < m->instructions; a++) {
        uint16_t w = m->code[a];
        instruction ins = ins_of(w);
        if (!in_a_run(w)) {
            long to = goes_to(a, w);
            if (to < 0) return(0);
            ++*called;                  // the call, or the jump
            if (ins.jmp.op_type == OP_TYPE_CALL) {
                if (!put_in_place(m, to, &here, code, len, room, called)) return(0);
                continue;
            }
            if (ins.jmp.op_type == OP_TYPE_JMP && !above && m->lands[to] == 2) return(put_in_place(m, to, &here, code, len, room, called));
            return(0);                  // it branches
        }
        int returned = 0;
        if (!ins.lit.lit_f) {
            if (ins.alu.r_eip && w == exits) { ++*called; return(!above); }     // a return by itself
                                                                                // (one folded in is had for nothing)
            if (ins.alu.r_eip) {
                w = unfolded(w, &returned);
                if (!returned) return(0);
                ins = ins_of(w);
            }
            int moves = ins.alu.rstack;
            if (ins.alu.in_mux == INPUT_R && !above) return(0);
            if (ins.alu.out_mux == OUTPUT_R && above + (moves < 0 ? moves : 0) < 1) return(0);
            if ((above += moves) < 0) return(0);
        }
        ONE_MORE(*code, *len, *room);
        (*code)[(*len)++] = w;
        if (returned) return(!above);
    }
    return(0);
}

// The turn of a loop, which says where it tests; or not, if it has none
// that is said here.  The words it calls are put in place.
static int turn_of(const cut_image* m, const loop* l, turn* t) {
    instruction last = ins_of(m->code[l->tail]);
    uint16_t* code = NULL;
    long len = 0, room = 0, out = -1, out_at = -1, branches = 0;
    int placed = 0, called = 0;
    *t = (turn){ NO_TURN };
    for(long at=l->head; at < l->tail; at++) {
        uint16_t w = m->code[at];
        instruction ins = ins_of(w);
        if (!ins.lit.lit_f && ins.alu.op_type == OP_TYPE_ALU && ins.alu.r_eip) { free(code); return(0); }
        if (!in_a_run(w) && ins.jmp.op_type == OP_TYPE_CALL) {
            long to = goes_to(at, w);
            called++;                   // the call
            if (to < 0 || !put_in_place(m, to, NULL, &code, &len, &room, &called)) { free(code); return(0); }
            placed++;
            continue;
        }
        if (!in_a_run(w)) branches++, out = len, out_at = at;
        ONE_MORE(code, len, room);
        code[len++] = w;
    }
    if (last.jmp.op_type == OP_TYPE_JMP && branches == 1 && ins_of(m->code[out_at]).jmp.op_type == OP_TYPE_CJMP &&
        goes_to(out_at, m->code[out_at]) == l->tail + 1) {
        *t = (turn){ TESTS_AT_TOP, code, (int)out, code + out + 1, (int)(len - out - 1), placed, called, code };
    } else if (last.jmp.op_type == OP_TYPE_CJMP && !branches) {
        *t = (turn){ TESTS_AT_BOTTOM, NULL, 0, code, (int)len, placed, called, code };
    } else {
        free(code);
    }
    return(t->tests_at != NO_TURN);
}

// The loops of an image, in order of where they end: how many there are,
// and the list, which is made here and is as long as that.
static int loops_of(const cut_image* m, loop** list) {
    loop* out = NULL;
    int n = 0;
    for(int filling=0; filling < 2; filling++) {        // they are counted, and then listed
        if (filling) out = calloc(n ? n : 1, sizeof(loop));
        if (filling && !out) { fprintf(stderr, "there is no room for %d loops\n", n); exit(1); }
        n = 0;
        for(long at=0; at < m->instructions; at++) {
            uint16_t w = m->code[at];
            instruction ins = ins_of(w);
            if (!w || in_a_run(w) || ins.jmp.op_type == OP_TYPE_CALL) continue;
            long head = goes_to(at, w);
            if (head < 0 || head > at) continue;
            int starts = 0;             // does a word start in it?
            for(long in=head; in <= at && !starts; in++) starts = m->lands[in] == 2;
            if (starts) continue;
            if (filling) out[n] = (loop){ head, at };
            n++;
        }
    }
    *list = out;
    return(n);
}

// Is a loop inside another: does it start and end within it?
static int loop_within(const loop* inner, const loop* outer) {
    return(inner != outer && inner->head >= outer->head && inner->tail <= outer->tail);
}

// How many instructions a turn runs in its loop, with the branches: one
// that tests at the top has a branch out and a jump back, and one that
// tests at the bottom the one branch; and the calls and returns of the
// words put in place.
static int turn_now(const turn* t) {
    return(t->bodies + t->tests + (t->tests_at == TESTS_AT_TOP ? 2 : 1) + t->called);
}

#endif // HEXAFORTH_LOOPS_H
