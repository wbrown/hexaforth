//
// prompt_test IMAGE.hex
//
// The nucleus at its prompt: what its own compiler makes of what is
// typed, variables, loops, tasks and names.  These are things the other
// tests do not reach.
//
// A check is lines typed at the nucleus on vm.c (nucleus.h), and what they
// are to leave on the data stack; a few are of what the machine prints,
// and say what it is to have said.  Each is typed at a machine just
// started, so a check has in it whatever it needs defined.  How many
// instructions a definition is, is asked by where the code pointer was
// before it and after: `cp @ : name ... ; cp @ swap -`, which counts the
// exit.
//
// Under what a check leaves is a number put there first: if the machine
// did not know a word it has emptied its stack, and the number is gone.
// var-base, in a check, is the first cell a variable's instruction
// reaches (vm_constants.h).
//
// It says how many checks it made and how many cells it compared, and
// fails if that is none: then it found nothing to test.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "nucleus.h"

#define UNDER 12345             // what is under what a check leaves

// Every character of a name that is not a letter.
#define EVERY "!\"#$%&'()*+,-./0123456789:;<=>?@[\\]^_`{|}~"

typedef struct {
    const char* label;
    const char* typed;
    int count;                  // how many cells the lines leave,
    int64_t leaves[8];          // and what those are, the top last
    const char* says;           // what the machine is to have said, if the check is of that
    int bits;                   // the cells a machine must have for it, if not any
} check;

static check CHECKS[] = {
    {"a sum", "1 2 +", 1, {3}},
    {"a definition", ": sq dup * ; 12 sq", 1, {144}},
    {"a word that calls itself", ": f dup 1 > if dup 1- recurse * then ; 7 f", 1, {5040}},
    {"a loop that ends in its middle", ": g 0 swap begin dup while tuck + swap 1- repeat drop ; 10 g", 1, {55}},
    {"one of two", ": h 0= if 7 else 9 then ; 0 h 1 h", 2, {7, 9}},
    {"numbers printed", "12 . -172 .", 0, {0}, "12 -172  ok"},
    {"a number printed in another base", "hex ff . decimal", 0, {0}, "decimal FF  ok"},
    // A number wider than 36 bits is made with a shift.
    {"a number wider than 36 bits", ": big 81985529216486895 ; big", 1, {81985529216486895}, NULL, 64},
    {"and one less than nothing",
     ": big 81985529216486895 ; : neg -81985529216486895 ; neg big +", 1, {0}, NULL, 64},

    // A variable is in the cells that one instruction fetches and stores,
    // and the compiler uses that instruction: `5 x ! x @` is a push, a
    // store, a fetch and the exit.
    {"variables stored and fetched",
     "variable x  variable y\n: t 5 x ! x @ 1+ y ! y @ x @ + ; t", 1, {11}},
    {"a variable is where one instruction reaches", "variable x  x var-base - 4096 u<", 1, {-1}},
    {"a store and a fetch of a variable are an instruction each",
     "variable x  cp @ : t2 5 x ! x @ ; cp @ swap -", 1, {4}},
    // Nothing is joined across the start of a loop: here the fetch is done
    // twice, of y and then of x, and only the first follows the address.
    {"a fetch at the start of a loop is not the variable's",
     "variable x  variable y\n: u 5 x ! x y ! y begin @ dup 100 u< until ; u", 1, {5}},

    // The nucleus joins instructions as it compiles them (join.fs): plain
    // words are typed, and each of these is one instruction and the exit.
    // 2dup is compiled in place, so that its two instructions can be joined.
    {"over + is one instruction", "cp @ : j1 over + ; cp @ swap -  3 4 j1", 3, {2, 3, 7}},
    {"2dup xor is one instruction", "cp @ : j2 2dup xor ; cp @ swap -  5 5 j2", 4, {2, 5, 5, 0}},
    {"and is the xor of the two", ": j2 2dup xor ; 6 7 j2", 3, {6, 7, 1}},
    {"dup @ is one instruction", "variable x  9 x !  cp @ : j3 dup @ ; cp @ swap -  x j3 nip", 2, {2, 9}},
    {"a number and + is one instruction", "cp @ : j4 2 + ; cp @ swap -  40 j4", 2, {2, 42}},
    // Nothing is joined across the start of a loop: the + after begin does
    // not take the over before it.  (It is counted, not run.)
    {"the + after begin does not take the over before it",
     "cp @ : j5 over begin + dup until ; cp @ swap -", 1, {5}},
    // An instruction shown as four words is reached a step at a time: r> @
    // is a step, and with over ! it is the instruction that move-row has.
    // So this is >r, that one, and the exit.
    {">r r> @ over ! is two instructions",
     "cp @ : j6 >r r> @ over ! ; cp @ swap -", 1, {3}, NULL, 64},
    {"and stores what it fetched",
     "variable x  create w 0 ,  : j6 >r r> @ over ! ;\n7 x !  w x j6 drop  w @", 1, {7}, NULL, 64},

    // Bits of a cell set, cleared and turned over, typed and compiled.
    {"or! typed", "create w 5 ,  10 w or!  w @", 1, {15}},
    {"or! compiled", "create w 15 ,  : o 48 w or! w @ ; o", 1, {63}},
    {"and! xor! *! not!",
     "create w 63 ,  60 w and!  w @  5 w xor!  w @  3 w *!  w @  w not!  w @", 4, {60, 57, 171, -172}},

    // A word that has a great deal in hand keeps it in variables, and sets
    // them aside if it is run while it is running.  Here each is told it is
    // running, with its variables all 77: it must draw, and leave them so.
    // What it drew is looked for in the rows it drew in: the first cell of
    // the rows of the character, and a cell of the rows the copy reaches.
    {"text drawn while text is being drawn, and a copy while one is being made",
     ": drawn? ( -- f ) 0  26 10 do i row @ or loop  0<> ;\n"
     ": copied? ( -- f ) 0  364 300 do i row 4 + @ or loop  0<> ;\n"
     "display-init  77 text-state text-end over - cells-fill\n"
     "true text-busy !  65 10 10 draw-char  0 text-busy !\n"
     "text-state @  text-end 1- @  drawn?\n"
     "77 copy-state copy-end over - cells-fill\n"
     "true copy-busy !  screen 0 0 300 300 64 64 copy-bits  0 copy-busy !\n"
     "copy-state @  copy-end 1- @  copied?", 6, {77, 77, -1, 77, 77, -1}, NULL, 64},

    // Tasks: one that counts, and a loop that keeps its index across pauses.
    {"a task that counts, and a loop that pauses",
     "create rec 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,\n"
     "create n 0 ,\n"
     ":noname 1 n +! ; 128 128 rec spawn pause pause pause n @\n"
     ": lp 3 0 do i pause loop ; lp n @", 5, {3, 0, 1, 2, 6}},

    // A name is packed five bits a character.  A capital is its letter; a
    // name that is the start of another is not that other, whichever cell
    // of it they part in; every character a name can have is one, and comes
    // back as it was when the name is read out of the header (name>).
    {"a capital is its letter", ": a-b!? 7 ; a-b!? A-B!?", 2, {7, 7}},
    {"a name that is the start of another", ": abc 1 ; : abcd 2 ; : ab 3 ; abc abcd ab", 3, {1, 2, 3}},
    {"names that part in each cell",
     ": n12 1 ; : n123 2 ; : n1234 3 ; : n12345 4 ; : n123456 5 ; n12 n123 n1234 n12345 n123456",
     5, {1, 2, 3, 4, 5}},
    {"every character a name can have", ": " EVERY " 5 ; " EVERY, 1, {5}},
    {"and they come back as they were",
     ": " EVERY " 5 ; forth @ 2 + name> type", 0, {0}, "type " EVERY " ok"},
    {"a name of every letter",
     ": abcdefghijklmnopqrstuvwxyz 6 ; forth @ 2 + name> type", 0, {0}, "type abcdefghijklmnopqrstuvwxyz ok"},
    {""}};

static int checks, compared, failed;

// A number as a cell of this machine holds it, which is how left() gives one.
static uint64_t as_cell(int64_t x) {
    return(CELL_BITS == 64 ? (uint64_t)x : (uint64_t)x & ((1ull << CELL_BITS) - 1));
}

static void run(const check* test) {
    start();
    line("%d constant var-base", (int)VAR_BASE);
    line("%d", UNDER);
    line("%s", test->typed);
    type();

    int differ = 0;
    if (machine.SP != test->count + 1 || left(test->count) != UNDER) {
        differ = test->count ? test->count : 1;
    } else {
        for(int idx=0; idx < test->count; idx++) {
            if (left(test->count - 1 - idx) != as_cell(test->leaves[idx])) differ++;
        }
    }
    int unsaid = test->says && !strstr(said, test->says);
    checks++;
    compared += test->count + (test->says != NULL);
    if (differ || unsaid) failed++;
    printf("%s %s: %d cells left, %d not as they should be%s\n", differ || unsaid ? "FAIL" : "ok  ",
           test->label, test->count, differ, unsaid ? ", and it did not say what it should" : "");
    if (differ || unsaid) {
        printf("     it left %d cells:", machine.SP);
        for(int idx=machine.SP - 1; idx >= 0 && idx > machine.SP - 10; idx--) printf(" %lld", (long long)left(idx));
        printf("\n     the machine said:\n%s\n", said);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s IMAGE.hex\n", argv[0]);
        return(1);
    }
    alarm(600);                 // a machine that does not stop is stopped
    nucleus_is(argv[1]);
    for(check* test = CHECKS; strlen(test->label); test++) {
        if (test->bits && CELL_BITS < test->bits) continue;
        run(test);
    }
    printf("the prompt, cells of %d bits: %d checks, %d failed; %d things compared\n", CELL_BITS, checks, failed, compared);
    return(failed || !compared);
}
