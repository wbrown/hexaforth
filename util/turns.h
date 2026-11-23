//
// turns.h - searches for fewer instructions that do one turn of a loop,
// for the tools and tests that include it.  It includes loops.h, which
// defines a loop's turn, and fewer.h, which searches for the fewest
// instructions that reach a target state.
//
// The peephole joins adjacent instructions and keeps every value where
// the source put it.  A loop allows more.  The values it carries from one
// turn to the next (a pointer, an end, a distance) can be in any cell, as
// long as each is in the same cell at the start of every turn.  And the
// end of one turn runs straight into the start of the next.
//
// A turn is taken as a whole: the body, followed by the test if the loop
// tests at the top.  The search looks for code that ends with the test, so
// that a single branch at the bottom goes back and no jump is needed:
// `begin ... until`, which repeats while the top is zero.  So the top only
// matters as zero or non-zero.  A loop that tests at the top repeats while
// its test is non-zero, so for such a loop the sense is inverted.
//
// shorten() searches four ways:
//
//   with the turn's kept values in their present cells
//   with them kept in any of the top cells of the two stacks
//   one part of the turn at a time, where the whole turn is too long to
//     search
//   with a number the turn pushes kept on the return stack instead, put
//     there before the loop, where it is an ALU input
//
// Every answer is run on vm.c beside the turn (agree_on_machine(),
// fewer.h) before it is reported.
//
// The turn determines what is searched: every length shorter than its
// present length, every arrangement of its kept values among the cells it
// uses, every part of it shorter than the whole, and every number it
// pushes.  The time limit of each search is fewer.h's (SEARCH_STATES), and
// is the same for all of them.  The report gives, for each way, the number
// of searches made and the number that gave up.
//

#ifndef HEXAFORTH_TURNS_H
#define HEXAFORTH_TURNS_H

#include "loops.h"
#include "fewer.h"

typedef struct {
    uint16_t code[LAID_MOST];                // the turn as one run; at most the length vm.c can run (fuse.h)
    int len;
    int now;                            // the number of instructions it runs in the image, with its branches and calls
    int at_top;                         // set if the loop tests at the top, and so repeats while the test is non-zero
    named left;                         // the run's final state
    int base;                           // the number of terms when it was stepped: `left` is expressed in those terms
    int kept[2 * DEEP], n_kept;         // the cells it keeps values in, as terms
    int of_data, of_return;             // how many of those are on each stack
} a_turn;

// Builds a turn as one run and computes its final state.  Returns 0 if the
// turn is too long, if the step cannot give its final state, or if it does
// not leave the data stack one deeper and the return stack at the same
// depth.  The terms of its final state stay valid until the next call.
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

// Enumerates every arrangement of a turn's kept values among the top cells
// of the two stacks, except the present arrangement.  The values are taken
// in every order, with anywhere from none to all of them on the data
// stack.  An arrangement maps each cell's term to the cell its value is
// kept in.  The arrangements are generated one at a time by next_way(),
// not stored.  A `keeping` starts with of_data set to -1.
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

// Formats a cell's name.  T, N and R are the cells the machine gives those
// names.  Any other cell is named by its stack (d or r) and its depth
// under the top.
static void say_cell(char* text, size_t room, int cell) {
    int data = OF_DATA(cell), depth = DEPTH_OF(cell);
    if (data && depth == 0) snprintf(text, room, "T");
    else if (data && depth == 1) snprintf(text, room, "N");
    else if (!data && depth == 0) snprintf(text, room, "R");
    else snprintf(text, room, "%s%d", data ? "d" : "r", depth);
}

// Formats an arrangement as text.  Returns an allocated string, which the
// caller frees.
static char* kept_said(const a_turn* t, const int* to) {
    char one[2 * 3 * sizeof(int) + sizeof(", x in x")];  // room for two cell names, each a letter and a number, and the text between
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

// Returns the flag the new top must match, so that `until` repeats exactly
// when the loop does: the flag "the turn's top is zero", inverted if the
// loop repeats while its test is non-zero.
static int round_on(const a_turn* t, int top) {
    int flag = nothing(top);
    return(t->at_top ? not_flag(flag) : flag);
}

static wanted WANTED;

// Searches for `length` instructions that do a turn, with its kept values
// arranged as `to` says, or left where they are if `to` is NULL.  Returns
// 1 if they were found and agree with the turn when run on vm.c.
// `*given_up` is set if the search stopped early, or if the target did not
// fit the term table.
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

// The same, with a number kept on the return stack and put there before
// the loop.  The answer starts with the number on the return stack and
// leaves it there.  `puts` is the PUTS instructions that put it there.
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
    uint16_t whole[LAID_MOST + PUTS];
    if (length + PUTS > LAID) return(0);
    memcpy(whole, puts, PUTS * sizeof(uint16_t));
    memcpy(whole + PUTS, found, length * sizeof(uint16_t));
    return(agree_on_machine(t->code, t->len, whole, length + PUTS, NULL, t->kept, t->n_kept, 1, t->at_top, 1) > 0);
}

// Lists the distinct numbers a turn pushes, with the instruction that
// pushes each, and returns how many there are.  The caller's arrays must
// have room for one per instruction of the turn.
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

// Finds the inverse of a turn's test, if the test is one instruction: an
// instruction that leaves the stacks as the test does, with a top that is
// zero exactly when the test's top is non-zero.  Returns the instruction,
// or 0 if there is none.  `*searched` is set if a search was made, and
// `*given_up` if it gave up.
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

// ---- searching a turn every way

enum { KEPT_HERE, KEPT_ANYWHERE, TEST_AT_BOTTOM, A_PART_AT_A_TIME, NUMBER_IN_R };

// An optional callback, called after each search so that a caller can
// report progress.  It gets the way (above), the length searched for, the
// length of the code being shortened, and whether the search gave up.  It
// is not called if it is NULL.
static void (*a_search)(int how, int length, int of, int given_up);
#define SEARCHED(how, length, of, given_up) do { if (a_search) a_search((how), (length), (of), (given_up)); } while (0)

typedef struct {
    int how;                            // the way it was found
    uint16_t code[LAID_MOST];                // the instructions before the backward branch
    int len;                            // the number of those
    int to[CELL_TERMS];                 // KEPT_ANYWHERE: the arrangement of the kept values
    int64_t number;                     // NUMBER_IN_R: the number kept on the return stack
} way;

typedef struct {
    a_turn turn;
    way found[NUMBER_IN_R + LAID_MOST]; // each shorter turn found, in the order searched: at most one for each way
    int n;                              // before the last, and one per pushed number for the last
    int fewest;                         // the fewest instructions per turn found, with the branch; the present count if none is shorter
    long searched[NUMBER_IN_R + 1];     // the number of searches made, by way
    long gave_up[NUMBER_IN_R + 1];      // the number of those that gave up, by way
} shortened;

static way* a_way(shortened* s, int how, const uint16_t* code, int len) {
    way* w = &s->found[s->n++];
    w->how = how;
    w->len = len;
    memcpy(w->code, code, len * sizeof(uint16_t));
    if (len + 1 < s->fewest) s->fewest = len + 1;
    return(w);
}

// Searches a turn the four ways, each search limited to `states` states.
// Returns 0 if take_turn() rejects the turn.  A result is recorded only if
// it is shorter than the earlier result it competes with: a turn with
// rearranged values is recorded only if it is shorter than the one with
// the values in place.
static int shorten(const turn* is, long states, shortened* s) {
    uint16_t found[LAID_MOST], known_to[LAID_MOST];
    a_turn* t = &s->turn;
    int at_top = is->tests_at == TESTS_AT_TOP;
    memset(s, 0, sizeof(shortened));
    if (!take_turn(is, t)) return(0);
    s->fewest = t->now;

    // First with the kept values in place, then with them rearranged, if
    // that is shorter.
    int most = t->now - 1, here = 0;
    for(int anywhere=0; anywhere < 2; anywhere++) {
        int length, way_found = 0, to[CELL_TERMS], found_to[CELL_TERMS];
        int how = anywhere ? KEPT_ANYWHERE : KEPT_HERE;
        for(length=1; length < most && !way_found; length++) {
            keeping g = { -1 };
            while (!way_found && (anywhere ? next_way(t, &g, to) : g.of_data++ < 0)) {      // in place there is one arrangement
                int given_up = 0;
                if (turn_in(t, anywhere ? to : NULL, length, states, found, &given_up)) {
                    way_found = 1;
                    memcpy(found_to, to, sizeof(to));
                }
                s->searched[how]++;
                s->gave_up[how] += given_up;
                SEARCHED(how, length, t->now - 1, given_up);
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

    // Take code known to do a turn with every value in place: the result
    // just found, or the loop itself with its test moved to the bottom and
    // inverted.  Then shorten that one part at a time.
    int known_len = 0;
    if (here) {
        known_len = here;
        memcpy(known_to, s->found[0].code, here * sizeof(uint16_t));
    } else {
        int searched, given_up;
        uint16_t turned = at_top ? test_turned(is, states, &searched, &given_up) : 0;
        if (at_top) s->searched[TEST_AT_BOTTOM] += searched, s->gave_up[TEST_AT_BOTTOM] += given_up;
        if (at_top && searched) SEARCHED(TEST_AT_BOTTOM, 1, is->tests, given_up);
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
        uint16_t parted[LAID_MOST];
        memcpy(parted, known_to, known_len * sizeof(uint16_t));
        int searched = 0, given_up = 0;
        int len = by_parts(parted, known_len, states, &searched, &given_up);
        s->searched[A_PART_AT_A_TIME] += searched;
        s->gave_up[A_PART_AT_A_TIME] += given_up;
        if (len < known_len && agree_on_machine(t->code, t->len, parted, len, NULL, t->kept, t->n_kept, 1, t->at_top, 0) > 0) {
            a_way(s, A_PART_AT_A_TIME, parted, len);
        }
    }

    // Try keeping each number the turn pushes on the return stack instead.
    // by_parts() discarded the turn's terms, so the turn is rebuilt first.
    int64_t pushed[LAID_MOST];
    uint16_t pushed_by[LAID_MOST], to_r = ins_named(">r");
    int pushes = to_r && take_turn(is, t) ? numbers_pushed(t, pushed, pushed_by) : 0;
    for(int n=0; n < pushes; n++) {
        const uint16_t puts[PUTS] = { pushed_by[n], to_r };
        for(int length=1; length < s->fewest - 1; length++) {
            int given_up = 0, is = turn_with(t, pushed[n], puts, length, states, found, &given_up);
            s->searched[NUMBER_IN_R]++;
            s->gave_up[NUMBER_IN_R] += given_up;
            SEARCHED(NUMBER_IN_R, length, s->fewest - 1, given_up);
            if (is) {
                a_way(s, NUMBER_IN_R, found, length)->number = pushed[n];
                break;
            }
        }
    }
    return(1);
}

// Shortens a run that ends with the value `until` tests, one part at a
// time.  It replaces any stretch of the run with fewer instructions that
// do the same, keeping every value in place.  For the last stretch, the
// top only matters as zero or non-zero.  Each replacement is run on vm.c
// beside the stretch it replaces.  The whole run is not tried as a
// stretch, because it was already searched whole.  Returns the run's new
// length, and adds to `*searched` and `*given_up`.
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
                    uint16_t found[LAID_MOST];
                    int gave_up_this, is = exactly(&WANTED, NULL, length, states, found, &gave_up_this);
                    ++*searched;
                    *given_up += gave_up_this;
                    SEARCHED(A_PART_AT_A_TIME, length, size, gave_up_this);
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

#endif // HEXAFORTH_TURNS_H
