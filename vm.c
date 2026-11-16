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
// The Forth system does not make any.  Nor does it run what is outside
// memory, and that stops the VM too: an instruction is in the cell it is
// in, so a jump, a call or a return to a place that is in no cell is seen
// before the VM looks there.
//
// Such a VM can also be told to run so many instructions and no more
// (strict_steps; none is until it halts), so that what is run is what was
// meant to be and not what happens to come after it.
//
// A machine may be given cells that are not in its memory: FAR_CELL says
// where the cell of an address is, or that it has none.  What runs a small
// machine to try what a run of instructions does gives it the cells the
// run names, wherever they are (util/fuse.c).  Such a cell is fetched from
// and stored to; nothing is run from it.
#ifndef FAR_CELL
#define FAR_CELL(addr) ((uint8_t *)0)
#endif

// What counts a machine's work is told that the instruction at a place is
// about to be run (bench/counting.c).  Here that is nothing.
#ifndef RUNS
#define RUNS(at)
#endif
#ifdef STRICT_ALIGN
static int64_t strict_pc;
static uint64_t strict_steps;
static void strict_at(const char *what, int64_t addr, int far_too) {
    if (ucell(addr) >> ADDR_BITS && !(far_too && FAR_CELL(addr))) {
        fprintf(stderr, "\n%s outside memory, at %llx, by the instruction at %llx\n", what,
                (unsigned long long)ucell(addr), (unsigned long long)strict_pc - 1);
        exit(3);
    }
}
#define strict(what, addr) strict_at(what, addr, 1)
#define strict_run(pc) strict_at("execution", (int64_t)((uint64_t)(pc) / (CELL_BYTES / 2)), 0)
#else
#define strict(what, addr)
#define strict_run(pc) ((void)0)
#endif

// Where a cell is.  An address is the number of a cell; one outside memory
// is undefined, and here it wraps, unless the machine has a cell for it.
static inline uint8_t *cell_at(context *ctx, int64_t addr) {
    if (ucell(addr) >> ADDR_BITS) {
        uint8_t *far = FAR_CELL(addr);
        if (far) return far;
    }
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
// above the kernel and makes no such store.  The ROM starts at the start
// of memory unless ROM_FROM says where, in bytes.
#ifndef ROM_FROM
#define ROM_FROM 0
#endif
static inline void mem_store(context *ctx, int64_t addr, int64_t v) {
    strict("store", addr);
#ifdef ROM_BYTES
    if (ucell(addr) >= ROM_FROM / CELL_BYTES && ucell(addr) < ROM_BYTES / CELL_BYTES) {
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

// A disk.  This VM can be given a file to be one (main.c), of blocks of
// 1,024 bytes, and it is reached as any device is, through ports:
//   0xb0  write: a block's number           read: how many blocks there are
//   0xb1  write: the address of a cell; the block is read into the 1,024
//         bytes from it, with nothing where the file has ended
//   0xb2  write: the address of a cell; the block is written from them
// Only this VM has it.  On a machine without, the ports read zero, which
// is a disk of no blocks, and writing them does nothing.
#define IO_DISK_BLOCK 0xb0
#define IO_DISK_READ  0xb1
#define IO_DISK_WRITE 0xb2

static inline void disk_io(context *ctx, int64_t addr, int write) {
    uint64_t at = ucell(addr) * CELL_BYTES;
    if (!ctx->DISK || at + DISK_BLOCK > sizeof(ctx->memory)) return;
    uint8_t *p = (uint8_t *)ctx->memory + at;
    fseek(ctx->DISK, (long)(ucell(ctx->DISKBLK) * DISK_BLOCK), SEEK_SET);
    if (write) {
        fwrite(p, 1, DISK_BLOCK, ctx->DISK);
        fflush(ctx->DISK);
    } else {
        size_t n = fread(p, 1, DISK_BLOCK, ctx->DISK);
        memset(p + n, 0, DISK_BLOCK - n);
    }
}

static inline int64_t io_write_handler(context *ctx, uint64_t io_addr, int64_t io_write) {
    if (io_addr == IO_ADDR) {
        ctx->IOADDR = io_write;
        return(TRUE);
    }
    if (io_addr == IO_DISK_BLOCK) {
        ctx->DISKBLK = io_write;
        return(TRUE);
    }
    if (io_addr == IO_DISK_READ || io_addr == IO_DISK_WRITE) {
        disk_io(ctx, io_write, io_addr == IO_DISK_WRITE);
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
    if (io_addr == IO_DISK_BLOCK) {
        return ctx->DISK ? ctx->DISKBLOCKS : 0;
    }
    if (io_width(io_addr)) {
        uint64_t value = 0;
        uint8_t *p = io_target(ctx, io_width(io_addr));
        if (p) memcpy(&value, p, io_width(io_addr));
        return (int64_t)value;
    }
    switch (io_addr) {
        case 0xe0:
            fflush(ctx->OUT);       // what was printed is seen before a key is waited for
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

// The arithmetic: what an operation makes of its two inputs, and of nothing
// else.  Which two they are is the step's to say (vm_step.h).
static inline int64_t alu(int op, int64_t a, int64_t b) {
    switch (op) {
        case ALU_ADD:    return a + b;
        case ALU_AND:    return a & b;
        case ALU_OR:     return a | b;
        case ALU_XOR:    return a ^ b;
        case ALU_MUL:    return a * b;
        case ALU_INVERT: return ~a;
        case ALU_EQ:     return a == b ? TRUE : FALSE;
        case ALU_GT:     return b < a ? TRUE : FALSE;
        case ALU_U_GT:   return ucell(b) < ucell(a) ? TRUE : FALSE;
        case ALU_RSHIFT: return ucell(a) >> (b & 63);       // from the cell width up, the result is 0
        case ALU_LSHIFT: return ucell(a) << (b & 63);
        default:         return 0;
    }
}

// What is so of the arithmetic whatever its inputs, for what reasons about
// it and does not run it (util/fuse.c, which also tries each on the
// machine): the operations whose two inputs may be exchanged; those of
// which a run may be regrouped as well, since the cell they make is the
// same, and for each of those the number that, as one input, leaves the
// other a cell as it was; the operation that done twice gives back the
// cell it was given; those that leave a flag, which is TRUE or FALSE and
// nothing else; the one whose flag is whether two cells are the same; and
// the one that makes nothing of two cells when they are the same, and of
// no others.
#define ALU_REGROUPS(op)    ((op) == ALU_ADD || (op) == ALU_AND || (op) == ALU_OR || (op) == ALU_XOR || (op) == ALU_MUL)
#define ALU_NOTHING(op)     ((op) == ALU_AND ? TRUE : (op) == ALU_MUL ? 1 : 0)
#define ALU_EITHER_WAY(op)  (ALU_REGROUPS(op) || (op) == ALU_EQ)
#define ALU_UNDOES(op)      ((op) == ALU_INVERT)
#define ALU_FLAGS(op)       ((op) == ALU_EQ || (op) == ALU_GT || (op) == ALU_U_GT)
#define ALU_SAME(op)        ((op) == ALU_EQ)
#define ALU_DIFFERS(op)     ((op) == ALU_XOR)

// The step on cells (vm_step.h says what each of these is).
#define VALUE           int64_t
#define DS(i)           ctx->DSTACK[i]
#define RS(i)           ctx->RSTACK[i]
#define NUMBER(n)       cell(n)
#define WHOLE(x)        cell(x)
#define ALU(op, a, b)   alu(op, a, b)
#define FETCH(a)        mem_load(ctx, a)
#define STORE(a, x)     mem_store(ctx, a, x)
#define RETURN(r)       EIP = ucell(r)
#define IO_IN(port)     ((port) == IO_SP ? SP : (port) == IO_RSP ? RSP : io_read_handler(ctx, port))
#define IO_OUT() \
    if (T == IO_SP) { \
        if (ins.alu.dstack >= 0) ctx->DSTACK[SP - 1] = T; \
        SP = OUT; \
        T = ctx->DSTACK[SP - 1]; \
        goto resolve_rstack; \
    } \
    if (T == IO_RSP) { \
        if (ins.alu.rstack >= 0) ctx->RSTACK[RSP - 1] = R; \
        RSP = OUT; \
        R = ctx->RSTACK[RSP - 1]; \
        goto resolve_dstack; \
    } \
    ctx->SP = SP; \
    ctx->RSP = RSP; \
    ctx->EIP = EIP; \
    io_write_handler(ctx, T, OUT); \
    goto resolve_dstack
#ifdef STRICT_STACK
#define DEPTHS() \
    if (SP < 0 || SP > STACK_CELLS || RSP < 0 || RSP > STACK_CELLS) { \
        fprintf(stderr, "\nstack out of range (data %d, return %d) at %llx\n", \
                SP, RSP, (unsigned long long)EIP - 1); \
        exit(3); \
    }
#else
#define DEPTHS()
#endif

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

    for (; ctx->memory[EIP] != 0; ++cycles, strict_run(EIP)) {
#ifdef STRICT_ALIGN
        if (strict_steps && cycles - ctx->CYCLES >= strict_steps) break;
#endif
        // #ifdef DEBUG
        //    show_registers(T, R, EIP, SP, RSP, ctx);
        // #endif // DEBUG
        instruction ins = *(instruction*)&(ctx->memory[EIP]);
        RUNS(EIP);
        #ifdef DEBUG
        print_state(ctx, RSP, SP, EIP, R, T);
        #endif // DEBUG
        // increment EIP to the next instruction for next cycle
        EIP++;
#ifdef STRICT_ALIGN
        strict_pc = EIP;
#endif
        // A literal (the MSB is set) or an instruction of the ALU: the step.
        if (ins.lit.lit_f || ins.alu.op_type == OP_TYPE_ALU) {
#include "vm_step.h"
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
                // Unconditional call.  The return address is a cell on the
                // return stack, as everything else there is.
                ctx->RSTACK[RSP-1] = R;
                R = cell(EIP);
                ctx->RSTACK[RSP] = R;
                RSP++;
                EIP = branch(EIP, ins.jmp.target);
                break;
            default:                // of the ALU, which the step did
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

// What has the step again, on something else, says these again.
#undef VALUE
#undef DS
#undef RS
#undef NUMBER
#undef WHOLE
#undef ALU
#undef FETCH
#undef STORE
#undef RETURN
#undef IO_IN
#undef IO_OUT
#undef DEPTHS