//
// fewer.c - the fewest instructions that leave what is wanted, for the
// tools that have this in them.  It is had after fuse.c, by which it asks
// the machine, and kinds.c, which lists the instructions that have a
// number in them.
//
// What is wanted is said as fuse.c says what a run leaves: the terms in
// the cells of the two stacks, memory as a term, and where it fetched
// from (a `named`).  It may be wanted with the top of the data stack
// only as far as whether it is nothing, which is all a branch asks of
// it: then two that leave tops that are nothing from the same states
// leave what is wanted, whatever the tops are otherwise.
//
// The search is of every instruction the step says of, the numbers and
// the variables that what is wanted has in it.  It does not know what an
// instruction does: each is stepped (fuse.c, which is vm_step.h on terms)
// and what the step leaves is looked at.  An instruction is gone on from
// only if what it made is part of what is wanted, or was there already,
// or is nothing when the top is to be; and a line of search is given up
// when what is left to do cannot be done in the instructions left.  It
// is given up altogether after so many states (`nodes`), and says so.
//
// What is found is found by terms, which say it is so from every state
// if the step and what fuse.c takes to be so of the arithmetic are right.
// So it is also run on vm.c, beside what it is to do the work of
// (agree_on_machine): a thing made otherwise, which nothing here is built
// from.
//
// A turn of a loop keeps things from one turn to the next in cells of
// the stacks.  kept_as says what a turn is to leave if each of those is
// kept in another cell: the same terms, with the cells they are made of
// and the cells they are left in renamed.
//

// How much is looked at is not the machine's to say: these were chosen.
#define FEW_SEEN_BITS   20              // the states a search remembers having come to: two to this many
#define FEW_SEEN        (1 << FEW_SEEN_BITS)
#define FEW_LIVE        6               // the most cells a turn may keep things in, to be run from few numbers,
#define FEW_FROM        4               // and the states of fuse.c's that they are given in
#define FEW_STORES      32              // the most stores in what is wanted
#define FEW_DEEPER      3               // a line of search goes no more than so many cells deeper than it is to end
#define FEW_NEVER       INT_MAX         // how many instructions it takes to do what cannot be done

#include <limits.h>

// ---- the cells of the stacks, by their terms (fuse.c says which term is which cell's)

#define DEPTH_OF(cell)        (DEEP - 1 - CELL_OF(cell))        // how far a cell is under the top, at the start
#define CELL_OF_DATA(depth)   DATA_TERM(DEEP - 1 - (depth))     // the term for a cell of the data stack, by how far
#define CELL_OF_RETURN(depth) RETURN_TERM(DEEP - 1 - (depth))   // under; and of the return stack

typedef uint64_t cell_set;              // some of the cells: a bit a cell's term
_Static_assert(CELL_TERMS <= 8 * sizeof(cell_set), "fewer.c: a set of cells has too few bits for the cells there are");
#define IN_SET(cell)          ((cell_set)1 << (cell))

// The term in a cell of what was left, and the cell given a term.
static int* cell_left(named* m, int cell) {
    int idx = CELL_OF(cell);
    if (OF_DATA(cell)) return(idx == m->SP - 1 ? &m->T : &m->DS[idx]);
    return(idx == m->RSP - 1 ? &m->R : &m->RS[idx]);
}

// ---- the instructions of the ALU, by what they make

// Many instructions make the same thing and put it in different places:
// those that take the same input through the same operation.  One of them
// that puts it on top and moves neither stack is stepped first, and the
// rest only if what it made is worth having.  That they make the same
// thing is asked of the step when they are listed (makes_as): one that
// does not is by itself, and is always stepped.
typedef struct { uint16_t probe; int first, n; } family;        // its members are so many of MEMBER, from the first
static family* FAMILY;
static uint16_t* MEMBER;
static int families, alike;             // how many there are; and how many are of instructions that make the same
                                        // thing, which are first: the rest are each one instruction by itself

// Does an instruction make what another puts on top, and nothing else?
// Both are stepped from the start: the cells of the stacks and what was
// stored are then to have only what was there at the start, and what the
// other left on top.
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

// It is had after kinds.c, and the kinds are listed before this is called.
// The instructions that do not return are put in order, those of a family
// together: each is in the family of the first that takes its input
// through its operation, puts what it makes on top and leaves everything
// else as it was, if it makes what that one does; and if not it is a
// family by itself.
static void fewer_ready(void) {
    static int probe_of[INPUTS][OPS];                   // by input and operation, where in ALU
    int* family_at = room_for(alus, sizeof(int));       // the family each instruction is in, or less than nothing
    int* by_probe = room_for(alus, sizeof(int));        // by where a probe is in ALU, its family
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
    for(int f=0, first=0; f < families; f++) {          // where each one's members start; and none are there yet
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

// ---- terms: the cells one is made of, and one with its cells renamed

// The cells of the stacks, as they were at the start, that a term is made
// of.
static cell_set cells_in(int t) {
    const term* is = &KNOWN[t];
    if (is->kind == IN_DATA || is->kind == IN_RETURN) return(t < CELL_TERMS ? IN_SET(t) : 0);
    cell_set in = 0;
    for(int idx=0; idx < is->n; idx++) in |= cells_in(is->of[idx]);
    return(in);
}

// Whether a term is nothing, as a term: a flag that says so.  Two terms
// are nothing from the same states if this is the same for both.
static int nothing(int t) {
    return(t_made(ALU_EQ, t, t_number(0)));
}

static int not_flag(int flag) {
    for(int op=0; op < OPS; op++) if (ALU_UNDOES(op)) return(t_made(op, flag, flag));
    return(flag);
}

// A term with the cells it is made of renamed: `to` is, for each cell's
// term, the cell's term it is to be.  It is made again of the renamed
// parts, as the step would have made it.
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

// ---- a turn of a loop: the cells it keeps things in, and the same turn with them kept elsewhere

// How many cells at the top of each stack a turn reads or changes.  The
// turn left the data stack one deeper, with what the branch asks about
// on top.
static void cells_kept(const named* turn, int* of_data, int* of_return) {
    named left = *turn;
    cell_set used = cells_in(left.T);
    for(int mem=left.M; KNOWN[mem].kind == STORED; mem = KNOWN[mem].of[0]) {
        used |= cells_in(KNOWN[mem].of[1]) | cells_in(KNOWN[mem].of[2]);
    }
    for(int idx=0; idx < left.fetches; idx++) used |= cells_in(left.fetched[idx]);
    for(int cell=0; cell < CELL_TERMS; cell++) {
        if (DEPTH_OF(cell) < 0) continue;               // above where the turn started
        int is = *cell_left(&left, cell);
        if (is != cell) used |= cells_in(is) | IN_SET(cell);
    }
    *of_data = *of_return = 0;
    for(int depth=0; depth < DEEP; depth++) {
        if (used & IN_SET(CELL_OF_DATA(depth))) *of_data = depth + 1;
        if (used & IN_SET(CELL_OF_RETURN(depth))) *of_return = depth + 1;
    }
}

// What a turn is to leave when what it keeps is kept as `to` says: `to`
// is, for each cell's term, the cell it is kept in, and is the cell itself
// for those the turn does not keep anything in.  What was in a cell is
// left in the cell it is renamed to, made of the renamed cells; every
// other cell is as it was.
static void kept_as(const named* turn, const int* to, named* wanted) {
    named left = *turn;
    start(wanted);
    wanted->SP = left.SP;
    wanted->RSP = left.RSP;
    wanted->DS[wanted->SP - 1] = wanted->T;             // the cell that was on top is under what the branch asks about
    for(int cell=0; cell < CELL_TERMS; cell++) {
        if (to[cell] == cell && *cell_left(&left, cell) == cell) continue;
        if (DEPTH_OF(cell) < 0) continue;               // above where the turn started
        *cell_left(wanted, to[cell]) = renamed(*cell_left(&left, cell), to);
    }
    wanted->T = renamed(left.T, to);
    wanted->R = wanted->RS[wanted->RSP - 1];
    wanted->M = renamed(left.M, to);
    wanted->fetches = 0;
    for(int idx=0; idx < left.fetches; idx++) wanted->fetched[wanted->fetches++] = renamed(left.fetched[idx], to);
}

// ---- what is wanted

typedef struct {
    named left;                         // what is to be left
    int fact;                           // the flag for whether the top is nothing, if that is all that is wanted of
                                        // the top; or less than nothing
    int base;                           // the terms there were when this was said: a later one is not part of it
    unsigned char needed[ROOM];         // the terms it is made of, and the parts of each
    short stores_to[ROOM];              // for memory as it is after so many of its stores, how many; or less
    int stores;
    int final[CELL_TERMS], finals;      // the terms its stacks have, each once
    cell_set cells;                     // the cells its stacks are made of, and what is asked of the top,
    cell_set cells_from[FEW_STORES + 1];        // and those its stores are, after so many of them are made
    int places[FETCHES], n_places;      // where it fetches from
    uint16_t pushes[ROOM], adds[ROOM];  // the instructions for its numbers: no more than the terms there can be,
    uint16_t fetches[FETCHES], stores_var[FEW_STORES];  // and for its variables: than its places, and its stores
    int n_pushes, n_adds, n_fetches, n_stores_var;
    int nothing_of[ROOM];               // of the terms there were: the flag for whether each is nothing, when asked
} wanted;

// Everything a term is made from; and of a run of one operation, what
// each part of the run makes.
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

// Say what is wanted.  The terms it is made of are there, and are not
// forgotten while it is looked for.  False if there was no room for them.
static int want(wanted* w, const named* left, int fact) {
    memset(w, 0, sizeof(wanted));
    w->left = *left;
    w->fact = fact;
    memset(w->stores_to, -1, sizeof(w->stores_to));
    int chain[ROOM], links = 0;
    for(int mem=left->M; KNOWN[mem].kind == STORED && links < ROOM; mem = KNOWN[mem].of[0]) chain[links++] = mem;
    if (links > FEW_STORES) return(0);
    w->stores = links;
    w->stores_to[MEMORY_TERM] = 0;
    for(int idx=0; idx < links; idx++) w->stores_to[chain[links - 1 - idx]] = idx + 1;

    named l = *left;
    for(int cell=0; cell < CELL_TERMS; cell++) {
        int data = OF_DATA(cell), idx = CELL_OF(cell);
        if (idx >= (data ? l.SP : l.RSP)) continue;
        if (data && idx == l.SP - 1 && fact >= 0) continue;     // the top, which is wanted only for whether it is nothing
        int is = *cell_left(&l, cell), had = 0;
        parts(w, is);
        w->cells |= cells_in(is);
        for(int f=0; f < w->finals; f++) had |= w->final[f] == is;
        if (!had) w->final[w->finals++] = is;
    }
    for(int idx=0; idx < links; idx++) {                // from the last store back: a store that is made
        parts(w, KNOWN[chain[idx]].of[1]);              // needs its cells no longer
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

    // The numbers and the variables it has in it, as the instructions
    // that push, add, fetch and store them.
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

// Is a term nothing when the top is wanted to be?  What is made to find
// out is forgotten again, but for a term there was when what is wanted
// was said, which is asked once.
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

// Has what was left what is wanted?
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

// The instructions needed to make a stack into another, with so many more
// cells on it: a cell can be written only when it is the top, or just
// under it.
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

// No fewer instructions than this are still needed.
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
    if ((w->cells | w->cells_from[made]) & ~have) return(FEW_NEVER);    // something needed is gone for good
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
static was_seen* SEEN_WITH;
static int searching;                   // which search this is: what an earlier one saw is not seen
static long states_left;
static int gave_up;

static uint64_t state_of(const named* m) {
    uint64_t h = mix(mix(mix(mix(mix(7, m->SP), m->RSP), KNOWN[m->T].is), KNOWN[m->R].is), KNOWN[m->M].is);
    for(int idx=0; idx < m->SP - 1; idx++) h = mix(h, KNOWN[m->DS[idx]].is);
    for(int idx=0; idx < m->RSP - 1; idx++) h = mix(h, KNOWN[m->RS[idx]].is);
    return(h | 1);
}

// Has this been come to before with as many instructions left, or more?
// It has now.
static int seen_with(const named* m, int left) {
    uint64_t state = state_of(m), at = (state * 0x9e3779b97f4a7c15ull) >> (64 - FEW_SEEN_BITS);
    while (SEEN_WITH[at].stamp == searching && SEEN_WITH[at].state != state) at = (at + 1) & (FEW_SEEN - 1);
    if (SEEN_WITH[at].stamp == searching && SEEN_WITH[at].left >= left) return(1);
    SEEN_WITH[at] = (was_seen){ state, searching, left };
    return(0);
}

// Is a fetch from memory as it is one that what is wanted makes too?  It
// fetches from where that does, and what it fetches was there when what
// is wanted was said.
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

// One instruction on from a state, if what it leaves is worth going on
// from; and on from there.
static int on(wanted* w, const named* m, uint16_t x, int left, uint16_t* path, int at) {
    named s = *m;
    step_named(&s, x);
    if (s.unsaid || lost || s.next >= 0) { lost = 0; return(0); }
    if (s.SP > DEEP + FEW_DEEPER || s.RSP > w->left.RSP + FEW_DEEPER) return(0);
    if (s.M != m->M && (s.M >= ROOM || w->stores_to[s.M] < 0)) return(0);       // a store that is not the next one wanted
    for(int idx=m->fetches; idx < s.fetches; idx++) if (!fetch_is_wanted(w, m, s.fetched[idx])) return(0);
    if (seen_with(&s, left - 1)) return(0);
    if (at_least(w, &s) > left - 1) return(0);
    path[at] = x;
    return(look(w, &s, left - 1, path, at + 1));
}

// Is what an instruction made worth having: part of what is wanted, or
// there already, or nothing when the top is to be?
static int worth_having(wanted* w, const named* m, int made) {
    return((made < w->base && w->needed[made]) || on_a_stack(m, made) || is_the_fact(w, made));
}

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

// So many instructions that leave what is wanted, from a state, which is
// the start if none is given: found, or not.  *given_up says whether the
// search was given up before it had looked at everything.
static int exactly(wanted* w, const named* from, int length, long states, uint16_t* found, int* given_up) {
    named first;
    if (!SEEN_WITH) SEEN_WITH = calloc(FEW_SEEN, sizeof(was_seen));
    if (from) first = *from; else start(&first);
    searching++;
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

// Run one of the two sequences in memory from a state, and keep what it
// left.  False if the machine stopped.
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

// Where in the machine's stack a cell is, by its term: the machine starts
// DEPTH deep, and the terms DEEP.  And the cell of a state, if the state
// gives one there.
static int in_machine(int cell) { return(CELL_OF(cell) - (DEEP - DEPTH)); }

static int64_t* cell_in(state* s, int cell) {
    int idx = in_machine(cell);
    return(idx < 0 || idx >= FILLED ? NULL : OF_DATA(cell) ? &s->dstack[idx] : &s->rstack[idx]);
}

// Do two sequences agree from a state, run on vm.c?  The second is run
// with what the first keeps in a cell kept where `to` says, and is to
// leave in each such cell what the first leaves in the cell it was, and
// every other cell as it was; the same memory; and the tops both nothing
// or neither (or one of them only, with `other_way`), if that is all that
// is wanted of the tops.  With no `to`, every cell is to be the same, and
// the second may leave `over` more cells on the return stack.  1 if they
// agree, 0 if not, and less than nothing if either stopped.
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
        // the cell at the bottom of the data stack is what sets the depths' own (fuse.c, lay_leads)
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

// Do two sequences agree, run on vm.c from every state there is here, and
// from every way of giving the cells the first keeps things in one of
// three numbers: nothing, one, and the flag that says yes?  A flag is
// seldom yes by chance (fuse.c says so of its own).  1 if they agree
// from every state, and both ran to the end from enough of them; 0 if
// they do not agree; less than nothing if they ran from too few to say.
static int agree_on_machine(const uint16_t* a, int alen, const uint16_t* b, int blen,
                            const int* to, const int* kept, int n_kept, int tops, int other_way, int over) {
    if (alen > RUN || blen > RUN) return(-1);
    // The cells the two name as numbers are given to the machine, as
    // fuse.c gives them (tried).
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
    int ways = 1;
    for(int idx=0; idx < n_kept && idx < FEW_LIVE; idx++) ways *= COUNT(FEW);
    for(int base=0; base < FEW_FROM && n_kept; base++) {
        for(int way=0; way < ways; way++) {
            state with = STATE[base];
            for(int idx=0, rest=way; idx < n_kept && idx < FEW_LIVE; idx++, rest /= COUNT(FEW)) {
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
