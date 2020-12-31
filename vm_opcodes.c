//
// Created by Wes Brown on 12/31/20.
//

#include "vm_opcodes.h"

// Basic equivalency operation, leveraging the fact that our encoded
// instructions can be compared as unsigned integers.
bool ins_eq(instruction a, instruction b) {
    if (*(uint16_t *)&a ==
                      *(uint16_t *)&b) {
        return true;
    } else {
        return false;
    }
}

// Given a string representing a `word` opcode,  search our opcodes for a
// match.  If matched, return the corresponding instruction structure, else
// null.
instruction* lookup_word(const char* word) {
    int idx = 0;
    while (strlen(FORTH_OPS[idx].repr)) {
        char* repr = FORTH_OPS[idx].repr;
        if (strcmp(repr, word) == 0) {
            return &FORTH_OPS[idx].ins[0];
        }
        idx++;
    }
    return NULL;
}

// Given an instruction, look up our table of instructions, and if a match
// is found, return the Forth representation of the opcode, else null.
const char* lookup_opcode(instruction ins) {
    int idx = 0;
    while (strlen(FORTH_OPS[idx].repr)) {
        if (ins_eq(ins, FORTH_OPS[idx].ins[0])) {
            return FORTH_OPS[idx].repr;
        }
        idx++;
    }
    return false;
}

// Given an instruction, generates a human readable decoding of it as a
// string and return it.
char* instruction_to_str(instruction ins) {
    char *ret_str;
    if (ins.lit.lit_f) {
        asprintf(&ret_str,
                 "lit: {lit_f=1, lit_add=%d, shifts=%d, lit.lit_v=%d => %u}",
                 ins.lit.lit_add, ins.lit.lit_shifts, ins.lit.lit_v,
                 (ins.lit.lit_v << (ins.lit.lit_shifts * LIT_BITS)));
    } else {
        char* mem_str;
        if (ins.alu.op_type == OP_TYPE_ALU) {
            if (ins.alu.mem_op) {
                asprintf(&mem_str,
                         ", mem_op: %s",
                         MEM_OPS_REPR[ins.alu.mem_op]);
            } else {
                asprintf(&mem_str, "");
            }
            const char* alu_ops_repr = ALU_OPS_REPR[ins.alu.alu_op];
            asprintf(&ret_str,
                     "alu: {alu_op: %d, op: %s%s, DSTACK: %d RSTACK: %d, N->[T]: %d}",
                     ins.alu.op_type,
                     alu_ops_repr,
                     mem_str,
                     ins.alu.dstack,
                     ins.alu.rstack,
                     ins.alu.ram_write);
            free(mem_str);
        } else {
            asprintf(&ret_str,
                     "%s: {target: %d}",
                     ALU_OP_TYPE_REPR[ins.jmp.op_type],
                     ins.jmp.target);
        }
    }
    return(ret_str);
}

