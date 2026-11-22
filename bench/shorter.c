//
// shorter IMAGE.hex IMAGE.tables COUNTS [REPORT]
//
// Fewer instructions that do what a loop does, wherever its values are
// kept: for each loop of the image that ran, with the words it calls put in
// place, what a turn costs now, and the fewest instructions found that do a
// turn.  What a loop and its turn are is loops.h's to say, the ways fewer
// are looked for turns.h's, and the search fewer.h's; this orders the loops
// by the work done in them and says what was found.
//
// COUNTS is the work by place, as bench/profile.c leaves it
// (build/profile/desktop.counts; work.h).  ONLY in the environment is
// words, with commas between, whose loops alone are looked at.  Every loop
// that ran and has a turn is looked at, however long its turn; how long a
// search may take is said once, in cmake/default-machine.cmake
// (SEARCH_STATES, which fewer.h has).
//
// The report is build/shorter.txt if none is named: for each loop, the
// turn as it is, each way that fewer do it, and for each way it was looked
// for how many searches were made and how many of them were given up.  It
// is written a line at a time, so what is done can be read while the rest
// is looked for.  A line a loop is printed as it is done, with how many
// instructions the turn is and the fewest found; and a line a search to
// stderr as each is made, with the loop, the way, how many instructions
// were looked for of how many, and whether it was given up, so that a long
// loop says how far it has got.  It fails if no loop ran.
//
// shorter IMAGE.hex IMAGE.tables --rom [REPORT]
//
// The same asked of a kernel's ROM, which is the first KROM_BYTES of the
// image, for what would make it shorter: every run of its words (cut.h),
// with everything where it is, whole and then a part at a time, and the
// fewest instructions found that leave what the run leaves; and every loop
// it has, whether it ran or not, as above.  A run that ends a
// word is looked for with the return in its last instruction, and then with
// a return of its own, which is an instruction more; which instructions a
// return can be in is asked of the step.  A run that a branch follows is
// wanted, on top, only for whether it is nothing.  A run the step says
// nothing of, as one that reads or writes a port, is not looked at; and
// the instruction after a call of a word that reads where it returns to is
// no part of a run, since such a word may take it for its own (inline:).
//
// The searches of one run have nothing to do with another's, so as many
// runs are looked at at once as the computer has processors, the longest
// first, and the loops beside them.  It says a line to stderr as each run
// is begun, and another if fewer do it; and when all are done, each run
// that fewer do, as it is and as found, how many instructions of the ROM
// the fewest found would save, and then the loops.  ONLY is as above.  It
// fails if no run was looked at.
//

#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include "../util/fuse.h"
#include "../util/cut.h"
#include "../util/reach.h"
#include "../util/kinds.h"
#include "../util/loops.h"
#include "../util/fewer.h"
#include "../util/turns.h"
#include "work.h"

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

// A search as it is made, a line to stderr (turns.h, a_search).
static const char* searching_in;        // the loop: its word and places
static long searches;                   // made of it so far

static void a_search_said(int how, int length, int of, int given_up) {
    static const char* const WAYS[] = {
        [KEPT_HERE] = "where they are", [KEPT_ANYWHERE] = "anywhere", [TEST_AT_BOTTOM] = "test at the bottom",
        [A_PART_AT_A_TIME] = "a part at a time", [NUMBER_IN_R] = "a number in R" };
    fprintf(stderr, "%s: search %ld, %s, %d of %d%s\n", searching_in, ++searches, WAYS[how], length, of,
            given_up ? ": given up" : "");
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

// ---- a kernel's ROM, a run at a time

#define ROM_ENDS (KROM_BYTES / (long)sizeof(uint16_t))  // the instruction its ROM ends before
#define PART 8                          // the longest part of a run that is looked at by itself: chosen, and not the
                                        // machine's, as survey.c's longest run is

// The instructions a return can be folded into (cross.fs, fold-exit): one
// with the return in it leaves what the instruction and then a return
// leave.  That is asked of the step, for each that has not the return's
// fields in use already.
static unsigned char TAKES_RETURN[EVERY_WORD];

static void list_returns(void) {
    uint16_t exits = ins_named("exit");
    for(int idx=0; idx < alus; idx++) {
        uint16_t pair[2] = { ALU[idx].ins, exits };
        instruction with = ins_of(ALU[idx].ins);
        if (with.alu.r_eip || with.alu.rstack) continue;
        with.alu.r_eip = 1;
        with.alu.rstack = ins_of(exits).alu.rstack;
        TAKES_RETURN[ALU[idx].ins] = same_work(pair, 2, *(uint16_t*)&with);
    }
    forget();
}

// What is after a run: more of the word; a branch, which asks only whether
// the top is nothing; or the word's return, which is in the run's last
// instruction or is an instruction of its own after it.
enum { GOES_ON, BRANCHES, RETURNS };

typedef struct {
    uint16_t code[LAID_MOST + 1];       // without the return
    int len;
    int after;                          // one of those,
    int own_return;                     // and whether a return is an instruction of its own
} a_run;

static int cost_of(const a_run* r) { return(r->len + (r->after == RETURNS && r->own_return)); }

static wanted OF_RUN;

// Fewer instructions that leave what a run leaves, with everything where
// it is: the run is made those, and what it then costs is given, which is
// what it cost if no fewer were found.  They are looked for from the fewest
// up; for a run that returns, with the return in the last instruction and
// then by itself.  What is found is run on vm.c beside the run.  Less than
// nothing if the step does not say what the run leaves.
static int fewer_for(a_run* r, long* searched, long* given_up) {
    named left;
    int now = cost_of(r), tops = r->after == BRANCHES, returns = r->after == RETURNS;
    forget();
    if (r->len > LAID || !names(r->code, r->len, &left) || left.next >= 0) return(-1);
    if (!want(&OF_RUN, &left, tops ? nothing(left.T) : -1)) return(-1);
    for(int length=1; length < now; length++) {
        for(int own=!returns; own < 2; own++) {
            uint16_t found[LAID_MOST];
            int gave_up = 0;
            if (length + (returns && own) >= now) continue;
            ends_in = returns && !own ? TAKES_RETURN : NULL;
            int is = exactly(&OF_RUN, NULL, length, SEARCH_STATES, found, &gave_up);
            ends_in = NULL;
            ++*searched;
            *given_up += gave_up;
            if (!is || agree_on_machine(r->code, r->len, found, length, NULL, NULL, 0, tops, 0, 0) <= 0) continue;
            memcpy(r->code, found, length * sizeof(uint16_t));
            r->len = length;
            r->own_return = returns && own;
            return(cost_of(r));
        }
    }
    return(now);
}

// The same a part at a time: any part of the run, of no more than PART
// instructions, that fewer do the same as, the longest first, until none is
// found.  The whole is not a part: it was looked for whole.
static int fewer_by_parts(a_run* r, long* searched, long* given_up) {
    for(int again=1; again; ) {
        again = 0;
        for(int size = r->len - 1 < PART ? r->len - 1 : PART; size > 1 && !again; size--) {
            for(int at=0; at + size <= r->len && !again; at++) {
                static a_run part;
                int last = at + size == r->len;
                memcpy(part.code, r->code + at, size * sizeof(uint16_t));
                part.len = size;
                part.after = last ? r->after : GOES_ON;
                part.own_return = last && r->own_return;
                int was = cost_of(&part), is = fewer_for(&part, searched, given_up);
                if (is < 0 || is >= was) continue;
                memmove(r->code + at + part.len, r->code + at + size, (r->len - at - size) * sizeof(uint16_t));
                memcpy(r->code + at, part.code, part.len * sizeof(uint16_t));
                r->len -= size - part.len;
                if (last) r->own_return = part.own_return;
                again = 1;
            }
        }
    }
    return(cost_of(r));
}

// A run as it is said, with what is after it.
static char* a_run_said(const a_run* r) {
    static const char* const AFTER[] = { [GOES_ON] = "", [BRANCHES] = "  (a branch asks of the top)" };
    char* text = run_said(r->code, r->len, 1);
    const char* ends = r->after != RETURNS ? AFTER[r->after] : r->own_return ? " exit" : ";";
    text = realloc(text, strlen(text) + strlen(ends) + 1);
    if (!text) { fprintf(stderr, "there is no room for the text of a run\n"); exit(EXIT_FAILURE); }
    return(strcat(text, ends));
}

// Does a word read where it returns to, before it has put anything of its
// own over that?  Such a word may take what follows its call as its own:
// inline: copies the instruction there (nuc.fs), and nothing runs it.
static int reads_its_return(long at) {
    int above = 0;
    for(long a=at; a >= 0 && a < IMAGE.instructions; a++) {
        uint16_t w = IMAGE.code[a];
        instruction ins = ins_of(w);
        if (!in_a_run(w)) return(0);
        if (ins.lit.lit_f) continue;
        if (ins.alu.in_mux == INPUT_R && !above) return(1);
        if (ins.alu.r_eip) return(0);
        if ((above += ins.alu.rstack) < 0) return(1);
    }
    return(0);
}

// A run that is to be looked at, where it is, and what was found of one.
typedef struct { long item, at, len; int in_word; a_run is; } to_do;
typedef struct { long nth; int now, fewest; long searched, given_up; a_run found; } done;

static int longest_first(const void* a, const void* b) {
    const to_do* x = a;
    const to_do* y = b;
    int by = cost_of(&y->is) - cost_of(&x->is);
    return(by ? by : x->item < y->item ? -1 : x->item > y->item);
}

static to_do* TO_DO;

static int by_item(const void* a, const void* b) {
    long x = TO_DO[*(const long*)a].item, y = TO_DO[*(const long*)b].item;
    return(x < y ? -1 : x > y);
}

// Every run of the ROM's words, and what fewer instructions do each: how
// many runs were looked at.  The searches of one run have nothing to do
// with another's, so as many runs are looked at at once as the computer
// has processors, each by a copy of this program, the longest first: a
// copy takes the next run there is, and leaves what it found in a file of
// its own, which is read when all are done.
static long rom_runs(FILE* out, const char* only) {
    uint16_t exits = ins_named("exit");
    long todos = 0, room = 0;
    list_returns();
    for(long idx=0; idx < IMAGE.count; idx++) {
        const item* it = &IMAGE.items[idx];
        const item* before = idx ? &IMAGE.items[idx - 1] : NULL;
        static a_run is;
        long at = it->at, len = it->len;
        if (!it->run || at >= ROM_ENDS) continue;
        // what a word that reads where it returns to was called before is that word's, and no part of the run
        if (before && !before->run && before->at + before->len == at && before->to >= 0 &&
            ins_of(IMAGE.code[at - 1]).jmp.op_type == OP_TYPE_CALL && reads_its_return(before->to)) at++, len--;
        if (len < 1 || len > LAID) continue;
        int in_word = holding(&IMAGE, at);
        if (in_word < 0 || (only && !named_in(only, IMAGE.words[in_word].name))) continue;
        uint16_t last = IMAGE.code[at + len - 1];
        const item* next = idx + 1 < IMAGE.count ? &IMAGE.items[idx + 1] : NULL;
        int returned = 0;
        memcpy(is.code, IMAGE.code + at, len * sizeof(uint16_t));
        is.len = len;
        is.after = GOES_ON;
        is.own_return = 0;
        if (last == exits) {
            is.after = RETURNS, is.own_return = 1, is.len--;
        } else if (!ins_of(last).lit.lit_f && ins_of(last).alu.r_eip) {
            is.code[is.len - 1] = unfolded(last, &returned);
            if (!returned) continue;                    // it returns some other way, which the step is not asked of
            is.after = RETURNS;
        } else if (next && !next->run && next->at == at + len &&
                   ins_of(IMAGE.code[next->at + next->len - 1]).jmp.op_type == OP_TYPE_CJMP) {
            is.after = BRANCHES;
        }
        if (cost_of(&is) < 2) continue;                 // nothing fewer than one instruction does its work
        ONE_MORE(TO_DO, todos, room);
        TO_DO[todos++] = (to_do){ idx, at, len, in_word, is };
    }
    if (!todos) return(0);
    qsort(TO_DO, todos, sizeof(to_do), longest_first);

    long* next = mmap(NULL, sizeof(long), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
    long at_once = sysconf(_SC_NPROCESSORS_ONLN);
    if (next == MAP_FAILED) { perror("shorter"); exit(EXIT_FAILURE); }
    if (at_once < 1) at_once = 1;
    if (at_once > todos) at_once = todos;
    FILE** from = room_for(at_once, sizeof(FILE*));
    pid_t* by = room_for(at_once, sizeof(pid_t));
    *next = 0;
    fflush(NULL);
    for(long k=0; k < at_once; k++) {
        from[k] = tmpfile();
        if (!from[k] || (by[k] = fork()) < 0) { perror("shorter"); exit(EXIT_FAILURE); }
        if (by[k]) continue;
        for(;;) {                                       // a copy: the next run there is, until there is none
            long nth = __atomic_fetch_add(next, 1, __ATOMIC_SEQ_CST);
            if (nth >= todos) break;
            const to_do* t = &TO_DO[nth];
            const char* word = IMAGE.words[t->in_word].name;
            long at = t->at;
            static done d;
            d = (done){ nth, cost_of(&t->is), 0, 0, 0, t->is };
            fprintf(stderr, "%s  %04lx: %d instructions\n", word, at, d.now);
            d.fewest = fewer_for(&d.found, &d.searched, &d.given_up);
            if (d.fewest == d.now) d.fewest = fewer_by_parts(&d.found, &d.searched, &d.given_up);
            if (d.fewest >= 0 && d.fewest < d.now) {
                char* text = a_run_said(&d.found);
                fprintf(stderr, "%s  %04lx: %d -> %d:  %s\n", word, at, d.now, d.fewest, text);
                free(text);
            }
            fwrite(&d, sizeof(d), 1, from[k]);
            fflush(from[k]);
        }
        _exit(0);
    }

    done* found = room_for(todos, sizeof(done));
    long* order = room_for(todos, sizeof(long));
    for(long nth=0; nth < todos; nth++) found[nth].nth = -1, order[nth] = nth;
    for(long k=0; k < at_once; k++) {
        int how = 0;
        static done d;
        if (waitpid(by[k], &how, 0) < 0 || !WIFEXITED(how) || WEXITSTATUS(how)) {
            fprintf(stderr, "shorter: a copy of it that was looking at the ROM's runs stopped\n");
            exit(EXIT_FAILURE);
        }
        rewind(from[k]);
        while (fread(&d, sizeof(d), 1, from[k]) == 1) found[d.nth] = d;
        fclose(from[k]);
    }
    qsort(order, todos, sizeof(long), by_item);

    long looked = 0, shorter_runs = 0, saved = 0, searched = 0, given_up = 0, not_said = 0;
    fprintf(out, "\nThe runs of the ROM's words, each with everything where it is: whole, and then a part of no more than "
            "%d instructions at a time.\n\n", PART);
    for(long idx=0; idx < todos; idx++) {
        const to_do* t = &TO_DO[order[idx]];
        const done* d = &found[order[idx]];
        if (d->nth < 0) {
            fprintf(stderr, "shorter: the run at %04lx was not looked at\n", t->at);
            exit(EXIT_FAILURE);
        }
        searched += d->searched;
        given_up += d->given_up;
        if (d->fewest < 0) { not_said++; continue; }
        looked++;
        if (d->fewest >= d->now) continue;
        char* was = a_run_said(&t->is);
        char* text = a_run_said(&d->found);
        fprintf(out, "%s  %04lx-%04lx, %d instructions:  %s\n    %d:  %s\n", IMAGE.words[t->in_word].name, t->at,
                t->at + t->len - 1, d->now, was, d->fewest, text);
        printf("%s  %04lx: %d -> %d\n", IMAGE.words[t->in_word].name, t->at, d->now, d->fewest);
        free(was);
        free(text);
        shorter_runs++;
        saved += d->now - d->fewest;
    }
    fprintf(out, "\nRuns looked at: %ld, in %ld searches, %ld of them given up; %ld more that the step does not say what "
            "they leave.\nRuns that fewer do: %ld.  Instructions of the ROM that the fewest found would save: %ld.\n",
            looked, searched, given_up, not_said, shorter_runs, saved);
    printf("\n%ld runs of the ROM were looked at, and fewer do %ld of them: %ld instructions.\n", looked, shorter_runs, saved);
    fflush(NULL);
    free(found);
    free(order);
    free(from);
    free(by);
    return(looked);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "usage: shorter IMAGE.hex IMAGE.tables COUNTS [REPORT]\n"
                        "       shorter IMAGE.hex IMAGE.tables --rom [REPORT]\n");
        return(1);
    }
    const char* report = argc > 4 ? argv[4] : "build/shorter.txt";
    const char* only = getenv("ONLY");
    int rom = !strcmp(argv[3], "--rom");
    machine_from(argv[1]);              // the cell is the image's, before anything is asked of it
    get_ready();
    list_kinds();
    fewer_ready();
    cut(&IMAGE, argv[1], argv[2]);
    long n = IMAGE.instructions;
    work = room_for(n, sizeof(double));                 // nothing, for an instruction the counts are not of
    if (rom) {
        for(long at=0; at < n && at < ROM_ENDS; at++) work[at] = 1;     // every loop of it, as if each had run once
    } else {
        double* counts;
        long counted = work_read(argv[3], &counts, NULL);
        if (!counted) return(1);
        for(long at=0; at < n && at < counted; at++) work[at] = counts[at];
        free(counts);
    }
    FILE* out = fopen(report, "w");
    if (!out) { perror(report); return(1); }
    setvbuf(out, NULL, _IOLBF, 0);                      // a line at a time, to be read while the rest is looked for
    a_search = rom ? NULL : a_search_said;

    int loops = loops_of(&IMAGE, &LOOPS), ran_loops = 0, looked = 0;
    int* order = room_for(loops, sizeof(int));
    double all = 0, saved = 0;
    for(long at=0; at < n; at++) all += IMAGE.code[at] ? work[at] : 0;
    for(int idx=0; idx < loops; idx++) if (work[LOOPS[idx].tail] >= 1) order[ran_loops++] = idx;
    qsort(order, ran_loops, sizeof(int), by_work);
    if (rom) {
        // The runs here, while a copy of this looks at the loops, and
        // leaves what it says of them in a file until the runs are said.
        fprintf(out, "%s: the kernel's ROM.  A search is given up after %ld states.\n", argv[1], (long)SEARCH_STATES);
        FILE* of_loops = tmpfile();
        fflush(NULL);
        pid_t loops_by = of_loops ? fork() : -1;
        if (loops_by < 0) { perror("shorter"); return(1); }
        if (loops_by) {
            long runs = rom_runs(out, only);
            char some[4096];
            size_t had;
            int how = 0;
            if (waitpid(loops_by, &how, 0) < 0 || !WIFEXITED(how) || WEXITSTATUS(how)) {
                fprintf(stderr, "shorter: the copy of it that was looking at the ROM's loops stopped\n");
                return(1);
            }
            rewind(of_loops);
            while ((had = fread(some, 1, sizeof(some), of_loops)) > 0) fwrite(some, 1, had, out);
            fclose(out);
            printf("all of it: %s\n", report);
            return(runs ? 0 : 1);
        }
        out = of_loops;
        fprintf(out, "\nThe loops of the ROM, with the words they call put in place.\n\n");
    } else {
        fprintf(out, "%s: the loops that ran, with the words they call put in place, by how often they ran.  A search is "
                "given up after %ld states.\n\n", argv[1], (long)SEARCH_STATES);
    }

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
        if (rom) snprintf(title, sizeof(title), "%s  %04lx-%04lx", word, l->head, l->tail);
        else snprintf(title, sizeof(title), "%s  %04lx-%04lx: %.0f turns", word, l->head, l->tail, turns);
        if (!turn_of(&IMAGE, l, &is)) {
            fprintf(out, "%s: not looked at (it branches inside, or calls a word that does or that touches where it "
                    "returns to)\n", title);
            continue;
        }
        if (is.bodies + is.tests > LAID) {
            fprintf(out, "%s, %d instructions a turn: not looked at (with the words it calls in place it is %d, and a run "
                    "vm.c is given is no more than %d: fuse.h, LAID)\n", title, turn_now(&is), is.bodies + is.tests, LAID);
            free(is.own);
            continue;
        }
        searching_in = title;
        searches = 0;
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
    if (rom) {
        fprintf(out, "\nInstructions a turn that the fewest found would save, of the ROM's loops: %.0f.\n", saved);
        printf("\n%d loops of the ROM, and %d were looked at; the fewest found would save %.0f instructions a turn.\n",
               ran_loops, looked, saved);
        fflush(NULL);
        _exit(0);                       // the copy that looked at the loops: the other says the rest
    }
    fprintf(out, "\nInstructions that the fewest found would save: %.0f of %.0f.\n", saved, all);
    fclose(out);
    printf("\n%d loops ran, and %d were looked at; the fewest found would save %.0f of %.0f instructions.\nall of it: %s\n",
           ran_loops, looked, saved, all, report);
    return(ran_loops ? 0 : 1);
}
