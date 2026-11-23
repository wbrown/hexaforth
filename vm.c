//
// Created by Wes Brown on 12/31/20.
//

#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "vm.h"
#include "vm_stopped.h"
#include "vm_instruction.h"
#include "vm_constants.h"
#include "util/stack.h"

vm_stops STOPS = { .break_at = -1, .break_store = -1 };

// Marks `cells` cells starting at `cell` as written to, so that an
// instruction in them may be run (STOPS.put).
void stops_put(int64_t cell, int64_t cells) {
    if (!STOPS.put) return;
    uint64_t from = (uint64_t)cell * (CELL_BYTES / 2), slots = (uint64_t)cells * (CELL_BYTES / 2);
    for(uint64_t at = from; at < from + slots && at < MEM_BYTES / 2; at++) STOPS.put[at >> 3] |= (uint8_t)(1 << (at & 7));
}
#ifdef DEBUG
#include "vm_debug.h"
#include "vm_opcodes.h"
#endif // DEBUG

// -DSTRICT turns on every check the Forth system is meant to pass.
#ifdef STRICT
#define STRICT_ALIGN
#define STRICT_STACK
#endif

// Built with -DSTRICT_ALIGN, the VM stops at a fetch or a store outside
// memory.  The Forth system never makes one.  The VM also stops before it
// executes an instruction outside memory: a jump, a call or a return to an
// address that is in no cell is caught before the VM reads there.
//
// A VM built this way can also be limited to a number of instructions
// (strict_steps; 0 means run until it halts), so that it runs exactly the
// instructions it was given and not whatever follows them.
//
// A machine can be given cells that are outside its memory.  FAR_CELL
// returns a pointer to the cell at an address, or NULL if there is none.
// util/fuse.h uses this: it runs short sequences on a small machine, and
// gives it the cells those sequences refer to, wherever they are.  Such a
// cell can be fetched from and stored to, but nothing is executed from it.
#ifndef FAR_CELL
#define FAR_CELL(addr) ((uint8_t *)0)
#endif

// RUNS(at) is called just before the instruction at `at` is executed.
// bench/counting.c defines it to count the machine's work.  Here it does
// nothing.
#ifndef RUNS
#define RUNS(at)
#endif

// STORES(addr, x) is called just before an instruction stores to a cell,
// and FILLS(addr, cells) after cells have been read in from a disk.
// bench/heap.c defines them to watch the machine's memory.  Here they do
// nothing.
#ifndef STORES
#define STORES(addr, x)
#endif
#ifndef FILLS
#define FILLS(addr, cells)
#endif
#ifdef STRICT_ALIGN
static int64_t strict_pc;
static uint64_t strict_steps;
#define strict(what, addr) strict_at(what, addr, 1)
#define strict_run(pc) strict_at("execution", (int64_t)((uint64_t)(pc) >> (BYTE_SHIFT - 1)), 0)
#else
#define strict(what, addr)
#define strict_run(pc) ((void)0)
#endif

// Built with -DROM_BYTES=n, the VM stops at a store below byte n, which
// shows that the first n bytes of the kernel can be a ROM.  The Forth
// system keeps every variable above the kernel and never stores there.
// The ROM starts at byte 0 unless ROM_FROM gives another starting byte.
#ifndef ROM_FROM
#define ROM_FROM 0
#endif

// The cell operations (vm_loop.h), included here with no fixed width: they
// take the cell width from the machine the program is run for.  The rest
// of this file, and any file that includes it, uses these.
#include "vm_loop.h"

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
    uint8_t *p = CELL_BITS == 64 ? (uint8_t *)(intptr_t)ctx->IOADDR : lo + ucell(ctx->IOADDR);
    return (p >= lo && p + width <= lo + MEM_BYTES) ? p : NULL;
}

// Disks.  This VM can be given files to use as disks (main.c).  A disk is
// blocks of 1,024 bytes, and its controller is reached through ports, like
// any device:
//   0xb0  write: a block number, which also selects the disk: the top four
//         of its sixteen bits are the disk, and the rest are the block on
//         that disk (vm_constants.h)
//         read: the number of blocks on the selected disk
//   0xb1  write: the address of a cell.  The block is read into the 1,024
//         bytes starting there.  Bytes past the end of the file are zero,
//         and the whole block is zero if there is no such disk
//   0xb2  write: the address of a cell.  The block is written from the
//         1,024 bytes starting there
// Only this VM has this controller.  On a machine without it the ports
// read zero, which means a disk with no blocks, and writes do nothing.
#define IO_DISK_BLOCK 0xb0
#define IO_DISK_READ  0xb1
#define IO_DISK_WRITE 0xb2

static inline void disk_io(context *ctx, int64_t addr, int write) {
    uint64_t at = ucell(addr) * CELL_BYTES;
    if (at + DISK_BLOCK > MEM_BYTES) return;
    FILE *disk = ctx->DISKS[DISK_DRIVE_OF(ucell(ctx->DISKBLK))];
    uint8_t *p = (uint8_t *)ctx->memory + at;
    if (disk) fseek(disk, (long)DISK_BLOCK_OF(ucell(ctx->DISKBLK)) * DISK_BLOCK, SEEK_SET);
    if (write) {
        if (!disk) return;
        fwrite(p, 1, DISK_BLOCK, disk);
        fflush(disk);
    } else {
        size_t n = disk ? fread(p, 1, DISK_BLOCK, disk) : 0;
        memset(p + n, 0, DISK_BLOCK - n);
        stops_put((int64_t)ucell(addr), DISK_BLOCK / CELL_BYTES);
        FILLS(ucell(addr), DISK_BLOCK / CELL_BYTES);
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
        return CELL_BITS == 64 ? (int64_t)(intptr_t)ctx->memory : 0;
    }
    if (io_addr == 0xe1) {
        return TRUE;        // key?: reading stdin just blocks until there is one
    }
    if (io_addr == IO_DISK_BLOCK) {
        int drive = DISK_DRIVE_OF(ucell(ctx->DISKBLK));
        return ctx->DISKS[drive] ? ctx->DISKBLOCKS[drive] : 0;
    }
    if (io_width(io_addr)) {
        uint64_t value = 0;
        uint8_t *p = io_target(ctx, io_width(io_addr));
        if (p) memcpy(&value, p, io_width(io_addr));
        return (int64_t)value;
    }
    switch (io_addr) {
        case 0xe0:
            fflush(ctx->OUT);       // show what was printed before waiting for a key
            STOPS.since = 0;        // it took a key, so it is not running away: restart the count
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

// Facts about the arithmetic (alu, vm_loop.h) that hold for every input.
// util/fuse.h reasons from these without running the arithmetic, and also
// tests each one against it:
//
//   ALU_EITHER_WAY   the operation is commutative
//   ALU_REGROUPS     it is also associative
//   ALU_NOTHING      the identity element of an associative operation: the
//                    number that, as one input, gives the other input back
//   ALU_UNDOES       applied twice, the operation gives its input back
//   ALU_FLAGS        the result is a flag: TRUE or FALSE, and no other
//                    value
//   ALU_SAME         the flag says whether the two inputs are equal
//   ALU_DIFFERS      the result is 0 exactly when the two inputs are equal
//   ALU_KEEPS        the result is the input when both inputs are the same
//                    cell
//   ALU_ORDERS       the flag says whether one input is less than the
//                    other, which is never true of a cell and itself
#define ALU_REGROUPS(op)    ((op) == ALU_ADD || (op) == ALU_AND || (op) == ALU_OR || (op) == ALU_XOR || (op) == ALU_MUL)
#define ALU_NOTHING(op)     ((op) == ALU_AND ? TRUE : (op) == ALU_MUL ? 1 : 0)
#define ALU_EITHER_WAY(op)  (ALU_REGROUPS(op) || (op) == ALU_EQ)
#define ALU_UNDOES(op)      ((op) == ALU_INVERT)
#define ALU_FLAGS(op)       ((op) == ALU_EQ || (op) == ALU_GT || (op) == ALU_U_GT)
#define ALU_SAME(op)        ((op) == ALU_EQ)
#define ALU_DIFFERS(op)     ((op) == ALU_XOR)
#define ALU_KEEPS(op)       ((op) == ALU_AND || (op) == ALU_OR)
#define ALU_ORDERS(op)      ((op) == ALU_GT || (op) == ALU_U_GT)

// The step function on cells.  vm_step.h describes each of these macros.
#define VALUE           int64_t
#define DS(i)           ctx->DSTACK[i]
#define RS(i)           ctx->RSTACK[i]
#define NUMBER(n)       cell(n)
#define WHOLE(x)        cell(x)
#define ALU(op, a, b)   alu(op, a, b)
#define FETCH(a)        mem_load(ctx, a)
#define STORE(a, x)     do { STORES(a, x); mem_store(ctx, a, x); } while (0)
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
        /* print the return stack, top first: the addresses it would have returned to */ \
        if (RSP > 0 && RSP <= STACK_CELLS) { \
            fprintf(stderr, "the return stack: %llx", (unsigned long long)R); \
            for(int at = RSP - 2; at >= 0; at--) fprintf(stderr, " %llx", (unsigned long long)ctx->RSTACK[at]); \
            fprintf(stderr, "\n"); \
        } \
        exit(3); \
    }
#else
#define DEPTHS()
#endif

// The interpreter loop is compiled once for each cell width, with the
// width as a constant (vm_loop.h): vm_64, vm_32 and vm_16.
#define CELL_IS 64
#include "vm_loop.h"
#undef CELL_IS
#define CELL_IS 32
#include "vm_loop.h"
#undef CELL_IS
#define CELL_IS 16
#include "vm_loop.h"
#undef CELL_IS

// Runs the machine with the loop for its cell width.  The width is checked
// once, here.
int vm(context *ctx) {
    switch (CELL_BITS) {
        case 64: return vm_64(ctx);
        case 32: return vm_32(ctx);
        default: return vm_16(ctx);
    }
}

void stops_from_env(void) {
    const char* at = getenv("STOP_AT");
    const char* store = getenv("STOP_ON_STORE");
    const char* budget = getenv("STOP_BUDGET");
    const char* pass = getenv("STOP_PASS");
    if (at) STOPS.break_at = (int64_t)strtoull(at, NULL, 16), STOPS.on = 1;
    if (store) STOPS.break_store = (int64_t)strtoull(store, NULL, 16), STOPS.on = 1;
    if (budget) STOPS.budget = strtoull(budget, NULL, 10), STOPS.on = 1;
    if (pass) STOPS.pass = strtoull(pass, NULL, 10);
}

// Marks everything now in memory as code that was loaded: the image.  A
// halt is the instruction 0, so a zero instruction next to a non-zero one
// is marked as loaded too.
void stops_watch_code(context *ctx) {
    const long slots = MEM_BYTES / 2;
    if (!STOPS.put) STOPS.put = calloc((size_t)slots / 8 + 1, 1);
    if (!STOPS.put) return;
    for(long at = 0; at < slots; at++) {
        if (ctx->memory[at] || (at && ctx->memory[at - 1]) || (at + 1 < slots && ctx->memory[at + 1])) {
            STOPS.put[at >> 3] |= (uint8_t)(1 << (at & 7));
        }
    }
    STOPS.on = 1;
}

const char* stop_reason(void) {
    switch (STOPS.reason) {
        case STOP_RAN_ON:   return("it ran on, taking no key and not halting");
        case STOP_NO_CODE:  return("it ran where nothing was ever put");
        case STOP_AT_BREAK: return("it came to the instruction STOP_AT says");
        case STOP_AT_STORE: return("it stored to the cell STOP_ON_STORE says");
        default:            return("it halted");
    }
}

void post_mortem(FILE *to, context *ctx, const char *image, const char *memory_path) {
    fprintf(to, "the machine stopped: %s, at %llx, which is %04x\n", stop_reason(),
            (unsigned long long)ctx->EIP, (unsigned)ctx->memory[ctx->EIP]);
    fprintf(to, "  the data stack, %d deep, the top first:", ctx->SP);
    for(int at = ctx->SP - 1; at >= 0 && at >= ctx->SP - 8; at--) {
        if (at < STACK_CELLS) fprintf(to, " %llx", (unsigned long long)ucell(ctx->DSTACK[at]));
    }
    fprintf(to, "\n  the return stack, %d deep, the top first:", ctx->RSP);
    for(int at = ctx->RSP - 1; at >= 0; at--) {
        if (at < STACK_CELLS) fprintf(to, " %llx", (unsigned long long)ucell(ctx->RSTACK[at]));
    }
    fprintf(to, "\n");
    if (STOPS.on && STOPS.ran) {
        fprintf(to, "  the instructions it ran before that, the last first:");
        uint64_t kept = STOPS.ran < STOP_RING ? STOPS.ran : STOP_RING;
        for(uint64_t back = 1; back <= kept; back++) {
            if (back % 16 == 1) fprintf(to, "\n   ");
            fprintf(to, " %llx", (unsigned long long)STOPS.ring[(STOPS.ran - back) % STOP_RING]);
        }
        fprintf(to, "\n");
    }
    if (!memory_path) memory_path = getenv("STOP_MEMORY");
    if (!memory_path) return;
    FILE* memory = fopen(memory_path, "wb");
    if (!memory) {
        perror(memory_path);
        return;
    }
    fwrite(ctx->memory, 1, MEM_BYTES, memory);
    fclose(memory);
    fprintf(to, "  its memory is in %s; a place is named with\n"
                "    build/cmake/disasm_hex %s --memory %s PLACE PLACE\n",
            memory_path, image ? image : "IMAGE.hex", memory_path);
}

// A file that includes the step function again, on other values, defines
// these again.
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