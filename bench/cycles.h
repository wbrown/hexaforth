//
// The number of cycles the Verilog CPU takes for an instruction.
// rtl/hexaforth.v defines this, and states it at its top: one cycle, plus
// one for each memory read, plus one for a shift, plus two for a multiply.
// A fetch of a variable reads memory.
//
// Tools use this to weight a profile by cycles.  The cycles a piece of
// work actually takes are measured on the CPU itself, by the target
// `bench`.
//

#ifndef HEXAFORTH_CYCLES_H
#define HEXAFORTH_CYCLES_H

#include "../vm_instruction.h"

// A literal instruction reads memory if it fetches a variable.  The caller
// says whether it does, because what a literal instruction does comes from
// the machine's step (util/kinds.h), not from its fields.
static int cycles_of(instruction ins, int fetches_a_variable) {
    if (ins.lit.lit_f) return(fetches_a_variable ? 2 : 1);
    if (ins.alu.op_type != OP_TYPE_ALU) return(1);
    return(1 + (ins.alu.in_mux == INPUT_LOAD_T) + (ins.alu.alu_op == ALU_LOAD) +
           (ins.alu.alu_op == ALU_RSHIFT || ins.alu.alu_op == ALU_LSHIFT) + 2 * (ins.alu.alu_op == ALU_MUL));
}

#endif //HEXAFORTH_CYCLES_H
