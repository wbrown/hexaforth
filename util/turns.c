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
// What is looked at is the turn's to say: every length shorter than it is
// now, every way of keeping what it keeps in the cells it keeps it in,
// every part of it shorter than the whole, and every number it pushes.
// How long each search may take is fewer.c's (SEARCH_STATES), and the
// same for every one; how many were made each way, and how many of those
// were given up, is said with what was found.
//

typedef struct {
    uint16_t code[LAID];                // the turn as one run, as long as one can be run on vm.c (fuse.c),
    int len;
    int now;                            // and how many instructions it runs in the loop, with its branches and calls
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
static int take_turn(const turn* t, a_turn* out) {
    named none;
    memset(out, 0, sizeof(a_turn));
    if (t->bodies + t->tests > LAID) return(0);
    memcpy(out->code, t->body, t->bodies * sizeof(uint16_t));
    if (t->tests) memcpy(out->code + t->bodies, t->test, t->tests * sizeof(uint16_t));
    out->len = t->bodies + t->tests;
    out->now = turn_now(t);
    out->at_top = t->tests_at == TESTS_AT_TOP;
    forget();
    start(&none);
    if (!names(out->code, out->len, &out->left)) return(0);
    if (out->left.next >= 0 || out->left.SP != none.SP + 1 || out->left.RSP != none.RSP) return(0);
    out->base = known;
    cells_kept(&out->left, &out->of_data, &out->of_return);
    for(int depth=0; depth < out->of_data; depth++) out->kept[out->n_kept++] = CELL_OF_DATA(depth);
    for(int depth=0; depth < out->of_return; depth++) out->kept[out->n_kept++] = CELL_OF_RETURN(depth);
    return(1);
}

// Every way of keeping what a turn keeps in the top cells of the two
// stacks, but the way it is kept now, one at a time: so many cells in
// every order, with from none to all of them on the data stack.  A way is,
// for every cell's term, the cell it is kept in.  They are as many as the
// orders of the cells, times one more than the cells, and are made as they
// are wanted, not listed: a keeping starts with of_data less than nothing.
typedef struct { int of_data, order[2 * DEEP]; } keeping;

static int next_order(int* order, int k) {
    int at = k - 2;
    while (at >= 0 && order[at] > order[at + 1]) at--;
    if (at < 0) return(0);
    int with = k - 1;
    while (order[with] < order[at]) with--;
    int swap = order[at];
    order[at] = order[with], order[with] = swap;
    for(int lo=at + 1, hi=k - 1; lo < hi; lo++, hi--) swap = order[lo], order[lo] = order[hi], order[hi] = swap;
    return(1);
}

static int next_way(const a_turn* t, keeping* g, int* to) {
    int k = t->n_kept;
    for(;;) {
        if (g->of_data < 0 || !next_order(g->order, k)) {
            if (++g->of_data > k) return(0);
            for(int idx=0; idx < k; idx++) g->order[idx] = idx;
        }
        int same = 1;
        for(int cell=0; cell < CELL_TERMS; cell++) to[cell] = cell;
        for(int idx=0; idx < k; idx++) {
            int slot = g->order[idx] < g->of_data ? CELL_OF_DATA(g->order[idx]) : CELL_OF_RETURN(g->order[idx] - g->of_data);
            to[t->kept[idx]] = slot;
            same &= slot == t->kept[idx];
        }
        if (!same) return(1);
    }
}

// A cell as it is said: T, N and R are the cells the machine has those
// names for, and any other is its stack and how far under the top it is.
static void say_cell(char* text, size_t room, int cell) {
    int data = OF_DATA(cell), depth = DEPTH_OF(cell);
    if (data && depth == 0) snprintf(text, room, "T");
    else if (data && depth == 1) snprintf(text, room, "N");
    else if (!data && depth == 0) snprintf(text, room, "R");
    else snprintf(text, room, "%s%d", data ? "d" : "r", depth);
}

// A way of keeping, in words: the text is made here, and is whoever
// asks's to free.
static char* kept_said(const a_turn* t, const int* to) {
    char one[2 * 3 * sizeof(int) + sizeof(", x in x")];  // two cells, each a letter and a number, and what is between
    char* text = room_for((size_t)t->n_kept + 1, sizeof(one));
    for(int idx=0; idx < t->n_kept; idx++) {
        char from[3 * sizeof(int) + 2], in[3 * sizeof(int) + 2];
        if (to[t->kept[idx]] == t->kept[idx]) continue;
        say_cell(from, sizeof(from), t->kept[idx]);
        say_cell(in, sizeof(in), to[t->kept[idx]]);
        snprintf(one, sizeof(one), "%s%s in %s", text[0] ? ", " : "", from, in);
        strcat(text, one);
    }
    if (!text[0]) strcpy(text, "as they are");
    return(text);
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
    int same[CELL_TERMS];
    named left;
    for(int cell=0; cell < CELL_TERMS; cell++) same[cell] = cell;
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
// is the instructions that put it there, and there are PUTS of them.
#define PUTS 2
static int turn_with(a_turn* t, int64_t number, const uint16_t* puts, int length, long states,
                     uint16_t* found, int* given_up) {
    int same[CELL_TERMS];
    named left, from;
    for(int cell=0; cell < CELL_TERMS; cell++) same[cell] = cell;
    known = t->base;
    lost = 0;
    kept_as(&t->left, same, &left);
    left.RS[left.RSP - 1] = left.R;
    left.RSP++;
    left.R = t_number(number);
    start(&from);
    for(int idx=0; idx < PUTS; idx++) step_named(&from, puts[idx]);
    if (lost || from.unsaid || from.R != left.R || !want(&WANTED, &left, round_on(t, left.T))) {
        *given_up = 1;
        return(0);
    }
    if (!exactly(&WANTED, &from, length, states, found, given_up)) return(0);
    uint16_t whole[LAID + PUTS];
    if (length + PUTS > LAID) return(0);
    memcpy(whole, puts, PUTS * sizeof(uint16_t));
    memcpy(whole + PUTS, found, length * sizeof(uint16_t));
    return(agree_on_machine(t->code, t->len, whole, length + PUTS, NULL, t->kept, t->n_kept, 1, t->at_top, 1) > 0);
}

// The numbers a turn pushes, each once: how many.  There is room for as
// many as the turn has instructions.
static int numbers_pushed(const a_turn* t, int64_t* pushed, uint16_t* by) {
    int n = 0;
    for(int idx=0; idx < t->len; idx++) {
        uint16_t x = t->code[idx];
        int had = 0;
        if (NUMBER_IN[x].does != PUSHES_IT) continue;
        for(int k=0; k < n; k++) had |= pushed[k] == NUMBER_IN[x].number;
        if (!had) pushed[n] = NUMBER_IN[x].number, by[n++] = x;
    }
    return(n);
}

// A turn's test the other way round, if it is one instruction: one that
// leaves the stacks as it does, and on top what is nothing when the
// test's is not.  The instruction, or nothing; *searched says whether it
// was looked for, and *given_up whether that was given up.
static uint16_t test_turned(const turn* loop_turn, long states, int* searched, int* given_up) {
    uint16_t found;
    named left;
    *searched = *given_up = 0;
    if (loop_turn->tests != 1) return(0);
    forget();
    if (!names(loop_turn->test, 1, &left)) return(0);
    if (!want(&WANTED, &left, not_flag(nothing(left.T)))) return(0);
    *searched = 1;
    return(exactly(&WANTED, NULL, 1, states, &found, given_up) ? found : 0);
}

static int by_parts(uint16_t* code, int len, long states, int* searched, int* given_up);

// ---- a turn looked at every way

enum { KEPT_HERE, KEPT_ANYWHERE, TEST_AT_BOTTOM, A_PART_AT_A_TIME, NUMBER_IN_R };

typedef struct {
    int how;                            // which way it was found
    uint16_t code[LAID];                // the instructions before the branch that goes back,
    int len;                            // and how many
    int to[CELL_TERMS];                 // KEPT_ANYWHERE: where what the turn keeps is kept
    int64_t number;                     // NUMBER_IN_R: the number
} way;

typedef struct {
    a_turn turn;
    way found[NUMBER_IN_R + LAID];      // every way that fewer do it, in the order they were looked for: each of
    int n;                              // the ways before the last once, and that once for each number pushed
    int fewest;                         // the fewest instructions a turn, with the branch: as many as now, if no fewer
    long searched[NUMBER_IN_R + 1];     // by way: the searches made,
    long gave_up[NUMBER_IN_R + 1];      // and how many of them were given up
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
static int shorten(const turn* is, long states, shortened* s) {
    uint16_t found[LAID], known_to[LAID];
    a_turn* t = &s->turn;
    int at_top = is->tests_at == TESTS_AT_TOP;
    memset(s, 0, sizeof(shortened));
    if (!take_turn(is, t)) return(0);
    s->fewest = t->now;

    // With what it keeps where it is; and then anywhere, if that is fewer.
    int most = t->now - 1, here = 0;
    for(int anywhere=0; anywhere < 2; anywhere++) {
        int length, way_found = 0, to[CELL_TERMS], found_to[CELL_TERMS];
        int how = anywhere ? KEPT_ANYWHERE : KEPT_HERE;
        for(length=1; length < most && !way_found; length++) {
            keeping g = { -1 };
            while (!way_found && (anywhere ? next_way(t, &g, to) : g.of_data++ < 0)) {      // where they are: once
                int given_up = 0;
                if (turn_in(t, anywhere ? to : NULL, length, states, found, &given_up)) {
                    way_found = 1;
                    memcpy(found_to, to, sizeof(to));
                }
                s->searched[how]++;
                s->gave_up[how] += given_up;
            }
        }
        if (!way_found) continue;
        most = --length;
        if (!anywhere) {
            a_way(s, KEPT_HERE, found, length);
            here = length;
        } else if (!here || length < here) {
            memcpy(a_way(s, KEPT_ANYWHERE, found, length)->to, found_to, sizeof(found_to));
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
        int searched, given_up;
        uint16_t turned = at_top ? test_turned(is, states, &searched, &given_up) : 0;
        if (at_top) s->searched[TEST_AT_BOTTOM] += searched, s->gave_up[TEST_AT_BOTTOM] += given_up;
        memcpy(known_to, is->body, is->bodies * sizeof(uint16_t));
        known_len = is->bodies;
        if (at_top && turned) known_to[known_len++] = turned;
        if (at_top && (!turned || agree_on_machine(t->code, t->len, known_to, known_len, NULL, t->kept, t->n_kept, 1, 1, 0) <= 0)) {
            known_len = 0;
        } else if (at_top) {
            a_way(s, TEST_AT_BOTTOM, known_to, known_len);
        }
    }
    if (known_len) {
        uint16_t parted[LAID];
        memcpy(parted, known_to, known_len * sizeof(uint16_t));
        int searched = 0, given_up = 0;
        int len = by_parts(parted, known_len, states, &searched, &given_up);
        s->searched[A_PART_AT_A_TIME] += searched;
        s->gave_up[A_PART_AT_A_TIME] += given_up;
        if (len < known_len && agree_on_machine(t->code, t->len, parted, len, NULL, t->kept, t->n_kept, 1, t->at_top, 0) > 0) {
            a_way(s, A_PART_AT_A_TIME, parted, len);
        }
    }

    // A number the turn pushes, kept on the return stack instead.  (The
    // parts forgot the turn's terms, so it is taken again.)
    int64_t pushed[LAID];
    uint16_t pushed_by[LAID], to_r = ins_named(">r");
    int pushes = to_r && take_turn(is, t) ? numbers_pushed(t, pushed, pushed_by) : 0;
    for(int n=0; n < pushes; n++) {
        const uint16_t puts[PUTS] = { pushed_by[n], to_r };
        for(int length=1; length < s->fewest - 1; length++) {
            int given_up = 0, is = turn_with(t, pushed[n], puts, length, states, found, &given_up);
            s->searched[NUMBER_IN_R]++;
            s->gave_up[NUMBER_IN_R] += given_up;
            if (is) {
                a_way(s, NUMBER_IN_R, found, length)->number = pushed[n];
                break;
            }
        }
    }
    return(1);
}

// Shorten a run that ends with what `until` asks about, a part at a time:
// any run of a few of its instructions that fewer do the same as, with
// everything where it is.  The last part is wanted, on top, only for
// whether it is nothing.  What is found is run on vm.c beside the part.
// The whole is not a part: it was looked for whole already.  How long the
// run is after; *searched and *given_up are added to.
static int by_parts(uint16_t* code, int len, long states, int* searched, int* given_up) {
    for(int again=1; again; ) {
        again = 0;
        for(int size = len - 1; size > 1 && !again; size--) {
            for(int at=0; at + size <= len && !again; at++) {
                named left;
                int last = at + size == len;
                forget();
                if (!names(code + at, size, &left) || left.next >= 0) continue;
                if (!want(&WANTED, &left, last ? nothing(left.T) : -1)) continue;
                for(int length=1; length < size && !again; length++) {
                    uint16_t found[LAID];
                    int gave_up_this, is = exactly(&WANTED, NULL, length, states, found, &gave_up_this);
                    ++*searched;
                    *given_up += gave_up_this;
                    if (!is) continue;
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
