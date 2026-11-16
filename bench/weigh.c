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
// the bytes of each source file's code and the instruction it starts at.
// COUNTS is a directory with, for each PIECE of work, PIECE.counts, and
// idle.counts: how often the instruction at each place was run with the
// work done, and with the machine only started (bench/counting.c).
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PLACES 0x20000                  // as bench/counting.c
#define PIECES 8
#define RULES 1024
#define FILES 128
#define SHARE 0.0001
#define RULE_BYTES 8                    // a rule is four instructions

static int64_t RAN[PIECES][PLACES], ALL[PIECES];
static int pieces;

typedef struct {
    char said[160];
    int of_three;                       // it joins three instructions, and saves two
    long uses;
    int64_t saves[PIECES], most;
} rule;
static rule RULE[RULES];
static int rules;

typedef struct {
    char name[80];
    long from, upto;
    long runs, typed, nucleus, cross;
} source;
static source FILE_OF[FILES + 1];       // and one for what is in none of them
static int files;

static void read_counts(const char* dir, const char* piece, uint64_t* to) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s.counts", dir, piece);
    FILE* in = fopen(path, "rb");
    if (!in || fread(to, sizeof(uint64_t), PLACES, in) != PLACES) {
        fprintf(stderr, "weigh: %s is not the counts of a piece of work\n", path);
        exit(1);
    }
    fclose(in);
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
    static char line[1 << 16];
    FILE* in = fopen(argv[2], "r");
    if (!in) { perror(argv[2]); return(1); }
    while (fgets(line, sizeof(line), in) && rules < RULES) {
        if (line[0] != '$') continue;                       // a rule is a line of instructions
        const char* said = strchr(line, '\\');
        strncpy(RULE[rules].said, said ? said + 2 : "", sizeof(RULE[rules].said) - 1);
        RULE[rules].said[strcspn(RULE[rules].said, "\n")] = 0;
        RULE[rules].of_three = strncmp(line, "$0000", 5) != 0;
        rules++;
    }
    fclose(in);

    in = fopen(argv[3], "r");
    if (!in) { perror(argv[3]); return(1); }
    while (fgets(line, sizeof(line), in) && files < FILES) {
        long code, data, headers, labels, start;
        source* f = &FILE_OF[files];
        if (sscanf(line, "%79s %ld %ld %ld %ld %ld", f->name, &code, &data, &headers, &labels, &start) != 6) continue;
        if (f->name[0] == '(') continue;                    // the line for all of it: not a file
        f->from = start; f->upto = start + code / 2;
        files++;
    }
    fclose(in);

    static uint64_t did[PLACES], idle[PLACES];
    read_counts(argv[4], "idle", idle);
    for(int a=5; a < argc && pieces < PIECES; a++, pieces++) {
        read_counts(argv[4], argv[a], did);
        for(long at=0; at < PLACES; at++) {
            RAN[pieces][at] = (int64_t)did[at] - (int64_t)idle[at];
            ALL[pieces] += RAN[pieces][at];
        }
        if (ALL[pieces] <= 0) { fprintf(stderr, "weigh: nothing was run in %s\n", argv[a]); return(1); }
    }

    in = fopen(argv[1], "r");
    if (!in) { perror(argv[1]); return(1); }
    long runs = 0, typed_all = 0, nucleus_all = 0, cross_all = 0;
    int64_t more[PIECES] = { 0 }, saved[PIECES] = { 0 };
    while (fgets(line, sizeof(line), in)) {
        char where[200];
        long typed, nucleus, cross;
        if (sscanf(line, "%199s %ld %ld %ld", where, &typed, &nucleus, &cross) != 4 || !strrchr(where, '@')) {
            fprintf(stderr, "weigh: this is not a run: %s", line);
            return(1);
        }
        long at = strtol(strrchr(where, '@') + 1, NULL, 16);
        runs++; typed_all += typed; nucleus_all += nucleus; cross_all += cross;
        source* f = NULL;                                   // the file it is in: the smallest that has the place
        for(int idx=0; idx < files; idx++) {
            source* x = &FILE_OF[idx];
            if (at >= x->from && at < x->upto && (!f || x->upto - x->from < f->upto - f->from)) f = x;
        }
        if (!f) f = &FILE_OF[files];                        // in none of them: the file that includes the rest
        f->runs++; f->typed += typed; f->nucleus += nucleus; f->cross += cross;
        for(int p=0; p < pieces; p++) more[p] += (nucleus - cross) * RAN[p][at];
        const char* bar = line;
        for(int n=0; n < 4 && bar; n++) bar = strchr(bar + 1, '|');
        for(const char* rest = bar ? bar + 1 : NULL; rest; ) {
            char* end;
            long which = strtol(rest, &end, 10);
            if (end == rest || *end != ':') break;
            long times = strtol(end + 1, &end, 10);
            rest = end;
            if (which >= rules) { fprintf(stderr, "weigh: rule %ld was used, and %s has %d\n", which, argv[2], rules); return(1); }
            RULE[which].uses += times;
            for(int p=0; p < pieces; p++) {
                int64_t fewer = times * (RULE[which].of_three ? 2 : 1) * RAN[p][at];
                RULE[which].saves[p] += fewer;
                saved[p] += fewer;
            }
        }
    }
    fclose(in);
    if (!runs) { fprintf(stderr, "weigh: %s has no runs\n", argv[1]); return(1); }

    printf("%-16s %6s %8s %8s %9s\n", "the code", "runs", "typed", "nucleus", "cross.fs");
    strcpy(FILE_OF[files].name, "the rest");
    for(int idx=0; idx <= files; idx++) {
        const source* f = &FILE_OF[idx];
        if (f->runs) printf("%-16s %6ld %8ld %8ld %9ld\n", f->name, f->runs, f->typed, f->nucleus, f->cross);
    }
    printf("%-16s %6ld %8ld %8ld %9ld\n\n", "all of it", runs, typed_all, nucleus_all, cross_all);

    printf("%-30s", "when it is run");
    for(int p=0; p < pieces; p++) printf(" %12s", argv[5 + p]);
    printf("\n%-30s", "instructions the work runs");
    for(int p=0; p < pieces; p++) printf(" %12lld", (long long)ALL[p]);
    printf("\n%-30s", "more, were it the nucleus's");
    for(int p=0; p < pieces; p++) printf(" %12lld", (long long)more[p]);
    printf("\n%-30s", "  to each hundred");
    for(int p=0; p < pieces; p++) printf(" %12.2f", 100.0 * more[p] / ALL[p]);
    printf("\n%-30s", "saved by the nucleus's rules");
    for(int p=0; p < pieces; p++) printf(" %12lld", (long long)saved[p]);
    printf("\n%-30s", "  to each hundred");
    for(int p=0; p < pieces; p++) printf(" %12.2f", 100.0 * saved[p] / ALL[p]);
    printf("\n\n");

    int used = 0, pay = 0, hot = 0, either = 0;
    for(int r=0; r < rules; r++) for(int p=0; p < pieces; p++) if (RULE[r].saves[p] > RULE[r].most) RULE[r].most = RULE[r].saves[p];
    qsort(RULE, rules, sizeof(rule), by_most);
    printf("%-30s %5s", "the rules", "uses");
    for(int p=0; p < pieces; p++) printf(" %12s", argv[5 + p]);
    printf("\n");
    for(int r=0; r < rules; r++) {
        const rule* x = &RULE[r];
        if (!x->uses) continue;
        int size = x->uses * (x->of_three ? 4 : 2) >= RULE_BYTES, run = 0;
        for(int p=0; p < pieces; p++) run |= x->saves[p] >= SHARE * ALL[p];
        used++; pay += size; hot += run; either += size || run;
        printf("%-30.30s %5ld", x->said, x->uses);
        for(int p=0; p < pieces; p++) printf(" %12lld", (long long)x->saves[p]);
        printf("  %s%s\n", size ? " size" : "", run ? " runs" : "");
    }
    printf("\n%d rules: %d are used in the code; %d save at least their own %d bytes there (size), %d save at least %g%% of what some piece of work runs (runs), %d do one or the other\n",
           rules, used, pay, RULE_BYTES, hot, 100 * SHARE, either);
    return(0);
}
