//
// tests.h - Test definitions for hexaforth VM
//
// This file contains all test case definitions used by the test framework.
// Tests are defined as an array of hexaforth_test structures that specify:
//   - Input Forth code to compile/execute
//   - Expected stack states (data and return stacks)
//   - Initial memory state
//   - Expected I/O behavior
//

#ifndef HEXAFORTH_TESTS_H
#define HEXAFORTH_TESTS_H

#include "vm_test.h"

static hexaforth_test TESTS[] = {
    {.label = "8-bit literals", .input = "1 2 3 4", .dstack = "1 2 3 4"},
    // saw issues due to the use of `malloc`, which re-used a prior
    // context's memory and didn't clear.  `calloc` solves this.
    {.label = "Context cleared after tests",
     .input = "6 7 8 9 10",
     .dstack = "6 7 8 9 10"},
    {.label = "halt ( -- )", .init = "0", .input = "1 halt", .dstack = "1"},
    {.label = "noop ( -- )", .input = "1 2 noop 3 noop", .dstack = "1 2 3"},
    // `invert` is required for negative literals to be pushed onto stack.
    {.label = "invert ( a -- ~a )",
     .input = "1 2 1024 invert",
     .dstack = "1 2 -1025"},
    {.label = "negative literals",
     .input = "-1 -2 -3 -4 -5",
     .dstack = "-1 -2 -3 -4 -5"},
    {.label = "lit16",
     .input = "65535 -65535 49151 -49151",
     .dstack = "65535 -65535 49151 -49151"},
    {.label = "lit32 max/min",
     .input = "4294967295 -4294967295 ",
     .dstack = "4294967295 -4294967295 "},
    {.label = "lit48 max/min",
     .input = "140737488355328 -140737488355328",
     .dstack = "140737488355328 -140737488355328"},
    {.label = "dup ( a -- a a )",
     .input = "1 2 3 dup 4 dup",
     .dstack = "1 2 3 3 4 4"},
    {.label = "swap ( a b -- b a )",
     .input = "5 4 2 1 swap",
     .dstack = "5 4 1 2"},
    {.label = "over ( a b -- a b a )",
     .input = "5 4 3 2 over",
     .dstack = "5 4 3 2 3"},
    {.label = "nip ( a b -- b )", .input = "5 4 3 2 nip", .dstack = "5 4 2"},
    {.label = "tuck ( a b -- b a b )",
     .input = "1024 2048 tuck",
     .dstack = "2048 1024 2048"},
    {.label = "rot ( a b c -- b c a )",
     .input = "1024 2048 4096 rot",
     .dstack = "2048 4096 1024"},
    {.label = "-rot ( a b c -- c a b )",
     .input = "1024 2048 4096 -rot",
     .dstack = "4096 1024 2048"},
    {.label = "drop ( a b -- a )",
     .input = "5 4 3 2 drop",
     .dstack = "5 4 3"},
    {.label = "2drop ( a b -- )", .input = "5 4 3 2 2drop", .dstack = "5 4"},
    {.label = "rdrop ( R: a -- R: )",
     .input = "1024 >r 2048 >r rdrop",
     .rstack = "1024"},
    {.label = "+ ( a b -- a+b )", .input = "10 15 +", .dstack = "25"},
    {.label = "* ( a b -- a*b )",
     .input = "10 15 * -520 10 *",
     .dstack = "150 -5200"},
    {.label = "1+ ( a -- a+1 )", .input = "1 2 3 1+", .dstack = "1 2 4"},
    {.label = "2+ ( a -- a+2 )", .input = "1 2 3 2+", .dstack = "1 2 5"},
    {.label = "xor ( a b -- a^b )",
     .input = "65535 32768 xor 65535 65535 xor -1 -1 xor",
     .dstack = "32767 0 0"},
    {.label = "and ( a b -- a&b )",
     .input = "65535 32768 and -1 65535 and",
     .dstack = "32768 65535"},
    {.label = "or ( a b -- a|b )",
     .input = "65535 32768 or -1 -1 or",
     .dstack = "65535 -1"},
    {
        .label = "lshift ( a b -- a << b )",
        .input = "2 1 lshift",
        .dstack = "4",
    },
    {
        .label = "48-bit literal test",
        .input = "20015998343868",
        .dstack = "20015998343868",
    },
    {
        .label = "edge case: 4096",
        .input = "4096",
        .dstack = "4096",
    },
    {
        .label = "lshift64 ( a b -- a << b )",
        .input = "140737488355328 16 lshift",
        .dstack = "9223372036854775808",
    },
    {.label = "rshift ( a b -- a >> b )",
     .input = "2 1 rshift",
     .dstack = "1"},
    {.label = "rshift ( a b -- a >> b )",
     .input = "4 1 rshift",
     .dstack = "2"},
    {.label = "- ( a b -- b-a )", .input = "500 300 -", .dstack = "200"},
    {.label = "2* ( a -- a << 1 )", .input = "512 2*", .dstack = "1024"},
    {.label = "2/ ( a -- a >> 1 )", .input = "512 2/", .dstack = "256"},
    {.label = "@ ( a -- [a] )",
     .init = "1024 -1024 2048 -2048 4096 -4096",
     .input = "0 @ 8 @ 16 @ 24 @ 32 @ 40 @",
     .dstack = "1024 -1024 2048 -2048 4096 -4096"},
    {.label = "! ( a b -- )",
     .init = "0 0",
     .input = "1024 0 ! -1024 8 ! 0 @ 8 @",
     .dstack = "1024 -1024"},
    {.label = ">r ( a -- R: a )",
     .input = "1024 >r",
     .dstack = "",
     .rstack = "1024"},
    {.label = "r@ ( R:a -- a R: a )",
     .input = "1024 >r r@ r@",
     .dstack = "1024 1024",
     .rstack = "1024"},
    {.label = "r> ( R:a -- a )",
     .input = "1024 >r r@ 2* r>",
     .dstack = "2048 1024",
     .rstack = ""},
    {.label = "= ( a b -- f )",
     .input = "1024 1024 = "
              "-1024 1024 = "
              "-1024 -1024 = ",
     .dstack = "-1 0 -1"},
    {.label = "u< ( aU bU -- f )",
     .input = "-1024 1024 u< 1024 -1024 u<",
     .dstack = "0 -1"},
    {.label = "< ( a b -- f )",
     .input = "-1024 0 < "
              "0 1024 < "
              "0 -1024 < "
              "2048 1024 < ",
     .dstack = "-1 -1 0 0"},
    {.label = "exit ( -- )",
     .init = "0",
     .input = "1024 >r 0 >r exit",
     .rstack = "1024"},
    {.label = "2dup< ( a b -- a b f )",
     .input = "1024 2048 2dup<",
     .dstack = "1024 2048 -1"},
    {.label = "dup@ ( addr -- addr n )",
     .init = "1024 2048",
     .input = "8 dup @",
     .dstack = "8 2048"},
    {.label = "overand ( a b -- a f )",
     .input = "1024 2048 overand 4096 4096 overand",
     .dstack = "1024 0 4096 4096"},
    {.label = "dup>r ( a -- a R: a )",
     .input = "1024 dup>r",
     .dstack = "1024",
     .rstack = "1024"},
    {.label = "dup>r with multiple values",
     .input = "10 20 30 dup>r",
     .dstack = "10 20 30",
     .rstack = "30"},
    {.label = "2dup< ( a b -- a b f )",
     .input = "1024 2048 2dup< 2048 1024 2dup<",
     .dstack = "1024 2048 -1 2048 1024 0"},
    {.label = "2dupxor ( a b -- a b a^b )",
     .input = "1024 2048 2dupxor",
     .dstack = "1024 2048 3072"},
    {.label = "over+ ( a b -- a a+b )",
     .input = "1024 4096 over+",
     .dstack = "1024 5120"},
    {.label = "over= ( a b -- a a=b? )",
     .input = "1024 4096 over= 2048 2048 over=",
     .dstack = "1024 0 2048 -1"},
    {.label = "@@ ( addr -- n )",
     .init = "8 1024 24 2048",
     .input = "0 @@ 16 @@",
     .dstack = "1024 2048"},
    {.label = "! ( n addr -- )",
     .init = "0 0 0 0",
     .input = "16 0 32 8 ! ! 8 @ 0 @",
     .dstack = "32 16"},
    {.label = "dup@ ( addr -- n )",
     .init = "0 0 24",
     .input = "16 dup@",
     .dstack = "16 24"},
    {.label = "dup@@ ( addr -- n )",
     .init = " 0 0 24 1024",
     .input = "16 dup@@",
     .dstack = "16 1024"},
    {.label = "over@ ( addr a -- addr a n )",
     .init = "1024",
     .input = "0 2048 over@",
     .dstack = "0 2048 1024"},
    {.label = "@r ( R: addr -- n )",
     .init = "1024 2048",
     .input = "8 >r @r",
     .dstack = "2048",
     .rstack = "8"},
    {.label = "swap>r ( a b -- b R: a )",
     .input = "1024 2048 swap>r",
     .dstack = "2048",
     .rstack = "1024"},
    {.label = "swapr> ( a b R: c -- b a c )",
     .input = "1024 2048 4096 >r swapr>",
     .dstack = "2048 1024 4096"},
    /* {
    .label = "48-bit string stack literal",
    .input = "'deadbe'",
    .dstack = "111473265304932 1"
    },
    {
    .label = "64-bit string stack literal",
    .input = "'deadbeef'",
    .dstack = "7378415037781730660 1"
    },
    {
    .label = "96-bit string stack literals",
    .input = "'deadbeefdead'",
    .dstack = "1684104548 7378415037781730660 2"
    },
    {.label = "8emit ( n -- )",
     .input = "'deadbe' drop 8emit",
     .io_expected = "deadbe"},
    {.label = "8emit ( n -- )",
     .input = "'deadbeef' drop 8emit",
     .io_expected = "deadbeef"},
    {.label = "8emit ( n -- )",
     .input = "'deadbeefdead' drop 8emit 8emit",
     .io_expected = "deadbeefdead"}, */
    {.label = "key ( -- ch )",
     .input = "key emit key emit key emit key emit",
     .io_input = "dead",
     .io_expected = "dead"},
    {.label = "+! ( n addr -- )",
     .init = "8",
     .input = "0 >r "
              "4  r@ +! r@ @ "
              "16 r@ +! r@ @ ",
     .dstack = "12 28",
     .rstack = "0"},
    {.label = "r@; ( R: addr -- addr )",
     .init = "0 0 0 0",
     .input = "4 >r r@;",
     .dstack = "4",
     .eip_expected = "4"},
    {.label = "@and ( n addr -- n )",
     .init = "32768",
     .input = "65535 0 @and",
     .dstack = "32768"},
    {.label = "2dup ( a b -- a b a b )",
     .input = "1024 2048 2dup",
     .dstack = "1024 2048 1024 2048"},
    {.label = "2swap ( ab cd -- cd ab )",
     .input = "1024 2048 4096 8192 2swap",
     .dstack = "4096 8192 1024 2048"},
    {.label = "2over ( ab cd - ab cd ab )",
     .input = "1024 2048 4096 8192 2over",
     .dstack = "1024 2048 4096 8192 1024 2048"},
    {.label = "3rd ( abc -- abc a )",
     .input = "1024 2048 4096 3rd",
     .dstack = "1024 2048 4096 1024"},
    {
        .label = "3dup ( abc -- abc abc )",
        .input = "1024 2048 4096 3dup",
        .dstack = "1024 2048 4096 1024 2048 4096",
    },
    {.label = "> ( a b -- f )",
     .input = "1024 2048 > 2048 1024 >",
     .dstack = "0 -1"},
    {
        .label = "u> ( a b -- f )",
        .input = "1024 2048 u> 2048 1024 u>",
        .dstack = "0 -1",
    },
    {
        .label = "0= ( a -- f )",
        .input = "0 0= 1 0= -1 0= ",
        .dstack = "-1 0 0",
    },
    {
        .label = "0< ( a -- f )",
        .input = "0 0< 1 0< -1 0< ",
        .dstack = "0 0 -1",
    },
    {
        .label = "0> ( a -- f )",
        .input = "0 0> 1 0> -1 0> ",
        .dstack = "0 -1 0",
    },
    {
        .label = "0<> ( a -- f )",
        .input = "0 0<> 1 0<> -1 0<> ",
        .dstack = "0 -1 -1",
    },
    {
        .label = "1- ( a -- a-1 )",
        .input = "1 1- 0 1- -1 1- ",
        .dstack = "0 -1 -2",
    },
    {
        .label = "bounds ( a b -- a+b a )",
        .input = "1024 2048 bounds",
        .dstack = "3072 1024",
    },
    {
        .label = "s>d ( s -- s f )",
        .input = "1024 s>d",
        .dstack = "1024 0",
    },
    {
        .label = "0xfffffffffffffff00",
        .input = "18446744073709551360",
        .dstack = "18446744073709551360",
    },
    {.label = "char literal ( -- c )", .input = "'a'", .dstack = "97"},
    {
        .label = "c@ ( addr -- c )",
        .input = "0 c@ 1 c@ 8 c@",
        .init = "7378415037781730660 101 0 0",
        .dstack = "100 101 101",
    },
    {.label = "c! ( c addr -- )",
     .init = "0 0 0 0",
     .input = "'d' 0 c! 'e' 1 c! 0 c@ 1 c@",
     .dstack = "100 101"},
    {.label = "w@ ( addr -- w )",
     .init = "305419896 0 0 0",  // 0x12345678 in first 8 bytes
     .input = "0 w@ 2 w@",
     .dstack = "22136 4660"},  // 0x5678 and 0x1234 (little-endian)
    {.label = "w! ( w addr -- )",
     .init = "0 0 0 0",
     .input = "43981 0 w! 61680 2 w! 0 @",
     .dstack = "4042304461"},  // 0xF0F0ABCD as decimal
    {.label = "w@ with various addresses",
     .init = "-1 -1 -1 -1",  // All bits set
     .input = "0 w@ 2 w@ 4 w@ 6 w@",
     .dstack = "65535 65535 65535 65535"},
    {.label = "w! preserves other bytes",
     .init = "72623859790382856 0 0 0",  // 0x0102030405060708
     .input = "43981 0 w! 48879 5 w! 0 @",  // Write 0xABCD to byte 0, 0xBEEF to byte 5
     .dstack = "125800640156183501"},  // 0x01BEEF040506ABCD
    {.label = "2w@ ( addr -- w1 w2 )",
     .init = "305419896 0 0 0",  // 0x12345678 in first 8 bytes
     .input = "0 2w@",
     .dstack = "22136 4660"},  // 0x5678 (addr 0) and 0x1234 (addr 2)
    {.label = "2w! ( w1 w2 addr -- )",
     .init = "0 0 0 0",
     .input = "43981 61680 0 2w! 0 @",
     .dstack = "4042304461"},  // 0xF0F0ABCD as decimal
    // Test array terminator
    {.label = "", .input = "", .dstack = ""},
    // {.label = "c@ ( addr -- c )",
    // {.label = "c! ( c addr -- )",
    // .init = "0 0 0 0",
    // .input = "'d' 0 c! 'e' 1 c! 'a' 2 c! 'd' 3 c! 'b' 4 c! 'e' 5 c! 'e' 6
    // c! 7 c! 0 @",
    //         .dstack = "7378415037781730660"},
    /* {.label = "depths ( -- n )",
            .input = "1024 2048 504 depths",
            .dstack = "1024 2048 504 3"},
    {.label = "depthr ( -- n )",
            .input = "1024 2048 >r >r depthr rdrop rdrop",
            .dstack = "2"}, */
};

#endif // HEXAFORTH_TESTS_H
