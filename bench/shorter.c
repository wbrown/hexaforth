//
// shorter IMAGE.hex IMAGE.tables COUNTS [REPORT]
//
// Searches for fewer instructions that do one turn of a loop, with the
// loop's values kept in any cells.  For each loop of the image that ran,
// it inlines the words the loop calls, reports what a turn costs now, and
// reports the fewest instructions found that do a turn.  loops.h defines a
// loop and its turn, turns.h defines the ways a shorter turn is searched
// for, and fewer.h does the search.  This program orders the loops by the
// work done in them and writes the report.
//
// COUNTS is the work by address, as bench/profile.c writes it
// (build/profile/desktop.counts; work.h).  The target shorter-loops runs
// this program on that file, after the profile.  If the environment
// variable ONLY is set, it is a list of words separated by commas, and
// only the loops in those words are searched.  Every loop that ran and has
// a turn is searched, however long its turn is.  The limit on one search
// is set in one place, cmake/default-machine.cmake (SEARCH_STATES, which
// fewer.h uses).
//
// The report is build/shorter.txt unless REPORT names another file.  For
// each loop it gives the turn as it is now, each shorter form found, and,
// for each way of searching, the number of searches made and the number
// that gave up.  The report is written a line at a time, so the finished
// part can be read while the search goes on.  One line is printed for each
// loop when it is finished: the length of its turn, and the fewest
// instructions found.  One line is written to stderr for each search as it
// is made: the loop, the way, the length searched for, the turn's length,
// and whether the search gave up.  That shows how far a long loop has got.
// The program fails if no loop ran.
//
// shorter IMAGE.hex IMAGE.tables --rom [REPORT]
//
// Does the same for a kernel's ROM, which is the first KROM_BYTES of the
// image, to find what would make it shorter.  It searches every
// straight-line run of the ROM's words (cut.h), with every value in its
// present cell: first the whole run, and then a part at a time.  For each
// run it reports the fewest instructions found that reach the run's final
// state.  It also searches every loop in the ROM as above, whether or not
// the loop ran.
//
// A run that ends a word is searched for first with the return in its last
// instruction, and then with a separate return instruction, which costs
// one more.  The step tells which instructions can have the return in
// them.  If a branch follows a run, the top of the stack matters only as
// zero or non-zero.  A run the step does not model, such as one that reads
// or writes a port, is not searched.  The instruction after a call to a
// word that reads its return address is not part of a run, because such a
// word may use that instruction as data (inline:).
//
// The searches for one run are independent of the searches for another.
// So as many runs are searched at the same time as the computer has
// processors, longest first, and the loops are searched alongside them.
// A line is written to stderr when each run is started, and another if a
// shorter form is found.  When all are finished, the report lists each run
// that has a shorter form, as it is now and as found; the number of the
// ROM's instructions the shorter forms would save; and then the loops.
// ONLY works as above.  The program fails if no run was searched.
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
static double* work;                    // the number of times each of the image's instructions ran
static loop* LOOPS;

// Returns the total of the counts of a loop's instructions.
static double work_in(const loop* l) {
    double is = 0;
    for(long at=l->head; at <= l->tail; at++) is += work[at];
    return(is);
}

static int by_work(const void* a, const void* b) {
    double x = work_in(&LOOPS[*(const int*)a]), y = work_in(&LOOPS[*(const int*)b]);
    return(x < y ? 1 : x > y ? -1 : *(const int*)a - *(const int*)b);
}

// Returns 1 if `name` is one of the names in a list separated by commas.
static int named_in(const char* list, const char* name) {
    size_t len = strlen(name);
    for(const char* at = list; at && *at; at = strchr(at, ',') ? strchr(at, ',') + 1 : NULL) {
        if (!strncmp(at, name, len) && (at[len] == ',' || !at[len])) return(1);
    }
    return(0);
}

// a_search_said() writes one line to stderr for each search as it is
// made.  turns.h calls it through `a_search`.
static const char* searching_in;        // the loop being searched: its word and its addresses
static long searches;                   // the number of searches made for that loop so far

static void a_search_said(int how, int length, int of, int given_up) {
    static const char* const WAYS[] = {
        [KEPT_HERE] = "where they are", [KEPT_ANYWHERE] = "anywhere", [TEST_AT_BOTTOM] = "test at the bottom",
        [A_PART_AT_A_TIME] = "a part at a time", [NUMBER_IN_R] = "a number in R" };
    fprintf(stderr, "%s: search %ld, %s, %d of %d%s\n", searching_in, ++searches, WAYS[how], length, of,
            given_up ? ": given up" : "");
}

// Writes one shorter form of a turn as a line of the report.
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

#define ROM_ENDS (KROM_BYTES / (long)sizeof(uint16_t))  // the address of the first instruction after the ROM
#define PART 8                          // the longest part of a run that is searched by itself.  It is a choice and not
                                        // a property of the machine, like the longest run in survey.c

// TAKES_RETURN[ins] is 1 for each instruction a return can be folded into
// (cross.fs, fold-exit): the instruction with the return's fields set is
// equivalent to the instruction followed by a return.  list_returns()
// fills it in.  It asks the step about every ALU instruction that does not
// already use the return's fields.
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

// What follows a run: more of the word (GOES_ON); a branch, which tests
// only whether the top is zero (BRANCHES); or the word's return (RETURNS),
// which is either in the run's last instruction or a separate instruction
// after it.
enum { GOES_ON, BRANCHES, RETURNS };

typedef struct {
    uint16_t code[LAID_MOST + 1];       // the run's instructions, without the return
    int len;
    int after;                          // GOES_ON, BRANCHES or RETURNS
    int own_return;                     // 1 if the return is a separate instruction
} a_run;

// Returns the number of instructions a run takes, counting a separate
// return.
static int cost_of(const a_run* r) { return(r->len + (r->after == RETURNS && r->own_return)); }

static wanted OF_RUN;

// Searches for fewer instructions that reach a run's final state, with
// every value in its present cell.  If it finds them, it replaces the run
// with them.  Returns the run's cost afterwards, which is its old cost if
// the search found no replacement.  Lengths are tried from the shortest up.  For
// a run that returns, each length is tried with the return in the last
// instruction and then with a separate return.  An answer is accepted only
// if it agrees with the run when both are run on vm.c.  Returns -1 if the
// step cannot give the run's final state.  `*searched` and `*given_up` are
// increased by the number of searches made and the number that gave up.
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

// Does the same a part at a time.  It searches each part of the run that
// has at most PART instructions, longest first, and replaces a part when a
// shorter equivalent is found.  It repeats until no part can be shortened.
// The whole run is not tried as a part, because fewer_for() has already
// searched it whole.  Returns the run's cost afterwards.
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

// Returns a run as text, with what follows it.  The caller frees the text.
static char* a_run_said(const a_run* r) {
    static const char* const AFTER[] = { [GOES_ON] = "", [BRANCHES] = "  (a branch asks of the top)" };
    char* text = run_said(r->code, r->len, 1);
    const char* ends = r->after != RETURNS ? AFTER[r->after] : r->own_return ? " exit" : ";";
    text = realloc(text, strlen(text) + strlen(ends) + 1);
    if (!text) { fprintf(stderr, "there is no room for the text of a run\n"); exit(EXIT_FAILURE); }
    return(strcat(text, ends));
}

// Returns 1 if the word at an address reads or drops its return address
// before it has pushed anything of its own on the return stack.  Such a
// word may use the instruction after its call as data: `inline:` copies
// that instruction (nuc.fs), and it is never run where it is.
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

// A to_do is a run to be searched: its item in the image, its address and
// length, the word it is in, and the run itself.  A done is the result for
// the nth to_do: its cost now, the fewest found, the searches made and
// given up, and the run as found.
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

// Searches every run of the ROM's words for a shorter equivalent, and
// writes what it finds to the report.  Returns the number of runs
// searched.  The searches for one run are independent of the searches for
// another, so the work is shared among as many child processes as the
// computer has processors.  The runs are taken longest first.  Each child
// takes the next run that has not been taken, and writes its results to a
// temporary file of its own.  The parent reads those files when every
// child has finished.
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
        // the instruction after a call to a word that reads its return address belongs to that word, and is not part of the run
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
            if (!returned) continue;                    // it returns in some other way, which this search does not cover
            is.after = RETURNS;
        } else if (next && !next->run && next->at == at + len &&
                   ins_of(IMAGE.code[next->at + next->len - 1]).jmp.op_type == OP_TYPE_CJMP) {
            is.after = BRANCHES;
        }
        if (cost_of(&is) < 2) continue;                 // a run of one instruction cannot be made shorter
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
        for(;;) {                                       // the child: take the next run, until none is left
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
    fprintf(out, "\nThe runs of the ROM's words, each with everything where it is: whole, and then in parts of at most "
            "%d instructions.\n\n", PART);
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
    machine_from(argv[1]);              // take the cell size from the image before the step is used
    get_ready();
    list_kinds();
    fewer_ready();
    cut(&IMAGE, argv[1], argv[2]);
    long n = IMAGE.instructions;
    work = room_for(n, sizeof(double));                 // 0 for an instruction the counts do not cover
    if (rom) {
        for(long at=0; at < n && at < ROM_ENDS; at++) work[at] = 1;     // search every loop of the ROM, as if each had run once
    } else {
        double* counts;
        long counted = work_read(argv[3], &counts, NULL);
        if (!counted) return(1);
        for(long at=0; at < n && at < counted; at++) work[at] = counts[at];
        free(counts);
    }
    FILE* out = fopen(report, "w");
    if (!out) { perror(report); return(1); }
    setvbuf(out, NULL, _IOLBF, 0);                      // write a line at a time, so the report can be read while the search goes on
    a_search = rom ? NULL : a_search_said;

    int loops = loops_of(&IMAGE, &LOOPS), ran_loops = 0, looked = 0;
    int* order = room_for(loops, sizeof(int));
    double all = 0, saved = 0;
    for(long at=0; at < n; at++) all += IMAGE.code[at] ? work[at] : 0;
    for(int idx=0; idx < loops; idx++) if (work[LOOPS[idx].tail] >= 1) order[ran_loops++] = idx;
    qsort(order, ran_loops, sizeof(int), by_work);
    if (rom) {
        // The parent searches the runs.  A child process searches the
        // loops and writes its part of the report to a temporary file,
        // which the parent appends after its own part.
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
        char title[sizeof(IMAGE.words[0].name) + 64];   // room for a word's name, two addresses and a count
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
                    "vm.c is given has at most %d: fuse.h, LAID)\n", title, turn_now(&is), is.bodies + is.tests, LAID);
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
                        found ? "" : ", so a replacement may exist");
            } else {
                fprintf(out, "    (%s: %ld searches, every one to the end%s)\n", WAY[how], s.searched[how],
                        found ? "" : ", and none found a replacement");
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
        _exit(0);                       // this is the child that searched the loops; the parent writes the rest
    }
    fprintf(out, "\nInstructions that the fewest found would save: %.0f of %.0f.\n", saved, all);
    fclose(out);
    printf("\n%d loops ran, and %d were looked at; the fewest found would save %.0f of %.0f instructions.\nall of it: %s\n",
           ran_loops, looked, saved, all, report);
    return(ran_loops ? 0 : 1);
}
