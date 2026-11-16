//
// typed type RUNS IMAGE.hex IMAGE.tables       what is to be typed at the nucleus
// typed read RUNS OUTPUT                       and what it answered
//
// What the nucleus's own compiler makes of the system's code as it is
// typed.  RUNS is what `runs` prints: a line a straight-line run, the word
// and place, the run as typed, a bar, and what cross.fs made of it.
//
// `type` writes lines for the prompt of the image.  Each run, without the
// exit that ends it, is given an instruction at a time to emit1 (join.fs),
// which is what compiles an instruction of a word that is compiled in
// place; and after it the nucleus prints how many instructions it made of
// them, what they are, and which of its rules it used and how often (the
// counts it keeps, join.fs), and takes them off again.  emit1 and sync,
// which says that a branch lands here, have no header: where they are is
// read from the image's labels.  A run that ends in an instruction with
// the return in it is not typed that way, and is left out.
//
// `read` prints a line a run: the word and place, how many instructions
// were typed, how many the nucleus made of them, how many cross.fs made
// (without an exit that is alone), and then, a bar before each, the run as
// typed, cross.fs's, the nucleus's, and the rules used as rule:times.
//

#include "../util/image.h"
#include "../util/words.c"

#define MOST 512
#define WORDS 8192

typedef struct {
    char word[200];
    int typed[MOST], n;
    int made[MOST], m;                  // by cross.fs
} run;

static instruction ins_of(uint16_t w) { return(*(instruction*)&w); }
static int is_exit(uint16_t w, const instruction* exit) { return(w == *(const uint16_t*)exit); }

// The next run that is typed: its exit is left off, and cross.fs's if it is alone.
static int next_run(FILE* in, run* r, const instruction* exit) {
    static char line[1 << 16];
    while (fgets(line, sizeof(line), in)) {
        int after = 0;
        r->n = r->m = 0;
        r->word[0] = 0;
        for(char* tok = strtok(line, " \n"); tok; tok = strtok(NULL, " \n")) {
            if (!r->word[0]) { strncpy(r->word, tok, sizeof(r->word) - 1); continue; }
            if (!strcmp(tok, "|")) { after = 1; continue; }
            if (after && r->m < MOST) r->made[r->m++] = strtol(tok, NULL, 16);
            if (!after && r->n < MOST) r->typed[r->n++] = strtol(tok, NULL, 16);
        }
        if (r->n && is_exit(r->typed[r->n - 1], exit)) r->n--;
        if (r->m && is_exit(r->made[r->m - 1], exit)) r->m--;
        if (!r->n) continue;
        instruction last = ins_of(r->typed[r->n - 1]);
        if (!last.lit.lit_f && last.alu.r_eip) continue;
        return(1);
    }
    return(0);
}

int main(int argc, char *argv[]) {
    if (argc < 4 || (strcmp(argv[1], "type") && strcmp(argv[1], "read"))) {
        fprintf(stderr, "usage: typed type RUNS IMAGE.hex IMAGE.tables\n       typed read RUNS OUTPUT\n");
        return(1);
    }
    init_opcodes(FORTH_WORDS);
    instruction exit_ins[8];            // lookup_word clears as many
    if (lookup_word(FORTH_WORDS, "exit", exit_ins) != 1) {
        fprintf(stderr, "typed: exit is not one instruction\n");
        return(1);
    }
    FILE* runs = fopen(argv[2], "r");
    if (!runs) { perror(argv[2]); return(1); }
    static run r;

    if (!strcmp(argv[1], "type")) {
        uint8_t* image;
        size_t len = load_image(argv[3], &image);
        static word words[WORDS];
        int n = words_of(image, len, argc > 4 ? argv[4] : NULL, words, WORDS);
        long emit1 = place_of(words, n, "emit1"), sync = place_of(words, n, "sync");
        if (emit1 < 0 || sync < 0) {
            fprintf(stderr, "typed: emit1 and sync are not among the words of %s\n", argv[3]);
            return(1);
        }
        printf("hex\nvariable c0\n");
        printf(": j, %lx execute ;\n", emit1);
        printf(": [[ %lx execute cp @ c0 ! ;\n", sync);
        printf(": rc join-rules 3 + @ ;\n");                                    // where the counts are
        printf(": r1 dup rc + i@ ?dup if over . . 0 over rc + i! then drop ;\n");  // a rule that was used: say so, and count afresh
        printf(": rr join-rules 1+ @ 0 ?do i r1 loop ;\n");
        printf(": ]] cp @ c0 @ - . cp @ c0 @ ?do i i@ . loop c0 @ cp ! 7c emit rr ;\n");
        printf("rr\n");                                 // what compiling these used is not a run's
        while (next_run(runs, &r, exit_ins)) {
            printf("[[\n");
            for(int idx=0; idx < r.n; idx++) printf("%04x j,%s", r.typed[idx], idx % 8 == 7 || idx == r.n - 1 ? "\n" : " ");
            printf("]]\n");
        }
        printf("decimal 8000 execute\n");
        return(0);
    }

    FILE* out = fopen(argv[3], "r");
    if (!out) { perror(argv[3]); return(1); }
    static char line[1 << 16];
    long count = 0;
    while (next_run(runs, &r, exit_ins)) {
        long made = -1;
        while (made < 0 && fgets(line, sizeof(line), out)) {
            if (strstr(line, "error")) { fprintf(stderr, "typed: the nucleus said: %s", line); return(1); }
            if (strncmp(line, "]] ", 3)) continue;
            char* rest;
            made = strtol(line + 3, &rest, 16);
            printf("%s %d %ld %d |", r.word, r.n, made, r.m);
            for(int idx=0; idx < r.n; idx++) printf(" %04x", r.typed[idx]);
            printf(" |");
            for(int idx=0; idx < r.m; idx++) printf(" %04x", r.made[idx]);
            printf(" |");
            for(long idx=0; idx < made; idx++) printf(" %04lx", strtol(rest, &rest, 16));
            printf(" |");
            rest = strchr(rest, '|');
            for(rest = rest ? rest + 1 : NULL; rest; ) {
                char* end;
                long rule = strtol(rest, &end, 16);
                if (end == rest) break;
                long times = strtol(end, &rest, 16);
                printf(" %ld:%ld", rule, times);
            }
            printf("\n");
        }
        if (made < 0) { fprintf(stderr, "typed: the nucleus gave no answer for run %ld\n", count); return(1); }
        count++;
    }
    if (!count) { fprintf(stderr, "typed: no runs were read\n"); return(1); }
    return(0);
}
