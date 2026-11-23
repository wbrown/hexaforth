//
// fewer_test
//
// Tests the search for the fewest instructions equivalent to a run, and to
// one turn of a loop (util/fewer.h, util/turns.h).
//
// A check is some words of vm_opcodes.h, written as source, and the
// smallest number of instructions equivalent to them.  That number is
// given here.  The checks are runs whose single instruction CLAUDE.md and
// vm_opcodes.h name, and runs that plainly have no shorter form.  The
// search must find exactly that many instructions, and what it finds must
// agree with the words when both are run on vm.c.  In some checks only one
// thing about the top of the stack matters: whether it is zero, which is
// what a branch tests.
//
// A turn is a loop's test and body, given as words, and examined every way
// the tools examine one (turns.h, shorten()).  The fewest instructions are
// given for three cases: with the loop's values kept where they are, with
// them kept in any cell, and with a number the loop pushes kept on the
// return stack.
//
// It prints how many checks it made, and fails if it made none.
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
    int fewest;                 // the fewest equivalent instructions; the run's own length if nothing shorter exists
    int tops;                   // 1 if all that matters about the top is whether it is zero
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
    // A cell equals itself, and the cell under the top can be both of the
    // ALU's inputs.  So -1 is one instruction, and so is 0.
    {"-1 is one instruction: the cell under the top compared with itself", "0 invert", 1},
    {"one taken away is two: that, and an add", "0 invert +", 2},
    {"two cells dropped and nothing pushed are one", "drop drop 0", 1},
    {"a cell pushed and told apart from itself is one: nothing", "dup dup xor", 1},
    // A cell and-ed or or-ed with itself is that cell, and no cell is less
    // than itself.
    {"a cell and-ed with itself is one that does nothing", "dup and", 1},
    {"and so is one or-ed with itself", "dup or", 1},
    {"a cell is not less than itself: one, which makes the top nothing", "dup <", 1},
    {"nor below itself", "dup u<", 1},
    {NULL}};

typedef struct {
    const char* label;
    const char* test;           // the test at the top of the loop, or NULL if the body ends with what the branch tests
    const char* body;
    int here;                   // the fewest instructions, counting the branch, with the loop's values kept where they are
    int anywhere;               // the fewest with them kept in any cell; `here` and `anywhere` are not checked when 0
    int in_r;                   // the fewest with a number the loop pushes kept on the return stack, if that is fewer
} turn_check;

static turn_check TURNS[] = {
    // `begin 2dup xor while 1+ repeat` is four instructions a turn; `begin 1+ 2dup = until` is three
    {"a loop up to an end that is under the count", "over over xor", "1+", 3, 3},
    // with the end on the return stack, `1+ r@ over = until` is four; with the end under the count it is three
    {"a loop up to an end that is on the return stack", "dup r@ xor", "1+", 4, 3},
    // a loop that already tests at the bottom, which has no shorter form
    {"a loop that tests at the bottom", NULL, "1+ over over =", 3, 3},
    // a number stored to every cell; with the number on the return stack this is the kernel's cells-fill, four a turn
    {"a number stored to every cell up to an end", NULL, "7 over ! 1+ over over =", 0, 0, 4},
    {NULL}};

// Every search here is for a few instructions, and none may be abandoned.
// None of them examines as many states as a search is allowed
// (SEARCH_STATES, which cmake/default-machine.cmake sets and fewer.h
// uses).

// Compiles words to their instructions in `code`, and returns how many
// instructions there are.  A number becomes the instruction that pushes
// it, which the step identified (kinds.h).
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
