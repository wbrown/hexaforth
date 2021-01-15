//
// Created by Wes Brown on 12/31/20.
//

#include <math.h>
#include <stdbool.h>
#include "vm.h"
#include "vm_opcodes.h"
#include "vm_constants.h"
#ifdef DEBUG
#include "vm_debug.h"
#endif // DEBUG

uint8_t clz(uint64_t N) {
    return N ? 64 - __builtin_clzll(N) : -(uint64_t)INFINITY;
}

uint8_t ctz(uint64_t N) {
    return N ? 64 - __builtin_ctzll(N) : -(uint64_t)INFINITY;
}

int64_t io_write_handler(context *ctx, uint64_t io_addr, int64_t io_write) {
    uint8_t c;
    switch (io_addr) {
        case 0xf1:
            c = (uint8_t)io_write;
            fputc(c, ctx->OUT);
            //fflush(ctx->OUT);
            return(TRUE);
        case 0xf0: {
            uint8_t msb = clz(io_write) + 1;
            uint8_t num_bytes = (msb / 8 + (msb % 8 ? 1 : 0));
            for(uint8_t idx=0; idx<num_bytes; idx++) {
                io_write = io_write << idx*8;
                fputc((uint8_t)io_write, ctx->OUT);
            }
            return(FALSE);
        }
        default:
            return(FALSE);
    }
}

int64_t io_read_handler(context *ctx, uint64_t io_addr) {
    uint8_t c;
    switch (io_addr) {
        case 0xe0:
            return(fgetc(ctx->IN));
        default:
            return(false);
    }
}

int vm(context *ctx) {
    register int16_t EIP = ctx->EIP;        // EIP = execution pointer
    register int16_t SP = 0;                // SP = data stack pointer
    register int16_t RSP = 0;               // RSP = return stack pointer
    register int64_t T = ctx->DSTACK[SP];   // T = Top Of Stakc / TOS
    register int64_t R = ctx->RSTACK[RSP];  // R = Top Of Return Stack / TOR
    register int64_t IN = 0;                 // I = input to ALU
    register int64_t OUT = 0;               // OUT - result from ALU

    for (; ctx->memory[EIP] != 0;) {
        // #ifdef DEBUG
        //    show_registers(T, R, EIP, SP, RSP, ctx);
        // #endif // DEBUG
        instruction ins = *(instruction*)&(ctx->memory[EIP]);
        #ifdef DEBUG
            char out[160];
            debug_address(out, ctx, EIP);
            dprintf("EXEC[0x%0.4x]: %s\n", EIP, out);
        #endif // DEBUG
        // == MSB set is an instruction literal.
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
#ifdef DEBUG
            print_stack(SP, T, ctx, false);
            print_stack(RSP, R, ctx, true);
#endif // DEBUG
            EIP++;
            continue;
        }
        switch (ins.alu.op_type) {
            case OP_TYPE_CJMP:
                // Conditional jump based on TOS value truthiness.
                SP--;
                bool RES=(uint64_t)T;
                T=ctx->DSTACK[SP-1];
                if (RES) {
                    EIP++;
                    break;
                }
            case OP_TYPE_JMP:
                // Unconditional jump
                EIP = ins.jmp.target;
                break;
            case OP_TYPE_CALL:
                // Unconditional call
                ctx->RSTACK[RSP-1] = R;
                R = EIP;
                ctx->RSTACK[RSP] = EIP;
                RSP++;
                EIP = ins.jmp.target;
                break;
            case OP_TYPE_ALU: {
                switch (ins.alu.in_mux) {
                    // Pick which data source for our input `I`:
                    case INPUT_N:
                        // `N->IN`
                        IN = ctx->DSTACK[SP - 2];
                        break;
                    case INPUT_T:
                        // `T->IN`
                        IN = T;
                        break;
                    case INPUT_LOAD_T: {
                        IN = *(uint64_t*)((uint8_t*)(&ctx->memory[0])+T);
                        break;
                    }
                    case INPUT_R:
                        // `R->IN`
                        IN = R;
                        break;
                }
                // Perform our ALU op
                switch (ins.alu.alu_op) {
                    case ALU_IN:
                        // `IN->OUT` -- passthrough to `out_mux`
                        OUT = IN;
                        break;
                    case ALU_ADD:
                        // `(T+IN)->OUT`
                        OUT = T + IN;
                        break;
                    case ALU_T_N:
                        // `N->T, IN->OUT`
                        ctx->DSTACK[SP - 2] = T;
                        OUT = IN;
                        break;
                    case ALU_AND:
                        // `(T&IN)->OUT`
                        OUT = T & IN;
                        break;
                    case ALU_OR:
                        // `(T|IN)>OUT`
                        OUT = T | IN;
                        break;
                    case ALU_XOR:
                        // `(T^IN)->OUT`
                        OUT = T ^ IN;
                        break;
                    case ALU_INVERT:
                        // `~IN->OUT`
                        OUT = ~IN;
                        break;
                    case ALU_EQ:
                        // `(T==IN)->OUT`
                        OUT = T == IN ? TRUE : FALSE;
                        break;
                    case ALU_GT:
                        // `(IN<T)->OUT`
                        OUT = IN < T ? TRUE : FALSE;
                        break;
                    case ALU_RSHIFT:
                        // `(IN>>T)->OUT`
                        OUT = (uint64_t) IN >> T;
                        break;
                    case ALU_LSHIFT:
                        // `(IN<<T)->OUT`
                        OUT = (uint64_t) IN << T;
                        break;
                    case ALU_LOAD: {
                        OUT = *(uint64_t*)((uint8_t*)(&ctx->memory[0])+IN);
                        break;
                    }
                        // `[IN]->OUT`
                        break;
                    case ALU_IO_WRITE:
                        // `(IN->io[T])->OUT`
                        OUT = io_write_handler(ctx, T, IN);
                        break;
                    case ALU_IO_READ:
                        // `io[IN]->OUT`
                        OUT = io_read_handler(ctx, IN);
                        break;
                    case ALU_STATUS:
                        // `depth(RSTACK|DSTACK)->OUT`
                        if (ins.alu.in_mux == INPUT_R) {
                            OUT = RSP;
                        } else {
                            OUT = SP;
                        }
                        break;
                    case ALU_U_GT: {
                        // `(INu<Tu)->OUT`
                        OUT = (uint64_t) IN < (uint64_t) T ? TRUE : FALSE;
                        break;
                    }
                    default:
                        break;
                };
                // R->EIP flag
                if (ins.alu.r_eip) EIP = R;
                // Adjust stack depths
                SP += ins.alu.dstack;
                RSP += ins.alu.rstack;
                // Update NOS (Next On Stack) if stack size was decremented
                if (ins.alu.dstack > 0) {
                    ctx->DSTACK[SP - 2] = T;
                }
                if (ins.alu.rstack > 0) {
                    ctx->RSTACK[RSP - 2] = R;
                }
                // Update R (Top of Return) if return stack size was incremnted.
                if (ins.alu.rstack < 0) {
                    R = ctx->RSTACK[RSP - 1];
                }
                // == Where does `OUT` go?
                switch (ins.alu.out_mux) {
                    // `OUT->T`
                    case OUTPUT_T:
                        T = OUT;
                        break;
                        // `OUT->R`
                    case OUTPUT_R:
                        R = OUT;
                        break;
                        // `OUT->memory[T]` -- memory write
                    case OUTPUT_MEM_T: {
                        *(int64_t*)((uint8_t*)(&ctx->memory[0])+T) = OUT;
                        break;
                    }
                        // Null sink, for ALU ops with side effects.
                    case OUTPUT_NULL:
                        if (ins.alu.dstack < 0) {
                            T = ctx->DSTACK[SP -1];
                        }
                        break;
                }
                // Next instruction
                EIP++;
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