//
// The cycles the Verilog CPU takes for an instruction (rtl/hexaforth.v says
// so at its top, and is where it is so): one, another for each read of
// memory and for a shift, and two more for a multiply.  A fetch of a
// variable reads memory.
//
// This is for what weighs a profile by them.  What a piece of work takes
// in cycles is measured, on the CPU itself: `make -C rtl bench`.
//

#ifndef HEXAFORTH_CYCLES_H
#define HEXAFORTH_CYCLES_H

#include "../vm_instruction.h"

// An instruction with a number in it reads memory if it fetches a
// variable, which whoever asks says: what such an instruction does is the
// machine's step's to say (util/kinds.c), and not its fields'.
static int cycles_of(instruction ins, int fetches_a_variable) {
    if (ins.lit.lit_f) return(fetches_a_variable ? 2 : 1);
    if (ins.alu.op_type != OP_TYPE_ALU) return(1);
    return(1 + (ins.alu.in_mux == INPUT_LOAD_T) + (ins.alu.alu_op == ALU_LOAD) +
           (ins.alu.alu_op == ALU_RSHIFT || ins.alu.alu_op == ALU_LSHIFT) + 2 * (ins.alu.alu_op == ALU_MUL));
}

#endif //HEXAFORTH_CYCLES_H
