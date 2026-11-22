//
// typed type RUNS IMAGE.hex IMAGE.tables       what is to be typed at the nucleus
// typed read RUNS OUTPUT                       and what it answered
// typed word TYPED [--differ] NAME...          of that, the runs of the words named
// typed shown TYPED forth/fused.fs             and whether the decompiler has words for what it made
//
// What the nucleus's own compiler makes of the system's code as it is
// typed.  RUNS is what `runs` prints: a line a straight-line run, the word
// and place, the run as typed, a bar, and what cross.fs made of it.
//
// `type` writes lines for the prompt of the image.  Each run, without the
// exit that ends it, is given an instruction at a time to emit1 (join.fs),
// which is what compiles an instruction of a word that is compiled in
// place; and after each the nucleus prints the instructions the run is so
// far.  When the run is done they are taken off again.  emit1 and sync,
// which says that a branch lands here, have no header: where they are is
// read from the image's labels.  A run that ends in an instruction with
// the return in it is not typed that way, and is left out.
//
// `read` prints a line a run: the word and place, how many instructions
// were typed, how many the nucleus made of them, how many cross.fs made
// (without an exit that is alone), and then, a bar before each, the run as
// typed, cross.fs's, the nucleus's, and what the nucleus made of what as
// it went.  Where giving it an instruction did not leave the run as it
// was with that instruction after it, the nucleus made something: the
// instructions at the end of the run and the one given, a >, and what
// is there in their place, with commas between (`6004,643c>6430`; nothing
// after the >, if they came to nothing).
//
// `word` takes what `read` printed (TYPED, which the target `peephole`
// keeps as build/peephole/typed.txt) and prints the runs of the words
// named: for each, the instructions as typed, as cross.fs made them and as
// the nucleus made them, each by its name if it has one, and whether the
// two made as many.  With --differ before the names, only the runs they
// made different lengths of.  It fails if no run is of a word so named.
//
// `shown` takes the same and the decompiler's table (forth/fused.fs, which
// is made of the instructions the image has, as cross.fs compiled them),
// and says which instructions of the ALU the nucleus made of the system's
// code are not in it: the decompiler has no words for those, and shows
// one by its number.  Each is printed, with how often the nucleus made it
// and a run it made it in.  It fails if the nucleus made none at all.
//

#include "../util/image.h"
#include "../util/words.h"
#include "../util/named.h"

// A run is as long as it is, and so is its line and the word's name: each
// is given room as it is read (room.h).
typedef struct {
    char* word;                         // the word and place, in the room of the line it was read from
    int* typed;                         // the run as typed,
    long n, typed_room;
    int* made;                          // and as cross.fs made it
    long m, made_room;
} run;

static instruction ins_of(uint16_t w) { return(*(instruction*)&w); }

// The next run that is typed: its exit is left off, and cross.fs's if it is alone.
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

// An instruction as it is said: its name, if it has one, or its number.
static void say(uint16_t ins) {
    const char* name = word_of(ins);
    if (name) printf(" %s", name);
    else printf(" $%04x", ins);
}

// What the nucleus printed when it was given the next instruction of a
// run: the instructions the run then is, which are kept here, and how
// many.  The line is what was typed, said back, and then what it printed:
// how many, and each.  Less than nothing, if it gave no such line.
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

// The runs of the words named, from what `read` printed: each run as it was
// typed, as cross.fs made it and as the nucleus made it.
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
        const char* part = strchr(line + all, '|');         // after the word, whose name may have a bar in it
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

// Whether the decompiler has words for what the nucleus made of the
// system's code: its table is a line an instruction, which the line
// starts with.
static int shown(const char* typed, const char* fused) {
    static unsigned char has[1 << 16];
    static long times[1 << 16];
    static char* in_run[1 << 16];               // a run the nucleus made it in
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

    FILE* in = fopen(typed, "r");
    if (!in) { perror(typed); return(1); }
    while (line_of(in, &line, &room)) {
        size_t all = strcspn(line, " ");
        const char* part = line + all;          // after the word, whose name may have a bar in it
        for(int n=0; n < 3 && part; n++) part = strchr(part + 1, '|');
        const char* end = part ? strchr(part + 1, '|') : NULL;
        for(const char* at = part ? part + 1 : NULL; at; ) {
            char* after;
            long ins = strtol(at, &after, 16);
            if (after == at || (end && after > end)) break;
            at = after;
            if ((ins & 0xe000) != 0x6000) continue;     // a number, a jump, a branch or a call
            if (!times[ins]++) text_more(&in_run[ins], "%.*s", (int)all, line);
        }
    }
    fclose(in);
    long made = 0, made_times = 0, without = 0, without_times = 0;
    for(long ins=0; ins < 1 << 16; ins++) {
        if (!times[ins]) continue;
        made++;
        made_times += times[ins];
        if (has[ins]) continue;
        without++;
        without_times += times[ins];
        printf("not in the decompiler's table:");
        say((uint16_t)ins);
        printf(", which the nucleus made %ld times, first in %s\n", times[ins], in_run[ins]);
    }
    printf("the nucleus made %ld different instructions of the ALU of the system's code, %ld times in all\n", made, made_times);
    printf("  the decompiler's table (%s, %ld instructions) has words for all but %ld of them, which are made %ld times\n",
           fused, listed, without, without_times);
    if (!made) { fprintf(stderr, "typed: %s has no instruction the nucleus made\n", typed); return(1); }
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
        // An instruction is given on a line of its own, and what the run
        // is then is printed after it: the line is found by what was typed.
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
    static int* was;                            // the run before an instruction was given, and after
    static int* is;
    static long was_room, is_room;
    char* makings = NULL;
    while (next_run(runs, &r, exit_ins)) {
        long had = 0, made = 0;
        if (makings) makings[0] = '\0';
        for(long idx=0; idx < r.n; idx++) {
            made = run_so_far(out, (uint16_t)r.typed[idx], &is, &is_room);
            if (made < 0) { fprintf(stderr, "typed: the nucleus gave no answer for run %ld\n", count); return(1); }
            long same = 0;                      // how far the run is as it was
            while (same < had && same < made && was[same] == is[same]) same++;
            if (same != had || made != had + 1 || is[had] != r.typed[idx]) {
                text_more(&makings, " ");
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
