//
// builds_test PLAIN.hex PLAIN.tables IMAGE.hex IMAGE.tables
//
// Compares the image cross.fs builds with its peephole on against the
// image it builds with the peephole off.  Every straight-line run of one
// must end in the same state as the same run of the other, and every
// call, jump and branch must go to the same place.  Run it after any
// change to a compiler or a peephole.
//
// Both images are cut into runs and into the instructions that leave a run
// (cut.h).  The two must have the same words, and each word must have the
// same runs and the same calls in the same order.  The words themselves
// need not be in the same address order: the image built with the peephole
// can go back to an earlier page to place code, and the plain image never
// does.  The machine decides whether two runs are equivalent: the step on
// terms gives the term in every cell after each run (fuse.h), and two runs
// that end with the same terms end in the same state from every starting
// state.
//
//   A run followed by a conditional branch only has to match the other
//   run in everything but the top of the stack, and to leave the top zero
//   exactly when the other does.  So `0 =` is appended to both runs before
//   they are compared, which is the test the branch makes.
//
//   Code is not at the same addresses in the two images.  A number in the
//   plain run that equals the address of something in the plain image may
//   be that address, pushed, or may be an unrelated number that happens to
//   equal it.  So a run is tried as it is, and then with each such number
//   replaced by the corresponding address in the other image, in every
//   combination.
//
//   The step does not model a read or a write of a port.  A run that
//   contains one is compared in pieces: the stretches between the port
//   instructions.  The port instructions themselves must be identical in
//   both runs, except that the last one may have a return folded into it
//   where the plain run has an exit after it.
//
// It prints how many runs and how many calls, jumps and branches it
// compared.  It fails if a pair is not shown to be equivalent, if there
// was nothing to compare, or if the two images have the same number of
// instructions, because then the peephole was not off for the plain one.
//

#include "../util/fuse.h"
#include "../util/cut.h"

static cut_image PLAIN, BUILT;
static uint16_t EXIT;                   // the exit instruction

#define MAY_BE_PLACES 12                // the most numbers in a run that are tried as addresses.  Every combination
                                        // of them is tried, so the limit is a choice made here, not the machine's

// Returns 1 if two runs are identical, or the step shows that they end in
// the same state.
static int leave_the_same(const uint16_t* a, int alen, const uint16_t* b, int blen) {
    if (alen == blen && !memcmp(a, b, alen * sizeof(uint16_t))) return(1);
    named x, y;
    forget();
    uint64_t left = names(a, alen, &x);
    return(left && names(b, blen, &y) == left && same_named(&x, &y));
}

// The same test, for two runs that are each followed by a conditional
// branch: the branch's test, `0 =`, is appended to both first.
static int leave_the_same_to_a_branch(const uint16_t* a, int alen, const uint16_t* b, int blen) {
    // Each copy has room for the run, a literal (NUMBER_ROOM in fuse.h is
    // the most instructions one takes) and the `=`.
    uint16_t* x = room_for(alen + NUMBER_ROOM + 1, sizeof(uint16_t));
    uint16_t* y = room_for(blen + NUMBER_ROOM + 1, sizeof(uint16_t));
    memcpy(x, a, alen * sizeof(uint16_t));
    memcpy(y, b, blen * sizeof(uint16_t));
    int asks = write_number(0, 0, x + alen);
    x[alen + asks] = IS_SAME;
    memcpy(y + blen, x + alen, (asks + 1) * sizeof(uint16_t));
    int same = leave_the_same(x, alen + asks + 1, y, blen + asks + 1);
    free(x);
    free(y);
    return(same);
}

// Returns an instruction with its return removed.
static uint16_t without_return(uint16_t w) {
    instruction ins = ins_of(w);
    ins.alu.r_eip = 0;
    ins.alu.rstack = 0;
    return(*(uint16_t*)&ins);
}
// Returns 1 if an instruction reads or writes a port, which the step does
// not model.
static int of_a_port(uint16_t w) {
    instruction ins = ins_of(w);
    return(!ins.lit.lit_f && ins.alu.op_type == OP_TYPE_ALU && alu_at[w] < 0 && alu_at[without_return(w)] < 0);
}

// Compares two runs piece by piece, where the pieces are the stretches
// between port instructions.  The port instructions must be identical and
// in the same order in both runs.  The exception is the last one: `b` may
// have it with a return folded in where `a`, the plain run, has it
// followed by an exit.
static int same_in_pieces(const uint16_t* a, int alen, const uint16_t* b, int blen, int branch) {
    int i = 0, j = 0;
    for(;;) {
        int ie = i, je = j;
        while (ie < alen && !of_a_port(a[ie])) ie++;
        while (je < blen && !of_a_port(b[je])) je++;
        int last = ie == alen || je == blen;
        if (last && branch ? !leave_the_same_to_a_branch(a + i, ie - i, b + j, je - j) : !leave_the_same(a + i, ie - i, b + j, je - j)) return(0);
        if (last) return(ie == alen && je == blen);
        if (a[ie] != b[je]) {
            return(ins_of(b[je]).alu.r_eip && without_return(b[je]) == a[ie] && je + 1 == blen &&
                   ie + 2 == alen && a[ie + 1] == EXIT);
        }
        i = ie + 1; j = je + 1;
    }
}

// Compares two runs as they are: whole, then with the branch's test
// appended, then in pieces.  `*pieces` is set to 1 if the last of those
// was needed.
static int same_one_way(const uint16_t* a, int alen, const uint16_t* b, int blen, int branch, int* pieces) {
    *pieces = 0;
    if (leave_the_same(a, alen, b, blen)) return(1);
    if (branch && leave_the_same_to_a_branch(a, alen, b, blen)) return(1);
    *pieces = 1;
    return(same_in_pieces(a, alen, b, blen, branch));
}

// Returns 1 if a run pushes a given number.
static int pushed_in(const uint16_t* run, int len, long number) {
    for(int at=0; at < len; ) {
        int end = at + 1;
        if (pushes_number(run[at], 0)) {
            while (end < len && pushes_number(run[end], 1)) end++;
            long pushed = 0;
            for(int idx=at; idx < end; idx++) pushed += number_in(run[idx]);
            if (pushed == number) return(1);
        }
        at = end;
    }
    return(0);
}

// Finds the numbers in a plain run that equal an address in the plain
// image.  For each it records where the number is in the run, and the
// corresponding address in the other image.  If the other run is given,
// it keeps only the numbers whose corresponding address the other run
// pushes: a number that really is an address appears in the other run as
// the new address, and a number that only happens to equal one appears
// there unchanged.
typedef struct { int from, upto; long there; } spot;

static int places_in(const uint16_t* run, int len, const uint16_t* other, int other_len, spot* out) {
    int n = 0;
    for(int at=0; at < len && n < MAY_BE_PLACES; ) {
        int end = at + 1;
        if (pushes_number(run[at], 0)) {
            while (end < len && pushes_number(run[end], 1)) end++;
            long pushed = 0;
            for(int idx=at; idx < end; idx++) pushed += number_in(run[idx]);
            long is = pushed ? item_at(&PLAIN, pushed) : -1;
            if (is >= 0 && BUILT.items[is].at != pushed &&
                (!other || pushed_in(other, other_len, BUILT.items[is].at))) {
                out[n].from = at; out[n].upto = end; out[n].there = BUILT.items[is].at;
                n++;
            }
        }
        at = end;
    }
    return(n);
}

// Writes to `out` the run with some of those numbers replaced by their
// addresses in the other image: the ones whose bit is set in `which`.
// Returns the new length.
static int with_places(const uint16_t* run, int len, const spot* places, int n, unsigned which, uint16_t* out) {
    int made = 0, p = 0;
    for(int at=0; at < len; ) {
        while (p < n && places[p].from < at) p++;
        if (p < n && places[p].from == at && (which >> p & 1)) {
            int wrote = write_number(places[p].there, 0, out + made);
            if (wrote > 0) { made += wrote; at = places[p].upto; continue; }
        }
        out[made++] = run[at++];
    }
    return(made);
}

// Returns 1 if two runs are shown to be equivalent, and sets `*pieces` to
// 1 if that was shown piece by piece.
//
// A run with a great many numbers in it (one that sets a table of
// variables) can have more numbers that happen to equal an address than
// MAY_BE_PLACES, with the number that really is an address after all of
// them.  So when the first search reaches that limit, a second search
// tries the numbers whose corresponding address the other run pushes.
static int runs_leave_the_same(const uint16_t* plain, int plen, const uint16_t* built, int blen, int branch, int* pieces) {
    if (same_one_way(plain, plen, built, blen, branch, pieces)) return(1);
    spot places[MAY_BE_PLACES];
    // room for the run with each of those numbers rewritten as an address
    uint16_t* tried = room_for(plen + MAY_BE_PLACES * NUMBER_ROOM, sizeof(uint16_t));
    int same = 0;
    for(int likely=0; likely < 2 && !same; likely++) {
        int n = likely ? places_in(plain, plen, built, blen, places) : places_in(plain, plen, NULL, 0, places);
        for(unsigned which=1; which < 1u << n && !same; which++) {
            int len = with_places(plain, plen, places, n, which, tried);
            same = same_one_way(tried, len, built, blen, branch, pieces);
        }
        if (n < MAY_BE_PLACES) break;       // the limit was not reached, so every candidate was tried
    }
    free(tried);
    return(same);
}

// Returns 1 if two instructions that leave a run go to the same place:
// the same item in each image, or, if the target is an item in neither
// image, the same address.
static int go_to_the_same(long idx) {
    const item* p = &PLAIN.items[idx];
    const item* q = &BUILT.items[idx];
    // an item's last instruction is its call, jump or branch
    if (ins_of(PLAIN.code[p->at + p->len - 1]).jmp.op_type != ins_of(BUILT.code[q->at + q->len - 1]).jmp.op_type) return(0);
    long pi = item_at(&PLAIN, p->to), qi = item_at(&BUILT, q->to);
    if (pi >= 0 || qi >= 0) return(pi == qi);
    return(p->to == q->to);
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "usage: builds_test PLAIN.hex PLAIN.tables IMAGE.hex IMAGE.tables\n");
        return(1);
    }
    machine_from(argv[3]);              // take the cell size from the image before the step is used
    get_ready();
    EXIT = ins_named("exit");
    if (!EXIT) {
        fprintf(stderr, "builds_test: exit is not one instruction\n");
        return(1);
    }
    cut(&PLAIN, argv[1], argv[2]);
    cut(&BUILT, argv[3], argv[4]);
    long parted = part(&PLAIN, &BUILT);
    if (parted >= 0) {
        printf("FAIL the two images are not the same runs and calls in the same order: %ld and %ld, and they part at %lx and %lx\n",
               PLAIN.count, BUILT.count, PLAIN.items[parted].at, BUILT.items[parted].at);
        return(1);
    }

    long runs = 0, alike = 0, shown = 0, in_pieces = 0, leaves = 0, leaves_differ = 0, differ = 0;
    long plain_all = 0, built_all = 0;
    for(long idx=0; idx < BUILT.count; idx++) {
        const item* p = &PLAIN.items[idx];
        const item* q = &BUILT.items[idx];
        plain_all += p->len; built_all += q->len;
        if (!q->run) {
            leaves++;
            if (!go_to_the_same(idx)) {
                leaves_differ++;
                printf("FAIL in %s, %04x at %lx goes to %lx, and %04x at %lx to %lx: of the runs and the instructions that leave them, those are number %ld and number %ld\n",
                       word_at(BUILT.words, BUILT.n, q->at), PLAIN.code[p->at + p->len - 1], p->at, p->to,
                       BUILT.code[q->at + q->len - 1], q->at, q->to,
                       item_at(&PLAIN, p->to), item_at(&BUILT, q->to));
            }
            continue;
        }
        runs++;
        const uint16_t* plain = PLAIN.code + p->at;
        const uint16_t* built = BUILT.code + q->at;
        if (p->len == q->len && !memcmp(plain, built, p->len * sizeof(uint16_t))) { alike++; continue; }
        // check whether a conditional branch follows the run directly
        const item* next = idx + 1 < BUILT.count ? &BUILT.items[idx + 1] : NULL;
        int branch = next && !next->run && next->at == q->at + q->len && ins_of(BUILT.code[next->at]).jmp.op_type == OP_TYPE_CJMP;
        int pieces;
        if (runs_leave_the_same(plain, p->len, built, q->len, branch, &pieces)) {
            if (pieces) in_pieces++; else shown++;
            continue;
        }
        differ++;
        printf("FAIL in %s, at %lx and %lx, these are not shown to leave the same:\n    ", word_at(BUILT.words, BUILT.n, q->at), p->at, q->at);
        for(long j=0; j < p->len; j++) printf(" %04x", plain[j]);
        printf("\n    ");
        for(long j=0; j < q->len; j++) printf(" %04x", built[j]);
        printf("\n");
    }
    int built_both = plain_all != built_all;
    if (!built_both) printf("FAIL the two images are as many instructions: the one without the peephole was not built\n");
    printf("the two builds, cells of %d bits: %ld runs, %ld the same instructions, %ld shown to leave the same and %ld a piece at a time, "
           "%ld not; %ld calls, jumps and branches, %ld not to the same place; %ld instructions and %ld\n",
           CELL_BITS, runs, alike, shown, in_pieces, differ, leaves, leaves_differ, plain_all, built_all);
    return(differ || leaves_differ || !built_both || !runs || !leaves);
}
