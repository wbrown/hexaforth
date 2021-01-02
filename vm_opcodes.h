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

enum ALU_TYPE {
    ALU_T = 0,
    ALU_N = 1,
    ALU_ADD = 2,
    ALU_AND = 3,
    ALU_OR = 4,
    ALU_XOR = 5,
    ALU_INVERT = 6,
    ALU_EQ = 7,
    ALU_GT = 8,
    ALU_RSHIFT = 9,
    ALU_LSHIFT = 10,
    ALU_R_T = 11,
    ALU_LOAD_T = 12,
    ALU_IO_T = 13,
    ALU_STATUS = 14,
    ALU_U_GT = 15
};

static char* ALU_OPS_REPR[] = {
        "T", "N", "T+N",
        "T&N", "T|N", "T^N",
        "~T", "T==N", "T>N",
        "N>>T", "N<<T", "R->T",
        "[T]", "io[T]", "depth",
        "T>Nu"
};

enum MEM_OP {
    MEM_T = 0,
    MEM_T_N = 1,
    MEM_T_R = 2,
    MEM_STORE_N_T = 3,
    MEM_IO_N_T = 4,
    MEM_IO_RD = 5,
    MEM_R_EIP = 6
};

static char* MEM_OPS_REPR[] = {
        "", "T->N", "T->R",
        "N->[T]", "io[N]->T", "io@",
        "R->EIP"
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
            bool  lit_f: 1;
            BYTE  op_type: 2;
            WORD  target: 13;
        } jmp;
        struct {
            bool    lit_f: 1;
            BYTE    op_type: 2;
            BYTE    alu_op: 4;
            BYTE    mem_op: 3;
            bool    __unused: 1;
            SBYTE   dstack: 2;
            SBYTE   rstack: 2;
            bool    ram_write: 1;
        } alu;
    };
} instruction;

typedef struct {
    char           repr[40];
    instruction    ins[8];
} forth_op;

static forth_op FORTH_OPS[] = {
        {"dup", {{ .alu.op_type = OP_TYPE_ALU,
                   .alu.alu_op = ALU_T,
                   .alu.mem_op = MEM_T_N,
                   .alu.dstack = 1 }}},
        {"over", {{ .alu.op_type = OP_TYPE_ALU,
                    .alu.alu_op = ALU_N,
                    .alu.mem_op = MEM_T_N,
                    .alu.dstack = 1 }}},
        {"invert", {{ .alu.op_type = OP_TYPE_ALU,
                      .alu.alu_op = ALU_INVERT }}},
        {"+", {{ .alu.op_type = OP_TYPE_ALU,
                 .alu.alu_op = ALU_ADD,
                 .alu.dstack = -1 }}},
        {"swap", {{ .alu.op_type = OP_TYPE_ALU,
                    .alu.alu_op = ALU_N,
                    .alu.mem_op = MEM_T_N}}},
        {"nip", {{ .alu.op_type = OP_TYPE_ALU,
                   .alu.alu_op = ALU_T,
                   .alu.dstack = -1 }}},
        {"drop", {{ .alu.op_type = OP_TYPE_ALU,
                    .alu.alu_op = ALU_N,
                    .alu.dstack = -1 }}},
        {"exit", {{ .alu.op_type = OP_TYPE_ALU,
                    .alu.alu_op = ALU_T,
                    .alu.mem_op = MEM_R_EIP,
                    .alu.rstack = -1 }}},
        {">r", {{ .alu.op_type = OP_TYPE_ALU,
                  .alu.alu_op = ALU_N,
                  .alu.mem_op = MEM_T_R,
                  .alu.rstack = 1,
                  .alu.dstack = -1 }}},
        {"r>", {{ .alu.op_type = OP_TYPE_ALU,
                  .alu.alu_op = ALU_R_T,
                  .alu.mem_op = MEM_T_N,
                  .alu.rstack = -1,
                  .alu.dstack = 1 }}},
        {"r@", {{ .alu.op_type = OP_TYPE_ALU,
                  .alu.alu_op = ALU_R_T,
                  .alu.mem_op = MEM_T_N,
                  .alu.dstack = 1 }}},
        {"@", {{ .alu.op_type = OP_TYPE_ALU,
                 .alu.alu_op = ALU_LOAD_T }}},
        {"!", {{ .alu.op_type = OP_TYPE_ALU,
                 .alu.alu_op = ALU_N,
                 .alu.dstack = -1,
                 .alu.ram_write = 1}}},
        {"1+", {{ .lit.lit_f = true,
                  .lit.lit_add = true,
                  .lit.lit_v = 1 }}},
        {"2+", {{ .lit.lit_f = true,
                  .lit.lit_add = true,
                  .lit.lit_v = 2 }}},
        {"", {{}}}};

bool ins_eq(instruction a, instruction b);
instruction* lookup_word(const char* word);
const char* lookup_opcode(instruction ins);
char* instruction_to_str(instruction ins);

#endif //HEXAFORTH_VM_OPCODES_H
