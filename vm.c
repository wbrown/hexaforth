//
// Created by Wes Brown on 12/31/20.
//

#include "vm.h"
#include "vm_constants.h"
#include "vm_debug.h"

int vm(context *ctx) {
    register int16_t EIP = 0;
    register int16_t SP = 0;
    register int16_t RSP = 0;
    register int16_t IO = 0;
    register int64_t T = ctx->DSTACK[SP];
    register int64_t N = ctx->DSTACK[SP-1];
    register int64_t R = ctx->RSTACK[RSP];

    for (; ctx->memory[EIP] != 0;) {
        #ifdef DEBUG
            show_registers(T, N, R, EIP, SP, RSP, ctx);
        #endif // DEBUG
        instruction ins = *(instruction*)&ctx->memory[EIP];
        #ifdef DEBUG
            printf("MEMORY[%d]: ", EIP);
            debug_instruction(ins);
        #endif // DEBUG
        if (ins.lit.lit_f) {
            int64_t lit = (uint64_t)ins.lit.lit_v <<
                              (ins.lit.lit_shifts * LIT_BITS);
            if (!ins.lit.lit_add) {
                if (SP) {
                    N = T;
                    ctx->DSTACK[SP-1] = T;
                }
                SP++;
                T = (int64_t)lit;
            } else {
                T += (int64_t)lit;
            }
            EIP++;
        } else {
            int64_t TMP;
            switch (ins.alu.op_type) {
                case OP_TYPE_ALU:
                    switch (ins.alu.alu_op) {
                        case ALU_ADD:
                            TMP = T + N;
                            break;
                        case ALU_T:
                            TMP = T;
                            break;
                        case ALU_N:
                            TMP = N;
                            break;
                        case ALU_AND:
                            TMP = T & N;
                            break;
                        case ALU_OR:
                            TMP = T | N;
                            break;
                        case ALU_XOR:
                            TMP = T ^ N;
                            break;
                        case ALU_INVERT:
                            TMP = -T;
                            break;
                        case ALU_EQ:
                            TMP = T == N;
                            break;
                        case ALU_GT:
                            TMP = T < N;
                            break;
                        case ALU_RSHIFT:
                            TMP = N >> T;
                            break;
                        case ALU_LSHIFT:
                            TMP = N << T;
                            break;
                        case ALU_R_T:
                            TMP = R;
                            break;
                        case ALU_LOAD_T:
                            TMP = ctx->memory[T];
                            break;
                        case ALU_IO_T:
                            TMP = IO;
                            break;
                        case ALU_STATUS:
                            TMP = SP+1;
                            break;
                        case ALU_U_GT:
                            TMP = (uint64_t)T < (uint64_t)N;
                            break;
                        default:
                            break;
                    };
                    switch (ins.alu.mem_op) {
                        case MEM_T:
                            T = TMP;
                        case MEM_T_N:
                            N = T;
                            T = TMP;
                            break;
                        case MEM_T_R:
                            R = T;
                            T = TMP;
                            break;
                        case MEM_STORE_N_T:
                            ctx->memory[T] = N;
                            break;
                        case MEM_IO_RD:
                            switch (T) {
                                case UART_D:
                                    IO = getchar();
                                default:
                                    dprintf("Unsupported IO address! %lld", T);
                            }
                            break;
                        case MEM_R_EIP:
                            EIP = R;
                            break;
                        default:
                            EIP = EIP;
                    }
                    SP+=ins.alu.dstack;
                    RSP+=ins.alu.rstack;
                    if (ins.alu.dstack < 0) {
                        N = ctx->DSTACK[SP - 2];
                        // ctx->DSTACK[SP - 1] = T;
                    } else if (ins.alu.dstack) {
                        ctx->DSTACK[SP-2] = N;
                        ctx->DSTACK[SP-1] = T;
                    }
                    if (ins.alu.rstack) {
                        ctx->RSTACK[SP] = R;
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
        print_stack(SP, T, N, ctx);
#endif // DEBUG
    }
    return 1;
}