//
// weigh TYPED IMAGE.sizes COUNTS PIECE...
//
// Reports what the nucleus's compiler is worth: what it makes of the
// system's code compared with what cross.fs makes, and what each of its
// joins saves, both in the size of the code and when the code runs.
//
// TYPED is the output of `typed read`: one line for each run of the
// system's code, with the word and address, the number of instructions
// typed, the number the nucleus made and the number cross.fs made.  After
// the line's fourth bar are the joins the nucleus made on the way: the
// instructions that went in, a `>`, and the instructions that replaced
// them (bench/typed.c).  IMAGE.sizes is the image's .sizes file: the bytes
// of each source file's code and the instruction it starts at (files.h).
// COUNTS is a directory that has PIECE.counts for each PIECE of work, and
// idle.counts.  Each file gives the number of times the instruction at
// each address ran: PIECE.counts with that work done, and idle.counts with
// the machine only started (bench/counting.c).
//
// One instruction fewer in a run is one fewer each time the run runs.  So
// a run's instructions are weighted by the number of times its first
// instruction ran.
//
// It prints:
//   - for each source file, the instructions typed, the number the
//     nucleus makes of them, and the number cross.fs makes
//   - for each piece of work, the instructions it runs, how many more it
//     would run if the nucleus had compiled the code instead of cross.fs,
//     and how many the nucleus's joins save
//   - each join the nucleus made: the number of times in the code, what
//     that saves in each piece of work, and `runs` if it saves at least
//     SHARE of the instructions of some piece.  A join of instructions
//     that have no number in them, into one instruction or into none, has
//     a line of its own, as the words that went in and the word that came
//     out.  Joins with a number in them have as many forms as there are
//     numbers, so they are totalled in two groups: a variable fetched or
//     stored by one instruction; and a number computed, added, or moved
//     across the instruction before it
//
// It also writes COUNTS/words.txt.  That file lists each word that has a
// run the nucleus compiles to a different length than cross.fs does, with
// the number of instructions each compiler makes of the word and how many
// more each piece of work would run, most expensive first.  It compares
// lengths and not the instructions themselves: a run that both compilers
// make the same length is not examined further.
//
// No list here has a fixed size.  The pieces of work, the joins, the
// source files and the addresses are each allocated for the number there
// are (room.h).
//

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../util/room.h"
#include "../util/named.h"
#include "files.h"

#define SHARE 0.0001                    // the share of a piece's work a join must save to be marked `runs`.  It is a choice

// A piece of work.  `ran` is the number of times each address ran in it,
// less the number of times that address runs when the machine is only
// started.
typedef struct {
    const char* name;
    int64_t* ran;
    long places;                        // the number of addresses `ran` covers
    int64_t all, more, saved;           // the total of `ran`; how many more the nucleus's code would run; what the nucleus's joins save
} piece;
static piece* PIECE;
static int pieces;

// Returns the count at an address in a piece of work, or 0 if the address
// is outside the counts.
static int64_t ran_at(const piece* p, long at) { return(at >= 0 && at < p->places ? p->ran[at] : 0); }

// A join the nucleus made: the instructions that went in and those that
// came out, or one of the two groups of joins that have a number in them.
typedef struct {
    char* key;                          // the join as TYPED has it, or the group's name
    char* said;                         // the join in words
    long uses;                          // the number of times it was made in the code
    long fewer;                         // the instructions it saves in the code
    int64_t* saves;                     // the instructions it saves in each piece of work
    int64_t most;                       // the largest of those
} joined;
static joined* JOINED;
static long joins, joins_room;

static const char VARIABLE[] = "a variable fetched or stored in one";
static const char NUMBER[] = "a number folded, added or moved across";

// is_number() returns true for a literal.  is_variable() returns true for
// a literal that fetches or stores a variable.
static int is_number(long ins) { return((ins & 0x8000) != 0); }
static int is_variable(long ins) { return((ins & 0xb000) == 0xb000); }

// Returns a join in words: the instructions that went in, ` =`, and the
// instructions that came out, each by its name or as a number.  The text
// ends with `nothing` if no instruction came out.
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

// Returns the record for the join that a part of a TYPED line describes,
// creating the record if the join is new.  `*fewer` receives the number of
// instructions the join saves.
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

typedef struct { long runs, typed, nucleus, cross; } tally;     // the number of runs, and the instructions typed, made by the nucleus and made by cross.fs

// A word of the code.
typedef struct {
    char* name;
    tally is;                           // the totals over its runs
    long differ;                        // the number of its runs that the two compilers make different lengths
    int64_t* more;                      // how many more instructions the nucleus's code would run in each piece of work
    int64_t most;                       // the largest of those
} worded;
static worded* WORDED;                  // named WORDED because vm_constants.h defines WORD
static long words, words_room;

// Returns the record for a word, creating it if the word is new.
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

// Reads the counts of a piece of work from DIR/NAME.counts.  `*counts`
// receives the counts, which are allocated.  Returns the number of
// addresses they cover.
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
    tally* OF_FILE = room_for(files + 1, sizeof(tally));    // one for each file, and one for code that is in none of them

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
        for(long at=PIECE[p].places; at < idle_places; at++) PIECE[p].all -= (int64_t)idle[at];    // addresses that only the idle counts cover
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
        int f = file_holding(FILES, files, at);             // the file the run is in (files.h)
        if (f < 0) f = files;                               // it is in none of them: that code is in the file that includes the others
        OF_FILE[f].runs++; OF_FILE[f].typed += typed; OF_FILE[f].nucleus += nucleus; OF_FILE[f].cross += cross;
        for(int p=0; p < pieces; p++) PIECE[p].more += (nucleus - cross) * ran_at(&PIECE[p], at);
        worded* w = word_called(line, (size_t)(place - 1 - line));
        w->is.runs++; w->is.typed += typed; w->is.nucleus += nucleus; w->is.cross += cross;
        w->differ += nucleus != cross;
        for(int p=0; p < pieces; p++) w->more[p] += (nucleus - cross) * ran_at(&PIECE[p], at);
        const char* bar = line + used;                      // search after the word, because a word's name can have a bar in it
        for(int n=0; n < 4 && bar; n++) bar = strchr(bar + 1, '|');
        for(const char* rest = bar ? bar + 1 : NULL; rest; ) {
            rest += strspn(rest, " \n");
            size_t len = strcspn(rest, " \n");
            if (!len) break;
            // Skip the instruction before the join, which is not part of
            // it (bench/typed.c): it ends with a colon.
            const char* colon = memchr(rest, ':', len);
            if (colon) { len -= (size_t)(colon + 1 - rest); rest = colon + 1; }
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

    // Write the words that the two compilers make different lengths to a
    // file beside the counts.
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
