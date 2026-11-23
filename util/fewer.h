//
// fewer.h - searches for the fewest instructions that produce a target
// state, for the tools that include it.  It includes fuse.h, through
// which it asks the machine, and kinds.h, which lists the instructions
// that have a number in them.
//
// The target is given in fuse.h's form for the final state of a run: the
// terms in the cells of the two stacks, memory as a term, and the
// addresses fetched from (a `named`).  The target may ask about the top of
// the data stack only whether it is zero, which is all a conditional
// branch tests.  In that case a candidate matches if its top is zero from
// exactly the same starting states, whatever else its top is.
//
// The search tries every instruction the step handles, plus literal and
// variable instructions for the numbers and variables in the target.  It
// knows nothing about what an instruction does: it steps each one (fuse.h,
// which is vm_step.h on terms) and examines the result.  It continues from
// an instruction only if the value the instruction produced is part of
// the target, or was already present, or is zero exactly when the top
// must be.  It abandons a line of search when the remaining work cannot be
// done in the instructions that are left.  It gives up entirely, and
// reports that, after examining a set number of states.
//
// An answer is found on terms, which hold for every state only if the
// step and the arithmetic facts fuse.h relies on are right.  So each
// answer is also run on vm.c beside the code it replaces
// (agree_on_machine()), which is an independent check.
//
// A turn of a loop carries values from one turn to the next in cells of
// the stacks.  kept_as() gives the target for a turn when each of those
// values is kept in a different cell: the same terms, with the cells they
// are computed from and the cells they end in renamed.
//

#ifndef HEXAFORTH_FEWER_H
#define HEXAFORTH_FEWER_H

#include "fuse.h"
#include "kinds.h"

// The problem itself bounds how deep a search goes: a line of search is
// abandoned when the remaining work cannot be done in the instructions
// that are left (at_least()), and the stacks can be as deep as the step
// has terms for.  The time limit is set in one place,
// cmake/default-machine.cmake: SEARCH_STATES is the number of states a
// search examines before it gives up, and it is the same for every search.
#ifndef SEARCH_STATES
#error "fewer.h: the states a search looks at are said in cmake/default-machine.cmake (SEARCH_STATES)"
#endif
#define FEW_SEEN_BITS   16              // the table of visited states starts with 2^16 slots, and grows
#define FEW_NEVER       INT_MAX         // the cost of something that cannot be done

#include <limits.h>

// ---- the cells of the stacks, by their terms (fuse.h defines which term belongs to which cell)

#define DEPTH_OF(cell)        (DEEP - 1 - CELL_OF(cell))        // a cell's depth under the top, at the start
#define CELL_OF_DATA(depth)   DATA_TERM(DEEP - 1 - (depth))     // the term for the data-stack cell at a depth
#define CELL_OF_RETURN(depth) RETURN_TERM(DEEP - 1 - (depth))   // the term for the return-stack cell at a depth

typedef uint64_t cell_set;              // a set of cells: one bit per cell's term
_Static_assert(CELL_TERMS <= 8 * sizeof(cell_set), "fewer.h: a set of cells has too few bits for the cells there are");
#define IN_SET(cell)          ((cell_set)1 << (cell))

// Returns a pointer to the term in a cell of a state, for reading or
// writing.
static int* cell_left(named* m, int cell) {
    int idx = CELL_OF(cell);
    if (OF_DATA(cell)) return(idx == m->SP - 1 ? &m->T : &m->DS[idx]);
    return(idx == m->RSP - 1 ? &m->R : &m->RS[idx]);
}

// ---- the ALU instructions, grouped by the value they produce

// Many instructions produce the same value and put it in different
// places: those that pass the same input through the same operation.
// They form a family.  One member that puts the value on top and moves
// neither stack is the probe.  The search steps the probe first, and
// steps the other members only if the probe's value is useful.  Whether
// the members really produce the same value is checked with the step when
// the families are built (makes_as()).  An instruction that does not is a
// family of its own, and is always stepped.
typedef struct { uint16_t probe; int first, n; } family;        // its members are MEMBER[first] to MEMBER[first + n - 1]
static family* FAMILY;
static uint16_t* MEMBER;
static int families, alike;             // the number of families; the first `alike` of them are real families,
                                        // and the rest are single instructions

// Returns 1 if instruction `x` produces the value that `probe` puts on
// top, and no other new value.  Both are stepped from the starting state.
// Afterwards the stack cells and the stores of `x` may hold only terms
// that existed at the start, and the term `probe` left on top.
static int makes_as(uint16_t x, uint16_t probe) {
    named p, m;
    forget();
    int was_known = known;
    start(&p);
    step_named(&p, probe);
    if (p.unsaid || lost) return(0);
    int made = p.T;
    start(&m);
    step_named(&m, x);
    if (m.unsaid || lost || m.next >= 0) return(0);
    for(int cell=0; cell < CELL_TERMS; cell++) {
        if (CELL_OF(cell) >= (OF_DATA(cell) ? m.SP : m.RSP)) continue;
        int is = *cell_left(&m, cell);
        if (is >= was_known && is != made) return(0);
    }
    for(int mem=m.M; KNOWN[mem].kind == STORED; mem = KNOWN[mem].of[0]) {
        if ((KNOWN[mem].of[1] >= was_known && KNOWN[mem].of[1] != made) ||
            (KNOWN[mem].of[2] >= was_known && KNOWN[mem].of[2] != made)) return(0);
    }
    return(1);
}

// Builds the families.  kinds.h must be included first, and list_kinds()
// called before this.  The instructions that do not return are grouped by
// family.  An instruction joins the family of the first instruction that
// has its input and its operation, puts the result on top and changes
// nothing else, provided it produces the same value as that one
// (makes_as()).  Otherwise it is a family of its own.
static void fewer_ready(void) {
    static int probe_of[INPUTS][OPS];                   // the ALU index of the probe for each input and operation
    int* family_at = room_for(alus, sizeof(int));       // the family of each instruction, or -1
    int* by_probe = room_for(alus, sizeof(int));        // the family of a probe, indexed by the probe's ALU index
    memset(probe_of, -1, sizeof(probe_of));
    free(FAMILY);
    free(MEMBER);
    FAMILY = room_for(alus, sizeof(family));
    MEMBER = room_for(alus, sizeof(uint16_t));
    families = 0;
    for(int idx=0; idx < alus; idx++) {
        instruction ins = *(instruction*)&ALU[idx].ins;
        named m, none;
        forget();
        start(&none);
        if (!names(&ALU[idx].ins, 1, &m) || m.next >= 0 || m.SP != none.SP || m.RSP != none.RSP || m.M != none.M) continue;
        if (memcmp(m.DS, none.DS, sizeof(m.DS)) || memcmp(m.RS, none.RS, sizeof(m.RS)) || m.R != none.R) continue;
        int* probe = &probe_of[ins.alu.in_mux][ins.alu.alu_op];
        if (*probe >= 0) continue;
        *probe = idx;
        by_probe[idx] = families;
        FAMILY[families++] = (family){ ALU[idx].ins };
    }
    alike = families;
    for(int idx=0; idx < alus; idx++) {
        instruction ins = *(instruction*)&ALU[idx].ins;
        named m;
        forget();
        family_at[idx] = -1;
        if (!names(&ALU[idx].ins, 1, &m) || m.next >= 0) continue;      // it returns
        int probe = probe_of[ins.alu.in_mux][ins.alu.alu_op];
        if (probe >= 0 && makes_as(ALU[idx].ins, ALU[probe].ins)) {
            family_at[idx] = by_probe[probe];
        } else {
            family_at[idx] = families;
            FAMILY[families++] = (family){ ALU[idx].ins };
        }
        FAMILY[family_at[idx]].n++;
    }
    for(int f=0, first=0; f < families; f++) {          // set where each family's members start, and reset the counts
        FAMILY[f].first = first;
        first += FAMILY[f].n;
        FAMILY[f].n = 0;
    }
    for(int idx=0; idx < alus; idx++) {
        family* f = family_at[idx] < 0 ? NULL : &FAMILY[family_at[idx]];
        if (f) MEMBER[f->first + f->n++] = ALU[idx].ins;
    }
    free(family_at);
    free(by_probe);
    forget();
}

// ---- terms: the cells a term depends on, and a term with its cells renamed

// Returns the set of starting stack cells a term is computed from.
static cell_set cells_in(int t) {
    const term* is = &KNOWN[t];
    if (is->kind == IN_DATA || is->kind == IN_RETURN) return(t < CELL_TERMS ? IN_SET(t) : 0);
    cell_set in = 0;
    for(int idx=0; idx < is->n; idx++) in |= cells_in(is->of[idx]);
    return(in);
}

// Returns the term for the flag "this term is zero".  Two terms are zero
// from the same states exactly when this returns the same term for both.
static int nothing(int t) {
    return(t_made(ALU_EQ, t, t_number(0)));
}

// Returns the term for a flag inverted.
static int not_flag(int flag) {
    for(int op=0; op < OPS; op++) if (ALU_UNDOES(op)) return(t_made(op, flag, flag));
    return(flag);
}

// Returns a term with its starting cells renamed.  `to` maps each cell's
// term to the term of the cell that replaces it.  The term is rebuilt from
// the renamed parts, as the step would have built it.
static int renamed(int t, const int* to) {
    const term is = KNOWN[t];
    named at;
    switch (is.kind) {
        case IN_DATA:
        case IN_RETURN:
            return(t < CELL_TERMS ? to[t] : t);
        case BY_ALU: {
            int made = renamed(is.of[0], to);
            if (is.n == 1) return(made);
            for(int idx=1; idx < is.n; idx++) made = t_made(is.op, made, renamed(is.of[idx], to));
            return(made);
        }
        case FETCHED:
            at.M = renamed(is.of[0], to);
            at.fetches = 0;
            return(t_fetch(&at, renamed(is.of[1], to)));
        case STORED:
            at.M = renamed(is.of[0], to);
            t_store(&at, renamed(is.of[1], to), renamed(is.of[2], to));
            return(at.M);
        default:
            return(t);
    }
}

// ---- a turn of a loop: the cells it keeps values in, and the same turn with them kept elsewhere

// Finds how many cells at the top of each stack a turn reads or changes.
// The turn's final state has the data stack one deeper, with the value the
// branch tests on top.
static void cells_kept(const named* turn, int* of_data, int* of_return) {
    named left = *turn;
    cell_set used = cells_in(left.T);
    for(int mem=left.M; KNOWN[mem].kind == STORED; mem = KNOWN[mem].of[0]) {
        used |= cells_in(KNOWN[mem].of[1]) | cells_in(KNOWN[mem].of[2]);
    }
    for(int idx=0; idx < left.fetches; idx++) used |= cells_in(left.fetched[idx]);
    for(int cell=0; cell < CELL_TERMS; cell++) {
        if (DEPTH_OF(cell) < 0) continue;               // above the stack top the turn started with
        int is = *cell_left(&left, cell);
        if (is != cell) used |= cells_in(is) | IN_SET(cell);
    }
    *of_data = *of_return = 0;
    for(int depth=0; depth < DEEP; depth++) {
        if (used & IN_SET(CELL_OF_DATA(depth))) *of_data = depth + 1;
        if (used & IN_SET(CELL_OF_RETURN(depth))) *of_return = depth + 1;
    }
}

// Builds the target for a turn whose kept values are moved as `to` says.
// `to` maps each cell's term to the cell its value is kept in, and maps a
// cell in which the turn keeps no value to itself.  The value that was
// left in a cell is left in the cell it is renamed to, computed from the
// renamed cells.  Every other cell is unchanged.
static void kept_as(const named* turn, const int* to, named* wanted) {
    named left = *turn;
    start(wanted);
    wanted->SP = left.SP;
    wanted->RSP = left.RSP;
    wanted->DS[wanted->SP - 1] = wanted->T;             // the old top is now under the value the branch tests
    for(int cell=0; cell < CELL_TERMS; cell++) {
        if (to[cell] == cell && *cell_left(&left, cell) == cell) continue;
        if (DEPTH_OF(cell) < 0) continue;               // above the stack top the turn started with
        *cell_left(wanted, to[cell]) = renamed(*cell_left(&left, cell), to);
    }
    wanted->T = renamed(left.T, to);
    wanted->R = wanted->RS[wanted->RSP - 1];
    wanted->M = renamed(left.M, to);
    wanted->fetches = 0;
    for(int idx=0; idx < left.fetches; idx++) wanted->fetched[wanted->fetches++] = renamed(left.fetched[idx], to);
}

// ---- the target

typedef struct {
    named left;                         // the final state to reach
    int fact;                           // if only "is the top zero?" matters, the term for that flag; otherwise -1
    int base;                           // the number of terms when the target was set: later terms are not part of it
    unsigned char needed[ROOM];         // set for each term the target is built from, and for the parts of each
    short stores_to[ROOM];              // for a memory term that is the target's memory after k of its stores: k; else -1
    int stores;                         // the number of stores in the target
    int final[CELL_TERMS], finals;      // the terms on the target's stacks, each listed once
    cell_set cells;                     // the starting cells its stacks, and the test of its top, are computed from
    cell_set cells_from[ROOM + 1];      // the starting cells its remaining stores need, after k of them are done
    int places[FETCHES], n_places;      // the addresses it fetches from
    uint16_t pushes[ROOM], adds[ROOM];  // literal instructions for its numbers: at most one per term
    uint16_t fetches[FETCHES], stores_var[ROOM];        // variable instructions: at most one per fetch address, and per store
    int n_pushes, n_adds, n_fetches, n_stores_var;
    int nothing_of[ROOM];               // a cache: for each earlier term, the term for "it is zero", once computed
} wanted;

// Marks as needed every term a term is built from.  For a chain of one
// associative operation, it also marks the result of every subset of the
// chain's operands.
static void parts(wanted* w, int t) {
    if (lost || t >= ROOM || w->needed[t]) return;
    w->needed[t] = 1;
    const term is = KNOWN[t];
    if (is.kind == FETCHED) {
        parts(w, is.of[1]);
    } else if (is.kind == BY_ALU) {
        for(int some=1; ALU_REGROUPS(is.op) && is.n > 2 && some < (1 << is.n) - 1; some++) {
            int made = -1;
            for(int idx=0; idx < is.n; idx++) {
                if (some >> idx & 1) made = made < 0 ? is.of[idx] : t_made(is.op, made, is.of[idx]);
            }
            if (!lost && made < ROOM) w->needed[made] = 1;
        }
        for(int idx=0; idx < is.n; idx++) parts(w, is.of[idx]);
    }
}

// Sets the target.  The terms it is built from must already exist, and are
// kept while the search runs.  Returns 0 if the term table overflowed.
static int want(wanted* w, const named* left, int fact) {
    memset(w, 0, sizeof(wanted));
    w->left = *left;
    w->fact = fact;
    memset(w->stores_to, -1, sizeof(w->stores_to));
    int chain[ROOM], links = 0;
    for(int mem=left->M; KNOWN[mem].kind == STORED && links < ROOM; mem = KNOWN[mem].of[0]) chain[links++] = mem;
    w->stores = links;                  // each store is a term, so there are at most ROOM of them
    w->stores_to[MEMORY_TERM] = 0;
    for(int idx=0; idx < links; idx++) w->stores_to[chain[links - 1 - idx]] = idx + 1;

    named l = *left;
    for(int cell=0; cell < CELL_TERMS; cell++) {
        int data = OF_DATA(cell), idx = CELL_OF(cell);
        if (idx >= (data ? l.SP : l.RSP)) continue;
        if (data && idx == l.SP - 1 && fact >= 0) continue;     // the top, when only "is it zero?" matters
        int is = *cell_left(&l, cell), had = 0;
        parts(w, is);
        w->cells |= cells_in(is);
        for(int f=0; f < w->finals; f++) had |= w->final[f] == is;
        if (!had) w->final[w->finals++] = is;
    }
    for(int idx=0; idx < links; idx++) {                // from the last store back: once a store is done,
        parts(w, KNOWN[chain[idx]].of[1]);              // its cells are no longer needed
        parts(w, KNOWN[chain[idx]].of[2]);
        w->cells_from[links - 1 - idx] = w->cells_from[links - idx] |
                                         cells_in(KNOWN[chain[idx]].of[1]) | cells_in(KNOWN[chain[idx]].of[2]);
    }
    for(int idx=0; idx < left->fetches; idx++) {
        int had = 0;
        parts(w, left->fetched[idx]);
        for(int p=0; p < w->n_places; p++) had |= w->places[p] == left->fetched[idx];
        if (!had) w->places[w->n_places++] = left->fetched[idx];
    }
    if (fact >= 0 && KNOWN[fact].kind == BY_ALU) {
        const term is = KNOWN[fact];
        for(int idx=0; idx < is.n; idx++) {
            parts(w, is.of[idx]);
            w->cells |= cells_in(is.of[idx]);
        }
    }
    if (lost) return(0);

    // Collect the literal and variable instructions for the numbers and
    // variables in the target: the instructions that push, add, fetch and
    // store them.
    for(int t=FIXED; t < known; t++) {
        const numbered* is;
        if (!w->needed[t] || KNOWN[t].kind != A_NUMBER) continue;
        if ((is = numbered_as(PUSHES_IT, KNOWN[t].number))) w->pushes[w->n_pushes++] = is->ins;
        if ((is = numbered_as(ADDS_IT, KNOWN[t].number))) w->adds[w->n_adds++] = is->ins;
    }
    for(int p=0; p < w->n_places; p++) {
        const numbered* is;
        if (KNOWN[w->places[p]].kind != A_NUMBER) continue;
        if ((is = numbered_as(FETCHES_AT_IT, KNOWN[w->places[p]].number))) w->fetches[w->n_fetches++] = is->ins;
    }
    for(int idx=0; idx < links; idx++) {
        const numbered* is;
        int at = KNOWN[chain[idx]].of[1];
        if (KNOWN[at].kind != A_NUMBER) continue;
        if ((is = numbered_as(STORES_AT_IT, KNOWN[at].number))) w->stores_var[w->n_stores_var++] = is->ins;
    }
    w->base = known;
    memset(w->nothing_of, -1, sizeof(w->nothing_of));
    return(1);
}

// Returns 1 if a term is zero exactly when the target's top must be.  The
// terms built to find out are discarded afterwards.  For a term that
// existed when the target was set, the answer is cached.
static int is_the_fact(wanted* w, int t) {
    if (w->fact < 0) return(0);
    if (t < w->base && w->nothing_of[t] >= 0) return(w->nothing_of[t] == w->fact);
    int had = known, was_lost = lost;
    int flag = nothing(t);
    int is = !lost && flag == w->fact;
    if (t < w->base && !lost) w->nothing_of[t] = flag < w->base ? flag : ROOM;
    known = had;
    lost = was_lost;
    return(is);
}

static int on_a_stack(const named* m, int t) {
    if (m->T == t || m->R == t) return(1);
    for(int idx=0; idx < m->SP - 1; idx++) if (m->DS[idx] == t) return(1);
    for(int idx=0; idx < m->RSP - 1; idx++) if (m->RS[idx] == t) return(1);
    return(0);
}

// Returns 1 if a state matches the target.
static int is_wanted(wanted* w, const named* m) {
    const named* l = &w->left;
    if (m->unsaid || lost || m->next >= 0) return(0);
    if (m->SP != l->SP || m->RSP != l->RSP || m->M != l->M || m->R != l->R) return(0);
    if (memcmp(m->DS, l->DS, (m->SP - 1) * sizeof(int)) || memcmp(m->RS, l->RS, (m->RSP - 1) * sizeof(int))) return(0);
    if (w->fact >= 0 ? !is_the_fact(w, m->T) : m->T != l->T) return(0);
    int x[FETCHES], y[FETCHES];
    int n = fetched_from(m, x);
    return(n == fetched_from(l, y) && !memcmp(x, y, n * sizeof(int)));
}

// Returns a lower bound on the instructions needed to turn one stack into
// another that has `extra` more cells on top.  It relies on a cell being
// writable only when it is the top, or the cell just under the top.
static int to_shape(const int* is, int size, const int* to, int to_size, int extra) {
    int want = to_size + extra, at = 0;
    while (at < size && at < to_size && is[at] == to[at]) at++;
    if (at == size || at == to_size) {
        if (size < to_size) return(want - size);
        return(size > want ? (size - want + 1) / 2 : want - size);
    }
    int first = size <= at + 3 ? 1 : (size - at - 2) / 2 + 1;
    return(first + (want > at + 2 ? want - (at + 2) : 0));
}

// Returns a lower bound on the instructions still needed to reach the
// target from a state, or FEW_NEVER if the target cannot be reached.
static int at_least(wanted* w, const named* m) {
    int d[SLOTS], r[SLOTS], to_d[SLOTS], to_r[SLOTS];
    const named* l = &w->left;
    cell_set have = 0;
    memcpy(d, m->DS, (m->SP - 1) * sizeof(int));
    d[m->SP - 1] = m->T;
    memcpy(r, m->RS, (m->RSP - 1) * sizeof(int));
    r[m->RSP - 1] = m->R;
    for(int idx=0; idx < m->SP; idx++) have |= cells_in(d[idx]);
    for(int idx=0; idx < m->RSP; idx++) have |= cells_in(r[idx]);
    int made = m->M < ROOM && w->stores_to[m->M] >= 0 ? w->stores_to[m->M] : 0;
    if ((w->cells | w->cells_from[made]) & ~have) return(FEW_NEVER);    // a cell the target needs has been lost
    int values = w->stores - made;
    for(int f=0; f < w->finals; f++) values += !on_a_stack(m, w->final[f]);
    if (w->fact >= 0) {
        int there = 0;
        for(int idx=0; idx < m->SP && !there; idx++) there = is_the_fact(w, d[idx]);
        for(int idx=0; idx < m->RSP && !there; idx++) there = is_the_fact(w, r[idx]);
        values += !there;
    }
    int to_size = l->SP - (w->fact >= 0);
    memcpy(to_d, l->DS, (l->SP - 1) * sizeof(int));
    if (w->fact < 0) to_d[l->SP - 1] = l->T;
    memcpy(to_r, l->RS, (l->RSP - 1) * sizeof(int));
    to_r[l->RSP - 1] = l->R;
    int data = to_shape(d, m->SP, to_d, to_size, w->fact >= 0);
    int ret = to_shape(r, m->RSP, to_r, l->RSP, 0);
    return(values > data ? (values > ret ? values : ret) : (data > ret ? data : ret));
}

// ---- the search

typedef struct { uint64_t state; int stamp; int left; } was_seen;
static was_seen* SEEN_WITH;             // a hash table of the states visited, kept at most half full
static int seen_bits;                   // the table has 2^seen_bits slots
static long seen_now;                   // the number of states this search has visited
static int searching;                   // the number of the current search: entries stamped by earlier searches are ignored
static long states_left;                // the number of states this search may still examine
static int gave_up;                     // set when a search has run out of states

static uint64_t state_of(const named* m) {
    uint64_t h = mix(mix(mix(mix(mix(7, m->SP), m->RSP), KNOWN[m->T].is), KNOWN[m->R].is), KNOWN[m->M].is);
    for(int idx=0; idx < m->SP - 1; idx++) h = mix(h, KNOWN[m->DS[idx]].is);
    for(int idx=0; idx < m->RSP - 1; idx++) h = mix(h, KNOWN[m->RS[idx]].is);
    return(h | 1);
}

static was_seen* seen_at(uint64_t state) {      // returns a state's slot in this search's table, or the empty slot for it
    uint64_t at = (state * 0x9e3779b97f4a7c15ull) >> (8 * sizeof(uint64_t) - seen_bits);
    while (SEEN_WITH[at].stamp == searching && SEEN_WITH[at].state != state) at = (at + 1) & (((uint64_t)1 << seen_bits) - 1);
    return(&SEEN_WITH[at]);
}

// Returns 1 if a state was already visited with at least as many
// instructions left.  Otherwise records the visit and returns 0.
static int seen_with(const named* m, int left) {
    uint64_t state = state_of(m);
    if (!SEEN_WITH || 2 * (seen_now + 1) > (long)1 << seen_bits) {      // grow the table to keep it at most half full
        was_seen* had = SEEN_WITH;
        long was = had ? (long)1 << seen_bits : 0;
        seen_bits = had ? seen_bits + 1 : FEW_SEEN_BITS;
        SEEN_WITH = room_for((size_t)1 << seen_bits, sizeof(was_seen));
        for(long idx=0; idx < was; idx++) if (had[idx].stamp == searching) *seen_at(had[idx].state) = had[idx];
        free(had);
    }
    was_seen* is = seen_at(state);
    if (is->stamp == searching && is->left >= left) return(1);
    if (is->stamp != searching) seen_now++;
    *is = (was_seen){ state, searching, left };
    return(0);
}

// Returns 1 if a fetch from the current memory is one the target also
// makes: it reads an address the target reads, and the term it reads
// existed when the target was set.
static int fetch_is_wanted(const wanted* w, const named* m, int place) {
    int allowed = 0;
    for(int p=0; p < w->n_places; p++) allowed |= w->places[p] == place;
    if (!allowed) return(0);
    if (KNOWN[m->M].kind == STORED && KNOWN[m->M].of[1] == place) return(1);
    for(int t=FIXED; t < w->base; t++) {
        if (KNOWN[t].kind == FETCHED && KNOWN[t].of[0] == m->M && KNOWN[t].of[1] == place) return(1);
    }
    return(0);
}

static int look(wanted* w, const named* m, int left, uint16_t* path, int at);

// An optional restriction on the last instruction of an answer, such as
// "a return can be folded into it".  It is indexed by instruction, and is
// 1 for the instructions allowed.  If it is NULL, any instruction may come
// last.
static const unsigned char* ends_in;

// Steps one instruction from a state and, if the result is worth pursuing,
// continues the search from it.  Returns 1 if an answer was found.
static int on(wanted* w, const named* m, uint16_t x, int left, uint16_t* path, int at) {
    if (left == 1 && ends_in && !ends_in[x]) return(0);
    named s = *m;
    step_named(&s, x);
    if (s.unsaid || lost || s.next >= 0) { lost = 0; return(0); }     // the step also reports a stack deeper than it has terms for
    if (s.M != m->M && (s.M >= ROOM || w->stores_to[s.M] < 0)) return(0);       // a store that is not the next store the target needs
    for(int idx=m->fetches; idx < s.fetches; idx++) if (!fetch_is_wanted(w, m, s.fetched[idx])) return(0);
    if (seen_with(&s, left - 1)) return(0);
    if (at_least(w, &s) > left - 1) return(0);
    path[at] = x;
    return(look(w, &s, left - 1, path, at + 1));
}

// Returns 1 if a value an instruction produced is useful: it is part of
// the target, or already on a stack, or zero exactly when the top must be.
static int worth_having(wanted* w, const named* m, int made) {
    return((made < w->base && w->needed[made]) || on_a_stack(m, made) || is_the_fact(w, made));
}

// Tries every instruction from a state, with `left` instructions
// remaining.  Returns 1 if an answer was found, with its instructions in
// `path`.
static int look(wanted* w, const named* m, int left, uint16_t* path, int at) {
    if (!left) return(is_wanted(w, m));
    if (gave_up || --states_left < 0) { gave_up = 1; return(0); }
    int had = known;
    for(int f=0; f < families; f++) {
        named probe = *m;
        if (f < alike) step_named(&probe, FAMILY[f].probe);
        if (f >= alike || (!probe.unsaid && !lost && worth_having(w, m, probe.T))) {
            for(int idx=0; idx < FAMILY[f].n; idx++) {
                if (on(w, m, MEMBER[FAMILY[f].first + idx], left, path, at)) return(1);
            }
        }
        lost = 0;
        known = had;
    }
    for(int idx=0; idx < w->n_pushes; idx++) {
        if (on(w, m, w->pushes[idx], left, path, at)) return(1);
        known = had;
    }
    for(int idx=0; idx < w->n_adds; idx++) {
        named probe = *m;
        step_named(&probe, w->adds[idx]);
        if (!probe.unsaid && !lost && worth_having(w, m, probe.T) && on(w, m, w->adds[idx], left, path, at)) return(1);
        lost = 0;
        known = had;
    }
    for(int idx=0; idx < w->n_fetches; idx++) {
        if (on(w, m, w->fetches[idx], left, path, at)) return(1);
        known = had;
    }
    for(int idx=0; idx < w->n_stores_var; idx++) {
        if (on(w, m, w->stores_var[idx], left, path, at)) return(1);
        known = had;
    }
    return(0);
}

// Searches for exactly `length` instructions that reach the target from a
// state, or from the starting state if `from` is NULL.  Returns 1 and
// writes them to `found` if there are such instructions.  `*given_up` is
// set if the search stopped before examining everything.
static int exactly(wanted* w, const named* from, int length, long states, uint16_t* found, int* given_up) {
    named first;
    if (from) first = *from; else start(&first);
    searching++;
    seen_now = 0;
    states_left = states;
    gave_up = 0;
    known = w->base;
    lost = 0;
    int is = at_least(w, &first) <= length && look(w, &first, length, found, 0);
    known = w->base;
    lost = 0;
    *given_up = gave_up;
    return(is);
}

// ---- on the machine

typedef struct {
    int64_t d[FILLED], r[FILLED];
    int sp, rsp, went_on;
    uint64_t data;
} leaving;

// Runs sequence `which` from a state and saves its result in `l`.  Returns
// 0 if the machine stopped early.
static int left_by(int which, int len, const state* from, leaving* l) {
    if (!run(which, len, from)) return(0);
    l->sp = machine.SP < FILLED ? machine.SP : FILLED;
    l->rsp = machine.RSP < FILLED ? machine.RSP : FILLED;
    memcpy(l->d, machine.DSTACK, sizeof(l->d));
    memcpy(l->r, machine.RSTACK, sizeof(l->r));
    l->went_on = machine.EIP == after[which];
    l->data = 0;
    for(int idx=0; idx < DATA_CELLS; idx++) l->data = mix(l->data, mem_load(&machine, WHERE(idx)));
    for(int idx=0; idx < far.n; idx++) l->data = mix(l->data, mem_load(&machine, far.at[idx]));
    return(1);
}

// in_machine() returns a cell's index in the machine's stack, given its
// term: the machine starts DEPTH deep, and the terms start DEEP deep.
// cell_in() returns a pointer to that cell in a state, or NULL if the
// state has no such cell.
static int in_machine(int cell) { return(CELL_OF(cell) - (DEEP - DEPTH)); }

static int64_t* cell_in(state* s, int cell) {
    int idx = in_machine(cell);
    return(idx < 0 || idx >= FILLED ? NULL : OF_DATA(cell) ? &s->dstack[idx] : &s->rstack[idx]);
}

// Runs two sequences on vm.c from one state and compares the results.
// Returns 1 if they agree, 0 if they differ, and -1 if either stopped
// early.
//
// With `to`, the second sequence keeps its values in other cells: it is
// started with each value the first keeps in a cell moved to the cell `to`
// gives.  It must leave in each such cell what the first leaves in the
// original cell, leave every other cell unchanged, and leave the same
// memory.  If `tops` is set, the two tops only have to be both zero or
// both non-zero (or, with `other_way`, exactly one of them zero).
//
// Without `to`, every cell must be the same, and the second sequence may
// leave `over` more cells on the return stack.
static int agree_from(int alen, int blen, const state* from, const int* to, int tops, int other_way, int over) {
    state with = *from;
    leaving a, b;
    for(int cell=0; to && cell < CELL_TERMS; cell++) {
        int64_t* is = cell_in((state*)from, cell);
        int64_t* kept = cell_in(&with, to[cell]);
        if (to[cell] != cell && is && kept) *kept = *is;
    }
    int ran = left_by(0, alen, from, &a), ran_too = left_by(1, blen, &with, &b);
    if (ran != ran_too) return(0);
    if (!ran) return(-1);
    if (!a.went_on || !b.went_on || a.sp != b.sp || b.rsp != a.rsp + over || a.data != b.data) return(0);
    if (tops && ((a.d[a.sp - 1] == 0) != ((b.d[b.sp - 1] == 0) != (other_way != 0)))) return(0);
    if (!to) {
        // skip the bottom cell of the data stack, which holds the lead-in's own value (lay_leads(), fuse.h)
        for(int idx=1; idx < a.sp - (tops != 0); idx++) if (a.d[idx] != b.d[idx]) return(0);
        for(int idx=0; idx < a.rsp; idx++) if (a.r[idx] != b.r[idx]) return(0);
        return(1);
    }
    state expected = with;
    for(int cell=0; cell < CELL_TERMS; cell++) {
        int data = OF_DATA(cell), idx = in_machine(cell);
        int64_t* kept = cell_in(&expected, to[cell]);
        if (idx < 0 || idx >= DEPTH || !kept) continue;
        if (to[cell] != cell || (data ? a.d[idx] != from->dstack[idx] : a.r[idx] != from->rstack[idx])) {
            *kept = data ? a.d[idx] : a.r[idx];
        }
    }
    for(int idx=1; idx < DEPTH; idx++) if (b.d[idx] != expected.dstack[idx]) return(0);
    for(int idx=0; idx < DEPTH; idx++) if (b.r[idx] != expected.rstack[idx]) return(0);
    return(1);
}

// Runs two sequences on vm.c from every random state, and from every
// assignment of three numbers (0, 1 and TRUE) to the cells the first
// sequence keeps values in.  The assignments are needed because a
// comparison is rarely true by chance (fuse.h explains this for its own
// check).  Returns 1 if the sequences agree from every state and both ran
// to the end from enough of them, 0 if they disagree, and -1 if they ran
// from too few states to tell.
static int agree_on_machine(const uint16_t* a, int alen, const uint16_t* b, int blen,
                            const int* to, const int* kept, int n_kept, int tops, int other_way, int over) {
    if (alen > LAID || blen > LAID) return(-1);
    // Give the machine a cell for each literal address the two sequences
    // use, as tried() does in fuse.h.
    int had = known, was_lost = lost;
    const uint16_t* both[2] = { a, b };
    const int len[2] = { alen, blen };
    far.n = 0;
    for(int s=0; s < 2; s++) {
        named m;
        start(&m);
        for(int idx=0; idx < len[s]; idx++) step_named(&m, both[s][idx]);
        for(int idx=0; idx < m.fetches; idx++) give_cell(m.fetched[idx]);
        for(int mem=m.M; KNOWN[mem].kind == STORED; mem = KNOWN[mem].of[0]) give_cell(KNOWN[mem].of[1]);
    }
    known = had;
    lost = was_lost;
    lay(0, a, alen);
    lay(1, b, blen);
    int ran_from = 0;
    for(int s=0; s < STATES; s++) {
        int is = agree_from(alen, blen, &STATE[s], to, tops, other_way, over);
        if (!is) return(0);
        ran_from += is > 0;
    }
    static const int64_t FEW[] = { 0, 1, TRUE };
    uint64_t ways = 1;
    for(int idx=0; idx < n_kept; idx++) ways *= COUNT(FEW);
    for(int base=0; base < STATES && n_kept; base++) {
        for(uint64_t way=0; way < ways; way++) {
            state with = STATE[base];
            uint64_t rest = way;
            for(int idx=0; idx < n_kept; idx++, rest /= COUNT(FEW)) {
                int64_t* given = cell_in(&with, kept[idx]);
                if (given) *given = cell(FEW[rest % COUNT(FEW)]);
            }
            int is = agree_from(alen, blen, &with, to, tops, other_way, over);
            if (!is) return(0);
            ran_from += is > 0;
        }
    }
    return(ran_from >= ENOUGH ? 1 : -1);
}

#endif // HEXAFORTH_FEWER_H
