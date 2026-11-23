//
// typed type RUNS IMAGE.hex IMAGE.tables       writes the input to type at the nucleus
// typed read RUNS OUTPUT                       reads what the nucleus printed
// typed word TYPED [--differ] NAME...          prints the runs of the named words
// typed shown TYPED forth/fused.fs             compares the decompiler's table with what the nucleus compiled
//
// Reports what the nucleus's own compiler makes of the system's code when
// that code is typed at it.  RUNS is the output of `runs`: one line for
// each straight-line run, with the word and address, the run as typed, a
// bar, and the run as cross.fs compiled it.
//
// `type` writes lines to be typed at the image's prompt.  Each run,
// without the exit that ends it, is passed one instruction at a time to
// emit1 (join.fs), the word that compiles one instruction of a word that
// is being inlined.  After each instruction the nucleus prints the
// instructions the run has become so far.  When the run is finished, its
// instructions are removed again.  emit1 and sync have no header, so their
// addresses are read from the image's labels.  (sync tells the compiler
// that a branch lands at this address.)  A run whose last instruction has
// the return in it cannot be typed this way, and is skipped.
//
// `read` prints one line for each run: the word and address; the number of
// instructions typed; the number the nucleus made of them; the number
// cross.fs made (not counting an exit that is a separate instruction); and
// then, with a bar before each, the run as typed, the run as cross.fs
// compiled it, the run as the nucleus compiled it, and the joins the
// nucleus made on the way.  The nucleus made a join wherever passing it an
// instruction did more than add that instruction to the end of the run.
// A join is printed as the instructions at the end of the run and the one
// passed, then `>`, then the instructions that replaced them, with commas
// between (`6004,643c>6430`).  Nothing follows the `>` if nothing replaced
// them.  If the run has an instruction before the ones that changed, that
// instruction is printed first, followed by a colon
// (`64bc:6064,649c>6480`).  Some joins are equivalent only after that
// instruction, and the tool that checks the joins needs it (util/pairs.c).
//
// `word` reads the output of `read` (TYPED, which the target `peephole`
// keeps as build/peephole/typed.txt) and prints the runs of the named
// words.  For each run it prints the instructions as typed, as cross.fs
// compiled them and as the nucleus compiled them, each by its name if it
// has one, and whether the two compilers made the same number.  With
// --differ before the names, it prints only the runs where the two numbers
// differ.  It fails if no run belongs to a named word.
//
// `shown` reads the same file and a decompiler's table (forth/fused.fs;
// util/fused.c describes how one is made).  It lists the ALU instructions
// the nucleus compiled from the system's code that are missing from the
// table: the decompiler shows those as numbers.  Each is printed with the
// number of times the nucleus compiled it and the first run it was in.  It
// fails if the nucleus compiled no ALU instruction at all.
//

#include "../util/image.h"
#include "../util/words.h"
#include "../util/named.h"
#include "../util/typed.h"

// A run, its line and its word's name can be any length: each array grows
// as it is read (room.h).
typedef struct {
    char* word;                         // the word and address; it points into the line the run was read from
    int* typed;                         // the run as typed
    long n, typed_room;
    int* made;                          // the run as cross.fs compiled it
    long m, made_room;
} run;

static instruction ins_of(uint16_t w) { return(*(instruction*)&w); }

// Reads the next run that can be typed.  The exit that ends the typed run
// is removed, and so is an exit that ends cross.fs's run as a separate
// instruction.  Returns 0 at the end of the file.
static int next_run(FILE* in, run* r, uint16_t exit) {
    static char* line;
    static long room;
    while (line_of(in, &line, &room)) {
        int after = 0;
        r->n = r->m = 0;
        r->word = NULL;
        for(char* tok = strtok(line, " \n"); tok; tok = strtok(NULL, " \n")) {
            if (!r->word) { r->word = tok; continue; }
            if (!strcmp(tok, "|")) { after = 1; continue; }
            if (after) {
                ONE_MORE(r->made, r->m, r->made_room);
                r->made[r->m++] = strtol(tok, NULL, 16);
            } else {
                ONE_MORE(r->typed, r->n, r->typed_room);
                r->typed[r->n++] = strtol(tok, NULL, 16);
            }
        }
        if (r->n && r->typed[r->n - 1] == exit) r->n--;
        if (r->m && r->made[r->m - 1] == exit) r->m--;
        if (!r->n) continue;
        instruction last = ins_of(r->typed[r->n - 1]);
        if (!last.lit.lit_f && last.alu.r_eip) continue;
        return(1);
    }
    return(0);
}

// Prints an instruction by its name if it has one, and as a number if it
// has none.
static void say(uint16_t ins) {
    const char* name = word_of(ins);
    if (name) printf(" %s", name);
    else printf(" $%04x", ins);
}

// Reads what the nucleus printed after it was passed the next instruction
// of a run.  The line starts with the input echoed back, and then has the
// number of instructions the run now has and each of them.  The
// instructions are stored in `*is`, and their number is returned.  Returns
// -1 if the output has no such line.
static long run_so_far(FILE* out, uint16_t given, int** is, long* is_room) {
    static char* line;
    static long room;
    char typed[16];
    snprintf(typed, sizeof(typed), "%04x j, ", given);
    while (line_of(out, &line, &room)) {
        if (strstr(line, "error")) { fprintf(stderr, "typed: the nucleus said: %s", line); exit(1); }
        if (strncmp(line, typed, strlen(typed))) continue;
        char* rest;
        long n = strtol(line + strlen(typed), &rest, 16);
        AS_MANY(*is, n, *is_room);
        for(long idx=0; idx < n; idx++) (*is)[idx] = (int)strtol(rest, &rest, 16);
        return(n);
    }
    return(-1);
}

// Prints the runs of the named words from the output of `read`: each run
// as it was typed, as cross.fs compiled it and as the nucleus compiled it.
static int show_words(const char* typed, int names, char* name[]) {
    static const char* const WHOSE[] = { "typed", "cross.fs", "the nucleus" };
    int only_differ = names && !strcmp(name[0], "--differ");
    if (only_differ) { names--; name++; }
    FILE* in = fopen(typed, "r");
    if (!in) { perror(typed); return(1); }
    char* line = NULL;
    long room = 0, shown = 0;
    while (line_of(in, &line, &room)) {
        size_t all = strcspn(line, " "), len = 0;
        for(size_t idx=0; idx < all; idx++) if (line[idx] == '@') len = idx;
        int wanted = 0;
        for(int k=0; k < names && !wanted; k++) wanted = len && strlen(name[k]) == len && !strncmp(line, name[k], len);
        long n, nucleus, cross;
        if (!wanted || sscanf(line + all, "%ld %ld %ld", &n, &nucleus, &cross) != 3) continue;
        shown++;
        if (only_differ && nucleus == cross) continue;
        printf("%.*s: %ld instructions typed, of which the nucleus makes %ld and cross.fs %ld%s\n",
               (int)all, line, n, nucleus, cross, nucleus == cross ? "" : ": they differ");
        const char* part = strchr(line + all, '|');         // search after the word, because a word's name can have a bar in it
        for(int which=0; which < 3 && part; which++) {
            const char* end = strchr(part + 1, '|');
            printf("  %-12s", WHOSE[which]);
            for(const char* at = part + 1; ; ) {
                char* after;
                long ins = strtol(at, &after, 16);
                if (after == at || (end && after > end)) break;
                say((uint16_t)ins);
                at = after;
            }
            printf("\n");
            part = end;
        }
    }
    fclose(in);
    if (!shown) { fprintf(stderr, "typed: %s has no run of a word so named\n", typed); return(1); }
    return(0);
}

// Lists the instructions the nucleus compiled from the system's code that
// the decompiler's table has no entry for.  The table has one line for
// each instruction, and the line starts with the instruction.
static int shown(const char* typed, const char* fused) {
    static unsigned char has[EVERY_MADE];
    static made_typed the;                      // the instructions the nucleus compiled
    char* line = NULL;
    long room = 0;
    FILE* table = fopen(fused, "r");
    if (!table) { perror(fused); return(1); }
    long listed = 0;
    while (line_of(table, &line, &room)) {
        unsigned ins;
        if (sscanf(line, "$%x w,", &ins) != 1) continue;
        listed += !has[ins & 0xffff];
        has[ins & 0xffff] = 1;
    }
    fclose(table);
    if (!listed) { fprintf(stderr, "typed: %s lists no instructions\n", fused); return(1); }

    long made = made_of_typed(typed, &the);
    if (!made) { fprintf(stderr, "typed: %s has no instruction the nucleus made\n", typed); return(1); }
    long made_times = 0, without = 0, without_times = 0;
    for(long ins=0; ins < EVERY_MADE; ins++) {
        if (!the.times[ins]) continue;
        made_times += the.times[ins];
        if (has[ins]) continue;
        without++;
        without_times += the.times[ins];
        printf("not in the decompiler's table:");
        say((uint16_t)ins);
        printf(", which the nucleus made %ld times, first in %s\n", the.times[ins], the.in_run[ins]);
    }
    printf("the nucleus made %ld different instructions of the ALU of the system's code, %ld times in all\n", made, made_times);
    printf("  the decompiler's table (%s, %ld instructions) has words for all but %ld of them, which are made %ld times\n",
           fused, listed, without, without_times);
    return(0);
}

int main(int argc, char *argv[]) {
    if (argc < 4 || (strcmp(argv[1], "type") && strcmp(argv[1], "read") && strcmp(argv[1], "word") && strcmp(argv[1], "shown"))) {
        fprintf(stderr, "usage: typed type RUNS IMAGE.hex IMAGE.tables\n       typed read RUNS OUTPUT\n"
                        "       typed word TYPED [--differ] NAME...\n       typed shown TYPED forth/fused.fs\n");
        return(1);
    }
    init_opcodes(FORTH_WORDS);
    if (!strcmp(argv[1], "word")) return(show_words(argv[2], argc - 3, argv + 3));
    if (!strcmp(argv[1], "shown")) return(shown(argv[2], argv[3]));
    uint16_t exit_ins = ins_named("exit");
    if (!exit_ins) {
        fprintf(stderr, "typed: exit is not one instruction\n");
        return(1);
    }
    FILE* runs = fopen(argv[2], "r");
    if (!runs) { perror(argv[2]); return(1); }
    static run r;

    if (!strcmp(argv[1], "type")) {
        uint8_t* image;
        size_t len = load_image(argv[3], &image);
        word* words;
        int n = words_of(image, len, argc > 4 ? argv[4] : NULL, &words);
        long emit1 = place_of(words, n, "emit1"), sync = place_of(words, n, "sync");
        if (emit1 < 0 || sync < 0) {
            fprintf(stderr, "typed: emit1 and sync are not among the words of %s\n", argv[3]);
            return(1);
        }
        // Each instruction is typed on a line of its own, and the nucleus
        // prints the run so far after it.  `read` finds that line by the
        // input echoed at its start.
        printf("hex\nvariable c0\n");
        printf(": j, %lx execute  cp @ c0 @ - .  cp @ c0 @ ?do i i@ . loop ;\n", emit1);
        printf(": [[ %lx execute cp @ c0 ! ;\n", sync);
        printf(": ]] c0 @ cp ! ;\n");
        while (next_run(runs, &r, exit_ins)) {
            printf("[[\n");
            for(long idx=0; idx < r.n; idx++) printf("%04x j,\n", r.typed[idx]);
            printf("]]\n");
        }
        printf("bye\n");
        return(0);
    }

    FILE* out = fopen(argv[3], "r");
    if (!out) { perror(argv[3]); return(1); }
    long count = 0;
    static int* was;                            // the run before an instruction was passed
    static int* is;                             // the run after it was passed
    static long was_room, is_room;
    char* makings = NULL;
    while (next_run(runs, &r, exit_ins)) {
        long had = 0, made = 0;
        if (makings) makings[0] = '\0';
        for(long idx=0; idx < r.n; idx++) {
            made = run_so_far(out, (uint16_t)r.typed[idx], &is, &is_room);
            if (made < 0) { fprintf(stderr, "typed: the nucleus gave no answer for run %ld\n", count); return(1); }
            long same = 0;                      // the number of instructions at the start of the run that did not change
            while (same < had && same < made && was[same] == is[same]) same++;
            if (same != had || made != had + 1 || is[had] != r.typed[idx]) {
                text_more(&makings, " ");
                if (same) text_more(&makings, "%04x:", was[same - 1]);
                for(long k=same; k < had; k++) text_more(&makings, "%04x,", was[k]);
                text_more(&makings, "%04x>", r.typed[idx]);
                for(long k=same; k < made; k++) text_more(&makings, "%s%04x", k > same ? "," : "", is[k]);
            }
            AS_MANY(was, made, was_room);
            memcpy(was, is, made * sizeof(int));
            had = made;
        }
        printf("%s %ld %ld %ld |", r.word, r.n, made, r.m);
        for(long idx=0; idx < r.n; idx++) printf(" %04x", r.typed[idx]);
        printf(" |");
        for(long idx=0; idx < r.m; idx++) printf(" %04x", r.made[idx]);
        printf(" |");
        for(long idx=0; idx < made; idx++) printf(" %04x", is[idx]);
        printf(" |%s\n", makings ? makings : "");
        count++;
    }
    if (!count) { fprintf(stderr, "typed: no runs were read\n"); return(1); }
    return(0);
}
