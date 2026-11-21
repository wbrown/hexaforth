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
    // A number in hex that has the top bit of a cell, typed and in a
    // definition: the top bit of each character of a cell (text.fs).
    {"a number in hex with the top bit of a cell",
     "$8080808080808080  : highs $8080808080808080 ; highs  $2020202020202020", 3,
     {(int64_t)0x8080808080808080ull, (int64_t)0x8080808080808080ull, 0x2020202020202020ll}, NULL, 64},

    // The kernel's arithmetic of two cells is a step for every bit of a
    // cell (nuc.fs): two cells added, with the carry from the low one; a
    // cell by a cell into two; two cells divided by one, and one by one.
    // -1 is a cell with every bit set, whatever the cell, so these go over
    // the top of a cell on any machine, and divide by what has its top bit.
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
    // A loop that counts by more than one, up and down, and one that lands
    // on its end.
    {"+loop up, down, and onto its end",
     ": lu 0 20 0 do i + 3 +loop ; : ld 0 0 20 do i + -3 +loop ; : le 0 10 0 do i + 5 +loop ; lu ld le",
     3, {63, 77, 5}},
    // A word that is nothing but its exit is compiled in place as any small
    // word is, and is nothing there: the number and the exit.  (It was a
    // call: `cells`, which is such a word, was called wherever it was said.)
    {"a word that is nothing", ": nil ;  cp @ : t nil 5 ; cp @ swap -  t", 2, {2, 5}},
    // A word that ends in a call ends in a jump, with no exit after it that
    // nothing comes to; where a branch lands after the call, the exit is there.
    {"a call at the end is a jump, and no exit",
     ": w2 dup dup dup dup ;  cp @ : t w2 ; cp @ swap -  cp @ : u if w2 then ; cp @ swap -", 2, {1, 3}},
    // What can be compiled in place is: a constant is its number and a
    // word of one instruction is the instruction, each with the exit.
    {"a constant is compiled in place", "7 constant seven  cp @ : t seven ; cp @ swap -  t", 2, {2, 7}},
    // An exit after an instruction that can return is that instruction,
    // returning: a word that is `dup` is one instruction, and so is a word
    // that is that word.  A number cannot, and has its exit after it.
    {"and a word of one instruction", ": w1 dup ;  cp @ : t w1 ; cp @ swap -  5 t", 3, {1, 5, 5}},
    {"an exit is in the instruction before it, where that can return",
     "cp @ : a + ; cp @ swap -  cp @ : b 2 ; cp @ swap -  cp @ : c >r r> ; cp @ swap -  1 2 a b 7 c",
     6, {1, 2, 1, 3, 2, 7}},
    // A small word is in place only where that is no longer than the call
    // of it: a word of two instructions that nothing is joined to is
    // called, and one that the instruction after it is joined to is in
    // place.  A word that only pushes a number is in place wherever it is
    // said.
    {"a small word is called where in place it is longer",
     "variable v  : f v @ 2 + ;  cp @ : g f f ; cp @ swap -  cp @ : h 2dup xor ; cp @ swap -\n"
     "cp @ : k 2dup ; cp @ swap -  70000 constant big  cp @ : m big big ; cp @ swap -  5 v !  g + 6 6 h nip nip",
     6, {2, 1, 1, 5, 14, 0}},

    // What the compiler's own words make: loops that are left, loops in
    // loops, words that make words, postpone, an exit in the middle, and a
    // word that calls itself last, which must not use the return stack up.
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
    {"finding a word, and whether it is immediate",
     ": imm? parse-name sfind nip ;  imm? dup  imm? if  ' dup 0=  ' if 0=", 4, {-1, 1, 0, 0}},
    {"a string interpreted",
     "create src 51 c, 32 c, 52 c, 32 c, 43 c, align  src >c 5 evaluate  : ev src >c 5 evaluate 2* ; ev", 2, {7, 14}},

    // Numbers as they may be typed, and what is typed taken back: a
    // character, a tab that is a space, and a character of two bytes whole.
    {"numbers said other ways", "$ff #10 %101 'a' -7 $-ff", 6, {255, 10, 5, 97, -7, -255}},
    {"numbers of two cells", "5. -5. $ff.", 6, {5, 0, -5, -1, 255, 0}},
    {"what is typed taken back", "12\b3  1\t2 +  123\x7f\x7f  9 9\xc3\xa9\b +", 4, {13, 3, 1, 18}},

    // The kernel's words for cells, for two cells and for characters.
    {"smaller, larger, without the sign and halved", "3 5 min 5 3 min -3 5 max -7 abs 7 2/ -8 2/ 5 negate",
     7, {3, 3, 5, 7, 3, -4, -5}},
    {"the halves of 32 bits", "305419896 hi16 305419896 lo16", 2, {4660, 22136}, NULL, 32},
    {"three cells turned", "1 2 3 rot  1 2 3 -rot", 6, {2, 3, 1, 3, 1, 2}},
    {"two pairs changed over, and one copied", "1 2 3 4 2swap", 4, {3, 4, 1, 2}},
    {"a pair copied over a pair", "1 2 3 4 2over", 6, {1, 2, 3, 4, 1, 2}},
    {"three pairs turned", "1 2 3 4 5 6 2rot", 6, {3, 4, 5, 6, 1, 2}},
    {"two cells compared", "1 2 3 4 d<  3 4 1 2 d<  1 2 1 2 d=  -1 0 0 1 du<  5 0 d0=  0 0 d0=  5 -1 d0<",
     7, {-1, 0, -1, -1, 0, -1, -1}},
    {"two cells halved, made less than nothing and more", "-8 -1 d2/  5 0 dnegate  -5 -1 dabs  5 s>d",
     8, {-4, -1, -5, -1, 5, 0, 5, 0}},
    {"characters stored and fetched",
     "create buf 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,  buf >c 16 65 fill  66 buf >c c!\n"
     "buf >c c@  buf >c 5 + c@  buf >c count nip  4660 buf >c 2 + w!  buf >c 2 + uw@", 4, {66, 65, 66, 4660}},
    {"characters moved, and moved over themselves",
     "create buf 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,  66 buf >c c!  67 buf >c 1+ c!  68 buf >c 2 + c!\n"
     "buf >c buf >c 8 + 3 cmove  buf >c 8 + c@  buf >c 10 + c@\n"
     "buf >c buf >c 1+ 3 cmove>  buf >c c@  buf >c 1+ c@  buf >c 2 + c@  buf >c 3 + c@", 6, {66, 68, 66, 66, 67, 68}},

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
    // words are typed, and each of these is one instruction, which returns.
    // 2dup is compiled in place, so that its two instructions can be joined.
    {"over + is one instruction", "cp @ : j1 over + ; cp @ swap -  3 4 j1", 3, {1, 3, 7}},
    {"2dup xor is one instruction", "cp @ : j2 2dup xor ; cp @ swap -  5 5 j2", 4, {1, 5, 5, 0}},
    {"and is the xor of the two", ": j2 2dup xor ; 6 7 j2", 3, {6, 7, 1}},
    {"dup @ is one instruction", "variable x  9 x !  cp @ : j3 dup @ ; cp @ swap -  x j3 nip", 2, {1, 9}},
    {"a number and + is one instruction", "cp @ : j4 2 + ; cp @ swap -  40 j4", 2, {2, 42}},
    // Nothing is joined across the start of a loop: the + after begin does
    // not take the over before it.  (It is counted, not run.)
    {"the + after begin does not take the over before it",
     "cp @ : j5 over begin + dup until ; cp @ swap -", 1, {5}},
    // An instruction shown as four words is reached a step at a time: r> @
    // is a step, and with over ! it is the instruction that move-row has.
    // So this is that one and the exit.  (It is counted, not run: nothing
    // is on the return stack for it.)  It is run after the start of a loop,
    // which nothing is joined across, so that the >r before it stays.
    {"r> @ over ! is one instruction", "cp @ : j6 r> @ over ! ; cp @ swap -", 1, {2}},
    {"and stores what it fetched",
     "variable x  create w 0 ,  : j6 >r begin r> @ over ! 1 until ;\n7 x !  w x j6 drop  w @", 1, {7}},
    // What the two before it are together may be nothing, and then nothing
    // is compiled: >r r> is only the exit, as cross.fs makes it.
    {">r r> is nothing", "cp @ : j7 >r r> ; cp @ swap -  5 j7", 2, {1, 5}},
    // After an instruction that leaves a flag, 0= is invert: a comparison
    // and an invert that returns.  After what leaves no flag 0= is called,
    // being longer in place, and at the end of a word that is a jump.
    {"0= after < is invert", "cp @ : j8 < 0= ; cp @ swap -  3 5 j8  5 3 j8", 3, {2, 0, -1}},
    {"and after = and u<", ": j8 = 0= ; : j9 u< 0= ;  cp @ : ja over = 0= ; cp @ swap -  4 4 j8  4 5 j8  1 -1 j9  -1 1 j9  7 7 ja",
     7, {2, 0, -1, 0, -1, 7, 0}},
    {"0= after what leaves no flag is not invert", "cp @ : jc + 0= ; cp @ swap -  1 2 jc  1 -1 jc", 3, {2, 0, -1}},
    // A dup is taken across a number that is two instructions, as it is
    // across one of one: the push, the add, and an `over and` that returns.
    {"a dup is taken across a number of two instructions",
     "cp @ : jd dup 5000 and ; cp @ swap -  -1 jd  cp @ : je dup 5 and ; cp @ swap -  -1 je", 6, {3, -1, 5000, 2, -1, 5}},

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

    // A text (text.fs) is lines of characters packed into cells, each line
    // from where a cell starts to an EOL (10).  What is put in crosses
    // cells on every machine: "hello world" is eleven characters.  A code
    // is a character of its line, and one taken back leaves the EOL where
    // it was.  A line of one character is a cell on every machine, so four
    // cells hold four such lines: a text that rolls loses its first line to
    // make room for a fifth, and one that does not takes no more.
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
    // Ninety-one lines, of every length from none to eleven, into a text of
    // sixteen cells that rolls: the lines left are the last ones, whole.
    // Line k is the first k mod 12 characters of "hello world": its EOL is
    // where it ends, and its last character is the string's.
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
    // As the Forth window fills its text: a code at a time, a line ended at
    // fifty, into sixteen lines' room.  Two hundred lines, line k of k mod
    // 50 characters, all of them the letter of their line, a to z over
    // again: every line left is whole, its last character its letter and
    // its EOL after, and the cell after the text is as it was.
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

    // Tasks: one that counts, and a loop that keeps its index across pauses.
    // The record is the eleven cells a task's is, with what is counted
    // straight after it: a spawn that went past its record would be seen.
    {"a task that counts, and a loop that pauses",
     "create rec 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,\n"
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
