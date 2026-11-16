//
// weigh TYPED forth/joins.fs IMAGE.sizes COUNTS PIECE...
//
// What the nucleus's compiler is worth: what it makes of the system's code
// beside what cross.fs makes, and what each of its rules saves, in the
// code and when the code is run.
//
// TYPED is what `typed read` prints: a line a run of the system's code,
// with the word and place, how many instructions were typed, how many the
// nucleus made and how many cross.fs made, and after its fourth bar the
// rules the nucleus used on it, as rule:times.  joins.fs is those rules, a
// line each, for what to call them.  IMAGE.sizes is the image's .sizes:
// the bytes of each source file's code and the instruction it starts at
// (files.c).  COUNTS is a directory with, for each PIECE of work,
// PIECE.counts, and idle.counts: how often the instruction at each place
// was run with the work done, and with the machine only started
// (bench/counting.c).
//
// An instruction fewer in a run is one fewer each time the run is run, so
// a run's instructions are weighed by how often its first is run.
//
// Prints:
//   by source file, the instructions typed, what the nucleus makes of
//     them, and what cross.fs makes
//   for each piece of work, the instructions it runs, how many more it
//     would run if the nucleus had compiled the code and not cross.fs, and
//     how many the rules save
//   each rule that is used: how often in the code, what it saves in each
//     piece, and two marks: `size` if it saves at least the bytes it is,
//     `runs` if it saves at least SHARE of the instructions of some piece
//
// There are as many pieces of work as are named, as many rules as joins.fs
// has, as many files as the .sizes has and as many places as were run:
// each has the room it needs (room.c).
//

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../util/room.c"
#include "../util/rules.c"
#include "files.c"

#define SHARE 0.0001                    // what a rule is to save of a piece's work to be marked `runs`: chosen

// A piece of work: how often each place ran in it, with what runs when the
// machine is only started taken out, and all of that.
typedef struct {
    const char* name;
    int64_t* ran;
    long places;
    int64_t all, more, saved;           // and how many more the nucleus's code would run, and its rules save
} piece;
static piece* PIECE;
static int pieces;

static int64_t ran_at(const piece* p, long at) { return(at >= 0 && at < p->places ? p->ran[at] : 0); }

typedef struct {
    char* said;
    int bytes;                          // how long it is: an instruction for each it is written as in joins.fs
    int of_three;                       // it joins three instructions, and saves two
    long uses;
    int64_t* saves;                     // in each piece,
    int64_t most;                       // and the most of them
} rule;
static rule* RULE;
static long rules;

typedef struct { long runs, typed, nucleus, cross; } tally;

// The counts of a piece of work: how many places they are of, and the
// counts, which are made here.
static long read_counts(const char* dir, const char* name, uint64_t** counts) {
    static const char ending[] = ".counts";
    char* path = room_for(strlen(dir) + 1 + strlen(name) + sizeof(ending), 1);
    unsigned char* bytes;
    sprintf(path, "%s/%s%s", dir, name, ending);
    size_t have = file_bytes(path, &bytes);
    if (!have || have % sizeof(uint64_t)) {
        fprintf(stderr, "weigh: %s is not the counts of a piece of work\n", path);
        exit(1);
    }
    free(path);
    *counts = (uint64_t*)bytes;
    return((long)(have / sizeof(uint64_t)));
}

static int by_most(const void* a, const void* b) {
    const rule* x = a;
    const rule* y = b;
    if (x->most != y->most) return(x->most < y->most ? 1 : -1);
    return(x->uses < y->uses ? 1 : x->uses > y->uses ? -1 : 0);
}

int main(int argc, char *argv[]) {
    if (argc < 6) {
        fprintf(stderr, "usage: weigh TYPED forth/joins.fs IMAGE.sizes COUNTS PIECE...\n");
        return(1);
    }
    char* line = NULL;
    long room = 0;
    pieces = argc - 5;
    PIECE = room_for(pieces, sizeof(piece));

    said_rule* said;
    rules = rules_of(argv[2], &said);
    RULE = room_for(rules, sizeof(rule));
    for(long r=0; r < rules; r++) {
        RULE[r].said = said[r].said;
        RULE[r].bytes = said[r].instructions * (int)sizeof(uint16_t);
        RULE[r].of_three = said[r].of_three;
        RULE[r].saves = room_for(pieces, sizeof(int64_t));
    }

    span* FILES;
    int files = files_of(argv[3], &FILES);
    tally* OF_FILE = room_for(files + 1, sizeof(tally));    // and one for what is in none of them

    uint64_t* idle;
    long idle_places = read_counts(argv[4], "idle", &idle);
    for(int p=0; p < pieces; p++) {
        uint64_t* did;
        PIECE[p].name = argv[5 + p];
        PIECE[p].places = read_counts(argv[4], PIECE[p].name, &did);
        PIECE[p].ran = room_for(PIECE[p].places, sizeof(int64_t));
        for(long at=0; at < PIECE[p].places; at++) {
            PIECE[p].ran[at] = (int64_t)did[at] - (int64_t)(at < idle_places ? idle[at] : 0);
            PIECE[p].all += PIECE[p].ran[at];
        }
        for(long at=PIECE[p].places; at < idle_places; at++) PIECE[p].all -= (int64_t)idle[at];    // run only when idle
        free(did);
        if (PIECE[p].all <= 0) { fprintf(stderr, "weigh: nothing was run in %s\n", PIECE[p].name); return(1); }
    }

    FILE* in = fopen(argv[1], "r");
    if (!in) { perror(argv[1]); return(1); }
    long runs = 0, typed_all = 0, nucleus_all = 0, cross_all = 0;
    while (line_of(in, &line, &room)) {
        long typed, nucleus, cross;
        int used = 0;
        const char* place = NULL;
        if (sscanf(line, "%*s%n %ld %ld %ld", &used, &typed, &nucleus, &cross) == 3) {
            for(const char* c = line; c < line + used; c++) if (*c == '@') place = c + 1;
        }
        if (!place) {
            fprintf(stderr, "weigh: this is not a run: %s", line);
            return(1);
        }
        long at = strtol(place, NULL, 16);
        runs++; typed_all += typed; nucleus_all += nucleus; cross_all += cross;
        int f = files;                                      // the file it is in: the smallest that has the place;
        for(int idx=0; idx < files; idx++) {                // or none of them, which is the file that includes the rest
            const span* x = &FILES[idx];
            if (at >= x->from && at < x->to && (f == files || x->to - x->from < FILES[f].to - FILES[f].from)) f = idx;
        }
        OF_FILE[f].runs++; OF_FILE[f].typed += typed; OF_FILE[f].nucleus += nucleus; OF_FILE[f].cross += cross;
        for(int p=0; p < pieces; p++) PIECE[p].more += (nucleus - cross) * ran_at(&PIECE[p], at);
        const char* bar = line;
        for(int n=0; n < 4 && bar; n++) bar = strchr(bar + 1, '|');
        for(const char* rest = bar ? bar + 1 : NULL; rest; ) {
            char* end;
            long which = strtol(rest, &end, 10);
            if (end == rest || *end != ':') break;
            long times = strtol(end + 1, &end, 10);
            rest = end;
            if (which >= rules) { fprintf(stderr, "weigh: rule %ld was used, and %s has %ld\n", which, argv[2], rules); return(1); }
            RULE[which].uses += times;
            for(int p=0; p < pieces; p++) {
                int64_t fewer = times * (RULE[which].of_three ? 2 : 1) * ran_at(&PIECE[p], at);
                RULE[which].saves[p] += fewer;
                PIECE[p].saved += fewer;
            }
        }
    }
    fclose(in);
    if (!runs) { fprintf(stderr, "weigh: %s has no runs\n", argv[1]); return(1); }

    printf("%-16s %6s %8s %8s %9s\n", "the code", "runs", "typed", "nucleus", "cross.fs");
    for(int idx=0; idx <= files; idx++) {
        const tally* f = &OF_FILE[idx];
        if (f->runs) printf("%-16s %6ld %8ld %8ld %9ld\n", idx < files ? FILES[idx].file : "the rest", f->runs, f->typed, f->nucleus, f->cross);
    }
    printf("%-16s %6ld %8ld %8ld %9ld\n\n", "all of it", runs, typed_all, nucleus_all, cross_all);

    printf("%-30s", "when it is run");
    for(int p=0; p < pieces; p++) printf(" %12s", PIECE[p].name);
    printf("\n%-30s", "instructions the work runs");
    for(int p=0; p < pieces; p++) printf(" %12lld", (long long)PIECE[p].all);
    printf("\n%-30s", "more, were it the nucleus's");
    for(int p=0; p < pieces; p++) printf(" %12lld", (long long)PIECE[p].more);
    printf("\n%-30s", "  to each hundred");
    for(int p=0; p < pieces; p++) printf(" %12.2f", 100.0 * PIECE[p].more / PIECE[p].all);
    printf("\n%-30s", "saved by the nucleus's rules");
    for(int p=0; p < pieces; p++) printf(" %12lld", (long long)PIECE[p].saved);
    printf("\n%-30s", "  to each hundred");
    for(int p=0; p < pieces; p++) printf(" %12.2f", 100.0 * PIECE[p].saved / PIECE[p].all);
    printf("\n\n");

    int used = 0, pay = 0, hot = 0, either = 0, rule_bytes = rules ? RULE[0].bytes : 0;     // they are all as long
    for(long r=0; r < rules; r++) for(int p=0; p < pieces; p++) if (RULE[r].saves[p] > RULE[r].most) RULE[r].most = RULE[r].saves[p];
    qsort(RULE, rules, sizeof(rule), by_most);
    printf("%-30s %5s", "the rules", "uses");
    for(int p=0; p < pieces; p++) printf(" %12s", PIECE[p].name);
    printf("\n");
    for(long r=0; r < rules; r++) {
        const rule* x = &RULE[r];
        if (!x->uses) continue;
        int size = x->uses * (x->of_three ? 2 : 1) * (int)sizeof(uint16_t) >= x->bytes, run = 0;
        for(int p=0; p < pieces; p++) run |= x->saves[p] >= SHARE * PIECE[p].all;
        used++; pay += size; hot += run; either += size || run;
        printf("%-30.30s %5ld", x->said, x->uses);
        for(int p=0; p < pieces; p++) printf(" %12lld", (long long)x->saves[p]);
        printf("  %s%s\n", size ? " size" : "", run ? " runs" : "");
    }
    printf("\n%ld rules: %d are used in the code; %d save at least their own %d bytes there (size), %d save at least %g%% of what some piece of work runs (runs), %d do one or the other\n",
           rules, used, pay, rule_bytes, hot, 100 * SHARE, either);
    return(0);
}
