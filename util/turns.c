//
// turns.c - fewer instructions that do what a turn of a loop does, for the
// tools and the tests that have this in them.  It is had after loops.c,
// which says what a loop's turn is, and fewer.c, which looks for the
// fewest instructions that leave what is wanted.
//
// The peephole joins instructions that are next to each other, and keeps
// every value where the source put it.  A loop has more to give: what it
// keeps from one turn to the next (a pointer, an end, a distance) can be
// in any cell, as long as it is in the same cell at the start of every
// turn; and the end of a turn is next to the start of the next.
//
// A turn is taken whole: the body, and then the test if the loop tests at
// the top.  What is looked for ends with the test, so that one branch at
// the bottom goes back and no jump is needed: `begin ... until`, which
// goes round while what is on top is nothing.  So the top is wanted only
// for whether it is nothing; and the other way about from the loop's own
// test, if the loop tests at the top and so goes round on something.
//
// It is looked for four ways (shorten): with what the turn keeps where it
// is; with it kept in any of the top cells of the two stacks; a part of
// the turn at a time, where the whole is too much to search; and with a
// number the turn pushes kept on the return stack instead, put there
// before the loop, where it is an input of the ALU.  What is found is run
// on vm.c beside the turn (fewer.c, agree_on_machine) before it is said.
//

#define TURN_KEPT   4                   // the most cells a turn keeps things in, for them to be tried elsewhere
#define TURN_WAYS   128                 // the most ways of keeping so many

typedef struct {
    uint16_t code[RUN];                 // the turn as one run,
    int len;
    int now;                            // and how many instructions it is in the loop, with its branches
    int at_top;                         // the loop tests at the top, and goes round on something
    named left;                         // what the run leaves
    int base;                           // the terms that is said in: those there were when it was run
    int kept[2 * DEEP], n_kept;         // the cells it keeps things in, by their terms
    int of_data, of_return;             // and how many of each stack's they are
} a_turn;

// A turn as one run, and what it leaves.  False if it is too long, or the
// step cannot say what it leaves, or it does not leave the data stack one
// deeper and the return stack as it was.  The terms it is said in are
// there until the next turn is taken.
static int take_turn(const turn* t, int how, a_turn* out) {
    memset(out, 0, sizeof(a_turn));
    if (t->bodies + t->tests > RUN) return(0);
    memcpy(out->code, t->body, t->bodies * sizeof(uint16_t));
    if (t->tests) memcpy(out->code + t->bodies, t->test, t->tests * sizeof(uint16_t));
    out->len = t->bodies + t->tests;
    out->now = t->bodies + (how == 1 ? t->tests + 2 : 1);
    out->at_top = how == 1;
    forget();
    if (!names(out->code, out->len, &out->left)) return(0);
    if (out->left.next >= 0 || out->left.SP != DEEP + 1 || out->left.RSP != DEEP) return(0);
    out->base = known;
    cells_kept(&out->left, &out->of_data, &out->of_return);
    for(int depth=0; depth < out->of_data; depth++) out->kept[out->n_kept++] = CELL_OF_DATA(depth);
    for(int depth=0; depth < out->of_return; depth++) out->kept[out->n_kept++] = CELL_OF_RETURN(depth);
    return(1);
}

// Every way of keeping what a turn keeps in the top cells of the two
// stacks, but the way it is kept now: each is, for every cell's term, the
// cell it is kept in.  How many there are.
static int ways_to_keep(const a_turn* t, int (*to)[2 * SLOTS], int most) {
    int n = 0, k = t->n_kept;
    if (k > TURN_KEPT) return(0);
    for(int of_data=0; of_data <= k; of_data++) {
        int slot[TURN_KEPT], order[TURN_KEPT];
        for(int idx=0; idx < k; idx++) {
            slot[idx] = idx < of_data ? CELL_OF_DATA(idx) : CELL_OF_RETURN(idx - of_data);
            order[idx] = idx;
        }
        for(;;) {
            int same = 1;
            for(int idx=0; idx < k; idx++) same &= slot[order[idx]] == t->kept[idx];
            if (!same && n < most) {
                for(int cell=0; cell < 2 * SLOTS; cell++) to[n][cell] = cell;
                for(int idx=0; idx < k; idx++) to[n][t->kept[idx]] = slot[order[idx]];
                n++;
            }
            int at = k - 2;                             // the next order of them
            while (at >= 0 && order[at] > order[at + 1]) at--;
            if (at < 0) break;
            int with = k - 1;
            while (order[with] < order[at]) with--;
            int swap = order[at];
            order[at] = order[with], order[with] = swap;
            for(int lo=at + 1, hi=k - 1; lo < hi; lo++, hi--) swap = order[lo], order[lo] = order[hi], order[hi] = swap;
        }
    }
    return(n);
}

// A way of keeping, in words.
static void say_kept(char* text, const a_turn* t, const int* to) {
    text[0] = '\0';
    for(int idx=0; idx < t->n_kept; idx++) {
        int from = t->kept[idx], in = to[from];
        if (in == from) continue;
        const int both[2] = { from, in };
        if (text[0]) strcat(text, ", ");
        for(int which=0; which < 2; which++) {
            int data = both[which] < SLOTS, depth = (data ? DEEP - 1 : SLOTS + DEEP - 1) - both[which];
            if (which) strcat(text, " in ");
            if (data && depth < 2) strcat(text, depth ? "N" : "T");
            else if (!data && !depth) strcat(text, "R");
            else sprintf(text + strlen(text), "%s%d", data ? "d" : "r", depth);
        }
    }
    if (!text[0]) strcpy(text, "as they are");
}

// What the top is to be nothing with, for `until` to go round when the
// loop does: the flag for whether what the turn leaves on top is nothing,
// and the other of it if the loop goes round on something.
static int round_on(const a_turn* t, int top) {
    int flag = nothing(top);
    return(t->at_top ? not_flag(flag) : flag);
}

static wanted WANTED;

// So many instructions that do a turn, with what it keeps kept as `to`
// says (where it is, with none): found and run on vm.c beside the turn, or
// not.  *given_up says the search stopped before it had looked at
// everything, or there was no room for what was wanted.
static int turn_in(a_turn* t, const int* to, int length, long states, uint16_t* found, int* given_up) {
    int same[2 * SLOTS];
    named left;
    for(int cell=0; cell < 2 * SLOTS; cell++) same[cell] = cell;
    known = t->base;
    lost = 0;
    kept_as(&t->left, to ? to : same, &left);
    if (lost || !want(&WANTED, &left, round_on(t, left.T))) {
        *given_up = 1;
        return(0);
    }
    if (!exactly(&WANTED, NULL, length, states, found, given_up)) return(0);
    return(agree_on_machine(t->code, t->len, found, length, to ? to : same, t->kept, t->n_kept, 1, t->at_top, 0) > 0);
}

// The same with a number kept on the return stack, put there before the
// loop: what is found starts with it there and leaves it there.  `puts`
// is the two instructions that put it there.
static int turn_with(a_turn* t, int64_t number, const uint16_t* puts, int length, long states,
                     uint16_t* found, int* given_up) {
    int same[2 * SLOTS];
    named left, from;
    for(int cell=0; cell < 2 * SLOTS; cell++) same[cell] = cell;
    known = t->base;
    lost = 0;
    kept_as(&t->left, same, &left);
    left.RS[left.RSP - 1] = left.R;
    left.RSP++;
    left.R = t_number(number);
    start(&from);
    step_named(&from, puts[0]);
    step_named(&from, puts[1]);
    if (lost || from.unsaid || from.R != left.R || !want(&WANTED, &left, round_on(t, left.T))) {
        *given_up = 1;
        return(0);
    }
    if (!exactly(&WANTED, &from, length, states, found, given_up)) return(0);
    uint16_t whole[RUN + 2] = { puts[0], puts[1] };
    if (length + 2 > RUN) return(0);
    memcpy(whole + 2, found, length * sizeof(uint16_t));
    return(agree_on_machine(t->code, t->len, whole, length + 2, NULL, t->kept, t->n_kept, 1, t->at_top, 1) > 0);
}

// The numbers a turn pushes, each once: how many.
static int numbers_pushed(const a_turn* t, int64_t* numbers, uint16_t* by, int most) {
    int n = 0;
    for(int idx=0; idx < t->len; idx++) {
        const numbered* is = NULL;
        for(int p=0; p < pushing && !is; p++) if (PUSHING[p].ins == t->code[idx]) is = &PUSHING[p];
        int had = 0;
        for(int k=0; is && k < n; k++) had |= numbers[k] == is->number;
        if (is && !had && n < most) numbers[n] = is->number, by[n++] = is->ins;
    }
    return(n);
}

// A turn's test the other way round, if it is one instruction: one that
// leaves the stacks as it does, and on top what is nothing when the
// test's is not.  The instruction, or nothing.
static uint16_t test_turned(const turn* loop_turn) {
    uint16_t found;
    named left;
    int given_up;
    if (loop_turn->tests != 1) return(0);
    forget();
    if (!names(loop_turn->test, 1, &left)) return(0);
    if (!want(&WANTED, &left, not_flag(nothing(left.T)))) return(0);
    return(exactly(&WANTED, NULL, 1, 100000, &found, &given_up) ? found : 0);
}

static int by_parts(uint16_t* code, int len, long states);

// ---- a turn looked at every way

enum { KEPT_HERE, KEPT_ANYWHERE, TEST_AT_BOTTOM, A_PART_AT_A_TIME, NUMBER_IN_R };

typedef struct {
    int how;                            // which way it was found
    uint16_t code[RUN];                 // the instructions before the branch that goes back,
    int len;                            // and how many
    int to[2 * SLOTS];                  // KEPT_ANYWHERE: where what the turn keeps is kept
    int64_t number;                     // NUMBER_IN_R: the number
} way;

typedef struct {
    a_turn turn;
    way found[4 + RUN];                 // every way that fewer do it, in the order they were looked for
    int n;
    int fewest;                         // the fewest instructions a turn, with the branch: as many as now, if no fewer
    int gave_up[2];                     // searches given up with what it keeps where it is, and anywhere
} shortened;

static way* a_way(shortened* s, int how, const uint16_t* code, int len) {
    way* w = &s->found[s->n++];
    w->how = how;
    w->len = len;
    memcpy(w->code, code, len * sizeof(uint16_t));
    if (len + 1 < s->fewest) s->fewest = len + 1;
    return(w);
}

// A turn looked at the four ways, each search given up after so many
// states.  False if the turn cannot be taken (take_turn).  A way that is
// found is there only if it is fewer than those found before it by the
// same means: anywhere, only if fewer than where it is.
static int shorten(const turn* is, int how, long states, shortened* s) {
    static int to[TURN_WAYS][2 * SLOTS];
    uint16_t found[RUN], known_to[RUN];
    a_turn* t = &s->turn;
    memset(s, 0, sizeof(shortened));
    if (!take_turn(is, how, t)) return(0);
    s->fewest = t->now;

    // With what it keeps where it is; and then anywhere, if that is fewer.
    int ways = ways_to_keep(t, to, TURN_WAYS), most = t->now - 1, here = 0;
    for(int anywhere=0; anywhere < 2; anywhere++) {
        int length, way_found = -1;
        for(length=1; length < most; length++) {
            for(int w=0; w < (anywhere ? ways : 1) && way_found < 0; w++) {
                int given_up = 0;
                if (turn_in(t, anywhere ? to[w] : NULL, length, anywhere ? states / 10 : states, found, &given_up)) way_found = w;
                s->gave_up[anywhere] += given_up;
            }
            if (way_found >= 0) break;
        }
        if (way_found < 0) continue;
        most = length;
        if (!anywhere) {
            a_way(s, KEPT_HERE, found, length);
            here = length;
        } else if (!here || length < here) {
            memcpy(a_way(s, KEPT_ANYWHERE, found, length)->to, to[way_found], sizeof(to[0]));
        }
    }

    // What is known to do a turn with everything where it is: what was
    // found, or the loop itself with its test at the bottom, the other way
    // round.  That is then shortened a part at a time.
    int known_len = 0;
    if (here) {
        known_len = here;
        memcpy(known_to, s->found[0].code, here * sizeof(uint16_t));
    } else {
        uint16_t turned = how == 1 ? test_turned(is) : 0;
        memcpy(known_to, is->body, is->bodies * sizeof(uint16_t));
        known_len = is->bodies;
        if (how == 1 && turned) known_to[known_len++] = turned;
        if (how == 1 && (!turned || agree_on_machine(t->code, t->len, known_to, known_len, NULL, t->kept, t->n_kept, 1, 1, 0) <= 0)) {
            known_len = 0;
        } else if (how == 1) {
            a_way(s, TEST_AT_BOTTOM, known_to, known_len);
        }
    }
    if (known_len) {
        uint16_t parted[RUN];
        memcpy(parted, known_to, known_len * sizeof(uint16_t));
        int len = by_parts(parted, known_len, states / 4);
        if (len < known_len && agree_on_machine(t->code, t->len, parted, len, NULL, t->kept, t->n_kept, 1, t->at_top, 0) > 0) {
            a_way(s, A_PART_AT_A_TIME, parted, len);
        }
    }

    // A number the turn pushes, kept on the return stack instead.  (The
    // parts forgot the turn's terms, so it is taken again.)
    instruction to_r[8];                // lookup_word clears as many
    int64_t numbers[RUN];
    uint16_t pushed_by[RUN];
    int pushed = lookup_word(FORTH_WORDS, ">r", to_r) == 1 && take_turn(is, how, t) ? numbers_pushed(t, numbers, pushed_by, RUN) : 0;
    for(int n=0; n < pushed; n++) {
        uint16_t puts[2] = { pushed_by[n], *(uint16_t*)to_r };
        for(int length=1; length < s->fewest - 1; length++) {
            int given_up = 0;
            if (turn_with(t, numbers[n], puts, length, states, found, &given_up)) {
                a_way(s, NUMBER_IN_R, found, length)->number = numbers[n];
                break;
            }
            if (given_up) break;
        }
    }
    return(1);
}

// Shorten a run that ends with what `until` asks about, a part at a time:
// any run of up to six of its instructions that fewer do the same as, with
// everything where it is.  The last part is wanted, on top, only for
// whether it is nothing.  What is found is run on vm.c beside the part.
// How long the run is after.
static int by_parts(uint16_t* code, int len, long states) {
    for(int again=1; again; ) {
        again = 0;
        for(int size = len < 6 ? len : 6; size > 1 && !again; size--) {
            for(int at=0; at + size <= len && !again; at++) {
                named left;
                int last = at + size == len;
                forget();
                if (!names(code + at, size, &left) || left.next >= 0) continue;
                if (!want(&WANTED, &left, last ? nothing(left.T) : -1)) continue;
                for(int length=1; length < size && !again; length++) {
                    uint16_t found[RUN];
                    int given_up;
                    if (!exactly(&WANTED, NULL, length, states, found, &given_up)) continue;
                    if (agree_on_machine(code + at, size, found, length, NULL, NULL, 0, last, 0, 0) <= 0) continue;
                    memmove(code + at + length, code + at + size, (len - at - size) * sizeof(uint16_t));
                    memcpy(code + at, found, length * sizeof(uint16_t));
                    len -= size - length;
                    again = 1;
                }
            }
        }
    }
    return(len);
}
