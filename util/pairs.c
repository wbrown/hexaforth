//
// pairs type IMAGE.hex IMAGE.tables            what is to be typed at the nucleus
// pairs read OUTPUT                            and what it answered
// pairs seen TYPED                             what it joined of the system's code
//
// The nucleus works out what two instructions are together, and three
// (forth/compose.fs).  This puts to it every pair of instructions it can
// meet, and every run of three that have a name, and holds each answer to
// the machine's step.
//
// The instructions the nucleus can meet are those that have a name
// (vm_opcodes.h), since those are what is typed, and whatever any two of
// what it can meet are together: a joined instruction is offered again to
// the one before it.  What a run is together is asked of the step
// (vm_step.h), which fuse.h has on terms, and is the instruction cross.fs
// makes of it.
//
// `type` writes lines for the prompt of the image: the instructions as a
// table, in the memory above the image, since the nucleus has not the
// room for it; and a word that has compose.fs's jtwo asked of every pair of them
// and its jthree of every three of the named, and prints each that it says
// is one instruction: where the two or three are in the table, and the
// instruction.  jtwo and jthree have no header: where they are is read
// from the image's labels.
//
// `read` says, of the pairs and of the threes: how many are one
// instruction by the step; of those, for how many the nucleus gave the
// very instruction cross.fs makes, for how many another that does the
// same, and how many it did not find; and how many it gave that do not do
// what the run does, each of which is printed, and is a fault.  It fails
// if the nucleus was wrong of any, or if nothing was compared.
//
// `seen` holds to the step what the nucleus joined when the system's own
// code was typed at it (TYPED, which `typed read` prints and the target
// `peephole` keeps): each time it made one instruction, or nothing, of
// two or three that have no number in them.  It says how often that was
// the very instruction cross.fs makes and how often another that does the
// same, and each of those others, with what cross.fs makes; and fails if
// any does not do what the run does, or if there were none.
//

#include "fuse.h"
#include "image.h"
#include "words.h"

static int MEETS[EVERY_WORD];           // the instructions the nucleus can meet,
static uint16_t MET[EVERY_WORD];        // in the order they were found,
static int met;
static int named_ones;                  // of which so many have a name

static void meet(uint16_t x) {
    if (MEETS[x]) return;
    MEETS[x] = 1;
    MET[met++] = x;
}

// The instruction that a run is, as cross.fs makes it, or nothing.
static uint16_t one_of(const uint16_t* run, int len) {
    named a, b;
    forget();
    uint64_t left = names(run, len, &a);
    if (!left) return(0);
    int found = find(left);
    if (found < 0) return(0);
    uint16_t one = ALU[found].stands;
    names(&one, 1, &b);
    return(same_named(&a, &b) && !returns(one) ? one : 0);
}

static uint16_t two(uint16_t a, uint16_t b) {
    uint16_t run[2] = { a, b };
    return(one_of(run, 2));
}

// Does an instruction do what a run does?
static int does(uint16_t y, const uint16_t* run, int len) {
    if (alu_at[y] < 0 || returns(y)) return(0);
    named a, b;
    forget();
    uint64_t left = names(run, len, &a);
    return(left && names(&y, 1, &b) == left && same_named(&a, &b));
}

// Those that have a name, and whatever any two that are met are together,
// until no more are.
static void meet_all(void) {
    for(word_node* w = FORTH_WORDS; w->repr && strlen(w->repr); w++) {
        if (w->num_ins != 1) continue;
        uint16_t ins = *(uint16_t*)&w->ins[0];
        if (alu_at[ins] >= 0 && !returns(ins)) meet(ins);
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

// What the nucleus said of a run: so many are the very instruction, so
// many another that does the same, so many it did not find, and so many
// are wrong.
typedef struct { long are, very, other, missed, wrong, said; } tally;

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

// An instruction by its name, or its number.
static void say_ins(uint16_t ins) {
    const char* name = word_of(ins);
    if (name) printf(" %s", name);
    else printf(" $%04x", ins);
}

// What the nucleus joined of the system's code, held to the step.
static int seen(const char* typed) {
    FILE* in = fopen(typed, "r");
    if (!in) { perror(typed); return(1); }
    uint16_t nothing = ins_named("noop");
    number_set had = {0};
    long times = 0, very = 0, other = 0, wrong = 0, different = 0, others = 0, not_asked = 0;
    char* line = NULL;
    long room = 0;
    while (line_of(in, &line, &room)) {
        const char* bar = line + strcspn(line, " ");        // after the word, whose name may have a bar in it
        for(int n=0; n < 4 && bar; n++) bar = strchr(bar + 1, '|');
        for(const char* rest = bar ? bar + 1 : NULL; rest; ) {
            rest += strspn(rest, " \n");
            size_t len = strcspn(rest, " \n");
            if (!len) break;
            uint16_t run[3], said = nothing;
            int n = 0, out = 0, after = 0, asked = 1;
            for(const char* at = rest; at < rest + len; ) {
                if (*at == '>') { after = 1; at++; continue; }
                if (*at == ',') { at++; continue; }
                char* end;
                uint16_t ins = (uint16_t)strtol(at, &end, 16);
                if (end == at) { asked = 0; break; }
                if (alu_at[ins] < 0 || returns(ins)) asked = 0;         // a number, or what the step does not say of
                if (after) { said = ins; out++; }
                else if (n < 3) run[n++] = ins;
                else asked = 0;
                at = end;
            }
            rest += len;
            if (!asked || n < 2 || out > 1) { not_asked++; continue; }
            uint16_t is = one_of(run, n);
            uint64_t key = (uint64_t)run[0] | (uint64_t)run[1] << 16 | (uint64_t)(n > 2 ? run[2] : 0) << 32 | (uint64_t)said << 48;
            int first = !already_in(&had, key);
            times++;
            different += first;
            if (said == is) { very++; continue; }
            if (!does(said, run, n)) {
                wrong++;
                printf("WRONG: the nucleus makes %04x of", said);
                for(int idx=0; idx < n; idx++) printf(" %04x", run[idx]);
                printf("\n");
                continue;
            }
            other++;
            if (!first) continue;
            others++;
            printf("another:");
            for(int idx=0; idx < n; idx++) say_ins(run[idx]);
            printf(" is");
            say_ins(said);
            printf(" as the nucleus makes it, and");
            if (is) say_ins(is); else printf(" not one instruction");
            printf(" as cross.fs does\n");
        }
    }
    fclose(in);
    printf("what the nucleus joined of the system's code, of instructions with no number in them: %ld times, %ld different\n",
           times, different);
    printf("  the very instruction cross.fs makes %ld times, another that does the same %ld times (%ld different), and it was wrong %ld times\n",
           very, other, others, wrong);
    printf("  (what has a number in it, or is more than three, is not asked here: %ld times)\n", not_asked);
    if (!times) {
        fprintf(stderr, "pairs: %s has nothing the nucleus joined: nothing was compared\n", typed);
        return(1);
    }
    return(wrong ? 1 : 0);
}

int main(int argc, char *argv[]) {
    int type = argc == 4 && !strcmp(argv[1], "type");
    int read = argc == 3 && !strcmp(argv[1], "read");
    if (!type && !read && !(argc == 3 && !strcmp(argv[1], "seen"))) {
        fprintf(stderr, "usage: pairs type IMAGE.hex IMAGE.tables\n       pairs read OUTPUT\n       pairs seen TYPED\n");
        return(1);
    }
    get_ready();
    if (!type && !read) return(seen(argv[2]));
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
        // The table is more than the data has room for, and is in the
        // memory above the image (halloc, machine.fs).
        printf("decimal %d >c halloc constant tb  variable tp  tb tp !\n", met);
        printf(": t, tp @ !  1 tp +! ;\nhex\n");
        for(int at=0; at < met; at++) printf("%x t,%s", MET[at], at % 8 == 7 || at == met - 1 ? "\n" : " ");
        printf(": j2 %lx execute ;\n: j3 %lx execute ;\ndecimal\n", jtwo, jthree);
        printf(": in tb + @ ;\n");
        printf(": row %d 0 do dup in i in j2 ?dup if over . i . . cr then loop drop ;\n", met);
        printf(": twos %d 0 do i row loop ;\n", met);
        printf(": r3 %d 0 do over in over in i in j3 ?dup if >r over . dup . r> i . . cr then loop 2drop ;\n", named_ones);
        printf(": threes %d 0 do %d 0 do j i r3 loop loop ;\n", named_ones, named_ones);
        printf("%d emit twos %d emit cr\n", '[', ']');  // by their numbers: what is typed is said back, and is not to have them
        printf("%d emit threes %d emit cr\n", '{', '}');
        printf("bye\n");
        return(0);
    }

    // What it said: between [ and ] a line a pair, and between { and } a
    // line a three; each where the instructions are in the table, and
    // the instruction, as numbers.  Every other pair and three it said
    // nothing of.
    FILE* in = fopen(argv[2], "r");
    if (!in) { perror(argv[2]); return(1); }
    uint16_t* said_two = room_for((size_t)met * met, sizeof(uint16_t));
    uint16_t* said_three = room_for((size_t)named_ones * named_ones * named_ones, sizeof(uint16_t));
    char* line = NULL;
    long room = 0;
    int part = 0, ended = 0;                    // before the pairs, in them, between, in the threes, after
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
        fprintf(stderr, "pairs: %s is not all of what the nucleus was to say\n", argv[2]);
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
