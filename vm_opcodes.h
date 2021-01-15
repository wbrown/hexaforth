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
        // ALU operation with 4 input sources, 16 ops, 4 output targets
        struct {
            SBYTE   rstack: 2;      //              {r+1, r-1, r-2}
            SBYTE   dstack: 2;      //              {d+1, d-1, d-2}
            BYTE    alu_op: 4;      // ALU_OPS:     {IN->I, N->T,I, ...}
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
        "ubranch", "0jump", "scall", "alu"
};

enum OP_TYPE {
    OP_TYPE_JMP = 0,   // jump    unconditional branch
    OP_TYPE_CJMP = 1,  // 0jump   conditional jump off top of stack test
    OP_TYPE_CALL = 2,  // call    EIP+1 onto return stack, branch to target
    OP_TYPE_ALU = 3,   // alu     ALU operation
} ;


// === INPUI_MUX: Where does the second operand come from?
static char* INPUT_MUX_REPR[] = {
        "N->IN", "T->IN", "[T]->IN", "R->IN"
};

enum INPUT_MUX {
    INPUT_N = 0,       // N->IN    NOS, next on stack
    INPUT_T = 1,       // T->IN    TOS, top of stack
    INPUT_LOAD_T = 2,  // [T]->IN  Load value at *TOS
    INPUT_R = 3,       // R->IN    TOR, top of return
};

// === ALU_TYPE: what ALU operation to perform on T and/or I.
static char* ALU_OPS_REPR[] = {
        "IN->IN", "N->T,IN", "T+IN",
        "T&IN", "T|IN", "T^IN",
        "~IN", "T==IN", "IN<T",
        "IN>>T", "IN<<T", "[IN]",
        "IN->io[T]", "io[IN]", "status",
        "INu<T"
};

enum ALU_OPS {
    ALU_IN = 0,         // IN->IN       Passthrough I
    ALU_T_N = 1,       // N->T,IN     Copy NOS to TOS, passthrough I
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
    ALU_IO_WRITE = 12, // IN->io[T]   Write IN to IO address T
    ALU_IO_READ = 13,  // io[I]      Read at IO address I
    ALU_STATUS = 14,   // status     Report on stack depth
    ALU_U_GT = 15,     // I<Tu       Unsigned <
};

// === OUTPUT_MUX: where to write the results of the ALU operation
static char* OUTPUT_MUX_REPR[] = {
        "->T", "->R", "->[T]", "->NULL"
};

enum OUTPUT_MUX {
    OUTPUT_T = 0,      // ->T       write result to top of data stack
    OUTPUT_R = 1,      // ->R       write result to top of return stack
    OUTPUT_MEM_T = 2,  // ->[T]     write result to memory address T
    OUTPUT_NULL = 3,   // ->NULL    discard result
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
    instruction    ins[8];
} forth_op;

static forth_op INS_FIELDS[] = {
        {"input_mux",  COMMT, {{}}},
        {"N->IN",      INPUT, {{.alu.in_mux = INPUT_N}}},
        {"T->IN",      INPUT, {{.alu.in_mux = INPUT_T}}},
        {"[T]->IN",    INPUT, {{.alu.in_mux = INPUT_LOAD_T}}},
        {"R->IN",      INPUT, {{.alu.in_mux = INPUT_R}}},
        {"alu_op",     COMMT, {{}}},
        {"IN->IN",     FIELD, {{.alu.alu_op = ALU_IN }}},
        {"N->T,IN",    FIELD, {{.alu.alu_op = ALU_T_N }}},
        {"T+IN",       FIELD, {{.alu.alu_op = ALU_ADD }}},
        {"T&IN",       FIELD, {{.alu.alu_op = ALU_AND }}},
        {"T|IN",       FIELD, {{.alu.alu_op = ALU_OR, }}},
        {"T|N",        INPUT, {{.alu.alu_op = ALU_OR,
                                .alu.in_mux = INPUT_N}}},
        {"T^IN",       FIELD, {{.alu.alu_op = ALU_XOR }}},
        {"~IN",        FIELD, {{.alu.alu_op = ALU_INVERT}}},
        {"~T",         INPUT, {{.alu.in_mux = INPUT_T,
                                .alu.alu_op = ALU_INVERT}}},
        {"T==IN",      FIELD, {{.alu.alu_op = ALU_EQ }}},
        {"IN<T",       FIELD, {{.alu.alu_op = ALU_GT }}},
        {"IN>>T",      FIELD, {{.alu.alu_op = ALU_RSHIFT }}},
        {"IN<<T",      FIELD, {{.alu.alu_op = ALU_LSHIFT }}},
        {"N<<T",       INPUT, {{.alu.in_mux = INPUT_T,
                                .alu.alu_op = ALU_LSHIFT}}},
        {"[IN]",       FIELD, {{.alu.alu_op = ALU_LOAD }}},
        {"IN->io[T]",  FIELD, {{.alu.alu_op = ALU_IO_WRITE }}},
        {"io[IN]",     FIELD, {{.alu.alu_op = ALU_IO_READ }}},
        {"status",     FIELD, {{.alu.alu_op = ALU_STATUS }}},
        {"INu<T",      FIELD, {{.alu.alu_op = ALU_U_GT }}},
        {"output_mux", COMMT, {{}}},
        {"->T",        FIELD, {{.alu.out_mux = OUTPUT_T}}},
        {"->R",        FIELD, {{.alu.out_mux = OUTPUT_R}}},
        {"->[T]",      FIELD, {{.alu.out_mux = OUTPUT_MEM_T}}},
        {"->NULL",     FIELD, {{.alu.out_mux = OUTPUT_NULL}}},
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
        {"halt",    "       N->IN                                 ubranch"},
        {"noop",    "       N->IN             ->NULL              alu"},
        {"+",       "       N->IN   T+IN      ->T       d-1       alu"},
        {"xor",     "       N->IN   T^IN      ->T       d-1       alu"},
        {"and",     "       N->IN   T&IN      ->T       d-1       alu"},
        {"or",      "       N->IN   T|IN      ->T       d-1       alu"},
        {"invert",  "       T->IN   ~IN       ->T                 alu"},
        {"=",       "       N->IN   T==IN     ->T       d-1       alu"},
        {"<",       "       N->IN   IN<T      ->T       d-1       alu"},
        {"u<",      "       N->IN   INu<T     ->T       d-1       alu"},
        {"swap",    "       N->IN   N->T,IN   ->T                 alu"},
        {"dup",     "       T->IN             ->T       d+1       alu"},
        {"nip",     "       T->IN   N->T,IN   ->T       d-1       alu"},
        {"drop",    "       N->IN             ->T       d-1       alu"},
        {"over",    "       N->IN             ->T       d+1       alu"},
        {">r",      "       T->IN             ->R       d-1  r+1  alu"},
        {"r>",      "       R->IN             ->T       d+1  r-1  alu"},
        {"r@",      "       R->IN             ->T       d+1       alu"},
        {"@",       "       [T]->IN           ->T                 alu"},
        {"!",       "       N->IN             ->[T]     d-2       alu"},
        {"io@",     "       T->IN   io[IN]    ->T                 alu"},
        {"io!",     "       N->IN   IN->io[T] ->NULL    d-2       alu"},
        {"rshift",  "       N->IN   IN>>T     ->T       d-1       alu"},
        {"lshift",  "       N->IN   IN<<T     ->T       d-1       alu"},
        {"depths",  "       T->IN   status    ->T       d+1       alu"},
        {"depthr",  "       R->IN   status    ->T       d+1       alu"},
        {"exit",    "       T->IN             ->T  RET       r-1  alu"},
        {"2dup<",   "       N->IN   IN<T      ->T       d+1       alu"},
        {"dup@",    "       [T]->IN           ->T       d+1       alu"},
        {"overand", "       N->IN   T&IN      ->T                 alu"},
        {"dup>r",   "       T->IN             ->R            r+1  alu"},
        {"2dupxor", "       N->IN   T^IN      ->T       d+1       alu"},
        {"over+",   "       N->IN   T+IN      ->T                 alu"},
        {"over=",   "       T->IN   T==IN     ->T                 alu"},
        {"rdrop",   "       T->IN             ->T            r-1  alu"},
        {"1+",      "1      imm+                                  imm"},
        {"2+",      "2      imm+                                  imm"},
        {"2*",      "1                                            imm lshift", CODE},
        {"2/",      "1                                            imm rshift", CODE},
        {"emit",    "241                                          imm io!", CODE},
        {"8emit",   "240                                          imm io!", CODE},
        {"key",     "224                                          imm io@", CODE},
        {"negate",  "invert 1 imm+ imm", CODE},
        {"-",       "invert 1 imm+ imm +", CODE},
        {"",        ""}};

// Instructions associated with string representations.
typedef struct {
    char* repr;
    instruction ins[8];
    uint8_t type;
} word_node;

// Where the compiler looks for definitions to look up.  This is initialized,
// and the `FORTH_OPS[]` table is interpreted into a compilied form stored in
// `FORTH_WORDS`.
static word_node FORTH_WORDS[256]={};

bool ins_eq(instruction a, instruction b);
bool interpret_imm(const char* word, instruction* literal);
bool lookup_word(word_node* nodes, const char* word, instruction* lookup);
const char* lookup_opcode(word_node nodes[], instruction ins);
char* instruction_to_str(instruction ins);
bool init_opcodes(word_node opcodes[]);

#endif //HEXAFORTH_VM_OPCODES_H
