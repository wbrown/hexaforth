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
        if (ins_eq(ins, FORTH_OPS[idx].ins[0]) &&
                *(uint16_t*)(&FORTH_OPS[idx].ins[1]) == 0) {
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
        if (ins.alu.op_type == OP_TYPE_ALU) {
            const char* input_mux = INPUT_REPR[ins.alu.in_mux];
            const char* output_mux = OUTPUT_REPR[ins.alu.out_mux];
            const char* alu_ops_repr = ALU_OPS_REPR[ins.alu.alu_op];
            asprintf(&ret_str,
                     "input: %s (%#x01d) => "
                     "{alu_op: %s (%#02xd), DSTACK: %d RSTACK: %d, R->EIP: %s} "
                     "=> output: %s (%#01xd)",
                     input_mux,
                     ins.alu.in_mux,
                     alu_ops_repr,
                     ins.alu.alu_op,
                     ins.alu.dstack,
                     ins.alu.rstack,
                     ins.alu.r_eip ? "true" : "false",
                     output_mux,
                     ins.alu.out_mux);
        } else {
            asprintf(&ret_str,
                     "%s: {target: %d (%xd)}",
                     ALU_OP_TYPE_REPR[ins.jmp.op_type],
                     ins.jmp.target,
                     ins.jmp.target);
        }
    }
    return(ret_str);
}

