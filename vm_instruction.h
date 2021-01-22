//
// Created by Wes Brown on 1/22/21.
//

#ifndef HEXAFORTH_VM_INSTRUCTION_H
#define HEXAFORTH_VM_INSTRUCTION_H

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

enum OP_TYPE {
    OP_TYPE_JMP = 0,   // jump    unconditional branch
    OP_TYPE_CJMP = 1,  // 0jump   conditional jump off top of stack test
    OP_TYPE_CALL = 2,  // call    EIP+1 onto return stack, branch to target
    OP_TYPE_ALU = 3,   // alu     ALU operation
} ;

enum INPUT_MUX {
    INPUT_N = 0,       // N->IN    NOS, next on stack
    INPUT_T = 1,       // T->IN    TOS, top of stack
    INPUT_LOAD_T = 2,  // [T]->IN  Load value at *TOS
    INPUT_R = 3,       // R->IN    TOR, top of return
};

enum ALU_OPS {
    ALU_IN = 0,        // IN->       Passthrough IN
    ALU_SWAP_IN = 1,   // T<->N,IN-> Swap TOS and NOS, passthrough IN
    ALU_T_N = 2,       // N->T,IN->  Copy NOS to TOS, passthrough IN
    ALU_ADD = 3,       // IN+N
    ALU_AND = 4,       // IN&N
    ALU_OR = 5,        // IN|N
    ALU_XOR = 6,       // IN^N
    ALU_MUL = 7,       // IN*N
    ALU_INVERT = 8,    // ~I
    ALU_EQ = 9,        // IN==N
    ALU_GT = 10,        // N<IN
    ALU_U_GT = 11,     // Nu<IN      Unsigned <
    ALU_RSHIFT = 12,   // IN>>T
    ALU_LSHIFT = 13,   // IN<<T
    ALU_LOAD = 14,     // [IN]       Load value at *I
    ALU_IO_READ = 15,  // io[IN]     Read at IO address I
};

enum OUTPUT_MUX {
    OUTPUT_T = 0,      // ->T       write result to top of data stack
    OUTPUT_R = 1,      // ->R       write result to top of return stack
    OUTPUT_IO_T = 2,   // ->IO[T]   write result to IO address T
    OUTPUT_MEM_T = 3,  // ->[T]     write result to memory address T
};

#endif //HEXAFORTH_VM_INSTRUCTION_H
