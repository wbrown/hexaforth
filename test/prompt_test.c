//
// prompt_test IMAGE.hex
//
// Tests the nucleus at its prompt: what its own compiler compiles from
// typed input, variables, loops, tasks and names.  The other tests do not
// cover these.
//
// A check is lines typed at the nucleus on vm.c (nucleus.h), and what they
// must leave on the data stack.  A few checks are about what the machine
// prints, and give the text it must have printed.  Each check is typed at
// a freshly started machine, so a check defines everything it needs.
//
// A check measures the length of a definition right after defining it,
// from the address where the definition starts and the code pointer:
// `: name ... ; long name`, which counts the exit.  (The code pointer
// before the definition is not always where the definition starts,
// because a definition can be started again on another page.)  A check's
// definitions must be on one page, where a call from one to another is
// one instruction.  So before each check the code pointer is moved off
// page 1, which may have little space left, to after all the existing
// code (code-on, placement.fs).
//
// Before each check's lines a known number is pushed.  If the machine met
// a word it did not know, it emptied its stack, and that number is gone.
// In a check, var-base is the first cell a variable instruction reaches
// (vm_constants.h).
//
// It prints how many checks it made and how many cells it compared.  It
// fails if it compared none, because then it tested nothing.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "nucleus.h"

#define UNDER 12345             // the number pushed under whatever a check leaves

// Every character a name can have that is not a letter.
#define EVERY "!\"#$%&'()*+,-./0123456789:;<=>?@[\\]^_`{|}~"

typedef struct {
    const char* label;
    const char* typed;
    int count;                  // the number of cells the lines leave
    int64_t leaves[8];          // those cells, the top of the stack last
    const char* says;           // text the machine must have printed, or NULL if the check is not about output
    int bits;                   // the least cell size in bits the check needs, or 0 for any
} check;

static check CHECKS[] = {
    {"a sum", "1 2 +", 1, {3}},
    {"a definition", ": sq dup * ; 12 sq", 1, {144}},
    {"a word that calls itself", ": f dup 1 > if dup 1- recurse * then ; 7 f", 1, {5040}},
    {"a loop that ends in its middle", ": g 0 swap begin dup while tuck + swap 1- repeat drop ; 10 g", 1, {55}},
    {"one of two", ": h 0= if 7 else 9 then ; 0 h 1 h", 2, {7, 9}},
    {"numbers printed", "12 . -172 .", 0, {0}, "12 -172  ok"},
    {"the stack printed, the top last, and kept", "7 8 9 .s", 3, {7, 8, 9}, "7 8 9  ok"},
    {"a number printed in another base", "hex ff . decimal", 0, {0}, "decimal FF  ok"},
    // A number wider than 36 bits is compiled with a shift.
    {"a number wider than 36 bits", ": big 81985529216486895 ; big", 1, {81985529216486895}, NULL, 64},
    {"and a negative one",
     ": big 81985529216486895 ; : neg -81985529216486895 ; neg big +", 1, {0}, NULL, 64},
    // A hex number with the top bit of a cell set, typed and inside a
    // definition.  This one is the top bit of every character in a cell
    // (text.fs).
    {"a number in hex with the top bit of a cell",
     "$8080808080808080  : highs $8080808080808080 ; highs  $2020202020202020", 3,
     {(int64_t)0x8080808080808080ull, (int64_t)0x8080808080808080ull, 0x2020202020202020ll}, NULL, 64},

    // The kernel's double-cell arithmetic takes a step for every bit of a
    // cell (nuc.fs).  These check: two double cells added, with the carry
    // from the low cell; a cell times a cell, giving a double cell; a
    // double cell divided by a cell; and a cell divided by a cell.  -1 has
    // every bit set at any cell size, so these checks overflow a cell on
    // every machine, and divide by a number that has its top bit set.
    {"two cells added, with a carry and without",
     "1 2 3 4 d+  -1 0 1 0 d+  -1 -1 1 0 d+  -1 5 -1 6 d+", 8, {4, 6, 0, 1, 0, 0, -2, 12}},
    {"a cell by a cell", "3 5 um*  100 200 um*  0 -1 um*  -1 1 um*", 8, {15, 0, 20000, 0, 0, 0, -1, 0}},
    {"a cell by a cell, into both cells",
     "-1 -1 um*  -1 2 um*  2 -1 um*  -2 -1 um*", 8, {1, -2, -2, 1, -2, 1, 2, -3}},
    {"two cells by one",
     "7 0 3 um/mod  -1 1 2 um/mod  0 1 -1 um/mod  -1 -2 -1 um/mod", 8, {1, 2, 1, -1, 1, 1, -2, -1}},
    {"a cell by a cell, divided",
     "100 7 u/mod  12345 100 u/mod  -1 -1 u/mod  -2 -1 u/mod", 8, {2, 14, 45, 123, 0, 1, -2, 0}},
    {"and a cell of every bit divided",
     "-1 1 u/mod  -1 10 u/mod drop  -1 2 u/mod 2* 1+  0 5 u/mod", 7, {0, -1, 5, 1, -1, 0, 0}},
    {"the largest and the least numbers printed", "32767 . -32768 . 0 .", 0, {0}, "32767 -32768 0  ok"},
    // A loop that counts by more than one, upward and downward, and one
    // whose count lands exactly on its limit.
    {"+loop up, down, and onto its end",
     ": lu 0 20 0 do i + 3 +loop ; : ld 0 0 20 do i + -3 +loop ; : le 0 10 0 do i + 5 +loop ; lu ld le",
     3, {63, 77, 5}},
    // A word that is only its exit is compiled inline like any small word,
    // and inline it is no instructions at all: `t` here is the literal and
    // the exit.  (It used to be compiled as a call.  `cells` is such a
    // word, and was called everywhere it was used.)
    {"a word that is nothing", ": nil ;  : t nil 5 ; long t  t", 2, {2, 5}},
    // A word whose last word is a call ends in a jump, with no unreachable
    // exit after it.  Where a branch lands after the call, the exit is
    // kept.
    {"a call at the end is a jump, and no exit",
     ": w2 dup dup dup dup ;  : t w2 ; long t  : u if w2 then ; long u", 2, {1, 3}},
    // Whatever can be compiled inline is: a constant becomes its number,
    // and a word of one instruction becomes that instruction.  The lengths
    // below include the exit of the definition that uses it.
    {"a constant is compiled in place", "7 constant seven  : t seven ; long t  t", 2, {2, 7}},
    // An exit after an instruction that can hold the return is folded into
    // that instruction.  A word that is just `dup` is one instruction, and
    // so is a word that is just that word.  A literal cannot hold the
    // return, so its exit follows it.
    {"and a word of one instruction", ": w1 dup ;  : t w1 ; long t  5 t", 3, {1, 5, 5}},
    {"an exit is in the instruction before it, where that can return",
     ": a + ; long a  : b 2 ; long b  : c >r r> ; long c  1 2 a b 7 c",
     6, {1, 2, 1, 3, 2, 7}},
    // A small word is compiled inline only where that is at most as long as
    // a call to it.  A word of two instructions that joins with nothing is
    // called.  One whose last instruction joins with the instruction after
    // it is compiled inline.  A word that only pushes a number is inline
    // everywhere.
    {"a small word is called where in place it is longer",
     "variable v  : f v @ 2 + ;  : g f f ; long g  : h 2dup xor ; long h\n"
     ": k 2dup ; long k  70000 constant big  : m big big ; long m  5 v !  g + 6 6 h nip nip",
     6, {2, 1, 1, 5, 14, 0}},

    // What the compiler's own words compile: loops that are left early,
    // nested loops, defining words, postpone, an exit in the middle of a
    // word, and a word whose last action is to call itself, which must not
    // fill the return stack.
    {"a loop left early, and one unlooped",
     ": tl 0 10 0 do i 4 = if leave then 1+ loop ; : tu 10 0 do i 6 = if i unloop exit then loop 99 ; tl tu", 2, {4, 6}},
    {"a loop in a loop, and one that does not turn",
     ": tn 0 3 0 do 4 0 do j i + + loop loop ; : tq 0 swap 0 ?do i + loop ; tn  0 tq  5 tq", 3, {30, 0, 10}},
    {"a word that makes words", ": mk create , does> @ 1+ ; 41 mk fortytwo  9 mk ten  fortytwo ten", 2, {42, 10}},
    {"and one whose words do no more", ": mk create , does> ; 5 mk five  five @", 1, {5}},
    {"postpone, of a word and of an immediate one",
     ": p1 postpone dup ; immediate  : p2 5 p1 + ;\n"
     ": p3 postpone if 7 postpone literal postpone then ; immediate  : p4 p3 ;  p2  1 p4", 2, {10, 7}},
    {"an exit in the middle, and a word that calls itself last",
     ": e1 dup 3 = if drop 33 exit then dup 4 = if drop 44 exit then drop 0 ;\n"
     ": tr dup if 1- recurse exit then ;  3 e1 4 e1 9 e1  3000 tr", 4, {33, 44, 0, 0}},
    {"the return stack's words", ": rs >r 1 r@ + r> + ; : rr 1 2 >r >r r> r> ; : rd 5 >r 7 rdrop ; 5 rs rr rd", 4, {11, 1, 2, 7}},
    {"a word with no name", ":noname 5 ; execute  1 allot :noname 6 ; execute", 2, {5, 6}},
    {"catch and throw", ": t1 5 throw ; : t2 1 2 3 9 throw ; : t3 ['] t2 catch ;  ' t1 catch  t3  7 ' dup catch",
     5, {5, 9, 7, 7, 0}},
    {"a throw out of a loop leaves the loop it comes back to as it was",
     ": t4 10 0 do i 3 = if 7 throw then loop ; : t5 0 5 0 do ['] t4 catch + i + loop ; t5", 1, {45}},
    {"what is not a picture is refused, and its memory given back (pixels.fs)",
     "variable h0  himem @ h0 !\n"
     "picture[\n"
     "' row| catch ##x#\n"
     "' beside catch\n"
     "picture[\n"
     "' row| catch ###.\n"
     "' ]picture catch  himem @ h0 @ -", 5, {-24, -24, 0, -24, 0}},
    // A word defined between `meta` and `target` is placed in the scratch,
    // which is not kept (placement.fs).  Code that is kept gets such a word
    // inline.  A word that cannot be compiled inline is refused there, and
    // so is a request for its execution token.
    {"a word of the scratch is in place in code that is kept",
     "meta : three 3 ; target  : t three ; long t  t", 2, {2, 3}},
    {"and one that cannot be in place is refused, and so is its xt",
     "meta : down begin dup while 1- repeat ; target\n"
     "' down ' compile, catch nip  ' ['] catch down", 2, {-13, -13}},
    {"finding a word, and whether it is immediate",
     ": imm? parse-name sfind nip ;  imm? dup  imm? if  ' dup 0=  ' if 0=", 4, {-1, 1, 0, 0}},
    {"a string interpreted",
     "create src 51 c, 32 c, 52 c, 32 c, 43 c, align  src >c 5 evaluate  : ev src >c 5 evaluate 2* ; ev", 2, {7, 14}},
    // The string here is `blk @`.  evaluate saves blk, which holds 0 while
    // the string is interpreted, and restores it afterwards.
    {"and blk kept while it is",
     "create sb 98 c, 108 c, 107 c, 32 c, 64 c, align  7 blk !  sb >c 5 evaluate  blk @  0 blk !", 2, {0, 7}},

    // The forms a typed number can have, and backspace over typed input.
    // Backspace removes one character, a tab counts as a space, and a
    // two-byte character is removed whole.
    {"numbers said other ways", "$ff #10 %101 'a' -7 $-ff", 6, {255, 10, 5, 97, -7, -255}},
    {"numbers of two cells", "5. -5. $ff.", 6, {5, 0, -5, -1, 255, 0}},
    {"what is typed taken back", "12\b3  1\t2 +  123\x7f\x7f  9 9\xc3\xa9\b +", 4, {13, 3, 1, 18}},
    {"and nothing taken back when nothing is typed", "\b\b5 6\b\b\b\b\b7", 1, {7}},
    // >number stops at a character that is not a digit, leaving the rest
    // of the text, and at the end of the text, leaving none.
    {">number stops at what is no digit, and at the end",
     "create s 49 c, 50 c, 120 c, align  0 0 s >c 3 >number nip  0 0 s >c 2 >number nip", 6, {12, 0, 1, 12, 0, 0}},
    {"what is a space and what is not",
     "65 isnotspace? 32 isnotspace? 9 isnotspace? 33 isnotspace? 32 isspace? 33 isspace?", 6, {-1, 0, 0, -1, -1, 0}},

    // A name that does not exist is not found by ' or by postpone, which
    // throw -13.  A throw that nothing catches prints its number and the
    // line as far as it was read, empties the stacks and restarts the
    // prompt in interpreting state.  So in these checks the number that
    // marks the stack is typed again after the throw.  abort throws -1,
    // which prints nothing.
    {"a name that is not there", "' ' catch nosuch  ' postpone catch nosuch", 2, {-13, -13}},
    {"a throw that nothing catches says its number", "5 throw\n12345", 0, {0}, "error: 5"},
    {"and the line as far as the word it came from", "1 2 nosuch 3 4\n12345", 0, {0}, "error: -13 1 2 nosuch"},
    {"and -1 says nothing", "-1 throw\n12345", 0, {0}, "throw 12345"},
    {"what is no number is refused", "12x\n12345", 0, {0}, "error: -13"},
    {"and so is a quote that is no character", "'ab\n12345", 0, {0}, "error: -13"},
    {"an error while compiling leaves the prompt interpreting", ": bad nosuch\n12345", 0, {0}, "error: -13"},

    // The kernel's words for single cells, double cells and characters.
    {"smaller, larger, without the sign and halved", "3 5 min 5 3 min -3 5 max -7 abs 7 abs 7 2/ -8 2/ 5 negate",
     8, {3, 3, 5, 7, 7, 3, -4, -5}},
    // `true` is the cell under the top compared with itself, and `false`
    // is that cell xored with itself.  Each is one instruction, and the
    // exit of a word that is just one of them is folded into it.
    {"true and false, each one instruction", ": t true ; long t  : f false ; long f  t f  true invert  false invert",
     6, {1, 1, -1, 0, 0, -1}},
    // Each of the two pushes a constant, so the compiler's number folding
    // treats it as a number.  `1-` is `true` and an add, so `16 1-` folds
    // to 15: one instruction, and then the dup with the exit.  `true` and
    // an add with no number before them stay two instructions, because
    // adding -1 as a literal would be no shorter.
    {"true and false are folded as numbers", ": t 16 1- dup ; long t  t  : u true 5 + ; long u  u",
     5, {2, 15, 15, 2, 4}},
    {"true and an add stay two instructions", ": v true + ; long v  5 v", 2, {2, 4}},
    // A typed -1 compiles to that instruction too, and so does a -1 the
    // nucleus computes by folding.  The numbers near -1 compile as they
    // did before.
    {"a -1 that is typed or computed is one instruction", ": m -1 ; long m  m  : k 0 invert ; long k  k", 4, {1, -1, 1, -1}},
    {"the numbers near -1 compile to the same values",
     ": n -2 ; n  : z 0 ; z  : o 1 ; o  : p -4096 ; p  : q -4097 ; q  : b -32768 ; b  : c 32767 ; c",
     7, {-2, 0, 1, -4096, -4097, -32768, 32767}},
    // A 0 compiles to `false`, which is an ALU instruction, so the nucleus
    // joins it with the instructions around it.  `drop 0` and `2drop 0`
    // are each one instruction, the exit of a word that ends in 0 is
    // folded into the `false`, and `0 >r r>` compiles to `false`.
    {"a 0 joins the instructions around it",
     ": a drop 0 ; long a  : b 2drop 0 ; long b  : c 0 ; long c  : d 0 >r r> ; long d  7 a  8 9 b  c  d",
     8, {1, 1, 1, 1, 0, 0, 0, 0}},
    // Number folding still counts a 0 as a number: `5 5 -` is 0, `3 0 +`
    // is 3 and its exit, and `0 0=` is -1.
    {"a 0 is still folded as a number",
     ": e 5 5 - ; long e  e  : f 3 0 + ; long f  f  : g 0 0= ; long g  g", 6, {1, 0, 2, 3, 1, -1}},
    // `drop false` and `2drop true` each compile to one instruction.
    {"false and true after a drop are one instruction",
     ": a drop false ; long a  : b 2drop true ; long b  1 2 a  3 4 5 b", 6, {1, 1, 1, 0, 3, -1}},
    {"and so is -1 after a drop", ": c drop -1 ; long c  6 7 c", 3, {1, 6, -1}},
    // `0=` after an instruction that leaves a flag is `invert`.  After
    // such an instruction, `dup 0=` is `dup invert`, which is one
    // instruction.
    {"0= after a flag is invert", ": f < 0= ; long f  3 5 f  5 3 f  : g = dup 0= ; long g  4 4 g  4 5 g",
     8, {2, 0, -1, 2, -1, 0, 0, -1}},
    // `@ 0=` is a fetch that xors with the cell under the top, and then
    // `over =`.  `dup @ 0=` keeps the address, so the xor goes to the
    // return stack and the compare takes it from there.
    {"a fetched cell compared with 0 is two instructions",
     "variable v  0 v !  : a @ 0= ; long a  v a  5 v !  v a  : b dup @ 0= 7 ; long b  v b drop nip  0 v !  v b drop nip",
     6, {2, -1, 0, 4, 0, -1}},
    // `over 0<` is `false >r` and a compare that takes the 0 from the
    // return stack.
    {"the cell under the top compared with 0 is two instructions",
     ": w over 0< 7 ; long w  -3 5 w drop nip nip  3 5 w drop nip nip", 3, {4, -1, 0}},
    // A literal add before `+` moves back across the instruction before
    // it, and `over +` is then one instruction.
    {"a literal add before + moves back", ": c over 1+ + ; long c  3 4 c", 3, {2, 3, 8}},
    // `r@ + r> !` is `r>` and one instruction that adds and stores.  In
    // the second word the `2 +` first moves back across the `r@`.
    {"an add to R's address and a store through it are two instructions",
     "variable v  : a >r 1+ r@ + r> ! ; long a  7 v a  v @ v -\n"
     ": b >r 1+ r@ 2 + + r> ! ; long b  7 v b  v @ v -", 4, {4, 8, 5, 10}},
    {"the halves of 32 bits", "305419896 hi16 305419896 lo16", 2, {4660, 22136}, NULL, 32},
    // Sixteen-bit fields compiled into the data, and read by the cell they
    // start in and a byte offset.
    {"fields of sixteen bits, laid and read", "create q 4660 w, 22136 w, 4369 w,  q >c uw@  q 2 ifield  q 4 ifield",
     3, {4660, 22136, 4369}},
    {"three cells turned", "1 2 3 rot  1 2 3 -rot", 6, {2, 3, 1, 3, 1, 2}},
    {"two pairs changed over, and one copied", "1 2 3 4 2swap", 4, {3, 4, 1, 2}},
    {"a pair copied over a pair", "1 2 3 4 2over", 6, {1, 2, 3, 4, 1, 2}},
    {"three pairs turned", "1 2 3 4 5 6 2rot", 6, {3, 4, 5, 6, 1, 2}},
    {"two cells compared", "1 2 3 4 d<  3 4 1 2 d<  1 2 1 2 d=  -1 0 0 1 du<  5 0 d0=  0 0 d0=  5 -1 d0<",
     7, {-1, 0, -1, -1, 0, -1, -1}},
    {"a double cell halved, negated and made positive, and one from a single cell", "-8 -1 d2/  5 0 dnegate  -5 -1 dabs  5 s>d",
     8, {-4, -1, -5, -1, 5, 0, 5, 0}},
    {"characters stored and fetched",
     "create buf 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,  buf >c 16 65 fill  66 buf >c c!\n"
     "buf >c c@  buf >c 5 + c@  buf >c count nip  4660 buf >c 2 + w!  buf >c 2 + uw@", 4, {66, 65, 66, 4660}},
    {"characters moved, and moved over themselves",
     "create buf 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,  66 buf >c c!  67 buf >c 1+ c!  68 buf >c 2 + c!\n"
     "buf >c buf >c 8 + 3 cmove  buf >c 8 + c@  buf >c 10 + c@\n"
     "buf >c buf >c 1+ 3 cmove>  buf >c c@  buf >c 1+ c@  buf >c 2 + c@  buf >c 3 + c@", 6, {66, 68, 66, 66, 67, 68}},

    // A variable is in the cells that one instruction can fetch and store,
    // and the compiler uses that instruction: `5 x ! x @` is a push, a
    // store, a fetch and the exit.
    {"variables stored and fetched",
     "variable x  variable y\n: t 5 x ! x @ 1+ y ! y @ x @ + ; t", 1, {11}},
    {"a variable is where one instruction reaches", "variable x  x var-base - 4096 u<", 1, {-1}},
    {"a store and a fetch of a variable are an instruction each",
     "variable x  : t2 5 x ! x @ ; long t2", 1, {4}},
    // No instructions are joined across the start of a loop.  Here the
    // fetch runs twice, first on y and then on x, and only the first time
    // does it directly follow the address.
    {"a fetch at the start of a loop is not the variable's",
     "variable x  variable y\n: u 5 x ! x y ! y begin @ dup 100 u< until ; u", 1, {5}},

    // The nucleus joins instructions as it compiles them (join.fs).  These
    // checks type ordinary words, and each definition comes out as one
    // instruction with the return folded in.  2dup is compiled inline so
    // that its two instructions can be joined with their neighbours.
    {"over + is one instruction", ": j1 over + ; long j1  3 4 j1", 3, {1, 3, 7}},
    {"2dup xor is one instruction", ": j2 2dup xor ; long j2  5 5 j2", 4, {1, 5, 5, 0}},
    {"and is the xor of the two", ": j2 2dup xor ; 6 7 j2", 3, {6, 7, 1}},
    {"dup @ is one instruction", "variable x  9 x !  : j3 dup @ ; long j3  x j3 nip", 2, {1, 9}},
    {"a number and + is one instruction", ": j4 2 + ; long j4  40 j4", 2, {2, 42}},
    // No instructions are joined across the start of a loop: the + after
    // `begin` is not joined with the `over` before it.  (The check only
    // measures the definition.  It does not run it.)
    {"the + after begin does not take the over before it",
     ": j5 over begin + dup until ; long j5", 1, {5}},
    // An instruction that takes four words to write is built up one join
    // at a time: `r> @` joins first, and with `over !` it becomes the
    // instruction move-row uses.  So this definition is that instruction
    // and the exit.  (The first check only measures it: there is nothing
    // on the return stack to run it with.)  The second check runs it after
    // the start of a loop, which nothing joins across, so that the `>r`
    // before it is kept.
    {"r> @ over ! is one instruction", ": j6 r> @ over ! ; long j6", 1, {2}},
    {"and stores what it fetched",
     "variable x  create w 0 ,  : j6 >r begin r> @ over ! 1 until ;\n7 x !  w x j6 drop  w @", 1, {7}},
    // Two instructions can join into no instruction at all, and then
    // nothing is compiled: `>r r>` is only the exit, as cross.fs compiles
    // it.
    {">r r> is nothing", ": j7 >r r> ; long j7  5 j7", 2, {1, 5}},
    // After an instruction that leaves a flag, `0=` compiles to `invert`:
    // here a comparison and an invert with the return folded in.  After an
    // instruction that leaves no flag, `0=` is called, because inline it
    // would be longer, and at the end of a word that call is a jump.
    {"0= after < is invert", ": j8 < 0= ; long j8  3 5 j8  5 3 j8", 3, {2, 0, -1}},
    {"and after = and u<", ": j8 = 0= ; : j9 u< 0= ;  : ja over = 0= ; long ja  4 4 j8  4 5 j8  1 -1 j9  -1 1 j9  7 7 ja",
     7, {2, 0, -1, 0, -1, 7, 0}},
    {"0= after what leaves no flag is not invert", ": jc + 0= ; long jc  1 2 jc  1 -1 jc", 3, {2, 0, -1}},
    // A dup is moved across a number that takes two instructions, as it is
    // across a number that takes one.  The result is the push, the add,
    // and an `over and` with the return folded in.
    {"a dup is taken across a number of two instructions",
     ": jd dup 5000 and ; long jd  -1 jd  : je dup 5 and ; long je  -1 je", 6, {3, -1, 5000, 2, -1, 5}},

    // Setting, clearing and inverting bits of a cell, typed and compiled.
    {"or! typed", "create w 5 ,  10 w or!  w @", 1, {15}},
    {"or! compiled", "create w 15 ,  : o 48 w or! w @ ; o", 1, {63}},
    {"and! xor! *! not!",
     "create w 63 ,  60 w and!  w @  5 w xor!  w @  3 w *!  w @  w not!  w @", 4, {60, 57, 171, -172}},

    // A word that works with many values keeps them in variables, and
    // saves those variables if it is called while it is already running.
    // Here each such word's busy flag is set, as it is while the word
    // runs, and all its variables are set to 77.  The word must still
    // draw, and must leave the variables at 77.
    // The check looks for what was drawn in the rows drawn to: the first
    // cell of each row of the character, and one cell of each row the copy
    // reaches.
    {"text drawn while text is being drawn, and a copy while one is being made",
     ": drawn? ( -- f ) 0  26 10 do i row @ or loop  0<> ;\n"
     ": copied? ( -- f ) 0  364 300 do i row 4 + @ or loop  0<> ;\n"
     "display-init  77 text-state text-end over - cells-fill\n"
     "true text-busy !  65 10 10 draw-char  0 text-busy !\n"
     "text-state @  text-end 1- @  drawn?\n"
     "77 copy-state copy-end over - cells-fill\n"
     "true copy-busy !  screen 0 0 300 300 64 64 copy-bits  0 copy-busy !\n"
     "copy-state @  copy-end 1- @  copied?", 6, {77, 77, -1, 77, 77, -1}, NULL, 64},

    // A text (text.fs) is lines of characters packed into cells.  Each
    // line starts at the start of a cell and ends with an EOL (10).  The
    // text added here spans cells on every machine: "hello world" is
    // eleven characters.  A code is a character of its line, and removing
    // one leaves the EOL where it was.  A line of one character takes one
    // cell on every machine, so four cells hold four such lines.  A text
    // created to roll drops its first line to make room for a fifth, and a
    // text that does not roll accepts no more.
    {"a line put in a text, and read back",
     "create tt 0 , 0 , 0 , 0 , 0 , 0 ,  create tc 64 >c allot  tc 64 false tt text-new\n"
     "create s 104 c, 101 c, 108 c, 108 c, 111 c, 32 c, 119 c, 111 c, 114 c, 108 c, 100 c, align\n"
     "s 0 5 tt text-add  s 5 6 tt text-add  tt text-nl  s 3 4 tt text-add\n"
     "0 tt text-line 80 char@  0 tt text-line 88 char@  1 tt text-line 24 char@  1 tt text-line 32 char@\n"
     "tt tb.lines @  tt tb.len @", 6, {100, 10, 119, 10, 2, 4}},
    {"codes in a line, and one taken back",
     "create tt 0 , 0 , 0 , 0 , 0 , 0 ,  create tc 64 >c allot  tc 64 false tt text-new\n"
     "create s 104 c, 101 c, align\n"
     "9 tt text-code  s 0 2 tt text-add  33 tt text-code  tt text-back\n"
     "tt tb.len @  0 tt text-line 0 char@  0 tt text-line 16 char@  0 tt text-line 24 char@", 4, {3, 9, 101, 10}},
    {"a text that rolls loses its first line, and one that does not takes no more",
     "create tt 0 , 0 , 0 , 0 , 0 , 0 ,  create tc 4 >c allot\n"
     "create s 104 c, 101 c, 108 c, 108 c, 111 c, align\n"
     ": lines ( -- ) 4 0 do s i 1 tt text-add tt text-nl loop ;\n"
     "tc 4 true tt text-new  lines  s 4 1 tt text-add\n"
     "tt tb.lines @  0 tt text-line 0 char@  3 tt text-line 0 char@\n"
     "tc 4 false tt text-new  lines\n"
     "tt tb.lines @  0 tt text-line 0 char@  3 tt text-line 0 char@", 6, {4, 101, 111, 4, 104, 108}},
    // Ninety-one lines, of every length from 0 to 11 characters, are added
    // to a rolling text of sixteen cells.  The lines that remain must be
    // the last ones, each whole.  Line k is the first k mod 12 characters
    // of "hello world".  So its EOL must be at index k mod 12, and the
    // character before the EOL must be the string's character at that
    // index.
    {"a text that rolls keeps its last lines whole",
     "create tt 0 , 0 , 0 , 0 , 0 , 0 ,  create tc 16 >c allot  tc 16 true tt text-new\n"
     "create s 104 c, 101 c, 108 c, 108 c, 111 c, 32 c, 119 c, 111 c, 114 c, 108 c, 100 c, align\n"
     ": put ( k -- ) s 0 rot 12 u/mod drop tt text-add ;\n"
     ": lines ( -- ) 90 0 do i put tt text-nl loop  90 put ;\n"
     "variable li\n"
     ": m ( -- m ) 91 tt tb.lines @ - li @ + 12 u/mod drop ;\n"
     ": at ( n -- c ) li @ tt text-line swap 3 lshift char@ ;\n"
     ": whole? ( -- f ) m at 10 =  m if m 1- at  s m 1- 3 lshift char@ = and then ;\n"
     ": ok? ( -- f ) true  tt tb.lines @ 0 do i li ! whole? and loop ;\n"
     "lines  tt tb.lines @ 1 >  ok?", 2, {-1, -1}},
    // This fills a text the way the Forth window does: one code at a time,
    // ending a line at fifty characters, into room for sixteen lines.  It
    // adds two hundred lines.  Line k has k mod 50 characters, all the
    // same letter, and the letters go from a to z and around again.  Every
    // line that remains must be whole: its last character is its letter,
    // and its EOL follows.  The cell after the text must be unchanged.
    {"a text filled a code at a time rolls its lines whole",
     "create tt 0 , 0 , 0 , 0 , 0 , 0 ,\n"
     ": room ( -- n ) 50 1 >c + 1 >c u/mod nip 16 * ;\n"
     "create tc room >c allot  create guard 12345 ,  tc room true tt text-new\n"
     ": put ( c -- ) tt text-code  tt tb.len @ 50 = if tt text-nl then ;\n"
     ": n ( k -- n ) 50 u/mod drop ;\n"
     ": line ( k -- ) dup 26 u/mod drop 97 +  swap n 0 ?do dup put loop drop  tt text-nl ;\n"
     ": lines ( -- ) 200 0 do i line loop ;\n"
     "variable li\n"
     ": k ( -- k ) 200 tt tb.lines @ - 1+ li @ + ;\n"
     ": at ( j -- c ) li @ tt text-line swap 3 lshift char@ ;\n"
     ": whole? ( -- f ) k n at 10 =  k n if k n 1- at  k 26 u/mod drop 97 + = and then ;\n"
     ": ok? ( -- f ) true  tt tb.lines @ 1- 0 do i li ! whole? and loop ;\n"
     "lines  tt tb.lines @ 1 >  ok?  guard @", 3, {-1, -1, 12345}},

    // Tasks: one that counts, and a loop that keeps its index across
    // pauses.  The task's record is the eleven cells a task record takes,
    // and the counter is the cell right after it, so a spawn that wrote
    // past its record would change the count.
    {"a task that counts, and a loop that pauses",
     "create rec 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,\n"
     "create n 0 ,\n"
     ":noname 1 n +! ; 128 128 rec spawn pause pause pause n @\n"
     ": lp 3 0 do i pause loop ; lp n @", 5, {3, 0, 1, 2, 6}},

    // A name is packed at five bits a character.  An upper-case letter is
    // the same as its lower-case letter.  A name that is a prefix of
    // another name is a different name, whichever cell the two first
    // differ in.  Every character a name can have is distinct, and comes
    // back unchanged when the name is read out of the header (name>).
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

static void run(const check* test) {
    start();
    line("code-on");            // continue compiling after all the existing code (placement.fs)
    line("%d constant var-base", (int)VAR_BASE);
    line(": long ' cp @ swap - ;");
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
    alarm(600);                 // kill the test if the machine never stops
    nucleus_is(argv[1]);
    for(check* test = CHECKS; strlen(test->label); test++) {
        if (test->bits && CELL_BITS < test->bits) continue;
        run(test);
    }
    printf("the prompt, cells of %d bits: %d checks, %d failed; %d things compared\n", CELL_BITS, checks, failed, compared);
    return(failed || !compared);
}
