//
// loops.c - the loops of an image, for the tools and the tests that have
// this in them.  It is had after cut.c, which cuts an image into its
// straight-line runs and says where a branch lands and where a word starts.
//
// A loop is the instructions from where a branch goes back to, up to the
// branch, within a word.  A jump back to the start of a word, or into
// another word, is a call that will not come back, and no loop.
//
// A turn of a loop is what it does once round.  A loop that tests at the
// top is `begin test while body repeat`: a branch after the test leaves
// the loop when the test left nothing, and a jump at the end goes back.
// One that tests at the bottom is `begin body until`: the branch at the
// end goes back when the body left nothing.  A loop that calls a word, or
// branches inside, or returns from inside, is neither, and has no turn
// that is said here.
//

typedef struct {
    long head, tail;                    // where it goes back to, and the branch that goes back
} loop;

typedef struct {
    const uint16_t* test;               // the test, if it is at the top: what is before the branch out
    int tests;
    const uint16_t* body;
    int bodies;
} turn;

// The loops of an image, in order of where they end: how many there are.
static int loops_of(const cut_image* m, loop* out, int most) {
    int n = 0;
    for(long at=0; at < m->instructions && n < most; at++) {
        uint16_t w = m->code[at];
        instruction ins = ins_of(w);
        if (!w || in_a_run(w) || ins.jmp.op_type == OP_TYPE_CALL) continue;
        long head = goes_to(at, w);
        if (head < 0 || head > at) continue;
        int starts = 0;                 // does a word start in it?
        for(long in=head; in <= at && !starts; in++) starts = m->lands[in] == 2;
        if (!starts) out[n++] = (loop){ head, at };
    }
    return(n);
}

// Is a loop inside another: does it start and end within it?
static int loop_within(const loop* inner, const loop* outer) {
    return(inner != outer && inner->head >= outer->head && inner->tail <= outer->tail);
}

// The turn of a loop: 1 if it tests at the top, 2 if at the bottom, and
// nothing if it is neither.
static int turn_of(const cut_image* m, const loop* l, turn* t) {
    const uint16_t* span = m->code + l->head;
    long len = l->tail - l->head, out = -1, branches = 0;
    instruction last = ins_of(m->code[l->tail]);
    for(long idx=0; idx < len; idx++) {
        instruction ins = ins_of(span[idx]);
        if (!ins.lit.lit_f && ins.alu.op_type == OP_TYPE_ALU && ins.alu.r_eip) return(0);
        if (!in_a_run(span[idx])) branches++, out = idx;
    }
    if (last.jmp.op_type == OP_TYPE_JMP && branches == 1 && ins_of(span[out]).jmp.op_type == OP_TYPE_CJMP &&
        goes_to(l->head + out, span[out]) == l->tail + 1) {
        *t = (turn){ span, (int)out, span + out + 1, (int)(len - out - 1) };
        return(1);
    }
    if (last.jmp.op_type == OP_TYPE_CJMP && !branches) {
        *t = (turn){ NULL, 0, span, (int)len };
        return(2);
    }
    return(0);
}
