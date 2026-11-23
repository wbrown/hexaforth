//
// loops.h - finds the loops of an image, for the tools and tests that
// include it.  It includes cut.h, which cuts an image into straight-line
// runs and records where branches land and where words start, and
// kinds.h, which removes a folded return from an instruction.
//
// A loop is the instructions from the target of a backward branch up to
// the branch, inside one word.  A jump back to the start of a word, or
// into another word, is a tail call and not a loop.
//
// A turn is one pass of a loop.  A loop that tests at the top has the form
// `begin test while body repeat`: a branch after the test leaves the loop
// when the test gave zero, and a jump at the end goes back.  A loop that
// tests at the bottom has the form `begin body until`: the branch at the
// end goes back when the body gave zero.
//
// A word the loop calls is inlined in place of the call if it runs
// straight to its return and does not touch its return address.  Inlined
// that way it does what it did when called, and the turn is the loop with
// the word inlined, however long that makes it.  A loop that calls any
// other kind of word, or branches inside, or returns from inside, has no
// turn that this file can give.
//

#ifndef HEXAFORTH_LOOPS_H
#define HEXAFORTH_LOOPS_H

#include "kinds.h"                  // first: it includes fuse.h, which must come before the other headers
#include "cut.h"

typedef struct {
    long head, tail;                    // the address it branches back to, and the address of the backward branch
} loop;

enum { NO_TURN, TESTS_AT_TOP, TESTS_AT_BOTTOM };       // where a loop tests, if it has a turn

typedef struct {
    int tests_at;                       // NO_TURN, TESTS_AT_TOP or TESTS_AT_BOTTOM
    const uint16_t* test;               // the test, if it is at the top: the instructions before the branch out
    int tests;                          // the number of instructions in the test
    const uint16_t* body;
    int bodies;                         // the number of instructions in the body
    int placed;                         // the number of calls in the turn, each replaced by its inlined word
    int called;                         // the calls, jumps and returns that run in the image but are not in the turn's code
    uint16_t* own;                      // the buffer `test` and `body` point into, which the caller frees
} turn;

// Appends a word to `code`, inlined in place of a call to it: its
// instructions up to its return, with the return removed from the last one
// (unfolded(), kinds.h), and with the words it calls inlined too.  A jump
// to another word at its end is a tail call: that word is inlined, and its
// return ends this one.
//
// The word must run straight to its return and must leave its return
// address alone: it may not read it, overwrite it, or pop it before
// returning.  Returns 0 if the word breaks either rule.  `in` is the chain
// of words currently being inlined, which this word must not be in, so
// that recursion is refused.  `*called` is increased by one for each call,
// jump or return that is removed.
typedef struct placing { long at; const struct placing* in; } placing;

static int put_in_place(const cut_image* m, long at, const placing* in, uint16_t** code, long* len, long* room, int* called) {
    placing here = { at, in };
    uint16_t exits = ins_named("exit");
    int above = 0;                      // the number of cells it has pushed on the return stack, above its return address
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
            if (ins.alu.r_eip && w == exits) { ++*called; return(!above); }     // a return on its own
                                                                                // (a folded return costs no instruction)
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

// Finds the turn of a loop, and records where it tests.  The words the
// loop calls are inlined.  Returns 0 if the loop has no turn this file can
// give.
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

// Lists the loops of an image, in order of the address where each ends.
// Returns their number and allocates the list.
static int loops_of(const cut_image* m, loop** list) {
    loop* out = NULL;
    int n = 0;
    for(int filling=0; filling < 2; filling++) {        // the first pass counts them, the second lists them
        if (filling) out = calloc(n ? n : 1, sizeof(loop));
        if (filling && !out) { fprintf(stderr, "there is no room for %d loops\n", n); exit(1); }
        n = 0;
        for(long at=0; at < m->instructions; at++) {
            uint16_t w = m->code[at];
            instruction ins = ins_of(w);
            if (!w || in_a_run(w) || ins.jmp.op_type == OP_TYPE_CALL) continue;
            long head = goes_to(at, w);
            if (head < 0 || head > at) continue;
            int starts = 0;             // set if a word starts inside it
            for(long in=head; in <= at && !starts; in++) starts = m->lands[in] == 2;
            if (starts) continue;
            if (filling) out[n] = (loop){ head, at };
            n++;
        }
    }
    *list = out;
    return(n);
}

// Returns 1 if a loop is inside another: it starts and ends within it.
static int loop_within(const loop* inner, const loop* outer) {
    return(inner != outer && inner->head >= outer->head && inner->tail <= outer->tail);
}

// Returns the number of instructions one turn runs in the image.  That is
// the turn's code, plus its branches (a branch out and a jump back for a
// loop that tests at the top, one branch for a loop that tests at the
// bottom), plus the calls and returns of the inlined words.
static int turn_now(const turn* t) {
    return(t->bodies + t->tests + (t->tests_at == TESTS_AT_TOP ? 2 : 1) + t->called);
}

#endif // HEXAFORTH_LOOPS_H
