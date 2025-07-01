//
// Created by Wes Brown on 12/31/20.
//

#ifndef HEXAFORTH_VM_OPCODES_H
#define HEXAFORTH_VM_OPCODES_H

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include "vm_constants.h"
#include "vm_instruction.h"

// === OP_TYPE: Instruction operation class.
static char* OP_TYPE_REPR[] = {
        "ubranch", "0jump", "scall", "alu"
};

// === INPUI_MUX: Where does the second operand come from?
static char* INPUT_MUX_REPR[] = {
        "N->IN", "T->IN", "[T]->IN", "R->IN"
};

// === ALU_TYPE: what ALU operation to perform on T and/or I.
static char* ALU_OPS_REPR[] = {
        "IN->", "T<>N,IN->", "T->N,IN->",
        "IN+N", "IN&N", "IN|N",
        "IN^N", "IN*N", "~IN",
        "IN==N", "N<IN", "Nu<IN",
        "IN>>T", "IN<<T", "[IN]",
        "io[IN]",
};

// === OUTPUT_MUX: where to write the results of the ALU operation
static char* OUTPUT_MUX_REPR[] = {
        "->T", "->R", "->io[T]", "->[T]"
};

// === Representation of various flags
static char* LIT_SHIFT_REPR[] = {
        "", "imm<<12", "imm<<24", "imm<<36"
};

static char* DSTACK_REPR[] = {
        "d+0", "d+1", "d-1", "d-2",
};

static char* RSTACK_REPR[] = {
        "r+0", "r+1", "r-1", "r-2"
};

// ==========================================================================
// Instruction field operation, instruction defines
// ==========================================================================

enum DEF_TYPE {
    INPUT,
    FIELD,        // Instruction field mapping
    TERM,         // Terminating field definition, writes instruction out
    INS,
    COMMT,        // Comment entry, for Forth code generation
    CODE
};

typedef struct {
    char           repr[40];
    uint8_t        type;
    instruction    ins[64];
    uint8_t        ins_ct;
} forth_op;

static forth_op INS_FIELDS[] = {
        {"input_mux",  COMMT, {{}}},
        {"N->IN",      INPUT, {{.alu.in_mux = INPUT_N}}},
        {"T->IN",      INPUT, {{.alu.in_mux = INPUT_T}}},
        {"[T]->IN",    INPUT, {{.alu.in_mux = INPUT_LOAD_T}}},
        {"R->IN",      INPUT, {{.alu.in_mux = INPUT_R}}},
        {"alu_op",     COMMT, {{}}},
        {"IN->",       FIELD, {{.alu.alu_op = ALU_IN }}},
        {"T<>N,IN->",  FIELD, {{.alu.alu_op = ALU_SWAP_IN }}},
        {"T->N,IN->",  FIELD, {{.alu.alu_op = ALU_T_N }}},
        {"IN+N",       FIELD, {{.alu.alu_op = ALU_ADD }}},
        {"IN&N",       FIELD, {{.alu.alu_op = ALU_AND }}},
        {"IN|N",       FIELD, {{.alu.alu_op = ALU_OR, }}},
        {"T|N",        INPUT, {{.alu.alu_op = ALU_OR,
                                .alu.in_mux = INPUT_T}}},
        {"IN^N",       FIELD, {{.alu.alu_op = ALU_XOR }}},
        {"IN*N",       FIELD, {{.alu.alu_op = ALU_MUL }}},
        {"~IN",        FIELD, {{.alu.alu_op = ALU_INVERT}}},
        {"~T",         INPUT, {{.alu.in_mux = INPUT_T,
                                .alu.alu_op = ALU_INVERT}}},
        {"IN==N",      FIELD, {{.alu.alu_op = ALU_EQ }}},
        {"N<IN",       FIELD, {{.alu.alu_op = ALU_GT }}},
        {"IN>>T",      FIELD, {{.alu.alu_op = ALU_RSHIFT }}},
        {"IN<<T",      FIELD, {{.alu.alu_op = ALU_LSHIFT }}},
        {"N<<T",       INPUT, {{.alu.in_mux = INPUT_N,
                                .alu.alu_op = ALU_LSHIFT}}},
        {"[IN]",       FIELD, {{.alu.alu_op = ALU_LOAD }}},
        {"io[IN]",     FIELD, {{.alu.alu_op = ALU_IO_READ }}},
        {"Nu<IN",      FIELD, {{.alu.alu_op = ALU_U_GT }}},
        {"output_mux", COMMT, {{}}},
        {"->T",        FIELD, {{.alu.out_mux = OUTPUT_T}}},
        {"->R",        FIELD, {{.alu.out_mux = OUTPUT_R}}},
        {"->io[T]",    FIELD, {{.alu.out_mux = OUTPUT_IO_T }}},
        {"->[T]",      FIELD, {{.alu.out_mux = OUTPUT_MEM_T}}},
        {"stack_ops",  COMMT, {{}}},
        {"d+1",        FIELD, {{.alu.dstack = 1}}},
        {"d+0",        FIELD, {{.alu.dstack = 0}}},
        {"d-1",        FIELD, {{.alu.dstack = -1}}},
        {"d-2",        FIELD, {{.alu.dstack = -2}}},
        {"r+1",        FIELD, {{.alu.rstack = 1}}},
        {"r+0",        FIELD, {{.alu.dstack = 0}}},
        {"r-1",        FIELD, {{.alu.rstack = -1}}},
        {"r-2",        FIELD, {{.alu.rstack = -2}}},
        {"RET",        FIELD, {{.alu.r_eip = true}}},
        {"literals",   COMMT, {{}}},
        {"imm+",       FIELD, {{.lit.lit_add = true}}},
        {"imm<<12",    FIELD, {{.lit.lit_shifts = 0x1}}},
        {"imm<<24",    FIELD, {{.lit.lit_shifts = 0x2}}},
        {"imm<<36",    FIELD, {{.lit.lit_shifts = 0x3}}},
        {"imm",        TERM,  {{.lit.lit_f = true}}},
        {"op_type",    COMMT, {{}}},
        {"ubranch",    TERM,  {{.alu.op_type = OP_TYPE_JMP}}},
        {"0branch",    TERM,  {{.alu.op_type = OP_TYPE_CJMP}}},
        {"scall",      TERM,  {{.alu.op_type = OP_TYPE_CALL}}},
        {"alu",        TERM,  {{.alu.op_type = OP_TYPE_ALU}}},
        {"",           FIELD, {{}}}};

typedef struct {
    char repr[40];
    char code[160];
    uint8_t type;
} forth_define;

// Our internal Forth-like assembler that defines Forth words by referencing
// words in `INS_FIELDS[]`.  Read left to right, each operation is OR'ed onto
// the prior operation's results.  Terminal fields such as `alu` and `imm`
// will do a final `OR` with its own field, and write out the 16-bit
// instruction.
//
// This table is read in at program startup, and interpreted to build a table
// of compiled instructions associated with each word.
//
// EXAMPLE:
//    `io@` is defined as `T->IN io[I] ->T alu`, so step by step:
//           T->IN   (0x0400)
//        || io[I]  (0x00d0)   => 0x04d0
//        || ->T    (0X0000)   => 0x04d0     (this could be implicit)
//        || alu    (0x6000)   => 0x64d0     (final 16-bit instruction written out)
//
// Additionally, it functions as a limited macro assembler in that defined words
// can refer to earlier words.  12-bit unsigned literals are supported.
//
// EXAMPLES:
//     `-` is built out of two instructions, as we don't have a subtraction op.
//           invert (0x6400)   (written out)
//           +      (0x602c)   (written out)
//
//     `1+` uses a literal `1`, as such:
//           1      (0x0001)
//        || imm+   (0x4001)   => 0x4001
//        || imm    (0x8000)   => 0xc001    (final 16-bit instruction written out)


static forth_define FORTH_OPS[] = {
        // word             in_mux|alu_op   |out_mux  | d  | r | op_type
        {"halt",    "       N->IN                                  ubranch"},
        {"noop",    "       T->IN                                  alu"},
        {"xor",     "       T->IN   IN^N       ->T       d-1       alu"},
        {"and",     "       T->IN   IN&N       ->T       d-1       alu"},
        {"or",      "       T->IN   IN|N       ->T       d-1       alu"},
        {"invert",  "       T->IN   ~IN        ->T                 alu"},
        {"+",       "       T->IN   IN+N       ->T       d-1       alu"},
        {"*",       "       T->IN   IN*N       ->T       d-1       alu"},
        {"=",       "       T->IN   IN==N      ->T       d-1       alu"},
        {"<",       "       T->IN   N<IN       ->T       d-1       alu"},
        {"u<",      "       T->IN   Nu<IN      ->T       d-1       alu"},
        {"swap",    "       N->IN   T->N,IN->  ->T                 alu"},
        {"dup>r",   "       T->IN              ->R            r+1  alu"},
        {"dup",     "       T->IN              ->T       d+1       alu"},
        {"nip",     "       T->IN   T->N,IN->  ->T       d-1       alu"},
        {"tuck",    "       T->IN   T<>N,IN->  ->T       d+1       alu"},
        {"drop",    "       N->IN              ->T       d-1       alu"},
        {"2drop",   "       R->IN              ->R       d-2       alu"},
        {"rdrop",   "       T->IN              ->T            r-1  alu"},
        {"over",    "       N->IN              ->T       d+1       alu"},
        {">r",      "       T->IN              ->R       d-1  r+1  alu"},
        {"r>",      "       R->IN              ->T       d+1  r-1  alu"},
        {"over>r",  "       N->IN              ->R            r+1  alu"},
        {"r@",      "       R->IN              ->T       d+1       alu"},
        {"@",       "       [T]->IN            ->T                 alu"},
        {"@+",      "       [T]->IN IN+N       ->T       d-1       alu"},
        {"@and",    "       [T]->IN IN&N       ->T       d-1       alu"},
        {"!",       "       N->IN              ->[T]     d-2       alu"},
        {"io@",     "       T->IN   io[IN]     ->T                 alu"},
        {"io!",     "       N->IN              ->io[T]   d-2       alu"},
        {"+!",      "       [T]->IN IN+N       ->[T]     d-2       alu"},
        {"rshift",  "       N->IN   IN>>T      ->T       d-1       alu"},
        {"lshift",  "       N->IN   IN<<T      ->T       d-1       alu"},
        {"exit",    "       T->IN              ->T  RET       r-1  alu"},
        {"@@",      "       [T]->IN [IN]       ->T                 alu"},
        {"dup@",    "       [T]->IN            ->T       d+1       alu"},
        {"dup@@",   "       [T]->IN [IN]       ->T       d+1       alu"},
        {"over@",   "       N->IN   [IN]       ->T       d+1       alu"},
        {"@r",      "       R->IN   [IN]       ->T       d+1       alu"},
        {"r@;",     "       R->IN              ->T  RET  d+1  r-1  alu"},
        {"2dup<",   "       T->IN   N<IN       ->T       d+1       alu"},
        {"overand", "       T->IN   IN&N       ->T                 alu"},
        {"2dupxor", "       T->IN   IN^N       ->T       d+1       alu"},
        {"over+",   "       T->IN   IN+N       ->T                 alu"},
        {"over=",   "       T->IN   IN==N      ->T                 alu"},
        {"swap>r",  "       N->IN   T->N,IN->  ->R       d-1  r+1  alu"},
        {"swapr>",  "       R->IN   T<>N,IN->  ->T       d+1  r-1  alu"},
        {"1+",      "1      imm+                                   imm"},
        {"2+",      "2      imm+                                   imm"},
        {"2*",      "1                                             imm lshift", CODE},
        {"2/",      "1                                             imm rshift", CODE},
        {"negate",  "invert 1+", CODE},
        {"-",       "negate +", CODE},
        {"emit",    "241                                           imm io!", CODE},
        {"8emit",   "240                                           imm io!", CODE},
        {"key",     "224                                           imm io@", CODE},
        {"rot",     ">r swap r> swap", CODE},
        {"-rot",    "swap>r swapr>", CODE},
        {"2dup",    "over over", CODE},
        {"2swap",   "rot >r rot r>", CODE},
        {"2over",   ">r >r over over r> swapr> swap>r >r swapr> swapr>", CODE},
        {"3rd",     ">r over r> swap", CODE},
        {"3dup",    "3rd 3rd 3rd", CODE},
        {">",       "swap <", CODE},
        {"u>",      "swap u< ", CODE},
        {"0=",      "0 imm =", CODE},
        {"0<",      "0 imm <", CODE},
        {"0>",      "0 imm >", CODE},
        {"<>",      "= invert", CODE},
        {"0<>",     "0 imm <>", CODE},
        {"1-",      "1 imm -", CODE},
        {"bounds",  "over+ swap", CODE},
        {"s>d",     "dup 0<", CODE},
        {"hi32",    "32 imm rshift", CODE},
        {"lo32",    "32 imm lshift 32 imm rshift", CODE},
        {"hi16",    "32 imm lshift 48 imm rshift", CODE},
        {"lo16",    "48 imm lshift 48 imm rshift", CODE},
        {">><<",    "tuck rshift swap lshift", CODE},
        {"nmask8",  "255 imm invert", CODE},
        {"w@",      "@ lo16", CODE},
        {"c@",      ">r @r 255 imm and r> 256 imm and or", CODE},
        {"c!",      ">r 255 imm and @r nmask8 and or r> !", CODE},
        {"nmask16",  "0 imm invert 16 imm lshift", CODE},
        {"w!",      ">r lo16 r@ @ nmask16 and or r> !", CODE},
        {"2w@",     "dup w@ swap 2+ w@", CODE},
        {"2w!",     ">r 16 imm lshift or r@ @ 0 imm invert 32 imm lshift and or r> !", CODE},
        {".s",      "0 imm 224 imm io!", CODE}};

// Instructions associated with string representations.
typedef struct {
    char* repr;
    uint8_t num_ins;
    instruction ins[64];
    uint8_t type;
} word_node;

// Where the compiler looks for definitions to look up.  This is initialized,
// and the `FORTH_OPS[]` table is interpreted into a compilied form stored in
// `FORTH_WORDS`.
static word_node FORTH_WORDS[256]={};

void decode_instruction(char* out, instruction ins, word_node words[]);
bool ins_eq(instruction a, instruction b);
bool interpret_imm(const char* word, instruction* literal);
uint8_t lookup_word(word_node* nodes, const char* word, instruction* lookup);
const char* lookup_opcode(word_node nodes[], instruction ins);
char* instruction_to_str(instruction ins);
bool init_opcodes(word_node opcodes[]);

#endif //HEXAFORTH_VM_OPCODES_H
