//
// shorter IMAGE.hex IMAGE.tables COUNTS [REPORT]
//
// Fewer instructions that do what a loop does, wherever its values are
// kept: for each loop of the image that ran and calls nothing, what a turn
// costs now, and the fewest instructions found that do a turn.  What a
// loop and its turn are is loops.c's to say, the ways fewer are looked for
// turns.c's, and the search fewer.c's; this orders the loops by the work
// done in them and says what was found.
//
// COUNTS is the work by place, as bench/profile.c leaves it
// (build/profile/desktop.counts; work.c).  ONLY in the environment is
// words, with commas between, whose loops alone are looked at.  Every loop
// that ran and calls nothing is looked at, however long its turn; how
// long a search may take is said once, in CMakeLists.txt (SEARCH_STATES,
// which fewer.c has).
//
// The report is build/shorter.txt if none is named: for each loop, the
// turn as it is, each way that fewer do it, and for each way it was looked
// for how many searches were made and how many of them were given up.  A
// line a loop is printed as it is done, with how many instructions the
// turn is and the fewest found.  It fails if no loop ran.
//

#include "../util/fuse.c"
#include "../util/cut.c"
#include "../util/reach.c"
#include "../util/kinds.c"
#include "../util/loops.c"
#include "../util/fewer.c"
#include "../util/turns.c"
#include "work.c"

static cut_image IMAGE;
static double* work;                    // how often each of the image's instructions ran
static loop* LOOPS;

static double work_in(const loop* l) {
    double is = 0;
    for(long at=l->head; at <= l->tail; at++) is += work[at];
    return(is);
}

static int by_work(const void* a, const void* b) {
    double x = work_in(&LOOPS[*(const int*)a]), y = work_in(&LOOPS[*(const int*)b]);
    return(x < y ? 1 : x > y ? -1 : *(const int*)a - *(const int*)b);
}

static int named_in(const char* list, const char* name) {
    size_t len = strlen(name);
    for(const char* at = list; at && *at; at = strchr(at, ',') ? strchr(at, ',') + 1 : NULL) {
        if (!strncmp(at, name, len) && (at[len] == ',' || !at[len])) return(1);
    }
    return(0);
}

// A way that fewer instructions do a turn, as a line of the report.
static void say_way(FILE* out, const a_turn* t, const way* w) {
    char* text = run_said(w->code, w->len, 1);
    char* where;
    switch (w->how) {
        case KEPT_HERE:
            fprintf(out, "    %d with the values where they are:  begin %s until\n", w->len + 1, text);
            break;
        case KEPT_ANYWHERE:
            where = kept_said(t, w->to);
            fprintf(out, "    %d with the values anywhere (%s):  begin %s until\n", w->len + 1, where, text);
            free(where);
            break;
        case TEST_AT_BOTTOM:
            fprintf(out, "    %d with the test at the bottom:  begin %s until\n", w->len + 1, text);
            break;
        case A_PART_AT_A_TIME:
            fprintf(out, "    %d a part at a time:  begin %s until\n", w->len + 1, text);
            break;
        case NUMBER_IN_R:
            fprintf(out, "    %d with %lld kept in R:  %lld >r begin %s until rdrop\n", w->len + 1,
                    (long long)w->number, (long long)w->number, text);
            break;
    }
    free(text);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "usage: shorter IMAGE.hex IMAGE.tables COUNTS [REPORT]\n");
        return(1);
    }
    const char* report = argc > 4 ? argv[4] : "build/shorter.txt";
    const char* only = getenv("ONLY");
    get_ready();
    list_kinds();
    fewer_ready();
    cut(&IMAGE, argv[1], argv[2]);
    double* counts;
    long counted = work_read(argv[3], &counts, NULL), n = IMAGE.instructions;
    if (!counted) return(1);
    work = room_for(n, sizeof(double));                 // nothing, for an instruction the counts are not of
    for(long at=0; at < n && at < counted; at++) work[at] = counts[at];
    free(counts);
    FILE* out = fopen(report, "w");
    if (!out) { perror(report); return(1); }

    int loops = loops_of(&IMAGE, &LOOPS), ran_loops = 0, looked = 0;
    int* order = room_for(loops, sizeof(int));
    double all = 0, saved = 0;
    for(long at=0; at < n; at++) all += IMAGE.code[at] ? work[at] : 0;
    for(int idx=0; idx < loops; idx++) if (work[LOOPS[idx].tail] >= 1) order[ran_loops++] = idx;
    qsort(order, ran_loops, sizeof(int), by_work);
    fprintf(out, "%s: loops that call nothing, by how often they ran.  A search is given up after %ld states.\n\n",
            argv[1], (long)SEARCH_STATES);

    for(int idx=0; idx < ran_loops; idx++) {
        const loop* l = &LOOPS[order[idx]];
        int in_word = holding(&IMAGE, l->head);
        const char* word = in_word >= 0 ? IMAGE.words[in_word].name : "?";
        double turns = work[l->tail];
        char title[sizeof(IMAGE.words[0].name) + 64];   // a word's name, two places and a count
        char* text;
        static shortened s;
        turn is;
        if (only && !named_in(only, word)) continue;
        snprintf(title, sizeof(title), "%s  %04lx-%04lx: %.0f turns", word, l->head, l->tail, turns);
        if (!turn_of(&IMAGE, l, &is)) {
            fprintf(out, "%s: not looked at (it branches inside, or calls a word that does or that touches where it "
                    "returns to)\n", title);
            continue;
        }
        if (is.bodies + is.tests > LAID) {
            fprintf(out, "%s, %d instructions a turn: not looked at (with the words it calls in place it is %d, and a run "
                    "vm.c is given is no more than %d: fuse.c, LAID)\n", title, turn_now(&is), is.bodies + is.tests, LAID);
            free(is.own);
            continue;
        }
        if (!shorten(&is, SEARCH_STATES, &s)) {
            fprintf(out, "%s: not looked at (the step cannot follow a turn)\n", title);
            free(is.own);
            continue;
        }
        looked++;
        fprintf(out, "%s, %d instructions a turn", title, s.turn.now);
        if (is.placed) fprintf(out, ", with %d calls: the words are put in place, and the %d instructions that call and "
                               "return are not", is.placed, is.called);
        fprintf(out, "\n");
        fprintf(out, "    now:  begin ");
        if (is.tests_at == TESTS_AT_TOP) {
            text = run_said(is.test, is.tests, 1);
            fprintf(out, "%s while ", text);
            free(text);
        }
        text = run_said(is.body, is.bodies, 1);
        fprintf(out, "%s %s\n", text, is.tests_at == TESTS_AT_TOP ? "repeat" : "until");
        free(text);
        free(is.own);
        static const char* const WAY[] = {
            [KEPT_HERE] = "with the values where they are", [KEPT_ANYWHERE] = "with the values anywhere",
            [TEST_AT_BOTTOM] = "with the test at the bottom", [A_PART_AT_A_TIME] = "a part at a time",
            [NUMBER_IN_R] = "with a number kept in R" };
        for(int how=0; how < COUNT(WAY); how++) {
            int found = 0;
            for(int w=0; w < s.n; w++) found |= s.found[w].how == how;
            if (!s.searched[how]) continue;
            if (s.gave_up[how]) {
                fprintf(out, "    (%s: %ld searches, %ld of them given up%s)\n", WAY[how], s.searched[how], s.gave_up[how],
                        found ? "" : ", so fewer may do");
            } else {
                fprintf(out, "    (%s: %ld searches, every one to the end%s)\n", WAY[how], s.searched[how],
                        found ? "" : ": no fewer do it");
            }
        }
        for(int w=0; w < s.n; w++) say_way(out, &s.turn, &s.found[w]);
        if (s.fewest < s.turn.now) saved += (s.turn.now - s.fewest) * turns;
        printf("%s: %d -> %d\n", title, s.turn.now, s.fewest);
        fflush(stdout);
    }
    fprintf(out, "\nInstructions that the fewest found would save: %.0f of %.0f.\n", saved, all);
    fclose(out);
    printf("\n%d loops ran, and %d were looked at; the fewest found would save %.0f of %.0f instructions.\nall of it: %s\n",
           ran_loops, looked, saved, all, report);
    return(ran_loops ? 0 : 1);
}
