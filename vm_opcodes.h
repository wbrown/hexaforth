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
    ALU_IO_RD = 13,
    ALU_STATUS = 13,
    ALU_U_GT = 14,
};

static char* ALU_OPS_REPR[] = {
        "I", "T->N", "I+N",
        "I&N", "I|N", "I^N",
        "~I", "I==N", "I>N",
        "N>>I", "N<<I", "[I]",
        "[T]", "N->io[I]", "depth",
        "T>Nu"
};

enum OUTPUT_MUX {
    OUTPUT_T = 0,
    OUTPUT_R = 1,
    OUTPUT_MEM_T = 2,
};

static char* OUTPUT_REPR[] = {
        "->T", "->R", "N->[T]"
};

typedef struct {
    union {
        struct {
            bool  lit_f: 1;
            bool  lit_add: 1;       // add
            BYTE  lit_shifts: 2;    // 0-3 shifts
            WORD  lit_v: 12;
        } lit;
        struct {
            BYTE  lit_f: 1;
            BYTE  op_type: 2;
            WORD  target: 13;
        } jmp;
        struct {
            bool    lit_f: 1;
            bool    r_eip: 1;       // R->EIP
            BYTE    op_type: 2;
            BYTE    in_mux: 2;      // input of {T, [T], io[T], R}
            BYTE    out_mux: 2;     // output of {T, N, R, N->[I]}
            BYTE    alu_op: 4;      // operation
            SBYTE   dstack: 2;
            SBYTE   rstack: 2;
        } alu;
    };
} instruction;

typedef struct {
    char           repr[40];
    instruction    ins[8];
} forth_op;

static forth_op FORTH_OPS[] = {
        {"dup", {{ .alu.op_type = OP_TYPE_ALU,
                   .alu.in_mux = INPUT_T,
                   .alu.alu_op = ALU_I,
                   .alu.dstack = 1 }}},
        {"over", {{ .alu.op_type = OP_TYPE_ALU,
                    .alu.in_mux = INPUT_N,
                    .alu.alu_op = ALU_I,
                    .alu.dstack = 1 }}},
        {"invert", {{ .alu.op_type = OP_TYPE_ALU,
                      .alu.in_mux = INPUT_T,
                      .alu.alu_op = ALU_INVERT}}},
        {"+", {{ .alu.op_type = OP_TYPE_ALU,
                 .alu.alu_op = ALU_ADD,
                 .alu.dstack = -1 }}},
        {"swap", {{ .alu.op_type = OP_TYPE_ALU,
                    .alu.in_mux = INPUT_N,
                    .alu.alu_op = ALU_T_N,
                    .alu.out_mux = OUTPUT_T}}},
        {"nip", {{ .alu.op_type = OP_TYPE_ALU,
                   .alu.in_mux = INPUT_T,
                   .alu.alu_op = ALU_T_N,
                   .alu.dstack = -1 }}},
        {"drop", {{ .alu.op_type = OP_TYPE_ALU,
                    .alu.in_mux = INPUT_N,
                    .alu.alu_op = ALU_I,
                    .alu.dstack = -1 }}},
        {"exit", {{ .alu.op_type = OP_TYPE_ALU,
                    .alu.r_eip = true,
                    .alu.rstack = -1 }}},
        {">r", {{ .alu.op_type = OP_TYPE_ALU,
                  .alu.in_mux = INPUT_T,
                  .alu.alu_op = ALU_I,
                  .alu.out_mux = OUTPUT_R,
                  .alu.rstack = 1,
                  .alu.dstack = -1 }}},
        {"r>", {{ .alu.op_type = OP_TYPE_ALU,
                  .alu.in_mux = INPUT_R,
                  .alu.out_mux = OUTPUT_T,
                  .alu.rstack = -1,
                  .alu.dstack = 1 }}},
        {"r@", {{ .alu.op_type = OP_TYPE_ALU,
                  .alu.in_mux = INPUT_R,
                  .alu.out_mux = OUTPUT_T,
                  .alu.dstack = 1 }}},
        {"@", {{ .alu.op_type = OP_TYPE_ALU,
                 .alu.in_mux = INPUT_LOAD_T,
                 .alu.out_mux = OUTPUT_T }}},
        {"!", {{ .alu.op_type = OP_TYPE_ALU,
                 .alu.in_mux = INPUT_N,
                 .alu.alu_op = ALU_I,
                 .alu.out_mux = OUTPUT_MEM_T,
                 .alu.dstack = -2}}},
        {"1+", {{ .lit.lit_f = true,
                  .lit.lit_add = true,
                  .lit.lit_v = 1 }}},
        {"2+", {{ .lit.lit_f = true,
                  .lit.lit_add = true,
                  .lit.lit_v = 2 }}},
        {"lshift", {{ .alu.op_type = OP_TYPE_ALU,
                      .alu.alu_op = ALU_LSHIFT,
                      .alu.dstack = -1}}},
        {"rshift", {{ .alu.op_type = OP_TYPE_ALU,
                      .alu.alu_op = ALU_RSHIFT,
                      .alu.dstack = -1}}},
        {"2*", {{.lit.lit_f = true,
                 .lit.lit_v = 1},
                { .alu.op_type = OP_TYPE_ALU,
                  .alu.alu_op = ALU_LSHIFT,
                  .alu.dstack = -1}}},
        {"2/", {{.lit.lit_f = true,
                 .lit.lit_v = 1},
                { .alu.op_type = OP_TYPE_ALU,
                  .alu.alu_op = ALU_RSHIFT,
                  .alu.dstack = -1}}},
        {"-", {{ .alu.op_type = OP_TYPE_ALU,
                 .alu.in_mux = INPUT_T,
                 .alu.alu_op = ALU_INVERT},
               { .alu.op_type = OP_TYPE_ALU,
                 .alu.alu_op = ALU_ADD,
                 .alu.dstack = -1}}},
        {"", {{}}}};

bool ins_eq(instruction a, instruction b);
instruction* lookup_word(const char* word);
const char* lookup_opcode(instruction ins);
char* instruction_to_str(instruction ins);

#endif //HEXAFORTH_VM_OPCODES_H
