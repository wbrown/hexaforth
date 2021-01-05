//
// Created by Wes Brown on 12/31/20.
//

#include "vm.h"
#include "vm_constants.h"
#include "vm_debug.h"

int vm(context *ctx) {
    register int16_t EIP = ctx->EIP;
    register int16_t SP = 0;
    register int16_t RSP = 0;
    register int16_t IO = 0;
    register int64_t T = ctx->DSTACK[SP];
    register int64_t R = ctx->RSTACK[RSP];
    register int64_t I = 0;
    register int64_t OUT = 0;

    for (; ctx->memory[EIP] != 0;) {
        #ifdef DEBUG
            show_registers(T, R, EIP, SP, RSP, ctx);
        #endif // DEBUG
        instruction ins = *(instruction*)&(ctx->memory[EIP]);
        #ifdef DEBUG
            printf("MEMORY[%d]: ", EIP);
            debug_instruction(ins);
        #endif // DEBUG
        if (ins.lit.lit_f) {
            int64_t lit = (uint64_t)ins.lit.lit_v <<
                              (ins.lit.lit_shifts * LIT_BITS);
            if (!ins.lit.lit_add) {
                ctx->DSTACK[SP-1] = T;
                SP++;
                T = (int64_t)lit;
            } else {
                T += (int64_t)lit;
            }
            EIP++;
        } else {
            switch (ins.alu.in_mux) {
                case INPUT_N:
                    I = ctx->DSTACK[SP-2];
                    break;
                case INPUT_T:
                    I = T;
                    break;
                case INPUT_LOAD_T:
                    I = *(int64_t*)&(ctx->memory[T]);
                    break;
                case INPUT_R:
                    I = R;
                    break;
            }
            switch (ins.alu.op_type) {
                case OP_TYPE_ALU:
                    switch (ins.alu.alu_op) {
                        case ALU_I:
                            OUT = I;
                            break;
                        case ALU_ADD:
                            OUT = T + I;
                            break;
                        case ALU_T_N:
                            ctx->DSTACK[SP-2] = T;
                            OUT = I;
                            break;
                        case ALU_AND:
                            OUT = T & I;
                            break;
                        case ALU_OR:
                            OUT = T | I;
                            break;
                        case ALU_XOR:
                            OUT = T ^ I;
                            break;
                        case ALU_INVERT:
                            OUT = -I;
                            break;
                        case ALU_EQ:
                            OUT = T == I;
                            break;
                        case ALU_GT:
                            OUT = T < I;
                            break;
                        case ALU_RSHIFT:
                            OUT = I >> T;
                            break;
                        case ALU_LSHIFT:
                            OUT = I << T;
                            break;
                        case ALU_LOAD:
                            OUT = *(int64_t*)&(ctx->memory[I]);
                            break;
                        case ALU_IO_WRITE:
                            break;
                        case ALU_STATUS:
                            OUT = SP+1;
                            break;
                        case ALU_U_GT:
                            OUT = (uint64_t)T < (uint64_t)I;
                            break;
                        default:
                            break;
                    };
                    if (ins.alu.r_eip) EIP = R;
                    SP+=ins.alu.dstack;
                    RSP+=ins.alu.rstack;
                    if (ins.alu.dstack > 0) {
                        ctx->DSTACK[SP-2] = T;
                    }
                    if (ins.alu.rstack > 0) {
                        ctx->RSTACK[SP-2] = R;
                    }
                    switch (ins.alu.out_mux) {
                        case OUTPUT_T:
                            T = OUT;
                            break;
                        case OUTPUT_R:
                            R = OUT;
                            break;
                        case OUTPUT_MEM_T:
                            *(int64_t*)&(ctx->memory[T]) = OUT;
                            break;
                    }
                    EIP++;
                    break;
                case OP_TYPE_CJMP:
                    SP--;
                    if (!T) break;
                case OP_TYPE_JMP:
                    EIP = ins.jmp.target;
                    break;
                case OP_TYPE_CALL:
                    ctx->RSTACK[RSP] = EIP;
                    EIP = ins.jmp.target;
                    break;
            }
        }
#ifdef DEBUG
        print_stack(SP, T, ctx, false);
        print_stack(RSP, R, ctx, true);
#endif // DEBUG
    }
    ctx->DSTACK[SP-1] = T;
    ctx->RSTACK[RSP-1] = R;
    ctx->SP = SP;
    ctx->RSP = RSP;
    ctx->EIP = EIP;
    return 1;
}