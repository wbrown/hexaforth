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
            return(TRUE);
        }
        case 0xe0: {
            print_stack(ctx->SP-2, ctx->DSTACK[ctx->SP-3], ctx, false);
            print_stack(ctx->RSP-1, ctx->RSTACK[ctx->RSP-2], ctx, true);
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
    register int64_t N = 0;                  // N = Next on Stack / NOS
    register int64_t OUT = 0;               // OUT - result from ALU

    for (; ctx->memory[EIP] != 0;) {
        // #ifdef DEBUG
        //    show_registers(T, R, EIP, SP, RSP, ctx);
        // #endif // DEBUG
        instruction ins = *(instruction*)&(ctx->memory[EIP]);
        #ifdef DEBUG
            char disasm[160];
            char rstack_repr[80];
            char dstack_repr[80];
            mini_stack(RSP, R, ctx->RSTACK, rstack_repr);
            mini_stack(SP, T, ctx->DSTACK, dstack_repr);
            debug_address(disasm, ctx, EIP);
            dprintf("EXEC[0x%0.4x]: %s S%-26s R%s\n", EIP, disasm, dstack_repr, rstack_repr);
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
                N = ctx->DSTACK[SP-2];
                switch (ins.alu.in_mux) {
                    // Pick which data source for our input `I`:
                    case INPUT_N:
                        // `N->IN`
                        IN = N;
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
                        // `(IN+N)->OUT`
                        OUT = IN + N;
                        break;
                    case ALU_T_N:
                        // `T->N, IN->OUT`
                        ctx->DSTACK[SP - 2] = T;
                        OUT = IN;
                        break;
                    case ALU_SWAP_IN:
                        // 'T<->N, IN->OUT'
                        OUT = ctx->DSTACK[SP - 2];
                        ctx->DSTACK[SP - 2] = T;
                        T = OUT;
                        OUT = IN;
                        break;
                    case ALU_AND:
                        // `(IN&N)->OUT`
                        OUT = IN & N;
                        break;
                    case ALU_OR:
                        // `(IN|N)>OUT`
                        OUT = IN | N;
                        break;
                    case ALU_XOR:
                        // `(IN^N)->OUT`
                        OUT = IN ^ N;
                        break;
                    case ALU_INVERT:
                        // `~IN->OUT`
                        OUT = ~IN;
                        break;
                    case ALU_EQ:
                        // `(IN==N)->OUT`
                        OUT = IN == N ? TRUE : FALSE;
                        break;
                    case ALU_GT:
                        // `(N<IN)->OUT`
                        OUT = N < IN ? TRUE : FALSE;
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
                        ctx->SP = SP;
                        ctx->RSP = RSP;
                        ctx->EIP = EIP;
                        OUT = io_write_handler(ctx, T, IN);
                        break;
                    case ALU_IO_READ:
                        // `io[IN]->OUT`
                        OUT = io_read_handler(ctx, IN);
                        break;
                    case ALU_U_GT: {
                        // `(Nu<INu)->OUT`
                        OUT = (uint64_t) N < (uint64_t) IN ? TRUE : FALSE;
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
                // Update NOS (Next On Stack) if stack size was incremented
                if (ins.alu.dstack > 0) {
                    ctx->DSTACK[SP - 2] = T;
                }
                if (ins.alu.rstack > 0) {
                    ctx->RSTACK[RSP - 2] = R;
                }
                // == Where does `OUT` go?
                switch (ins.alu.out_mux) {
                    // `OUT->T`
                    case OUTPUT_T:
                        T = OUT;
                        if (ins.alu.rstack < 0) {
                            R = ctx->RSTACK[RSP - 1];
                        }
                        break;
                        // `OUT->R`
                    case OUTPUT_R:
                        R = OUT;
                        if (ins.alu.dstack < 0) {
                            T = ctx->DSTACK[SP - 1];
                        }
                        break;
                        // `OUT->memory[T]` -- memory write
                    case OUTPUT_N:
                        ctx->DSTACK[SP-2] = OUT;
                        T = ctx->DSTACK[SP-1];
                        if (ins.alu.rstack < 0) {
                            R = ctx->RSTACK[RSP - 1];
                        }
                        break;
                    case OUTPUT_MEM_T:
                        *(int64_t*)((uint8_t*)(&ctx->memory[0])+T) = OUT;
                    default:
                        if (ins.alu.dstack < 0) {
                            T = ctx->DSTACK[SP -1];
                        }
                        if (ins.alu.rstack < 0) {
                            R = ctx->RSTACK[RSP - 1];
                        }
                        break;
                }
                // Next instruction
                EIP++;
                break;
            }
        }
        // print_stack(SP,T, ctx, false);
    }
    ctx->DSTACK[SP-1] = T;
    ctx->RSTACK[RSP-1] = R;
    ctx->SP = SP;
    ctx->RSP = RSP;
    ctx->EIP = EIP;
    return 1;
}