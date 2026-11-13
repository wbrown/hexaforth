//
// Created by Wes Brown on 12/31/20.
//

#include <math.h>
#include <stdbool.h>
#include <string.h>
#include "vm.h"
#include "vm_instruction.h"
#include "vm_constants.h"
#include "util/stack.h"
#ifdef DEBUG
#include "vm_debug.h"
#include "vm_opcodes.h"
#endif // DEBUG

// Cells are held in 64 bits, sign-extended from CELL_BITS.
static inline int64_t cell(int64_t x) {
#if CELL_BITS == 64
    return x;
#else
    return (int64_t)((uint64_t)x << (64 - CELL_BITS)) >> (64 - CELL_BITS);
#endif
}

static inline uint64_t ucell(int64_t x) {
#if CELL_BITS == 64
    return (uint64_t)x;
#else
    return (uint64_t)x & ((1ull << CELL_BITS) - 1);
#endif
}

// -DSTRICT turns on every check the Forth system is meant to pass.
#ifdef STRICT
#define STRICT_ALIGN
#define STRICT_STACK
#endif

// Built with -DSTRICT_ALIGN, a fetch or store outside memory stops the VM.
// The Forth system does not make any.
#ifdef STRICT_ALIGN
static int64_t strict_pc;
static void strict(const char *what, int64_t addr) {
    if (ucell(addr) >> ADDR_BITS) {
        fprintf(stderr, "\n%s outside memory, at %llx, by the instruction at %llx\n", what,
                (unsigned long long)ucell(addr), (unsigned long long)strict_pc - 1);
        exit(3);
    }
}
#else
#define strict(what, addr)
#endif

// Where a cell is.  An address is the number of a cell; one outside memory
// is undefined, and here it wraps.
static inline uint8_t *cell_at(context *ctx, int64_t addr) {
    return (uint8_t *)ctx->memory + (ucell(addr) & ((1ull << ADDR_BITS) - 1)) * CELL_BYTES;
}

static inline int64_t mem_load(context *ctx, int64_t addr) {
    strict("fetch", addr);
    int64_t v = 0;
    memcpy(&v, cell_at(ctx, addr), CELL_BYTES);
    return cell(v);
}

// Built with -DROM_BYTES=n, a store below address n stops the VM: that part
// of the kernel can then be a ROM.  The Forth system keeps every variable
// above the kernel and makes no such store.
static inline void mem_store(context *ctx, int64_t addr, int64_t v) {
    strict("store", addr);
#ifdef ROM_BYTES
    if (ucell(addr) < ROM_BYTES / CELL_BYTES) {
        fprintf(stderr, "\nstore to the kernel at %llx by the instruction at %llx\n",
                (unsigned long long)ucell(addr), (unsigned long long)strict_pc - 1);
        exit(3);
    }
#endif
    memcpy(cell_at(ctx, addr), &v, CELL_BYTES);
}

static inline uint8_t clz(uint64_t N) {
    return N ? 64 - __builtin_clzll(N) : -(uint64_t)INFINITY;
}

static inline uint8_t ctz(uint64_t N) {
    return N ? 64 - __builtin_ctzll(N) : -(uint64_t)INFINITY;
}

// Stack depth ports.  Reading gives the depth of the data or return stack
// as the instruction starts (so `port io@` counts the port number itself).
// Writing sets the depth and changes nothing else: every cell keeps its
// value, whether it is covered or uncovered by the change, so lowering a
// depth and raising it again gives back the same stack.  T and R are only
// copies of the top cells, and are written back before a depth moves.
#define IO_SP  0xd0
#define IO_RSP 0xd1

// Device access ports.  Devices have registers of a fixed width, and `!`
// always moves 64 bits, so these give exact-width access at a latched
// machine address:
//   0xc0  write: latch an address       read: machine address of VM byte 0
//   0xc1, 0xc2, 0xc4, 0xc8  read or write 8, 16, 32 or 64 bits at the latch
// This VM has no devices: its machine address space is just the VM memory,
// and an access outside it reads zero and writes nothing.
#define IO_ADDR 0xc0

static inline int io_width(uint64_t port) {
    int width = (port == 0xc1 || port == 0xc2 || port == 0xc4 || port == 0xc8) ? (int)(port & 0xf) : 0;
    return width > CELL_BYTES ? CELL_BYTES : width;
}

// With 64-bit cells the latch holds a host address, as it holds a physical
// address on bare metal.  A narrower cell cannot hold one, so there the
// latch is an offset into the VM memory and port 0xc0 reads as zero.
static inline uint8_t *io_target(context *ctx, int width) {
    uint8_t *lo = (uint8_t *)ctx->memory;
#if CELL_BITS == 64
    uint8_t *p = (uint8_t *)(intptr_t)ctx->IOADDR;
#else
    uint8_t *p = lo + ucell(ctx->IOADDR);
#endif
    return (p >= lo && p + width <= lo + sizeof(ctx->memory)) ? p : NULL;
}

static inline int64_t io_write_handler(context *ctx, uint64_t io_addr, int64_t io_write) {
    if (io_addr == IO_ADDR) {
        ctx->IOADDR = io_write;
        return(TRUE);
    }
    if (io_width(io_addr)) {
        uint8_t *p = io_target(ctx, io_width(io_addr));
        if (p) memcpy(p, &io_write, io_width(io_addr));
        return(TRUE);
    }
    switch (io_addr) {
        case 0xf1:
            fputc((uint8_t)io_write, ctx->OUT);
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

static inline int64_t io_read_handler(context *ctx, uint64_t io_addr) {
    if (io_addr == IO_ADDR) {
#if CELL_BITS == 64
        return (int64_t)(intptr_t)ctx->memory;
#else
        return 0;
#endif
    }
    if (io_addr == 0xe1) {
        return TRUE;        // key?: reading stdin just blocks until there is one
    }
    if (io_width(io_addr)) {
        uint64_t value = 0;
        uint8_t *p = io_target(ctx, io_width(io_addr));
        if (p) memcpy(&value, p, io_width(io_addr));
        return (int64_t)value;
    }
    switch (io_addr) {
        case 0xe0:
            return(fgetc(ctx->IN));
        default:
            return(false);
    }
}

#ifdef DEBUG
void print_state(context *ctx, int16_t RSP, int16_t SP, int64_t EIP, int64_t R, int64_t T) {
    char disasm[160];
    char rstack_repr[80];
    char dstack_repr[80];
    mini_stack(RSP, R, ctx->RSTACK, rstack_repr);
    mini_stack(SP, T, ctx->DSTACK, dstack_repr);
    debug_address(disasm, ctx, EIP);
    dprintf("EXEC[0x%0.4x]: %s S%-26s R%s\n", (unsigned)EIP, disasm, dstack_repr, rstack_repr);
}
#endif

// Code is paged.  The program counter is a flat word address, and a page is
// 4096 words (8KB).  Page 0 is the kernel.  A 13-bit branch target with bit 12
// clear is an absolute address in the kernel, reachable from anywhere; with
// bit 12 set it is an offset within the page the branch itself is in.  Return
// addresses and execution tokens are full program counters, so `exit` and
// `execute` cross pages with no extra state.
static inline int64_t branch(int64_t next_pc, uint16_t target) {
    if (target & 0x1000) {
        return ((next_pc - 1) & ~(int64_t)0xfff) | (target & 0xfff);
    }
    return target;
}

int vm(context *ctx) {
    register int64_t EIP = ctx->EIP;        // EIP = execution pointer
    register int16_t SP = 0;                // SP = data stack pointer
    register int16_t RSP = 0;               // RSP = return stack pointer
    register int64_t T = ctx->DSTACK[SP];   // T = Top Of Stack / TOS
    register int64_t R = ctx->RSTACK[RSP];  // R = Top Of Return Stack / TOR
    register int64_t IN = 0;                // I = input to ALU
    register int64_t N;                     // N = Next on Stack / NOS
    register int64_t OUT = 0;               // OUT - result from ALU
    register uint64_t cycles = ctx->CYCLES; // how many instructions processed

    for (; ctx->memory[EIP] != 0; ++cycles) {
        // #ifdef DEBUG
        //    show_registers(T, R, EIP, SP, RSP, ctx);
        // #endif // DEBUG
        instruction ins = *(instruction*)&(ctx->memory[EIP]);
        #ifdef DEBUG
        print_state(ctx, RSP, SP, EIP, R, T);
        #endif // DEBUG
        // increment EIP to the next instruction for next cycle
        EIP++;
#ifdef STRICT_ALIGN
        strict_pc = EIP;
#endif
        // == MSB set is an instruction literal.
        if (ins.lit.lit_f) {
            if (ins.lit.lit_shifts == 3) {      // a variable: fetch it, or store T in it
                int64_t addr = VAR_BASE + ins.lit.lit_v;
                if (!ins.lit.lit_add) {
                    ctx->DSTACK[SP-1] = T;
                    SP++;
                    T = mem_load(ctx, addr);
                } else {
                    mem_store(ctx, addr, T);
                    SP--;
                    T = ctx->DSTACK[SP-1];
                }
                continue;
            }
            int64_t lit = (uint64_t)ins.lit.lit_v <<
                              (ins.lit.lit_shifts * LIT_BITS);
            if (!ins.lit.lit_add) {
                ctx->DSTACK[SP-1] = T;
                SP++;
                T = cell(lit);
            } else {
                T = cell(T + lit);
            }
            continue;
        }
        switch (ins.alu.op_type) {
            case OP_TYPE_CJMP: {
                // Conditional jump - jumps if TOS is zero (0branch)
                SP--;
                bool RES=(uint64_t)T;
                T=ctx->DSTACK[SP-1];
                if (!RES) {
                    EIP = branch(EIP, ins.jmp.target);
                }
                break;
            }
            case OP_TYPE_JMP:
                // Unconditional jump
                EIP = branch(EIP, ins.jmp.target);
                break;
            case OP_TYPE_CALL:
                // Unconditional call
                ctx->RSTACK[RSP-1] = R;
                R = EIP;
                ctx->RSTACK[RSP] = EIP;
                RSP++;
                EIP = branch(EIP, ins.jmp.target);
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
                        IN = mem_load(ctx, T);
                        break;
                    }
                    case INPUT_R:
                        // `R->IN`
                        IN = R;
                        break;
                    default:
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
                        OUT = ucell(IN) >> (T & 63);        // from the cell width up, the result is 0
                        break;
                    case ALU_LSHIFT:
                        // `(IN<<T)->OUT`
                        OUT = ucell(IN) << (T & 63);
                        break;
                    case ALU_MUL:
                        // `(N*IN)->OUT`
                        OUT = IN * N;
                        break;
                    case ALU_LOAD: {
                        OUT = mem_load(ctx, IN);
                        break;
                    }
                    case ALU_IO_READ:
                        // `io[IN]->OUT`
                        if (IN == IO_SP) OUT = SP;
                        else if (IN == IO_RSP) OUT = RSP;
                        else OUT = io_read_handler(ctx, IN);
                        break;
                    case ALU_U_GT: {
                        // `(Nu<INu)->OUT`
                        OUT = ucell(N) < ucell(IN) ? TRUE : FALSE;
                        break;
                    }
                    default:
                        break;
                };
                OUT = cell(OUT);
                // R->EIP flag
                if (ins.alu.r_eip) EIP = ucell(R);
                // Adjust stack depths
                SP += ins.alu.dstack;
                RSP += ins.alu.rstack;
#ifdef STRICT_STACK
                if (SP < 0 || SP > STACK_CELLS || RSP < 0 || RSP > STACK_CELLS) {
                    fprintf(stderr, "\nstack out of range (data %d, return %d) at %llx\n",
                            SP, RSP, (unsigned long long)EIP - 1);
                    exit(3);
                }
#endif
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
                        goto resolve_rstack;
                    // `OUT->R`
                    case OUTPUT_R:
                        R = OUT;
                        if (ins.alu.dstack < 0) {
                            T = ctx->DSTACK[SP - 1];
                        }
                        break;
                    // `OUT->io[T]` IO write op.
                    case OUTPUT_IO_T:
                        if (T == IO_SP) {
                            if (ins.alu.dstack >= 0) ctx->DSTACK[SP - 1] = T;
                            SP = OUT;
                            T = ctx->DSTACK[SP - 1];
                            goto resolve_rstack;
                        }
                        if (T == IO_RSP) {
                            if (ins.alu.rstack >= 0) ctx->RSTACK[RSP - 1] = R;
                            RSP = OUT;
                            R = ctx->RSTACK[RSP - 1];
                            goto resolve_dstack;
                        }
                        ctx->SP = SP;
                        ctx->RSP = RSP;
                        ctx->EIP = EIP;
                        io_write_handler(ctx, T, OUT);
                        goto resolve_dstack;
                    // `OUT->memory[T]` -- memory write
                    case OUTPUT_MEM_T:
                        mem_store(ctx, T, OUT);
                    default:
                    resolve_dstack:
                        if (ins.alu.dstack < 0) {
                            T = ctx->DSTACK[SP -1];
                        }
                    resolve_rstack:
                        if (ins.alu.rstack < 0) {
                            R = ctx->RSTACK[RSP - 1];
                        }
                        break;
                }
                break;
            }
            // This should never happen.
            default:
                break;
        }
        // print_stack(SP,T, ctx, false);
    }
#ifdef DEBUG
    print_state(ctx, RSP, SP, EIP, R, T);
#endif
    ctx->CYCLES = cycles;
    ctx->DSTACK[SP-1] = T;
    ctx->RSTACK[RSP-1] = R;
    ctx->SP = SP;
    ctx->RSP = RSP;
    ctx->EIP = EIP;
    fflush(ctx->OUT);
    return 1;
}