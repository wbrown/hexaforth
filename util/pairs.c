//
// pairs type IMAGE.hex IMAGE.tables            prints the input to type at the nucleus
// pairs read IMAGE.hex OUTPUT                  checks what the nucleus answered
// pairs seen IMAGE.hex TYPED                   checks what it joined in the system's code
//
// Every mode works on the machine the image was built for: the step runs
// with that machine's cell size.
//
// The nucleus computes the single instruction equivalent to two
// instructions, or to three (forth/compose.fs).  This program gives it
// every pair of instructions it can be given, and every run of three
// named instructions, and checks each answer against the machine's step.
//
// The instructions the nucleus can be given are the named ones
// (vm_opcodes.h), because those are what a person types, plus the result
// of joining any two instructions it can be given: after a join, the
// nucleus tries to join the result with the instruction before it.  The
// expected answer for a run comes from the step (vm_step.h), which fuse.h
// runs on terms.  It is the instruction cross.fs compiles for that run.
//
// `type` prints lines to type at the image's prompt.  The lines put the
// instructions in a table, which is allocated in the memory above the
// image because the nucleus's data area has no room for it.  They define a
// word that calls compose.fs's jtwo on every pair in the table, and its
// jthree on every run of three named instructions, and prints each result
// that is one instruction: the table indexes of the two or three
// instructions, and then the result.  jtwo and jthree have no dictionary
// header, so their addresses come from the image's labels.
//
// `read` reports, for the pairs and then for the runs of three:
//
//   how many the step shows are equivalent to one instruction
//   of those, how many the nucleus answered with exactly the instruction
//     cross.fs compiles, how many with a different equivalent instruction,
//     and how many it did not join
//   how many answers are not equivalent to the run.  Each of these is
//     printed, and is a bug.
//
// It fails if any answer was wrong, or if nothing was compared.
//
// `seen` checks against the step what the nucleus joined when the
// system's own source was typed at it.  TYPED is the file `typed read`
// prints, which the target `peephole` keeps.  It checks every place where
// the nucleus replaced instructions with other instructions: ALU
// instructions, literals and the instructions that fetch or store a
// variable, however many there are on each side.  For the joins of ALU
// instructions that leave one instruction or none, it reports how often
// the result was exactly the instruction cross.fs compiles, and how often
// a different equivalent one, and lists each of the different ones beside
// what cross.fs compiles.
//
// Some of the nucleus's rules hold only after a particular instruction:
// `false =` is `invert` after an instruction that leaves a flag.  TYPED
// records the instruction before each join.  A join that is not equivalent
// on its own is checked again with that instruction in front of both
// sides, and counted separately.
//
// A join the step cannot follow is counted and not checked.
//
// It fails if any result is not equivalent to the run, or if there were no
// joins to check.
//

#include "fuse.h"
#include "image.h"
#include "words.h"

static int MEETS[EVERY_WORD];           // 1 for each instruction the nucleus can be given
static uint16_t MET[EVERY_WORD];        // those instructions, in the order they were found
static int met;                         // how many there are
static int named_ones;                  // the first named_ones entries of MET are the named instructions

static void meet(uint16_t x) {
    if (MEETS[x]) return;
    MEETS[x] = 1;
    MET[met++] = x;
}

// Returns the single instruction cross.fs compiles for a run, or 0 if the
// run is not equivalent to one instruction.
static uint16_t one_of(const uint16_t* run, int len) {
    named a, b;
    forget();
    uint64_t left = names(run, len, &a);
    if (!left) return(0);
    int found = find(left);
    if (found < 0) return(0);
    uint16_t one = ALU[found].stands;
    names(&one, 1, &b);
    return(same_named(&a, &b) && returns(one) == returns(run[len - 1]) ? one : 0);
}

static uint16_t two(uint16_t a, uint16_t b) {
    uint16_t run[2] = { a, b };
    return(one_of(run, 2));
}

// Returns 1 if two runs are equivalent on the step.  The final states
// include where each run returns to, so a run that returns is equivalent
// only to another that returns.
static int same_runs(const uint16_t* a, int alen, const uint16_t* b, int blen) {
    named x, y;
    forget();
    uint64_t left = names(a, alen, &x);
    return(left && names(b, blen, &y) == left && same_named(&x, &y));
}

// Returns 1 if the step can follow a run to its end.  It cannot follow an
// instruction it does not model, or a run that goes on after a return.
static int followed(const uint16_t* run, int len) {
    named m;
    forget();
    return(names(run, len, &m) != 0);
}

// Returns 1 if an instruction is equivalent to a run.
static int does(uint16_t y, const uint16_t* run, int len) {
    if (alu_at[y] < 0 || returns(y) != returns(run[len - 1])) return(0);
    named a, b;
    forget();
    uint64_t left = names(run, len, &a);
    return(left && names(&y, 1, &b) == left && same_named(&a, &b));
}

// Fills MET with the named instructions, and then with the result of
// joining any two instructions already in it, until no join gives a new
// one.  Of the instructions that return, only `exit` is included, because
// it is the only one a person types.  The nucleus tries to join `exit`
// with the instruction before it, and then to join that result with the
// instruction before that.
static void meet_all(void) {
    uint16_t the_exit = ins_named("exit");
    for(word_node* w = FORTH_WORDS; w->repr && strlen(w->repr); w++) {
        if (w->num_ins != 1) continue;
        uint16_t ins = *(uint16_t*)&w->ins[0];
        if (alu_at[ins] >= 0 && (!returns(ins) || ins == the_exit)) meet(ins);
    }
    named_ones = met;
    for(int done=0; done < met; done++) {
        for(int other=0; other <= done; other++) {
            uint16_t y = two(MET[done], MET[other]);
            if (y) meet(y);
            if (other != done && (y = two(MET[other], MET[done]))) meet(y);
        }
    }
}

// Counts of the nucleus's answers for a set of runs:
//
//   are      runs the step shows are equivalent to one instruction
//   very     answers that are exactly the instruction cross.fs compiles
//   other    answers that are a different equivalent instruction
//   missed   runs equivalent to one instruction that the nucleus did not
//            join
//   wrong    answers that are not equivalent to the run
//   said     runs the nucleus gave any answer for
typedef struct { long are, very, other, missed, wrong, said; } tally;

// Checks the nucleus's answer for one run against the step, and counts it
// in the tally.  `said` is the answer, or 0 if the nucleus gave none.  A
// wrong answer is printed.
static void hold(tally* t, const uint16_t* run, int len, uint16_t said) {
    uint16_t is = one_of(run, len);
    t->are += is != 0;
    if (!said) { t->missed += is != 0; return; }
    t->said++;
    if (said == is) { t->very++; return; }
    if (does(said, run, len)) { t->other++; return; }
    t->wrong++;
    printf("WRONG: the nucleus makes %04x of", said);
    for(int idx=0; idx < len; idx++) printf(" %04x", run[idx]);
    if (is) printf(", which are %04x", is);
    printf("\n");
}

static void say(const char* what, const tally* t) {
    printf("%s that are one instruction: %ld\n", what, t->are);
    printf("  the nucleus gave the very instruction for %ld, another that does the same for %ld, and none for %ld\n",
           t->very, t->other, t->missed);
    printf("  it was wrong of %ld\n", t->wrong);
}

// Prints an instruction's name, or its number in hex if it has no name.
static void say_ins(uint16_t ins) {
    const char* name = word_of(ins);
    if (name) printf(" %s", name);
    else printf(" $%04x", ins);
}

// Checks against the step everything the nucleus joined in the system's
// code, as recorded in the file `typed`.  Returns the program's exit
// status.
static int seen(const char* typed) {
    FILE* in = fopen(typed, "r");
    if (!in) { perror(typed); return(1); }
    uint16_t nothing = ins_named("noop");
    number_set had = {0};
    long times = 0, very = 0, other = 0, wrong = 0, different = 0, others = 0;
    long after_one = 0;                 // joins that are equivalent only after the instruction before them
    long to_more = 0;                   // joins of ALU instructions that leave two instructions or more
    long numbered = 0;                  // joins that contain a literal or a variable's instruction
    long not_followed = 0;              // joins the step cannot follow
    char* line = NULL;
    long room = 0;
    while (line_of(in, &line, &room)) {
        const char* bar = line + strcspn(line, " ");        // start after the word's name, which may itself contain a bar
        for(int n=0; n < 4 && bar; n++) bar = strchr(bar + 1, '|');
        for(const char* rest = bar ? bar + 1 : NULL; rest; ) {
            rest += strspn(rest, " \n");
            size_t len = strcspn(rest, " \n");
            if (!len) break;
            // run[0] is the instruction before the join, if there is one.
            // The instructions that went in follow it from run[1], and
            // `made` has the same first instruction and then the ones that
            // came out.
            uint16_t run[RUN], made[RUN];
            int n = 0, out = 0, before = 0, after = 0, alu_only = 1;
            for(const char* at = rest; at < rest + len; ) {
                if (*at == '>') { after = 1; at++; continue; }
                if (*at == ',') { at++; continue; }
                char* end;
                uint16_t ins = (uint16_t)strtol(at, &end, 16);
                int* count = after ? &out : &n;
                if (end == at || 1 + *count == RUN) {
                    fprintf(stderr, "pairs: %s has a join that is not instructions in hex, or is longer than a run: %.*s\n",
                            typed, (int)len, rest);
                    fclose(in);
                    return(1);
                }
                at = end;
                if (*end == ':') {
                    before = 1;
                    run[0] = made[0] = ins;
                    at++;
                    continue;
                }
                alu_only &= alu_at[ins] >= 0;
                (after ? made : run)[1 + (*count)++] = ins;
            }
            rest += len;
            if (!out) made[1 + out++] = nothing;
            if (!n || !followed(run + 1, n) || !followed(made + 1, out)) { not_followed++; continue; }
            int alone = same_runs(run + 1, n, made + 1, out);
            if (!alone && before && !followed(run, n + 1)) { not_followed++; continue; }
            times++;
            if (!alone && !(before && same_runs(run, n + 1, made, out + 1))) {
                wrong++;
                printf("WRONG: the nucleus replaced");
                for(int idx=0; idx < n; idx++) printf(" %04x", run[1 + idx]);
                printf(" with");
                for(int idx=0; idx < out; idx++) printf(" %04x", made[1 + idx]);
                if (before) printf(", after %04x", run[0]);
                printf(", and the two are not equivalent\n");
                continue;
            }
            after_one += !alone;
            if (!alu_only) { numbered++; continue; }
            if (out > 1) { to_more++; continue; }
            if (!alone) continue;
            uint16_t said = made[1], is = one_of(run + 1, n);
            uint64_t key = mix(n, said);
            for(int idx=0; idx < n; idx++) key = mix(key, run[1 + idx]);
            int first = !already_in(&had, key);
            different += first;
            if (said == is) { very++; continue; }
            other++;
            if (!first) continue;
            others++;
            printf("differs from cross.fs:");
            for(int idx=0; idx < n; idx++) say_ins(run[1 + idx]);
            printf(" becomes");
            say_ins(said);
            printf(" in the nucleus, and");
            if (is) say_ins(is); else printf(" stays as written");
            printf(" in cross.fs\n");
        }
    }
    fclose(in);
    printf("joins the nucleus made in the system's code, checked against the step: %ld, of which %ld are wrong\n", times, wrong);
    printf("  %ld are of ALU instructions, leave one instruction or none, and are equivalent on their own (%ld distinct joins)\n",
           very + other, different);
    printf("    %ld give the instruction cross.fs compiles, and %ld give a different equivalent one (%ld distinct)\n",
           very, other, others);
    printf("  %ld are of ALU instructions and leave two instructions or more\n", to_more);
    printf("  %ld contain a number or a variable's instruction\n", numbered);
    printf("  %ld are equivalent only after the instruction before them\n", after_one);
    printf("  %ld more joins are not checked, because the step cannot follow them\n", not_followed);
    if (!times) {
        fprintf(stderr, "pairs: %s records no joins that can be checked\n", typed);
        return(1);
    }
    return(wrong ? 1 : 0);
}

int main(int argc, char *argv[]) {
    int type = argc == 4 && !strcmp(argv[1], "type");
    int read = argc == 4 && !strcmp(argv[1], "read");
    if (!type && !read && !(argc == 4 && !strcmp(argv[1], "seen"))) {
        fprintf(stderr, "usage: pairs type IMAGE.hex IMAGE.tables\n       pairs read IMAGE.hex OUTPUT\n"
                        "       pairs seen IMAGE.hex TYPED\n");
        return(1);
    }
    machine_from(argv[2]);                      // take the cell size from the image before the step is used
    get_ready();
    if (!type && !read) return(seen(argv[3]));
    meet_all();

    if (type) {
        uint8_t* image;
        size_t len = load_image(argv[2], &image);
        word* words;
        int n = words_of(image, len, argv[3], &words);
        long jtwo = place_of(words, n, "jtwo"), jthree = place_of(words, n, "jthree");
        if (jtwo < 0 || jthree < 0) {
            fprintf(stderr, "pairs: jtwo and jthree are not among the words of %s\n", argv[2]);
            return(1);
        }
        // The table is larger than the nucleus's data area, so it is
        // allocated in the memory above the image (halloc, machine.fs).
        printf("decimal %d >c halloc constant tb  variable tp  tb tp !\n", met);
        printf(": t, tp @ !  1 tp +! ;\nhex\n");
        for(int at=0; at < met; at++) printf("%x t,%s", MET[at], at % 8 == 7 || at == met - 1 ? "\n" : " ");
        printf(": j2 %lx execute ;\n: j3 %lx execute ;\ndecimal\n", jtwo, jthree);
        printf(": in tb + @ ;\n");
        printf(": row %d 0 do dup in i in j2 ?dup if over . i . . cr then loop drop ;\n", met);
        printf(": twos %d 0 do i row loop ;\n", met);
        printf(": r3 %d 0 do over in over in i in j3 ?dup if >r over . dup . r> i . . cr then loop 2drop ;\n", named_ones);
        printf(": threes %d 0 do %d 0 do j i r3 loop loop ;\n", named_ones, named_ones);
        printf("%d emit twos %d emit cr\n", '[', ']');  // the brackets are emitted by character code, so the echo of this input has none
        printf("%d emit threes %d emit cr\n", '{', '}');
        printf("bye\n");
        return(0);
    }

    // Read the nucleus's output.  Between [ and ] there is one line for
    // each pair it joined, and between { and } one line for each run of
    // three.  A line has the table indexes of the instructions and then
    // the result, all as numbers.  A pair or a run of three with no line
    // was not joined.
    FILE* in = fopen(argv[3], "r");
    if (!in) { perror(argv[3]); return(1); }
    uint16_t* said_two = room_for((size_t)met * met, sizeof(uint16_t));
    uint16_t* said_three = room_for((size_t)named_ones * named_ones * named_ones, sizeof(uint16_t));
    char* line = NULL;
    long room = 0;
    int part = 0, ended = 0;                    // part: 0 before the pairs, 1 in them, 2 between, 3 in the threes, 4 after
    while (line_of(in, &line, &room)) {
        if (strstr(line, "error") || strstr(line, "out of range") || strstr(line, "outside memory")) {
            fprintf(stderr, "pairs: the nucleus said: %s", line);
            return(1);
        }
        const char* at = line;
        if (strchr(line, '[') && part == 0) { part = 1; at = strchr(line, '[') + 1; }
        if (strchr(line, '{') && part == 2) { part = 3; at = strchr(line, '{') + 1; }
        long a, b, c, y;
        if (part == 1 && sscanf(at, "%ld %ld %ld", &a, &b, &y) == 3 && a < met && b < met) said_two[a * met + b] = y;
        if (part == 3 && sscanf(at, "%ld %ld %ld %ld", &a, &b, &c, &y) == 4 && a < named_ones && b < named_ones && c < named_ones) {
            said_three[(a * named_ones + b) * named_ones + c] = y;
        }
        if (strchr(line, ']') && part == 1) part = 2;
        if (strchr(line, '}') && part == 3) { part = 4; ended = 1; }
    }
    fclose(in);
    if (!ended) {
        fprintf(stderr, "pairs: %s is not all of what the nucleus was to say\n", argv[3]);
        return(1);
    }

    tally twos = {0}, threes = {0};
    for(int a=0; a < met; a++) {
        for(int b=0; b < met; b++) {
            uint16_t run[2] = { MET[a], MET[b] };
            hold(&twos, run, 2, said_two[(long)a * met + b]);
        }
    }
    for(int a=0; a < named_ones; a++) {
        for(int b=0; b < named_ones; b++) {
            for(int c=0; c < named_ones; c++) {
                uint16_t run[3] = { MET[a], MET[b], MET[c] };
                hold(&threes, run, 3, said_three[((long)a * named_ones + b) * named_ones + c]);
            }
        }
    }
    printf("instructions that have a name: %d; that the nucleus can meet, with what any two of those are together: %d\n",
           named_ones, met);
    say("pairs of those", &twos);
    say("runs of three that have a name", &threes);

    if (!twos.said || !threes.said) {
        fprintf(stderr, "pairs: the nucleus said nothing is one instruction: nothing was compared\n");
        return(1);
    }
    return(twos.wrong || threes.wrong ? 1 : 0);
}
