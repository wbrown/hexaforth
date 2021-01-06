//
// Created by Wes Brown on 12/31/20.
//

#include <math.h>
#include "vm.h"
#include "vm_constants.h"
#include "vm_debug.h"

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
            fputc((uint8_t)io_write, ctx->OUT);
            return(true);
        case 0xf0: {
            uint8_t msb = clz(io_write) + 1;
            uint8_t num_bytes = (msb / 8 + (msb % 8 ? 1 : 0));
            for(uint8_t idx=0; idx<num_bytes; idx++) {
                io_write = io_write << idx*8;
                fputc((uint8_t)io_write, ctx->OUT);
            }
            return(true);
        }
        default:
            return(false);
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
    register int64_t I = 0;                 // I = input to ALU
    register int64_t OUT = 0;               // OUT - result from ALU

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
            // Pick which data source for our input `I`:
            switch (ins.alu.in_mux) {
                // `N->I`
                case INPUT_N:
                    I = ctx->DSTACK[SP-2];
                    break;
                // `T->I`
                case INPUT_T:
                    I = T;
                    break;
                // `[T]->I`
                case INPUT_LOAD_T:
                    I = *(int64_t*)&(ctx->memory[T]);
                    break;
                // `R->I`
                case INPUT_R:
                    I = R;
                    break;
            }
            switch (ins.alu.op_type) {
                case OP_TYPE_ALU:
                    switch (ins.alu.alu_op) {
                        // `I->OUT` -- passthrough to `out_mux`
                        case ALU_I:
                            OUT = I;
                            break;
                        // `(T+I)->OUT`
                        case ALU_ADD:
                            OUT = T + I;
                            break;
                        // `N->T, I->OUT`
                        case ALU_T_N:
                            ctx->DSTACK[SP-2] = T;
                            OUT = I;
                            break;
                        // `(T&I)->OUT`
                        case ALU_AND:
                            OUT = T & I;
                            break;
                        // `(T|I)>OUT`
                        case ALU_OR:
                            OUT = T | I;
                            break;
                        // `(T^I)->OUT`
                        case ALU_XOR:
                            OUT = T ^ I;
                            break;
                        // `~I->OUT`
                        case ALU_INVERT:
                            OUT = -I;
                            break;
                        // `(T==I)->OUT`
                        case ALU_EQ:
                            OUT = T == I;
                            break;
                        // `(I<T)->OUT`
                        case ALU_GT:
                            OUT = I<T;
                            break;
                        // `(I>>T)->OUT`
                        case ALU_RSHIFT:
                            OUT = (uint64_t)I >> T;
                            break;
                        // `(I<<T)->OUT`
                        case ALU_LSHIFT:
                            OUT = (uint64_t)I << T;
                            break;
                        // `[I]->OUT`
                        case ALU_LOAD:
                            OUT = *(int64_t*)&(ctx->memory[I]);
                            break;
                        // `(I->io[T])->OUT`
                        case ALU_IO_WRITE:
                            OUT = io_write_handler(ctx, T, I);
                            break;
                        // `io[I]->OUT`
                        case ALU_IO_READ:
                            OUT = io_read_handler(ctx, I);
                            break;
                        // `depth(RSTACK|DSTACK)->OUT`
                        case ALU_STATUS:
                            if (ins.alu.in_mux==INPUT_R) {
                                OUT = RSP+1;
                            } else {
                                OUT = SP+1;
                            }
                            break;
                        // `(Tu<Iu)->OUT`
                        case ALU_U_GT:
                            OUT = (uint64_t)T < (uint64_t)I;
                            break;
                        default:
                            break;
                    };
                    // R->EIP flag
                    if (ins.alu.r_eip) EIP = R;
                    // Adjust stack depths
                    SP+=ins.alu.dstack;
                    RSP+=ins.alu.rstack;
                    // Update top of stacks if decrement.
                    if (ins.alu.dstack > 0) {
                        ctx->DSTACK[SP-2] = T;
                    }
                    if (ins.alu.rstack > 0) {
                        ctx->RSTACK[RSP-2] = R;
                    }
                    // Where does `OUT` go?
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
                        case OUTPUT_MEM_T:
                            *(int64_t*)&(ctx->memory[T]) = OUT;
                            break;
                        // Null sink, for ALU ops with side effects.
                        case OUTPUT_NULL:
                            break;
                    }
                    // Next instruction
                    EIP++;
                    break;
                case OP_TYPE_CJMP:
                    SP--;
                    if (!T) break;
                case OP_TYPE_JMP:
                    EIP = ins.jmp.target;
                    break;
                case OP_TYPE_CALL:
                    EIP++;
                    R=EIP;
                    ctx->RSTACK[RSP] = EIP;
                    RSP++;
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