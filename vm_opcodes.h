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

// ==========================================================================
// Hexaforth Instruction struct and enums for fields
// ==========================================================================

// Our core instruction structure, which is 16-bits with bitfields.  Three main
// operation types.
typedef struct {
    union {
        // 12-bit unsigned literal (values: 0-4095)
        struct {
            WORD    lit_v: 12;      //              unsigned 12-bit integer
            BYTE    lit_shifts: 2;  // shifts:      {imm<<12,imm<<24,imm<<36}
            bool    lit_add: 1;     // add to T?:   {true, false}
            bool    lit_f: 1;       // literal?:    {true, false}
        } lit;
        // jump operations, 13-bit target, 8192 words (16384 bytes) addressable.
        struct {
            WORD    target: 13;     //              8192 words addressable
            BYTE    op_type: 2;     // OP_TYPE:     {jump, 0jump, call, alu}
            bool    lit_f: 1;       // literal?:    {true, false}
        } jmp;
        struct {
            SBYTE   rstack: 2;      //              {r+1, r-1, r-2}
            SBYTE   dstack: 2;      //              {d+1, d-1, d-2}
            BYTE    alu_op: 4;      // ALU_OPS:     {I->I, N->T,I, ...}
            BYTE    out_mux: 2;     // OUTPUT_MUX:  {T, N, R, N->[I]}
            BYTE    in_mux: 2;      // INPUT_MUX:   {T, [T], io[T], R}
            bool    r_eip: 1;       // R->EIP, copy top of return to PC
            BYTE    op_type: 2;     // OP_TYPE:     {jump, 0jump, call, alu}
            bool    lit_f: 1;       // literal?:    {true, false}
        } alu;
    };
} instruction;

// === OP_TYPE: Instruction operation class.
static char* OP_TYPE_REPR[] = {
        "jump", "0jump", "call", "alu"
};

enum OP_TYPE {
    OP_TYPE_JMP = 0,   // jump    unconditional branch
    OP_TYPE_CJMP = 1,  // 0jump   conditional jump off top of stack test
    OP_TYPE_CALL = 2,  // call    EIP+1 onto return stack, branch to target
    OP_TYPE_ALU = 3,   // alu     ALU operation
} ;


// === INPUI_MUX: Where does the second operand come from?
static char* INPUT_MUX_REPR[] = {
        "N->I", "T->I", "[T]->I", "R->I"
};

enum INPUT_MUX {
    INPUT_N = 0,       // N->I    NOS, next on stack
    INPUT_T = 1,       // T->I    TOS, top of stack
    INPUT_LOAD_T = 2,  // [T]->I  Load value at *TOS
    INPUT_R = 3,       // R->I    TOR, top of return
};

// === ALU_TYPE: what ALU operation to perform on T and/or I.
static char* ALU_OPS_REPR[] = {
        "I->I", "N->T,I", "T+I",
        "T&I", "T|I", "T^I",
        "~I", "T==I", "I<T",
        "I>>T", "I<<T", "[I]",
        "I->io[T]", "io[I]", "status",
        "I<Tu"
};

enum ALU_OPS {
    ALU_I = 0,         // I->I       Passthrough I
    ALU_T_N = 1,       // N->T,I     Copy NOS to TOS, passthrough I
    ALU_ADD = 2,       // T+I
    ALU_AND = 3,       // T&I
    ALU_OR = 4,        // T|I
    ALU_XOR = 5,       // T^I
    ALU_INVERT = 6,    // ~I
    ALU_EQ = 7,        // T==I
    ALU_GT = 8,        // I<T
    ALU_RSHIFT = 9,    // I>>T
    ALU_LSHIFT = 10,   // I<<T
    ALU_LOAD = 11,     // [I]        Load value at *I
    ALU_IO_WRITE = 12, // I->io[T]   Write I to IO address T
    ALU_IO_READ = 13,  // io[I]      Read at IO address I
    ALU_STATUS = 14,   // status     Report on stack depth
    ALU_U_GT = 15,     // I<Tu       Unsigned <
};

// === OUTPUT_MUX: where to write the results of the ALU operation
enum OUTPUT_MUX {
    OUTPUT_T = 0,      // ->T       write result to top of data stack
    OUTPUT_R = 1,      // ->R       write result to top of return stack
    OUTPUT_MEM_T = 2,  // ->[T]     write result to memory address T
    OUTPUT_NULL = 3,   // ->NULL    discard result
};

static char* OUTPUT_MUX_REPR[] = {
        "->T", "->R", "->[T]", "->NULL"
};

// ==========================================================================
// Instruction field operation, instruction defines
// ==========================================================================

enum DEF_TYPE {
    FIELD,        // Instruction field mapping
    TERM,         // Terminating field definition, writes instruction out
    INS,
    COMMT,        // Comment entry, for Forth code generation
    CODE
};

typedef struct {
    char           repr[40];
    uint8_t        type;
    instruction    ins[8];
} forth_op;

static forth_op INS_FIELDS[] = {
        {"input_mux",  COMMT, {{}}},
        {"N->I",       FIELD, {{.alu.in_mux = INPUT_N}}},
        {"T->I",       FIELD, {{.alu.in_mux = INPUT_T}}},
        {"[T]->I",     FIELD, {{.alu.in_mux = INPUT_LOAD_T}}},
        {"R->I",       FIELD, {{.alu.in_mux = INPUT_R}}},
        {"alu_op",     COMMT, {{}}},
        {"I->I",       FIELD, {{.alu.alu_op = ALU_I }}},
        {"N->T,I",     FIELD, {{.alu.alu_op = ALU_T_N }}},
        {"T+I",        FIELD, {{.alu.alu_op = ALU_ADD }}},
        {"T&I",        FIELD, {{.alu.alu_op = ALU_AND }}},
        {"T|I",        FIELD, {{.alu.alu_op = ALU_OR }}},
        {"T^I",        FIELD, {{.alu.alu_op = ALU_XOR }}},
        {"~I",         FIELD, {{.alu.alu_op = ALU_INVERT}}},
        {"T==I",       FIELD, {{.alu.alu_op = ALU_EQ }}},
        {"I<T",        FIELD, {{.alu.alu_op = ALU_GT }}},
        {"I>>T",       FIELD, {{.alu.alu_op = ALU_RSHIFT }}},
        {"I<<T",       FIELD, {{.alu.alu_op = ALU_LSHIFT }}},
        {"[I]",        FIELD, {{.alu.alu_op = ALU_LOAD }}},
        {"I->io[T]",   FIELD, {{.alu.alu_op = ALU_IO_WRITE }}},
        {"io[I]",      FIELD, {{.alu.alu_op = ALU_IO_READ }}},
        {"status",     FIELD, {{.alu.alu_op = ALU_STATUS }}},
        {"Iu<T",       FIELD, {{.alu.alu_op = ALU_U_GT }}},
        {"output_mux", COMMT, {{}}},
        {"->T",        FIELD, {{.alu.out_mux = OUTPUT_T}}},
        {"->R",        FIELD, {{.alu.out_mux = OUTPUT_R}}},
        {"->[T]",      FIELD, {{.alu.out_mux = OUTPUT_MEM_T}}},
        {"->NULL",     FIELD, {{.alu.out_mux = OUTPUT_NULL}}},
        {"stack_ops",  COMMT, {{}}},
        {"d+1",        FIELD, {{.alu.dstack = 1}}},
        {"d-1",        FIELD, {{.alu.dstack = -1}}},
        {"d-2",        FIELD, {{.alu.dstack = -2}}},
        {"r+1",        FIELD, {{.alu.rstack = 1}}},
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
//    `io@` is defined as `T->I io[I] ->T alu`, so step by step:
//           T->I   (0x0400)
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
        {"noop",    "                        ->NULL              alu"},
        {"+",       "       N->I   T+I       ->T       d-1       alu"},
        {"invert",  "       T->I   ~I        ->T                 alu"},
        {"swap",    "       N->I   N->T,I    ->T                 alu"},
        {"nip",     "       T->I   N->T,I    ->T       d-1       alu"},
        {"drop",    "       N->I             ->T       d-1       alu"},
        {"over",    "       N->I             ->T       d+1       alu"},
        {">r",      "       T->I             ->R       d-1  r+1  alu"},
        {"r>",      "       R->I             ->T       d+1  r-1  alu"},
        {"r@",      "       R->I             ->T       d+1       alu"},
        {"@",       "       [T]->I           ->T                 alu"},
        {"!",       "       N->I             ->[T]     d-2       alu"},
        {"io@",     "       T->I   io[I]     ->T                 alu"},
        {"io!",     "       N->I   I->io[T]  ->T       d-2       alu"},
        {"rshift",  "       N->I   I>>T      ->T       d-1       alu"},
        {"lshift",  "       N->I   I<<T      ->T       d-1       alu"},
        {"depths",  "              status    ->T                 alu"},
        {"depthr",  "       R->I   status    ->T                 alu"},
        {"exit",    "                            RET        r-1  alu"},
        {"1+",      "1      imm+                                 imm"},
        {"2+",      "2      imm+                                 imm"},
        {"2*",      "1                                           imm lshift"},
        {"2/",      "1                                           imm rshift"},
        {"emit",    "241                                         imm io!"},
        {"8emit",   "240                                         imm io!"},
        {"key",     "224                                         imm io@"},
        {"-",       "invert +"},
        {"",        ""}};

// Instructions associated with string representations.
typedef struct {
    char* repr;
    instruction ins[8];
} word_node;

// Where the compiler looks for definitions to look up.  This is initialized,
// and the `FORTH_OPS[]` table is interpreted into a compilied form stored in
// `FORTH_WORDS`.
static word_node FORTH_WORDS[256];

bool ins_eq(instruction a, instruction b);
instruction* lookup_word(const char* word);
const char* lookup_opcode(instruction ins);
char* instruction_to_str(instruction ins);
bool init_opcodes();

#endif //HEXAFORTH_VM_OPCODES_H
