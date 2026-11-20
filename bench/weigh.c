//
// weigh TYPED IMAGE.sizes COUNTS PIECE...
//
// What the nucleus's compiler is worth: what it makes of the system's code
// beside what cross.fs makes, and what each thing it joins saves, in the
// code and when the code is run.
//
// TYPED is what `typed read` prints: a line a run of the system's code,
// with the word and place, how many instructions were typed, how many the
// nucleus made and how many cross.fs made, and after its fourth bar what
// the nucleus made of what as it went: the instructions that went in, a >,
// and what is there in their place (bench/typed.c).  IMAGE.sizes is the
// image's .sizes: the bytes of each source file's code and the instruction
// it starts at (files.h).  COUNTS is a directory with, for each PIECE of
// work, PIECE.counts, and idle.counts: how often the instruction at each
// place was run with the work done, and with the machine only started
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
//     how many what the nucleus joins saves
//   each thing the nucleus joined: how often in the code, what that saves
//     in each piece, and `runs` if it saves at least SHARE of the
//     instructions of some piece.  Instructions with no number in them
//     that it made one of, or nothing, are said each by themselves, as the
//     words that went in and the word that came out.  What has a number in
//     it is of as many kinds as there are numbers, and is said together:
//     a variable fetched or stored by one instruction, and a number worked
//     out, added, or taken across what was before it
//
// And it writes COUNTS/words.txt: each word that has a run the nucleus
// makes more or fewer instructions of than cross.fs does, with how many
// each makes of the word and how many more each piece of work would run,
// the dearest first.  It is by how many instructions and not which: a run
// both make as long is not looked at further.
//
// There are as many pieces of work as are named, as many things joined as
// the nucleus joined, as many files as the .sizes has and as many places
// as were run: each has the room it needs (room.h).
//

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../util/room.h"
#include "../util/named.h"
#include "files.h"

#define SHARE 0.0001                    // what a thing joined is to save of a piece's work to be marked `runs`: chosen

// A piece of work: how often each place ran in it, with what runs when the
// machine is only started taken out, and all of that.
typedef struct {
    const char* name;
    int64_t* ran;
    long places;
    int64_t all, more, saved;           // and how many more the nucleus's code would run, and what it joins saves
} piece;
static piece* PIECE;
static int pieces;

static int64_t ran_at(const piece* p, long at) { return(at >= 0 && at < p->places ? p->ran[at] : 0); }

// A thing the nucleus joined: what went in and what came out, or one of
// the two kinds that have a number in them.
typedef struct {
    char* key;                          // as TYPED has it, or the kind
    char* said;                         // in words
    long uses;
    long fewer;                         // the instructions it saves in the code
    int64_t* saves;                     // in each piece,
    int64_t most;                       // and the most of them
} joined;
static joined* JOINED;
static long joins, joins_room;

static const char VARIABLE[] = "a variable fetched or stored in one";
static const char NUMBER[] = "a number worked out, added or taken across";

static int is_number(long ins) { return((ins & 0x8000) != 0); }
static int is_variable(long ins) { return((ins & 0xb000) == 0xb000); }

// What a thing joined is called: the words that went in, and the word
// that came out, each by its name or its number.
static char* said_of(const char* key, size_t len) {
    char* said = NULL;
    int out = 0;
    for(const char* at = key; at < key + len; ) {
        if (*at == '>') { text_more(&said, " ="); out = 1; at++; continue; }
        if (*at == ',') { at++; continue; }
        char* end;
        uint16_t ins = (uint16_t)strtol(at, &end, 16);
        if (end == at) break;
        const char* name = word_of(ins);
        if (name) text_more(&said, "%s%s", said ? " " : "", name);
        else text_more(&said, "%s$%04x", said ? " " : "", ins);
        out += out > 0;
        at = end;
    }
    if (out == 1) text_more(&said, " nothing");
    return(said);
}

// The thing joined that a part of TYPED is, which is made if it is new:
// and how many instructions fewer it leaves.
static joined* joined_as(const char* part, size_t len, int* fewer) {
    int in = 0, out = 0, after = 0, number = 0, variable = 0;
    for(const char* at = part; at < part + len; ) {
        if (*at == '>') { after = 1; at++; continue; }
        if (*at == ',') { at++; continue; }
        char* end;
        long ins = strtol(at, &end, 16);
        if (end == at) break;
        if (after) out++; else in++;
        number |= is_number(ins);
        variable |= after && is_variable(ins);
        at = end;
    }
    *fewer = in - out;
    const char* key = variable ? VARIABLE : number ? NUMBER : part;
    size_t key_len = variable || number ? strlen(key) : len;
    for(long j=0; j < joins; j++) {
        if (strlen(JOINED[j].key) == key_len && !strncmp(JOINED[j].key, key, key_len)) return(&JOINED[j]);
    }
    ONE_MORE(JOINED, joins, joins_room);
    joined* new = &JOINED[joins++];
    new->key = room_for(key_len + 1, 1);
    memcpy(new->key, key, key_len);
    new->said = variable || number ? new->key : said_of(part, len);
    new->saves = room_for(pieces, sizeof(int64_t));
    return(new);
}

typedef struct { long runs, typed, nucleus, cross; } tally;

// A word of the code: its runs, how many of them the two compilers make
// different lengths of, and what the nucleus's would run more in each piece.
typedef struct {
    char* name;
    tally is;
    long differ;
    int64_t* more;
    int64_t most;                       // the most of those
} worded;
static worded* WORDED;                  // (WORD is the machine's: vm_constants.h)
static long words, words_room;

static worded* word_called(const char* name, size_t len) {
    for(long w=0; w < words; w++) {
        if (strlen(WORDED[w].name) == len && !strncmp(WORDED[w].name, name, len)) return(&WORDED[w]);
    }
    ONE_MORE(WORDED, words, words_room);
    worded* new = &WORDED[words++];
    new->name = room_for(len + 1, 1);
    memcpy(new->name, name, len);
    new->more = room_for(pieces, sizeof(int64_t));
    return(new);
}

static int by_more(const void* a, const void* b) {
    const worded* x = a;
    const worded* y = b;
    if (x->most != y->most) return(x->most < y->most ? 1 : -1);
    long dx = x->is.nucleus - x->is.cross, dy = y->is.nucleus - y->is.cross;
    return(dx < dy ? 1 : dx > dy ? -1 : strcmp(x->name, y->name));
}

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
    const joined* x = a;
    const joined* y = b;
    if (x->most != y->most) return(x->most < y->most ? 1 : -1);
    return(x->uses < y->uses ? 1 : x->uses > y->uses ? -1 : strcmp(x->said, y->said));
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "usage: weigh TYPED IMAGE.sizes COUNTS PIECE...\n");
        return(1);
    }
    init_opcodes(FORTH_WORDS);
    char* line = NULL;
    long room = 0;
    pieces = argc - 4;
    PIECE = room_for(pieces, sizeof(piece));

    span* FILES;
    int files = files_of(argv[2], &FILES);
    tally* OF_FILE = room_for(files + 1, sizeof(tally));    // and one for what is in none of them

    uint64_t* idle;
    long idle_places = read_counts(argv[3], "idle", &idle);
    for(int p=0; p < pieces; p++) {
        uint64_t* did;
        PIECE[p].name = argv[4 + p];
        PIECE[p].places = read_counts(argv[3], PIECE[p].name, &did);
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
        worded* w = word_called(line, (size_t)(place - 1 - line));
        w->is.runs++; w->is.typed += typed; w->is.nucleus += nucleus; w->is.cross += cross;
        w->differ += nucleus != cross;
        for(int p=0; p < pieces; p++) w->more[p] += (nucleus - cross) * ran_at(&PIECE[p], at);
        const char* bar = line + used;                      // after the word, whose name may have a bar in it
        for(int n=0; n < 4 && bar; n++) bar = strchr(bar + 1, '|');
        for(const char* rest = bar ? bar + 1 : NULL; rest; ) {
            rest += strspn(rest, " \n");
            size_t len = strcspn(rest, " \n");
            if (!len) break;
            int fewer;
            joined* j = joined_as(rest, len, &fewer);
            j->uses++;
            j->fewer += fewer;
            for(int p=0; p < pieces; p++) {
                int64_t saves = fewer * ran_at(&PIECE[p], at);
                j->saves[p] += saves;
                PIECE[p].saved += saves;
            }
            rest += len;
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
    printf("\n%-30s", "saved by what the nucleus joins");
    for(int p=0; p < pieces; p++) printf(" %12lld", (long long)PIECE[p].saved);
    printf("\n%-30s", "  to each hundred");
    for(int p=0; p < pieces; p++) printf(" %12.2f", 100.0 * PIECE[p].saved / PIECE[p].all);
    printf("\n\n");

    int hot = 0;
    long fewer_all = 0;
    for(long j=0; j < joins; j++) for(int p=0; p < pieces; p++) if (JOINED[j].saves[p] > JOINED[j].most) JOINED[j].most = JOINED[j].saves[p];
    qsort(JOINED, joins, sizeof(joined), by_most);
    printf("%-44s %5s %5s", "what the nucleus joined", "uses", "fewer");
    for(int p=0; p < pieces; p++) printf(" %12s", PIECE[p].name);
    printf("\n");
    for(long j=0; j < joins; j++) {
        const joined* x = &JOINED[j];
        int run = 0;
        for(int p=0; p < pieces; p++) run |= x->saves[p] >= SHARE * PIECE[p].all;
        hot += run;
        fewer_all += x->fewer;
        printf("%-44.44s %5ld %5ld", x->said, x->uses, x->fewer);
        for(int p=0; p < pieces; p++) printf(" %12lld", (long long)x->saves[p]);
        printf("  %s\n", run ? " runs" : "");
    }
    printf("\n%ld things are joined in the code, which is %ld instructions fewer; %d save at least %g%% of what some piece of work runs (runs)\n",
           joins, fewer_all, hot, 100 * SHARE);

    // The words that are made another length, in a file beside the counts.
    static const char listed[] = "/words.txt";
    char* path = room_for(strlen(argv[3]) + sizeof(listed), 1);
    sprintf(path, "%s%s", argv[3], listed);
    FILE* out = fopen(path, "w");
    if (!out) { perror(path); return(1); }
    long other = 0;
    for(long w=0; w < words; w++) {
        for(int p=0; p < pieces; p++) if (WORDED[w].more[p] > WORDED[w].most) WORDED[w].most = WORDED[w].more[p];
    }
    qsort(WORDED, words, sizeof(worded), by_more);
    fprintf(out, "%-24s %5s %6s %6s %8s %9s", "the word", "runs", "differ", "typed", "nucleus", "cross.fs");
    for(int p=0; p < pieces; p++) fprintf(out, " %12s", PIECE[p].name);
    fprintf(out, "\n");
    for(long w=0; w < words; w++) {
        const worded* x = &WORDED[w];
        if (!x->differ) continue;
        other++;
        fprintf(out, "%-24s %5ld %6ld %6ld %8ld %9ld", x->name, x->is.runs, x->differ, x->is.typed, x->is.nucleus, x->is.cross);
        for(int p=0; p < pieces; p++) fprintf(out, " %12lld", (long long)x->more[p]);
        fprintf(out, "\n");
    }
    fprintf(out, "\n%ld of the code's %ld words have a run that the nucleus makes another length than cross.fs does\n", other, words);
    fclose(out);
    printf("\n%ld of the code's %ld words have a run that the nucleus makes another length than cross.fs does: %s\n",
           other, words, path);
    return(0);
}
