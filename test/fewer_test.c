//
// fewer_test
//
// The search for the fewest instructions that do the work of a run, and of
// a turn of a loop (util/fewer.h, util/turns.h).
//
// A check is words of vm_opcodes.h, as they are written, and how many
// instructions are the fewest that do their work: which is said here, of
// runs whose one instruction CLAUDE.md and vm_opcodes.h name, and of runs
// that plainly have none.  The search is to find as many and no fewer,
// and what it finds is to agree with the words when both are run on vm.c.
// Some are wanted, on top, only for whether what they leave is nothing,
// as a branch asks.
//
// A turn is a loop's test and body, as words, looked at every way the
// tools look at one (turns.h, shorten).  The fewest that do it are said
// for what it keeps where it is, for it kept in any cell, and for a number
// it pushes kept on the return stack.
//
// It says how many checks it made, and fails if that is none.
//

#include "../util/fuse.h"
#include "../util/cut.h"
#include "../util/kinds.h"
#include "../util/loops.h"
#include "../util/fewer.h"
#include "../util/turns.h"

typedef struct {
    const char* label;
    const char* words;
    int fewest;                 // the fewest instructions that do it: as many as it is, if no fewer do
    int tops;                   // the top is wanted only for whether it is nothing
} run_check;

static run_check RUNS[] = {
    {"over and + are one instruction", "over +", 1},
    {"swap and drop are one: nip", "swap drop", 1},
    {"dup and @ are one", "dup @", 1},
    {"r@ and @ are one: the top of the return stack is an input", "r@ @", 1},
    {"r@ and + are two: the ALU's other input is the cell under the top", "r@ +", 2},
    {"over r@ + is one: the cell under the top and the top of the return stack", "over r@ +", 1},
    {"drop r@ + is one", "drop r@ +", 1},
    {"2dup and xor are one", "over over xor", 1},
    {"over @ swap ! is one: a cell to a cell", "over @ swap !", 1},
    {"invert twice is one that does nothing", "invert invert", 1},
    {"swap twice is one that does nothing", "swap swap", 1},
    {"two cells pushed are two instructions", "over over", 2},
    {"a cell pushed twice is two", "dup dup", 2},
    {"two cells dropped are one", "drop drop", 1},
    {"three cells dropped are two", "drop drop drop", 2},
    {"= and then not, for whether it is nothing, is xor", "= invert", 1, 1},
    {"xor and then = of nothing, for whether it is nothing, is =", "xor 0=", 1, 1},
    {"a flag is wanted whole: = and invert are two", "= invert", 2},
    {NULL}};

typedef struct {
    const char* label;
    const char* test;           // at the top, or none: the body ends with what the branch asks
    const char* body;
    int here;                   // the fewest, with the branch, for what it keeps where it is,
    int anywhere;               // and kept in any cell: each is not asked, if it is nothing;
    int in_r;                   // and the fewest with a number it pushes kept on the return stack, if that is fewer
} turn_check;

static turn_check TURNS[] = {
    // begin 2dup xor while 1+ repeat is four instructions a turn; begin 1+ 2dup = until is three
    {"a loop up to an end that is under the count", "over over xor", "1+", 3, 3},
    // the end on the return stack: 1+ r@ over = until, four; and with the end under the count, three
    {"a loop up to an end that is on the return stack", "dup r@ xor", "1+", 4, 3},
    // a loop that tests at the bottom already, and has no fewer
    {"a loop that tests at the bottom", NULL, "1+ over over =", 3, 3},
    // a number stored to every cell: with it on the return stack that is the kernel's cells-fill, four a turn
    {"a number stored to every cell up to an end", NULL, "7 over ! 1+ over over =", 0, 0, 4},
    {NULL}};

// A search here is of a few instructions, and is never to be given up: none
// of these looks at as many states as a search may (SEARCH_STATES, which
// CMakeLists.txt says and fewer.h has).

// Words as their instructions: how many.  A number is the instruction
// that pushes it, as the step found them (kinds.h).
static int compiled(const char* words, uint16_t* code) {
    int len = 0;
    char word[64];
    for(int used=0; sscanf(words, "%63s%n", word, &used) == 1; words += used) {
        char* end;
        long number = strtol(word, &end, 10);
        const numbered* pushes = *end ? NULL : numbered_as(PUSHES_IT, number);
        const word_node* is = pushes ? NULL : word_named(word);
        int n = pushes ? 1 : is ? is->num_ins : 0;
        if (!n || len + n > RUN) {
            fprintf(stderr, "fewer_test: %s is not a word of vm_opcodes.h, or the run is too long\n", word);
            exit(EXIT_FAILURE);
        }
        if (pushes) code[len] = pushes->ins; else memcpy(code + len, is->ins, n * sizeof(uint16_t));
        len += n;
    }
    return(len);
}

static wanted OF_RUN;

int main(void) {
    int checks = 0, failed = 0;
    get_ready();
    list_kinds();
    fewer_ready();
    printf("instructions of the ALU that are looked at: %d makings, each by one of them first, and %d that are each alone\n",
           alike, families - alike);

    for(const run_check* c = RUNS; c->label; c++) {
        uint16_t code[RUN], found[RUN];
        int len = compiled(c->words, code), fewest = len, agrees = 1, gave_up = 0;
        named left;
        forget();
        if (!names(code, len, &left) || !want(&OF_RUN, &left, c->tops ? nothing(left.T) : -1)) {
            printf("FAIL %s: the step does not say what `%s` leaves\n", c->label, c->words);
            checks++, failed++;
            continue;
        }
        for(int length=1; length < len && fewest == len; length++) {
            int given_up = 0;
            if (exactly(&OF_RUN, NULL, length, SEARCH_STATES, found, &given_up)) {
                fewest = length;
                agrees = agree_on_machine(code, len, found, length, NULL, NULL, 0, c->tops, 0, 0) > 0;
            }
            gave_up += given_up;
        }
        int right = fewest == c->fewest && agrees && !gave_up;
        checks++;
        if (!right) failed++;
        printf("%s %s: `%s` is %d instructions, and the fewest found is %d%s%s\n", right ? "ok  " : "FAIL", c->label,
               c->words, len, fewest, agrees ? "" : ", which does not agree with it on vm.c",
               gave_up ? ", with a search given up" : "");
    }

    for(const turn_check* c = TURNS; c->label; c++) {
        uint16_t test[RUN], body[RUN];
        turn is = { c->test ? TESTS_AT_TOP : TESTS_AT_BOTTOM, c->test ? test : NULL, c->test ? compiled(c->test, test) : 0,
                    body, compiled(c->body, body) };
        static shortened s;
        checks++;
        if (!shorten(&is, SEARCH_STATES, &s)) {
            printf("FAIL %s: the step cannot follow a turn\n", c->label);
            failed++;
            continue;
        }
        int here = s.turn.now, anywhere, in_r = 0;
        long gave_up = 0;
        for(int how=0; how < COUNT(s.gave_up); how++) gave_up += s.gave_up[how];
        for(int n=0; n < s.n; n++) if (s.found[n].how == KEPT_HERE) here = s.found[n].len + 1;
        anywhere = here;
        for(int n=0; n < s.n; n++) {
            if (s.found[n].how == KEPT_ANYWHERE) anywhere = s.found[n].len + 1;
            if (s.found[n].how == NUMBER_IN_R) in_r = s.found[n].len + 1;
        }
        int right = (!c->here || here == c->here) && (!c->anywhere || anywhere == c->anywhere) && in_r == c->in_r &&
                    !gave_up && s.fewest <= here && s.fewest <= anywhere;
        if (!right) failed++;
        printf("%s %s: %d instructions a turn; the fewest found is %d with what it keeps where it is, %d with it "
               "anywhere", right ? "ok  " : "FAIL", c->label, s.turn.now, here, anywhere);
        if (in_r) printf(", and %d with a number kept on the return stack", in_r);
        printf("%s\n", gave_up ? ", with a search given up" : "");
    }
    printf("the fewest instructions: %d checks, %d failed\n", checks, failed);
    return(failed || !checks);
}
