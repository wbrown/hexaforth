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

enum OP_TYPE {
    OP_TYPE_JMP = 0,
    OP_TYPE_CJMP = 1,
    OP_TYPE_CALL = 2,
    OP_TYPE_ALU = 3
} ;

static char* ALU_OP_TYPE_REPR[] = {
        "jump", "0jump", "call", "alu"
};

enum INPUT_MUX {
    INPUT_N = 0,
    INPUT_T = 1,
    INPUT_LOAD_T = 2,
    INPUT_R = 3,
};

static char* INPUT_REPR[] = {
        "N->I", "T->I", "[T]->I", "R->I"
};

enum ALU_TYPE {
    ALU_I = 0,
    ALU_T_N = 1,
    ALU_ADD = 2,
    ALU_AND = 3,
    ALU_OR = 4,
    ALU_XOR = 5,
    ALU_INVERT = 6,
    ALU_EQ = 7,
    ALU_GT = 8,
    ALU_RSHIFT = 9,
    ALU_LSHIFT = 10,
    ALU_LOAD = 11,
    ALU_IO_WRITE = 12,
    ALU_IO_READ = 13,
    ALU_STATUS = 14,
    ALU_U_GT = 15,
};

static char* ALU_OPS_REPR[] = {
        "I", "T->N", "I+N",
        "I&N", "I|N", "I^N",
        "~I", "I==N", "I>N",
        "N>>I", "N<<I", "[T]",
        "I->io[T]", "io[I]", "depth",
        "T>Nu"
};

enum OUTPUT_MUX {
    OUTPUT_T = 0,
    OUTPUT_R = 1,
    OUTPUT_MEM_T = 2,
    OUTPUT_NULL = 3,
};

static char* OUTPUT_REPR[] = {
        "->T", "->R", "N->[T]"
};

typedef struct {
    union {
        struct {
            WORD  lit_v: 12;
            BYTE  lit_shifts: 2;    // 0-3 shifts
            bool  lit_add: 1;       // add
            bool  lit_f: 1;
        } lit;
        struct {
            WORD  target: 13;
            BYTE  op_type: 2;
            BYTE  lit_f: 1;
        } jmp;
        struct {
            SBYTE   rstack: 2;
            SBYTE   dstack: 2;
            BYTE    alu_op: 4;      // operation
            BYTE    out_mux: 2;     // output of {T, N, R, N->[I]}
            BYTE    in_mux: 2;      // input of {T, [T], io[T], R}
            bool    r_eip: 1;       // R->EIP
            BYTE    op_type: 2;
            bool    lit_f: 1;
        } alu;
    };
} instruction;

enum DEF_TYPE {
    FIELD,
    TERM,
    INS,
    COMMT,
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
        {"T+I",        FIELD, {{.alu.alu_op = ALU_ADD }}},
        {"N->T,I",     FIELD, {{.alu.alu_op = ALU_T_N }}},
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
        {"imm<<16",    FIELD, {{.lit.lit_shifts = 0x1}}},
        {"imm<<32",    FIELD, {{.lit.lit_shifts = 0x2}}},
        {"imm<<48",    FIELD, {{.lit.lit_shifts = 0x3}}},
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

static forth_define FORTH_OPS[] = {
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

typedef struct {
    char* repr;
    instruction ins[8];
} word_node;

static word_node FORTH_WORDS[256];

static forth_op FORTH_OPS_OLD[] = {
        {"dup",              INS, {{ .alu.op_type = OP_TYPE_ALU,
                                     .alu.in_mux = INPUT_T,
                                     .alu.alu_op = ALU_I,
                                     .alu.dstack = 1 }}},
        {"over",             INS, {{ .alu.op_type = OP_TYPE_ALU,
                                     .alu.in_mux = INPUT_N,
                                     .alu.alu_op = ALU_I,
                                     .alu.dstack = 1 }}},
        {"invert",           INS, {{ .alu.op_type = OP_TYPE_ALU,
                                     .alu.in_mux = INPUT_T,
                                     .alu.alu_op = ALU_INVERT}}},
        {"+",                INS, {{ .alu.op_type = OP_TYPE_ALU,
                                     .alu.alu_op = ALU_ADD,
                                     .alu.dstack = -1 }}},
        {"swap",             INS, {{ .alu.op_type = OP_TYPE_ALU,
                                     .alu.in_mux = INPUT_N,
                                     .alu.alu_op = ALU_T_N,
                                     .alu.out_mux = OUTPUT_T}}},
        {"nip",              INS, {{ .alu.op_type = OP_TYPE_ALU,
                                     .alu.in_mux = INPUT_T,
                                     .alu.alu_op = ALU_T_N,
                                     .alu.dstack = -1 }}},
        {"drop",             INS, {{ .alu.op_type = OP_TYPE_ALU,
                                     .alu.in_mux = INPUT_N,
                                     .alu.alu_op = ALU_I,
                                     .alu.dstack = -1 }}},
        {"exit",             INS, {{ .alu.op_type = OP_TYPE_ALU,
                                     .alu.r_eip = true,
                                     .alu.rstack = -1 }}},
        {">r",               INS, {{ .alu.op_type = OP_TYPE_ALU,
                                     .alu.in_mux = INPUT_T,
                                     .alu.alu_op = ALU_I,
                                     .alu.out_mux = OUTPUT_R,
                                     .alu.rstack = 1,
                                     .alu.dstack = -1 }}},
        {"r>",               INS, {{ .alu.op_type = OP_TYPE_ALU,
                                     .alu.in_mux = INPUT_R,
                                     .alu.out_mux = OUTPUT_T,
                                     .alu.rstack = -1,
                                     .alu.dstack = 1 }}},
        {"r@",               INS, {{ .alu.op_type = OP_TYPE_ALU,
                                     .alu.in_mux = INPUT_R,
                                     .alu.out_mux = OUTPUT_T,
                                     .alu.dstack = 1 }}},
        {"@",                INS, {{ .alu.op_type = OP_TYPE_ALU,
                                     .alu.in_mux = INPUT_LOAD_T,
                                     .alu.out_mux = OUTPUT_T }}},
        {"!",                INS, {{ .alu.op_type = OP_TYPE_ALU,
                                     .alu.in_mux = INPUT_N,
                                     .alu.alu_op = ALU_I,
                                     .alu.out_mux = OUTPUT_MEM_T,
                                     .alu.dstack = -2}}},
        {"1+",               INS, {{ .lit.lit_f = true,
                                     .lit.lit_add = true,
                                     .lit.lit_v = 1 }}},
        {"2+",               INS, {{ .lit.lit_f = true,
                                     .lit.lit_add = true,
                                     .lit.lit_v = 2 }}},
        {"lshift",           INS, {{ .alu.op_type = OP_TYPE_ALU,
                                     .alu.alu_op = ALU_LSHIFT,
                                     .alu.dstack = -1}}},
        {"rshift",           INS, {{ .alu.op_type = OP_TYPE_ALU,
                                     .alu.alu_op = ALU_RSHIFT,
                                     .alu.dstack = -1}}},
        {"2*",               INS, {{ .lit.lit_f = true,
                                     .lit.lit_v = 1},
                                   { .alu.op_type = OP_TYPE_ALU,
                                     .alu.alu_op = ALU_LSHIFT,
                                     .alu.dstack = -1}}},
        {"2/",               INS, {{ .lit.lit_f = true,
                                     .lit.lit_v = 1},
                                   { .alu.op_type = OP_TYPE_ALU,
                                     .alu.alu_op = ALU_RSHIFT,
                                     .alu.dstack = -1}}},
        {"-",                INS, {{ .alu.op_type = OP_TYPE_ALU,
                                     .alu.in_mux = INPUT_T,
                                     .alu.alu_op = ALU_INVERT},
                                   { .alu.op_type = OP_TYPE_ALU,
                                     .alu.alu_op = ALU_ADD,
                                     .alu.dstack = -1}}},
        {"emit",             INS, {{ .lit.lit_f = true,
                                     .lit.lit_v = 0xf1},
                                   { .alu.op_type = OP_TYPE_ALU,
                                     .alu.in_mux = INPUT_N,
                                     .alu.alu_op = ALU_IO_WRITE,
                                     .alu.out_mux = OUTPUT_NULL,
                                     .alu.dstack = -2}}},
        {"8emit",            INS, {{ .lit.lit_f = true,
                                     .lit.lit_v = 0xf0},
                                   { .alu.op_type = OP_TYPE_ALU,
                                     .alu.in_mux = INPUT_N,
                                     .alu.alu_op = ALU_IO_WRITE,
                                     .alu.out_mux = OUTPUT_NULL,
                                     .alu.dstack = -2}}},
        {"key",              INS, {{ .lit.lit_f = true,
                                     .lit.lit_v = 0xe0},
                                   { .alu.op_type = OP_TYPE_ALU,
                                     .alu.in_mux = INPUT_T,
                                     .alu.alu_op = ALU_IO_READ,
                                     .alu.out_mux = OUTPUT_T}}},
        {"",                 INS, {{}}}};

bool ins_eq(instruction a, instruction b);
instruction* lookup_word(const char* word);
const char* lookup_opcode(instruction ins);
char* instruction_to_str(instruction ins);
bool init_opcodes();

#endif //HEXAFORTH_VM_OPCODES_H
