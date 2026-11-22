//
// builds_test PLAIN.hex PLAIN.tables IMAGE.hex IMAGE.tables
//
// The image cross.fs makes with its peephole, against the image it makes
// without: every straight-line run of the one must leave what the same
// run of the other leaves, and every call, jump and branch must go to the
// same place.  It is what is run after any change to a compiler or a
// peephole.
//
// Both images are cut into runs and the instructions that leave a run
// (cut.h).  They are to be the same words, and in each word the same runs
// and the same calls in the same order: the words themselves need not be
// in the same order of place, since the image that is run may go back to
// a page and this one's other never does.  Whether two runs leave the same is asked of the machine: the
// step on terms says what each leaves in every cell (fuse.h), and two that
// leave the same terms leave the same from every state.
//
//   A run that a conditional branch comes after need only leave the same
//   but for the top, and the top nothing when the other leaves it nothing:
//   so `0 =` is put after both, which is what the branch asks.
//
//   Code is not at the same place in the two images.  A number in the
//   plain run that is a place something starts at in the plain image may
//   be that place, pushed, or may be a number that happens to be it: so a
//   run is tried as it is, and then with each such number made the place
//   in the other image, every way that can be done.
//
//   The step says nothing of a read or a write of a port.  A run with one
//   in it is taken a piece at a time: the pieces between the ports, which
//   are to be the same instructions in both, but that the last may have
//   the return in it that is an exit after it in the plain run.
//
// It says how many runs and how many calls, jumps and branches it
// compared, and fails if a pair is not shown to be the same, if there was
// nothing to compare, or if the two images are as many instructions: then
// the one built without the peephole was not built.
//

#include "../util/fuse.h"
#include "../util/cut.h"

static cut_image PLAIN, BUILT;
static uint16_t EXIT;                   // the instruction that is an exit

#define MAY_BE_PLACES 12                // numbers of a run that may be places: more are not tried every way,
                                        // since each way of taking them is tried.  Chosen, and not the machine's

static int leave_the_same(const uint16_t* a, int alen, const uint16_t* b, int blen) {
    if (alen == blen && !memcmp(a, b, alen * sizeof(uint16_t))) return(1);
    named x, y;
    forget();
    uint64_t left = names(a, alen, &x);
    return(left && names(b, blen, &y) == left && same_named(&x, &y));
}

// The same, where a conditional branch comes after: with what the branch
// asks after each.
static int leave_the_same_to_a_branch(const uint16_t* a, int alen, const uint16_t* b, int blen) {
    // each with room for itself, a number (fuse.h says how long one can be) and what asks
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

// Is an instruction a read or a write of a port, of which the step says nothing?
static uint16_t without_return(uint16_t w) {
    instruction ins = ins_of(w);
    ins.alu.r_eip = 0;
    ins.alu.rstack = 0;
    return(*(uint16_t*)&ins);
}
static int of_a_port(uint16_t w) {
    instruction ins = ins_of(w);
    return(!ins.lit.lit_f && ins.alu.op_type == OP_TYPE_ALU && alu_at[w] < 0 && alu_at[without_return(w)] < 0);
}

// The same, a piece at a time: the pieces between the instructions of
// ports.  Those are to be the same instructions in the same order; the
// last may have the return in it, where the plain run has an exit after.
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

static int same_one_way(const uint16_t* a, int alen, const uint16_t* b, int blen, int branch, int* pieces) {
    *pieces = 0;
    if (leave_the_same(a, alen, b, blen)) return(1);
    if (branch && leave_the_same_to_a_branch(a, alen, b, blen)) return(1);
    *pieces = 1;
    return(same_in_pieces(a, alen, b, blen, branch));
}

// Is a number one that a run pushes?
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

// The numbers of a plain run that are places in the plain image: where
// each is in the run, and the place it would be in the other image.  If
// the other run is given, only those whose place there is a number that
// run pushes: a number that is a place is that place there, and one that
// only happens to be a place's number is itself there.
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

// The run with some of those numbers made the places: the ones `which` has a bit for.
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

// Do two runs leave the same?  And was it found a piece at a time?
//
// A run of a great many numbers (one that sets a table of variables) has
// more that happen to be a place's number than are tried every way, and
// the one that is a place may come after them all.  So where the first
// that are found are as many as are tried, the ones the other run has the
// place of are tried too.
static int runs_leave_the_same(const uint16_t* plain, int plen, const uint16_t* built, int blen, int branch, int* pieces) {
    if (same_one_way(plain, plen, built, blen, branch, pieces)) return(1);
    spot places[MAY_BE_PLACES];
    // the run, with room for each of those numbers written as a place's is
    uint16_t* tried = room_for(plen + MAY_BE_PLACES * NUMBER_ROOM, sizeof(uint16_t));
    int same = 0;
    for(int likely=0; likely < 2 && !same; likely++) {
        int n = likely ? places_in(plain, plen, built, blen, places) : places_in(plain, plen, NULL, 0, places);
        for(unsigned which=1; which < 1u << n && !same; which++) {
            int len = with_places(plain, plen, places, n, which, tried);
            same = same_one_way(tried, len, built, blen, branch, pieces);
        }
        if (n < MAY_BE_PLACES) break;       // every one of them was tried
    }
    free(tried);
    return(same);
}

// Do two instructions that leave a run go to the same place?  The same
// item of each image; or, where it is no item of either, the same number.
static int go_to_the_same(long idx) {
    const item* p = &PLAIN.items[idx];
    const item* q = &BUILT.items[idx];
    // its last instruction is the call, the jump or the branch
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
    machine_from(argv[3]);              // the cell is the image's, before anything is asked of it
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
        // does a conditional branch come straight after it?
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
