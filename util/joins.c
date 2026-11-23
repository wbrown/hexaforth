//
// joins.c - generates forth/folds.fs, the table of rules that the
// nucleus's peephole (forth/join.fs) reads.  Every entry is computed by
// the machine's step.  The table is data: numbers of sixteen bits, which
// the nucleus reads with `i@`.
//
//   joins forth/folds.fs
//
// The nucleus runs on the machine, so it finds what an instruction does
// to known numbers by running the instruction.  Running cannot tell it
// which instructions are safe to run that way: the ones that compute
// their result from the top of the data stack, or from the top two cells,
// and change nothing else.  This program lists them.  It considers only
// the instructions that have a name (vm_opcodes.h), because those are the
// ones a person types.
//
// Two named instructions compute a number from no stack cell at all: the
// cell under the top compared with itself (`true`, -1) and xored with
// itself (`false`, 0).  Where the nucleus looks for a number, either one
// counts as a number.  They are not in the table.  The generated file
// stores them in two cells of the nucleus (`of-none`, join.fs), each of
// which takes one instruction to fetch.
//
// The table also has rules for moving a push across a `dup`.  `dup 16
// rshift` is a dup, a push and an instruction, and it is equivalent to
// the push followed by one other instruction.  A rule finds that other
// instruction from bits: an instruction that matches the rule's mask and
// value, after a dup and a push, is equivalent to the push followed by the
// same instruction with the rule's bits inverted.  There is a set of rules
// for `dup`, one for `r@` and one for `r>` (ACROSS): `r> 8 rshift` is
// equivalent to pushing 8 and then a shift that takes its input from the
// return stack.  A rule holds for every instruction it matches and for
// anything pushed: a number, a variable, a number that takes two
// instructions (a push and an add), or `false` or `true`, which the
// nucleus compiles for 0 and -1.  To establish that, the step runs with a
// number it knows nothing about (fuse.h, any_number).  Each rule is then
// run on vm.c with the named instructions it was derived from and each
// kind of push, like every other answer from fuse.h.
//
// The table's last part is for a 0 that the nucleus compiled as `false`.
// It has two lists of sets.  The first is for `0 =` after an instruction
// X: the machine gives the two instructions equivalent to X, `false` and
// `=`.  After an instruction that leaves a flag those two are X and
// `invert`, and after a fetch they are a fetch that xors and `over =`.
// The second list is for an instruction, `false` and X, such as
// `over 0 <`.  In both lists a rule matches X by a mask and a value and
// gives the bits to invert in X.  The comments at ZEROS and BEFORES
// describe the sets.
//
// This file does not encode what any instruction does or what its fields
// mean.  All of that comes from the step (vm_step.h), which fuse.h runs on
// terms.
//
// Before generating anything, it checks vm_opcodes.h against the machine.
// A named instruction must be the representative of the instructions
// equivalent to it (fuse.h), and the instruction given for a phrase must
// do what the phrase's words do in sequence.  If a name fails either
// check, it prints which one and stops.
//
#include "fuse.h"

#define MOST COUNT(FORTH_WORDS)         // the number of words in vm_opcodes.h: an upper bound on the named
                                        // instructions, and on the rules, since each rule starts from one
static uint16_t NAMED[MOST];            // the named ALU instructions that do not return
static const char* NAME_OF[MOST];       // the name of each
static int named_ones;

static void list_named(void) {
    for(word_node* w = FORTH_WORDS; w->repr && strlen(w->repr); w++) {
        if (w->num_ins != 1) continue;
        uint16_t ins = *(uint16_t*)&w->ins[0];
        if (alu_at[ins] < 0 || returns(ins)) continue;
        int at = 0;
        while (at < named_ones && NAMED[at] != ins) at++;
        if (at < named_ones) continue;
        NAMED[named_ones] = ins;
        NAME_OF[named_ones++] = w->repr;
    }
}

// Returns 1 if a term is built only from numbers and the allowed starting
// cells.  `allowed` is a bit mask: 1 allows the cell that was on top of
// the data stack, and 2 the cell under it.
static int made_only_of(int t, int allowed) {
    const term* x = &KNOWN[t];
    if (x->kind == A_NUMBER) return(1);
    if (x->kind == IN_DATA) {
        if (x->number == DEEP - 1) return(allowed & 1);
        if (x->number == DEEP - 2) return((allowed & 2) != 0);
        return(0);
    }
    if (x->kind != BY_ALU) return(0);
    for(int idx=0; idx < x->n; idx++) if (!made_only_of(x->of[idx], allowed)) return(0);
    return(1);
}

// Returns how many data stack cells an instruction computes its result
// from, if it changes nothing else: 0 (OF_NONE), 1 or 2.  Returns NOT_SUCH
// for any other instruction.  An instruction that uses no cell pushes its
// result, which is built from numbers alone.
enum { NOT_SUCH = -1, OF_NONE = 0 };
#define KEPT_OF_NONE 2                  // the number of cells the nucleus has for the OF_NONE instructions (of-none, join.fs)
static int folds(uint16_t ins) {
    named m;
    forget();
    start(&m);
    step_named(&m, ins);
    if (m.unsaid || lost || m.next >= 0 || m.fetches || m.M != MEMORY_TERM) return(NOT_SUCH);
    if (m.RSP != DEEP || m.R != RETURN_TERM(DEEP - 1)) return(NOT_SUCH);
    for(int idx=0; idx < DEEP - 1; idx++) if (m.RS[idx] != RETURN_TERM(idx)) return(NOT_SUCH);
    int of = DEEP + 1 - m.SP;
    if (of < OF_NONE || of > 2) return(NOT_SUCH);
    for(int idx=0; idx < m.SP - 1; idx++) if (m.DS[idx] != DATA_TERM(idx)) return(NOT_SUCH);
    return(made_only_of(m.T, of == 2 ? 3 : of) ? of : NOT_SUCH);
}

// The named instructions that compute a number from no stack cell
// (`false` and `true`).  The nucleus compiles 0 and -1 as these two.
// main() puts the one that pushes 0 last.
static uint16_t OF_NONE_INS[MOST];
static int nones;

// Writes the instructions of one kind of push to `out`, and returns how
// many there are.  The kinds are a literal number, a fetch of a variable,
// a number that takes two instructions (a literal, then a literal that
// adds), and each of the OF_NONE_INS instructions.  While any_number is
// set, the step treats the number and the variable's address as unknown.
enum { A_NUMBER_PUSHED, A_VARIABLE, A_NUMBER_OF_TWO, LITERAL_PUSHES };
#define PUSHED (LITERAL_PUSHES + nones)
static int a_push(int what, uint16_t* out) {
    if (what >= LITERAL_PUSHES) {
        out[0] = OF_NONE_INS[what - LITERAL_PUSHES];
        return(1);
    }
    instruction ins = {0};
    ins.lit.lit_f = true;
    ins.lit.lit_v = 1;
    if (what == A_VARIABLE) ins.lit.lit_shifts = 3;
    out[0] = *(uint16_t*)&ins;
    if (what != A_NUMBER_OF_TWO) return(1);
    ins.lit.lit_add = true;
    out[1] = *(uint16_t*)&ins;
    return(2);
}

// Builds the two sequences a rule equates.  `before` is `across`, a push
// and p.  `after` is the same push and y.  Their lengths are stored in
// `*blen` and `*alen`.
static void crossing(uint16_t across, uint16_t p, uint16_t y, int what, uint16_t* before, int* blen, uint16_t* after, int* alen) {
    before[0] = across;
    *blen = 1 + a_push(what, before + 1);
    before[(*blen)++] = p;
    *alen = a_push(what, after);
    after[(*alen)++] = y;
}

// Returns 1 if `across`, a push and p are equivalent to the push and y,
// for every kind of push.
static int crosses(uint16_t across, uint16_t p, uint16_t y) {
    if (alu_at[y] < 0 || returns(y)) return(0);
    int is = 1;
    any_number = 1;
    for(int what=0; what < PUSHED && is; what++) {
        uint16_t before[4], after[3];
        int blen, alen;
        crossing(across, p, y, what, before, &blen, after, &alen);
        named a, b;
        forget();
        uint64_t left = names(before, blen, &a);
        is = left && names(after, alen, &b) == left && same_named(&a, &b);
    }
    any_number = 0;
    return(is);
}

// Returns the instruction y for which `across`, a push and p are
// equivalent to the push and y, or 0 if there is none.
static uint16_t crossed(uint16_t across, uint16_t p) {
    for(int at=0; at < alus; at++) {
        uint16_t y = ALU[at].ins;
        if (ALU[at].stands == y && crosses(across, p, y)) return(y);
    }
    return(0);
}

// The rules are grouped in sets, one set for each instruction a push can
// be moved across.  Each of those instructions (ACROSS) puts on top of the
// data stack the value that the instruction after the push operates on.
typedef struct { uint16_t mask, is, change; } rule;
typedef struct {
    uint16_t across;
    rule rules[MOST];
    int n;
} rule_set;
static const char* const ACROSS[] = { "dup", "r@", "r>" };
static rule_set SET[COUNT(ACROSS)];
static int sets;

// Returns 1 if the nucleus's peephole can be given this instruction: it is
// an ALU instruction and does not return.  The step on terms does not
// model some of these (the IO instructions), and no rule may match one of
// those.
static int of_alu(uint16_t x) {
    instruction ins = *(instruction*)&x;
    return(!ins.lit.lit_f && ins.alu.op_type == OP_TYPE_ALU && !ins.alu.r_eip);
}

static int right_for_all(uint16_t across, uint16_t mask, uint16_t is, uint16_t change) {
    for(int word=0; word < EVERY_WORD; word++) {
        if (!of_alu(word) || (word & mask) != is) continue;
        if (alu_at[word] < 0 || !crosses(across, word, word ^ change)) return(0);
    }
    return(1);
}

// Sets the mask and value of a rule that starts from the instruction p.
// At first the rule matches only p.  Its mask is then cleared one bit at a
// time, and a bit stays cleared only while `holds` is true.  `holds` is an
// expression of `wider`, the mask being tried.  It must be true only if
// the rule holds for every instruction that `wider` matches.
#define WIDEN(new, p, holds) do { \
        (new)->mask = 0xffff; \
        for(int bit=0; bit < 8 * (int)sizeof(instruction); bit++) { \
            uint16_t wider = (new)->mask & ~(1 << bit); \
            if (holds) (new)->mask = wider; \
        } \
        (new)->is = (p) & (new)->mask; \
    } while (0)

// Finds a set's rules.  Each rule starts from one named instruction that
// has an equivalent after the push (WIDEN).
static void find_rules(rule_set* s) {
    uint16_t across = s->across;
    for(int at=0; at < named_ones; at++) {
        uint16_t p = NAMED[at], y = crossed(across, p);
        if (!y) continue;
        int taken = 0;
        for(int r=0; r < s->n; r++) taken |= (p & s->rules[r].mask) == s->rules[r].is;
        if (taken) continue;
        rule* new = &s->rules[s->n++];
        new->change = p ^ y;
        WIDEN(new, p, right_for_all(across, wider, p & wider, new->change));
    }
    // Run each rule on vm.c with every named instruction it matches.
    for(int at=0; at < named_ones; at++) {
        uint16_t p = NAMED[at];
        for(int r=0; r < s->n; r++) {
            if ((p & s->rules[r].mask) != s->rules[r].is) continue;
            for(int what=0; what < PUSHED; what++) {
                uint16_t before[4], after[3];
                int blen, alen;
                crossing(across, p, p ^ s->rules[r].change, what, before, &blen, after, &alen);
                tried(before, blen, after, alen);
            }
            break;
        }
    }
}

// Returns the instruction of OF_NONE_INS that pushes 0, or 0 if none does.
// It is the one that the step finds equivalent to a literal 0.  The
// nucleus compiles a 0 as this instruction, so the rules that involve a 0
// are found and tried with it.
static uint16_t ZERO;
static uint16_t push_of_zero(void) {
    instruction ins = {0};
    ins.lit.lit_f = true;
    uint16_t literal = *(uint16_t*)&ins;
    for(int at=0; at < nones; at++) {
        named a, b;
        forget();
        uint64_t left = names(&literal, 1, &a);
        if (left && names(&OF_NONE_INS[at], 1, &b) == left && same_named(&a, &b)) return(OF_NONE_INS[at]);
    }
    return(0);
}

// Returns the single instruction equivalent to a run, or 0 if the run is
// not equivalent to one instruction.
static uint16_t one_for(const uint16_t* run, int len) {
    named a, b;
    forget();
    uint64_t left = names(run, len, &a);
    if (!left) return(0);
    int found = find(left);
    if (found < 0) return(0);
    uint16_t one = ALU[found].stands;
    names(&one, 1, &b);
    return(same_named(&a, &b) ? one : 0);
}

// Returns 1 if two runs are equivalent on the step.
static int same_runs(const uint16_t* a, int alen, const uint16_t* b, int blen) {
    named x, y;
    forget();
    uint64_t left = names(a, alen, &x);
    return(left && names(b, blen, &y) == left && same_named(&x, &y));
}

// Returns 1 if the nucleus joins part of a run before it has the whole
// run: two or three adjacent instructions of the run are equivalent to one
// instruction.  The nucleus then never has the run as it is written, and a
// rule for the run would never apply.
static int joined_first(const uint16_t* run, int len) {
    for(int part=2; part <= 3; part++) {
        for(int at=0; at + part <= len; at++) if (one_for(run + at, part)) return(1);
    }
    return(0);
}

// The sets of rules for `0 =` after an instruction X.  For each named X,
// the machine gives the two instructions equivalent to X, `false` and `=`
// (pair_for(), fuse.h).  The second of the two is the set's `head`, and
// the rule's `change` is the bits that differ between X and the first.  So
// a rule says: an instruction that matches, followed by `false` and `=`,
// is equivalent to the instruction with those bits inverted, followed by
// the set's head.  The instructions that leave a flag are the set whose
// head is `invert`: their rules change no bits.
//
// A set can have one fixed instruction between X and `false` (BETWEEN).
// After an instruction that leaves a flag, `dup`, `false` and `=` are
// equivalent to `dup invert`.  The sets with an instruction between come
// first in ZEROS and in the table, and the nucleus tries the sets in that
// order, so it prefers the rule that replaces the longer run.
typedef struct {
    uint16_t head, between;             // `between` is 0 if `false` follows X directly
    rule rules[MOST];
    int n;
} zero_set;
static const char* const BETWEEN[] = { "dup" };
static zero_set ZEROS[MOST * (1 + COUNT(BETWEEN))];
static int zero_sets;

// Writes to `run` the run that a rule of a set replaces: X, the set's
// `between` if it has one, `false` and `=`.  Returns the run's length.
#define ZERO_RUN 4                      // the longest such run
static int zero_run(const zero_set* s, uint16_t x, uint16_t same, uint16_t* run) {
    int len = 0;
    run[len++] = x;
    if (s->between) run[len++] = s->between;
    run[len++] = ZERO;
    run[len++] = same;
    return(len);
}

// Returns 1 if a rule of a set with this `change` holds for X.
static int zero_rule(const zero_set* s, uint16_t x, uint16_t change, uint16_t same) {
    uint16_t y = x ^ change;
    if (alu_at[x] < 0 || returns(x) || alu_at[y] < 0 || returns(y)) return(0);
    uint16_t before[ZERO_RUN], after[2] = { y, s->head };
    return(same_runs(before, zero_run(s, x, same, before), after, 2));
}

static int zero_rule_for_all(const zero_set* s, uint16_t mask, uint16_t is, uint16_t change, uint16_t same) {
    for(int word=0; word < EVERY_WORD; word++) {
        if (!of_alu(word) || (word & mask) != is) continue;
        if (!zero_rule(s, word, change, same)) return(0);
    }
    return(1);
}

// Returns the first rule that matches an instruction, among the sets with
// one `between`, and stores the rule's set in `*in`.  Returns NULL if no
// rule matches.  The nucleus applies the first rule that matches, in the
// same order.
static const rule* zero_rule_of(uint16_t between, uint16_t p, const zero_set** in) {
    for(int s=0; s < zero_sets; s++) {
        if (ZEROS[s].between != between) continue;
        for(int r=0; r < ZEROS[s].n; r++) {
            if ((p & ZEROS[s].rules[r].mask) != ZEROS[s].rules[r].is) continue;
            *in = &ZEROS[s];
            return(&ZEROS[s].rules[r]);
        }
    }
    return(NULL);
}

// Returns the instruction which, after `first`, is equivalent to a run, or
// 0 if there is none.
static uint16_t one_after(uint16_t first, const uint16_t* run, int len) {
    for(int at=0; at < alus; at++) {
        uint16_t two[2] = { first, ALU[at].ins };
        if (ALU[at].stands == two[1] && !returns(two[1]) && same_runs(run, len, two, 2)) return(two[1]);
    }
    return(0);
}

// Finds the sets with one `between` instruction, or with none if
// `between` is 0, and adds them to ZEROS.  Where X can stay as it is, the
// rule leaves it so (one_after()).  Otherwise the rule comes from the two
// instructions the machine gives for the whole run.
static void find_zeros(uint16_t between, uint16_t same) {
    const int first = zero_sets;        // the first set with this `between`
    const zero_set shape = { .between = between };
    for(int at=0; at < named_ones; at++) {
        uint16_t p = NAMED[at], run[ZERO_RUN], best[2];
        const zero_set* in;
        int len = zero_run(&shape, p, same, run);
        if (zero_rule_of(between, p, &in) || joined_first(run, len)) continue;
        best[0] = p;
        best[1] = one_after(p, run, len);
        if (!best[1] && !pair_for(run, len, best)) continue;
        zero_set* s = NULL;
        for(int k=first; k < zero_sets; k++) if (ZEROS[k].head == best[1]) s = &ZEROS[k];
        if (!s) {
            s = &ZEROS[zero_sets++];
            s->head = best[1];
            s->between = between;
        }
        rule* new = &s->rules[s->n++];
        new->change = p ^ best[0];
        WIDEN(new, p, zero_rule_for_all(s, wider, p & wider, new->change, same));
    }
    // Run each rule on vm.c with every named instruction it applies to.
    for(int at=0; at < named_ones; at++) {
        uint16_t p = NAMED[at], before[ZERO_RUN];
        const zero_set* in;
        const rule* r = zero_rule_of(between, p, &in);
        if (!r) continue;
        uint16_t after[2] = { p ^ r->change, in->head };
        tried(before, zero_run(in, p, same, before), after, 2);
    }
}

// The sets of rules for an instruction A, `false` and a third instruction
// p.  For each A in BEFORE_ZERO and each named p, the machine gives the two
// instructions equivalent to the three.  The first of the two is the set
// the rule goes in, and the rule's `change` is the bits that differ
// between p and the second.  So a rule says: A and `false`, followed by an
// instruction that matches, are equivalent to the set's instruction
// followed by the matching instruction with those bits inverted.
//
// p is skipped if a rule for `0 =` already applies to A and p is `=`: the
// nucleus tries those rules first.
typedef struct {
    uint16_t before, head;              // A, and the instruction that replaces A and `false`
    rule rules[MOST];
    int n;
} before_set;
static const char* const BEFORE_ZERO[] = { "over" };
static before_set BEFORES[MOST * COUNT(BEFORE_ZERO)];
static int before_sets;

static int before_rule(uint16_t a, uint16_t head, uint16_t p, uint16_t change) {
    uint16_t y = p ^ change;
    if (alu_at[p] < 0 || returns(p) || alu_at[y] < 0 || returns(y)) return(0);
    uint16_t was[3] = { a, ZERO, p }, is[2] = { head, y };
    return(same_runs(was, 3, is, 2));
}

static int before_rule_for_all(uint16_t a, uint16_t head, uint16_t mask, uint16_t is, uint16_t change) {
    for(int word=0; word < EVERY_WORD; word++) {
        if (!of_alu(word) || (word & mask) != is) continue;
        if (!before_rule(a, head, word, change)) return(0);
    }
    return(1);
}

// Returns the first rule of the BEFORES sets for A that matches p, and
// stores the rule's set in `*in`.  Returns NULL if no rule matches.
static const rule* before_rule_of(uint16_t a, uint16_t p, const before_set** in) {
    for(int s=0; s < before_sets; s++) {
        if (BEFORES[s].before != a) continue;
        for(int r=0; r < BEFORES[s].n; r++) {
            if ((p & BEFORES[s].rules[r].mask) != BEFORES[s].rules[r].is) continue;
            *in = &BEFORES[s];
            return(&BEFORES[s].rules[r]);
        }
    }
    return(NULL);
}

// Returns 0 if a name in BEFORE_ZERO is not one instruction.
static int find_befores(uint16_t same) {
    for(int k=0; k < COUNT(BEFORE_ZERO); k++) {
        uint16_t a = ins_named(BEFORE_ZERO[k]);
        if (!a) {
            fprintf(stderr, "joins: %s is not one instruction\n", BEFORE_ZERO[k]);
            return(0);
        }
        const zero_set* zero_in;
        for(int at=0; at < named_ones; at++) {
            uint16_t p = NAMED[at], best[2];
            uint16_t run[3] = { a, ZERO, p };
            const before_set* in;
            if (before_rule_of(a, p, &in) || (p == same && zero_rule_of(0, a, &zero_in))) continue;
            if (joined_first(run, 3) || !pair_for(run, 3, best)) continue;
            before_set* s = NULL;
            for(int j=0; j < before_sets; j++) if (BEFORES[j].before == a && BEFORES[j].head == best[0]) s = &BEFORES[j];
            if (!s) {
                s = &BEFORES[before_sets++];
                s->before = a;
                s->head = best[0];
            }
            rule* new = &s->rules[s->n++];
            new->change = p ^ best[1];
            WIDEN(new, p, before_rule_for_all(a, s->head, wider, p & wider, new->change));
        }
        // Run each rule on vm.c with every named instruction it applies to.
        for(int at=0; at < named_ones; at++) {
            uint16_t p = NAMED[at];
            const before_set* in;
            const rule* r = before_rule_of(a, p, &in);
            if (!r) continue;
            uint16_t was[3] = { a, ZERO, p }, is[2] = { in->head, p ^ r->change };
            tried(was, 3, is, 2);
        }
    }
    return(1);
}

// The sets of rules for a store after three instructions A, X and B.  For
// each pair A and B in BEFORE_STORE and each named X, the machine gives the
// two instructions equivalent to A, X, B and `!`.  The first of the two is
// the set's `head`, and the rule's `change` is the bits that differ between
// X and the second.  So a rule says: A, an instruction that matches, B and
// `!` are equivalent to the set's head followed by the matching
// instruction with those bits inverted.  `r@ + r> !` is `r>` and one
// instruction that adds the cell under the top to the top and stores the
// sum at the address on top.
//
// Where B can be the head, the rule has it so (one_after()).  The second
// instruction then leaves the return stack alone, and a return can be
// folded into it.
typedef struct {
    uint16_t first, last, head;         // A, B, and the instruction that replaces A, X and B
    rule rules[MOST];
    int n;
} store_set;
static const char* const BEFORE_STORE[][2] = { { "r@", "r>" }, { "r>", "over" } };
static store_set STORES[MOST * COUNT(BEFORE_STORE)];
static int store_sets;

static int store_rule(const store_set* s, uint16_t x, uint16_t change, uint16_t store) {
    uint16_t y = x ^ change;
    if (alu_at[x] < 0 || returns(x) || alu_at[y] < 0 || returns(y)) return(0);
    uint16_t was[4] = { s->first, x, s->last, store }, is[2] = { s->head, y };
    return(same_runs(was, 4, is, 2));
}

static int store_rule_for_all(const store_set* s, uint16_t mask, uint16_t is, uint16_t change, uint16_t store) {
    for(int word=0; word < EVERY_WORD; word++) {
        if (!of_alu(word) || (word & mask) != is) continue;
        if (!store_rule(s, word, change, store)) return(0);
    }
    return(1);
}

// Returns the first rule of the STORES sets for A and B that matches X,
// and stores the rule's set in `*in`.  Returns NULL if no rule matches.
static const rule* store_rule_of(uint16_t a, uint16_t b, uint16_t x, const store_set** in) {
    for(int s=0; s < store_sets; s++) {
        if (STORES[s].first != a || STORES[s].last != b) continue;
        for(int r=0; r < STORES[s].n; r++) {
            if ((x & STORES[s].rules[r].mask) != STORES[s].rules[r].is) continue;
            *in = &STORES[s];
            return(&STORES[s].rules[r]);
        }
    }
    return(NULL);
}

// Returns 0 if a name in BEFORE_STORE is not one instruction.
static int find_stores(uint16_t store) {
    for(int k=0; k < COUNT(BEFORE_STORE); k++) {
        uint16_t a = ins_named(BEFORE_STORE[k][0]), b = ins_named(BEFORE_STORE[k][1]);
        if (!a || !b) {
            fprintf(stderr, "joins: %s or %s is not one instruction\n", BEFORE_STORE[k][0], BEFORE_STORE[k][1]);
            return(0);
        }
        for(int at=0; at < named_ones; at++) {
            uint16_t x = NAMED[at], best[2];
            uint16_t run[4] = { a, x, b, store };
            const store_set* in;
            if (store_rule_of(a, b, x, &in) || joined_first(run, 4)) continue;
            best[0] = b;
            best[1] = one_after(b, run, 4);
            if (!best[1] && !pair_for(run, 4, best)) continue;
            store_set* s = NULL;
            for(int j=0; j < store_sets; j++) {
                if (STORES[j].first == a && STORES[j].last == b && STORES[j].head == best[0]) s = &STORES[j];
            }
            if (!s) {
                s = &STORES[store_sets++];
                s->first = a;
                s->last = b;
                s->head = best[0];
            }
            rule* new = &s->rules[s->n++];
            new->change = x ^ best[1];
            WIDEN(new, x, store_rule_for_all(s, wider, x & wider, new->change, store));
        }
        // Run each rule on vm.c with every named instruction it applies to.
        for(int at=0; at < named_ones; at++) {
            uint16_t x = NAMED[at];
            const store_set* in;
            const rule* r = store_rule_of(a, b, x, &in);
            if (!r) continue;
            uint16_t was[4] = { a, x, b, store }, is[2] = { in->head, x ^ r->change };
            tried(was, 4, is, 2);
        }
    }
    return(1);
}

// The set of rules for a literal add before `+`.  A rule matches an
// instruction X for which X, a literal add and `+` are equivalent to the
// literal add, X and `+`, whatever number is added.  The nucleus moves the
// add back across X.  X and `+` are then adjacent, and it can join them.
// These rules change no bits.  The set's `across` field holds `+`.
static rule_set BACK;

// Returns a literal that adds 1.
static uint16_t an_add(void) {
    uint16_t pushed[2];
    a_push(A_NUMBER_OF_TWO, pushed);
    return(pushed[1]);
}

// Returns 1 if X, a literal add and `+` are equivalent to the add, X and
// `+`, whatever number is added.
static int moves_back(uint16_t x, uint16_t plus) {
    if (alu_at[x] < 0 || returns(x)) return(0);
    uint16_t add = an_add();
    uint16_t was[3] = { x, add, plus }, is[3] = { add, x, plus };
    any_number = 1;
    int same = same_runs(was, 3, is, 3);
    any_number = 0;
    return(same);
}

static int moves_back_for_all(uint16_t mask, uint16_t is, uint16_t plus) {
    for(int word=0; word < EVERY_WORD; word++) {
        if (!of_alu(word) || (word & mask) != is) continue;
        if (!moves_back(word, plus)) return(0);
    }
    return(1);
}

static void find_back(uint16_t plus) {
    rule_set* s = &BACK;
    s->across = plus;
    for(int at=0; at < named_ones; at++) {
        uint16_t x = NAMED[at];
        int taken = 0;
        for(int r=0; r < s->n; r++) taken |= (x & s->rules[r].mask) == s->rules[r].is;
        if (taken || !moves_back(x, plus)) continue;
        rule* new = &s->rules[s->n++];
        new->change = 0;
        WIDEN(new, x, moves_back_for_all(wider, x & wider, plus));
    }
    // Run each rule on vm.c with every named instruction it matches.
    for(int at=0; at < named_ones; at++) {
        uint16_t x = NAMED[at], add = an_add();
        for(int r=0; r < s->n; r++) {
            if ((x & s->rules[r].mask) != s->rules[r].is) continue;
            uint16_t was[3] = { x, add, plus }, is[3] = { add, x, plus };
            tried(was, 3, is, 3);
            break;
        }
    }
}

static const char* name_of(uint16_t ins) {
    for(int at=0; at < named_ones; at++) if (NAMED[at] == ins) return(NAME_OF[at]);
    return("");
}

// Returns an instruction's name.  If it has no name, writes its number to
// `number` as Forth source writes one, and returns that.
#define SHOWN 8                         // the size of `number`: a $, four digits and the end of the text, rounded up
static const char* shown(uint16_t ins, char* number) {
    const char* name = name_of(ins);
    if (*name) return(name);
    snprintf(number, SHOWN, "$%04x", ins);
    return(number);
}

// Starts some lines of the table that belong together.  The generated
// file is source that goes on a disk, so it follows doc/formatting.md: no
// line is longer than DISK_LINE characters, and a blank line comes before
// these lines if they would make a run of lines longer than a block holds.
static void table_lines(FILE* out, int lines) {
    static int run;                     // table lines since the last blank line
    if (run + lines > DISK_BLOCK / DISK_LINE) {
        fprintf(out, "\n");
        run = 0;
    }
    run += lines;
}

static void table_line(FILE* out) {
    table_lines(out, 1);
}

// Writes a set's rules.  The comment after each rule lists the named
// instructions the rule matches, and continues on further lines if they do
// not fit on one.
static void write_rules(FILE* out, const rule* rules, int n) {
    for(int r=0; r < n; r++) {
        char* text = NULL;              // the rule's lines
        size_t text_len = 0;
        FILE* line = open_memstream(&text, &text_len);
        if (!line) { perror("joins"); exit(EXIT_FAILURE); }
        int lines = 1;
        int len = fprintf(line, "$%04x w, $%04x w, $%04x w,   \\", rules[r].mask, rules[r].is, rules[r].change);
        int column = len - 1;           // the column of the \ comment, where a continuation line puts its own
        for(int at=0; at < named_ones; at++) {
            if ((NAMED[at] & rules[r].mask) != rules[r].is) continue;
            if (len + 1 + (int)strlen(NAME_OF[at]) > DISK_LINE) {
                len = fprintf(line, "\n%*s\\", column, "") - 1;
                lines++;
            }
            len += fprintf(line, " %s", NAME_OF[at]);
        }
        fclose(line);
        table_lines(out, lines);
        fprintf(out, "%s\n", text);
        free(text);
    }
}

// Expands a phrase into the instructions of its words, in order, in an
// allocated array stored in `*run`.  Returns the number of instructions,
// or 0 if one of the words is not in vm_opcodes.h.
static int phrase_run(const word_node* phrase, uint16_t** run) {
    // An upper bound on the size: a phrase has fewer words than characters,
    // and a word has at most COUNT(phrase->ins) instructions.
    *run = room_for(strlen(phrase->repr) * COUNT(phrase->ins), sizeof(uint16_t));
    char* said = room_for(strlen(phrase->repr) + 1, 1);
    strcpy(said, phrase->repr);
    int len = 0;
    for(char* word = strtok(said, " "); word; word = strtok(NULL, " ")) {
        const word_node* part = word_named(word);
        if (!part) { len = 0; break; }
        for(int idx=0; idx < part->num_ins; idx++) (*run)[len++] = *(const uint16_t*)&part->ins[idx];
    }
    free(said);
    return(len);
}

// Checks every name in vm_opcodes.h against the machine, and returns how
// many fail.  A named instruction must be the representative of the
// instructions equivalent to it (fuse.h, plainer()).  Both compilers
// produce the representative, so a name for any other instruction would be
// a second way to write the same operation.  A phrase's instruction must
// be equivalent to the phrase's words in sequence, which is established on
// the step and then run on vm.c.
static int names_not_so(void) {
    int not_so = 0;
    for(const word_node* w = FORTH_WORDS; w->repr && strlen(w->repr); w++) {
        uint16_t ins = *(const uint16_t*)&w->ins[0];
        if (is_phrase(w->repr) && (w->num_ins != 1 || alu_at[ins] < 0)) {
            fprintf(stderr, "joins: the phrase `%s` is not one instruction that the step says of\n", w->repr);
            not_so++;
            continue;
        }
        if (w->num_ins != 1 || alu_at[ins] < 0) continue;
        uint16_t stands = ALU[alu_at[ins]].stands;
        if (stands != ins) {
            fprintf(stderr, "joins: %s is %04x in vm_opcodes.h, and what stands for all that do the same is %04x\n",
                    w->repr, ins, stands);
            not_so++;
        }
        if (!is_phrase(w->repr)) continue;
        uint16_t* run;
        int len = phrase_run(w, &run);
        named a, b;
        forget();
        uint64_t left = len ? names(run, len, &a) : 0;
        if (!left || names(&ins, 1, &b) != left || !same_named(&a, &b)) {
            fprintf(stderr, "joins: the phrase `%s` is %04x in vm_opcodes.h, which is not what its words do\n", w->repr, ins);
            not_so++;
        } else {
            tried(run, len, &ins, 1);
        }
        free(run);
    }
    return(not_so);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage: joins forth/folds.fs\n");
        return(1);
    }
    get_ready();
    list_named();
    if (names_not_so()) return(1);
    uint16_t of_one[MOST], of_two[MOST];
    int ones = 0, twos = 0;
    for(int at=0; at < named_ones; at++) {
        int of = folds(NAMED[at]);
        if (of == OF_NONE) OF_NONE_INS[nones++] = NAMED[at];
        if (of == 1) of_one[ones++] = NAMED[at];
        if (of == 2) of_two[twos++] = NAMED[at];
    }
    ZERO = push_of_zero();
    if (!ZERO) {
        fprintf(stderr, "joins: no named ALU instruction pushes 0\n");
        return(1);
    }
    // The push of 0 goes last, where the nucleus reads it (`of-none 1+`,
    // join.fs).
    for(int at=0; at < nones; at++) {
        if (OF_NONE_INS[at] != ZERO) continue;
        OF_NONE_INS[at] = OF_NONE_INS[nones - 1];
        OF_NONE_INS[nones - 1] = ZERO;
    }
    int rules = 0;                      // the total over all the sets
    for(int at=0; at < COUNT(ACROSS); at++) {
        rule_set* s = &SET[sets];
        s->across = ins_named(ACROSS[at]);
        if (!s->across) {
            fprintf(stderr, "joins: %s is not one instruction\n", ACROSS[at]);
            return(1);
        }
        find_rules(s);
        if (!s->n) continue;            // no named instruction has an equivalent across this one
        rules += s->n;
        sets++;
    }

    const uint16_t same = ins_named("=");
    for(int at=0; at < COUNT(BETWEEN); at++) {
        uint16_t between = ins_named(BETWEEN[at]);
        if (!between) {
            fprintf(stderr, "joins: %s is not one instruction\n", BETWEEN[at]);
            return(1);
        }
        find_zeros(between, same);
    }
    find_zeros(0, same);
    const uint16_t store = ins_named("!"), plus = ins_named("+");
    if (!find_befores(same) || !find_stores(store)) return(1);
    find_back(plus);
    int zero_rules = 0, before_rules = 0, store_rules = 0;
    for(int s=0; s < zero_sets; s++) zero_rules += ZEROS[s].n;
    for(int s=0; s < before_sets; s++) before_rules += BEFORES[s].n;
    for(int s=0; s < store_sets; s++) store_rules += STORES[s].n;
    if (!ones || !twos || !rules || !zero_rules || !before_rules || !store_rules || !BACK.n) {
        fprintf(stderr, "joins: expected at least one of each, and found %d instructions that compute a number from one number, "
                        "%d that compute one from two, %d rules for a push, %d rules for `0 =`, %d rules for a 0 after "
                        "another instruction, %d rules for a store, and %d rules for an add\n",
                ones, twos, rules, zero_rules, before_rules, store_rules, BACK.n);
        return(1);
    }
    if (nones != KEPT_OF_NONE) {
        fprintf(stderr, "joins: %d named instructions compute a number from no stack cell, and the nucleus has %d cells for "
                        "them (of-none, join.fs)\n", nones, KEPT_OF_NONE);
        return(1);
    }
    // The table's layout, which the generated file's first comment
    // describes.  All of these count the table's numbers.
    const int zeros_at = 6 + ones + twos + 2 * sets + 3 * rules;         // from the table's start to the part for a 0
    const int befores_at = 2 + 3 * zero_sets + 3 * zero_rules + 1;       // from that part's start to its second list
    const int stores_at = zeros_at + befores_at + 3 * before_sets + 3 * before_rules + 1;   // to the list for a store
    const int back_at = stores_at + 4 * store_sets + 3 * store_rules + 1;                    // to the set for an add

    char* made = NULL;                  // the file's text
    size_t made_len = 0;
    FILE* out = open_memstream(&made, &made_len);
    if (!out) { perror(argv[1]); return(1); }
    fprintf(out, "\\ folds.fs - the table the nucleus's peephole (join.fs) reads.\n"
                 "\\ GENERATED by `joins forth/folds.fs` (util/joins.c).\n\n"
                 "\\ The first lines store true and false in join.fs's cells\n"
                 "\\ (of-none).  The table is numbers of sixteen bits, which i@\n"
                 "\\ reads, and folds-at holds where it starts.  Its first six:\n"
                 "\\   how many instructions compute a number from one number\n"
                 "\\   how many compute a number from two\n"
                 "\\   how many sets there are for a push\n"
                 "\\   the offset of the part for a 0\n"
                 "\\   the offset of the list for a store\n"
                 "\\   the offset of the set for an add\n"
                 "\\ Then come those instructions, the sets for a push, and the\n"
                 "\\ three parts.  A rule is a mask, the value an instruction has\n"
                 "\\ under the mask, and the bits to invert in the instruction.\n\n"
                 "\\ A set for a push: the instruction the push moves across, how\n"
                 "\\ many rules, the rules.\n"
                 "\\ The part for a 0: `=`, the offset of its second list, and two\n"
                 "\\ lists of sets.  A 0 ends each list.\n"
                 "\\ In the first list a set is for X, a 0 and `=`: what replaces\n"
                 "\\ the 0 and `=`; the instruction between X and the 0, or 0 for\n"
                 "\\ none; how many rules; the rules, which change X.\n"
                 "\\ In the second a set is for A, a 0 and X: A; what replaces A\n"
                 "\\ and the 0; how many rules; the rules, which change X.\n\n"
                 "\\ In the list for a store a set is for A, X, B and `!`: B; A;\n"
                 "\\ what replaces A, X and B; how many rules; the rules, which\n"
                 "\\ change X and give what replaces `!`.  A 0 ends the list.\n"
                 "\\ The set for an add: `+`, how many rules, the rules.  X, a\n"
                 "\\ literal add and `+` are the add, X and `+`.\n\n");
    for(int at=0; at < nones; at++) {
        table_line(out);
        fprintf(out, "meta $%04x t' of-none %d + tvar! target   \\ %s\n", OF_NONE_INS[at], at, name_of(OF_NONE_INS[at]));
    }
    table_line(out);
    fprintf(out, "high\n");
    table_line(out);
    fprintf(out, "create folds-table\n");
    table_line(out);
    fprintf(out, "$%04x w, $%04x w, $%04x w,   \\ how many of each\n", ones, twos, sets);
    table_line(out);
    fprintf(out, "$%04x w, $%04x w, $%04x w,   \\ the offsets of the three parts\n", zeros_at, stores_at, back_at);
    for(int at=0; at < ones; at++) {
        table_line(out);
        fprintf(out, "$%04x w,   \\ %s\n", of_one[at], name_of(of_one[at]));
    }
    for(int at=0; at < twos; at++) {
        table_line(out);
        fprintf(out, "$%04x w,   \\ %s\n", of_two[at], name_of(of_two[at]));
    }
    for(int set=0; set < sets; set++) {
        const rule_set* s = &SET[set];
        table_line(out);
        fprintf(out, "$%04x w, $%04x w,   \\ across %s\n", s->across, s->n, name_of(s->across));
        write_rules(out, s->rules, s->n);
    }
    char head[SHOWN], other[SHOWN];
    table_line(out);
    fprintf(out, "$%04x w, $%04x w,   \\ %s, and the offset of the second list\n", same, befores_at, name_of(same));
    for(int set=0; set < zero_sets; set++) {
        const zero_set* s = &ZEROS[set];
        table_line(out);
        fprintf(out, "$%04x w, $%04x w, $%04x w,   \\ ", s->head, s->between, s->n);
        if (s->between) fprintf(out, "%s ", shown(s->between, other));
        fprintf(out, "0 %s becomes %s\n", name_of(same), shown(s->head, head));
        write_rules(out, s->rules, s->n);
    }
    table_line(out);
    fprintf(out, "$0000 w,   \\ the end of the first list\n");
    for(int set=0; set < before_sets; set++) {
        const before_set* s = &BEFORES[set];
        table_line(out);
        fprintf(out, "$%04x w, $%04x w, $%04x w,   \\ %s 0 becomes %s\n", s->before, s->head, s->n, shown(s->before, other),
                shown(s->head, head));
        write_rules(out, s->rules, s->n);
    }
    table_line(out);
    fprintf(out, "$0000 w,   \\ the end of the second list\n");
    for(int set=0; set < store_sets; set++) {
        const store_set* s = &STORES[set];
        char first[SHOWN];
        table_line(out);
        fprintf(out, "$%04x w, $%04x w, $%04x w, $%04x w,   \\ %s X %s %s: %s\n", s->last, s->first, s->head, s->n,
                shown(s->first, first), shown(s->last, other), name_of(store), shown(s->head, head));
        write_rules(out, s->rules, s->n);
    }
    table_line(out);
    fprintf(out, "$0000 w,   \\ the end of the list for a store\n");
    table_line(out);
    fprintf(out, "$%04x w, $%04x w,   \\ an add moves back from before %s\n", BACK.across, BACK.n, name_of(BACK.across));
    write_rules(out, BACK.rules, BACK.n);
    table_line(out);
    fprintf(out, "unhigh\n");
    table_line(out);
    fprintf(out, "meta t' folds-table byte-shift lshift 2/  t' folds-at tvar! target\n");
    fclose(out);
    if (!write_whole(argv[1], made, made_len)) { perror(argv[1]); return(1); }
    free(made);
    printf("wrote %s: %d instructions compute a number from no stack cell, %d from one and %d from two; %d rules in %d sets "
           "move a push across", argv[1], nones, ones, twos, rules, sets);
    for(int set=0; set < sets; set++) printf("%s %s", set ? "," : "", name_of(SET[set].across));
    printf("; %d rules in %d sets are for `0 =`; %d rules in %d sets are for a 0 after another instruction; %d rules in %d "
           "sets are for a store; %d rules move a literal add back\n",
           zero_rules, zero_sets, before_rules, before_sets, store_rules, store_sets, BACK.n);
    return(0);
}
