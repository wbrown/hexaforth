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
// words, with commas between, whose loops alone are looked at; NODES is
// the states a search looks at before it is given up (20,000), and LONGEST
// the instructions in a turn that is looked at (12).
//
// The report is build/shorter.txt if none is named: for each loop, the
// turn as it is, and each way that fewer do it.  A line a loop is printed
// as it is done, with how many instructions the turn is and the fewest
// found.  It fails if no loop ran.
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
static double work[WORKED];
static loop LOOPS[4096];
static int order[4096];

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
    char text[1024], where[256];
    say_run(text, w->code, w->len, 1);
    switch (w->how) {
        case KEPT_HERE:
            fprintf(out, "    %d with the values where they are:  begin %s until\n", w->len + 1, text);
            break;
        case KEPT_ANYWHERE:
            say_kept(where, t, w->to);
            fprintf(out, "    %d with the values anywhere (%s):  begin %s until\n", w->len + 1, where, text);
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
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "usage: shorter IMAGE.hex IMAGE.tables COUNTS [REPORT]\n");
        return(1);
    }
    const char* report = argc > 4 ? argv[4] : "build/shorter.txt";
    const char* only = getenv("ONLY");
    long states = getenv("NODES") ? atol(getenv("NODES")) : 20000;
    int longest = getenv("LONGEST") ? atoi(getenv("LONGEST")) : 12;
    get_ready();
    list_kinds();
    fewer_ready();
    cut(&IMAGE, argv[1], argv[2]);
    if (!work_read(argv[3], work, NULL)) return(1);
    FILE* out = fopen(report, "w");
    if (!out) { perror(report); return(1); }

    int loops = loops_of(&IMAGE, LOOPS, 4096), ran = 0, looked = 0;
    double all = 0, saved = 0;
    for(long at=0; at < IMAGE.instructions && at < WORKED; at++) all += IMAGE.code[at] ? work[at] : 0;
    for(int idx=0; idx < loops; idx++) if (LOOPS[idx].tail < WORKED && work[LOOPS[idx].tail] >= 1) order[ran++] = idx;
    qsort(order, ran, sizeof(int), by_work);
    fprintf(out, "%s: loops that call nothing, by how often they ran.\n\n", argv[1]);

    for(int idx=0; idx < ran; idx++) {
        const loop* l = &LOOPS[order[idx]];
        int in_word = holding(&IMAGE, l->head);
        const char* word = in_word >= 0 ? IMAGE.words[in_word].name : "?";
        double turns = work[l->tail];
        char title[256], text[1024];
        static shortened s;
        turn is;
        if (only && !named_in(only, word)) continue;
        snprintf(title, sizeof(title), "%s  %04lx-%04lx: %.0f turns", word, l->head, l->tail, turns);
        int how = turn_of(&IMAGE, l, &is);
        if (!how) {
            fprintf(out, "%s: not looked at (it calls a word, or branches inside)\n", title);
            continue;
        }
        int now = is.bodies + (how == 1 ? is.tests + 2 : 1);
        if (now > longest) {
            fprintf(out, "%s, %d instructions a turn: not looked at (more than %d)\n", title, now, longest);
            continue;
        }
        if (!shorten(&is, how, states, &s)) {
            fprintf(out, "%s: not looked at (the step cannot follow a turn)\n", title);
            continue;
        }
        looked++;
        fprintf(out, "%s, %d instructions a turn\n", title, s.turn.now);
        fprintf(out, "    now:  begin ");
        if (how == 1) {
            say_run(text, is.test, is.tests, 1);
            fprintf(out, "%s while ", text);
        }
        say_run(text, is.body, is.bodies, 1);
        fprintf(out, "%s %s\n", text, how == 1 ? "repeat" : "until");
        static const char* const kept[2] = { "where they are", "anywhere" };
        for(int anywhere=0; anywhere < 2; anywhere++) {
            int found = 0;
            for(int n=0; n < s.n; n++) found |= s.found[n].how == (anywhere ? KEPT_ANYWHERE : KEPT_HERE);
            if (found) continue;
            if (s.gave_up[anywhere]) {
                fprintf(out, "    (values %s: %d searches were given up, so fewer may do)\n", kept[anywhere], s.gave_up[anywhere]);
            } else {
                fprintf(out, "    (values %s: no fewer instructions do it)\n", kept[anywhere]);
            }
        }
        for(int n=0; n < s.n; n++) say_way(out, &s.turn, &s.found[n]);
        if (s.fewest < s.turn.now) saved += (s.turn.now - s.fewest) * turns;
        printf("%s: %d -> %d\n", title, s.turn.now, s.fewest);
        fflush(stdout);
    }
    fprintf(out, "\nInstructions that the fewest found would save: %.0f of %.0f.\n", saved, all);
    fclose(out);
    printf("\n%d loops ran, and %d were looked at; the fewest found would save %.0f of %.0f instructions.\nall of it: %s\n",
           ran, looked, saved, all, report);
    return(ran ? 0 : 1);
}
