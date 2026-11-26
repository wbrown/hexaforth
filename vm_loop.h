//
// vm_loop.h - the machine's interpreter loop and its cell operations, for
// one cell width.
//
// vm.c includes this file once for each cell width, with CELL_IS defined
// as 64, 32 or 16.  In each of those copies the width is a constant, every
// function below has the width added to its name (cell_64, mem_load_64),
// and the loop is vm_64, vm_32 or vm_16.  vm() checks the machine's width
// once and runs the matching loop.  So one program contains all three
// loops, and is not built for a width.
//
// vm.c also includes this file once with CELL_IS undefined.  In that copy
// the width is read where it is used, from the machine the program is run
// for (CELL_BITS, vm_constants.h), the functions keep the names written
// here, and there is no loop.  That copy is for code outside the loop that
// handles cells: the ports, the report of where a machine stopped, and the
// files that include vm.c (util/fuse.h, bench/counting.c).
//
// What does not depend on the width is defined once, in vm.c: the strict
// machine's checks, the ports, and the macros the step function uses
// (vm_step.h).  This file is not valid C on its own.
//

#ifdef CELL_IS
#define WIDTH_(name, bits)  name##_##bits
#define WIDTH(name, bits)   WIDTH_(name, bits)
#define cell(x)                 WIDTH(cell, CELL_IS)(x)
#define ucell(x)                WIDTH(ucell, CELL_IS)(x)
#define strict_at(what, a, far) WIDTH(strict_at, CELL_IS)(what, a, far)
#define cell_at(ctx, a)         WIDTH(cell_at, CELL_IS)(ctx, a)
#define cell_from(at)           WIDTH(cell_from, CELL_IS)(at)
#define cell_to(at, v)          WIDTH(cell_to, CELL_IS)(at, v)
#define mem_load(ctx, a)        WIDTH(mem_load, CELL_IS)(ctx, a)
#define mem_store(ctx, a, v)    WIDTH(mem_store, CELL_IS)(ctx, a, v)
#define alu(op, a, b)           WIDTH(alu, CELL_IS)(op, a, b)
#define vm_loop(ctx)            WIDTH(vm, CELL_IS)(ctx)
#pragma push_macro("CELL_BITS")
#undef CELL_BITS
#define CELL_BITS   CELL_IS
#endif

// Cells are held in 64 bits, sign-extended from CELL_BITS.
static inline int64_t cell(int64_t x) {
    const int spare = 64 - CELL_BITS;
    return (int64_t)((uint64_t)x << spare) >> spare;
}

static inline uint64_t ucell(int64_t x) {
    const int spare = 64 - CELL_BITS;
    return ((uint64_t)x << spare) >> spare;
}

#ifdef STRICT_ALIGN
static void strict_at(const char *what, int64_t addr, int far_too) {
    if (ucell(addr) >> ADDR_BITS && !(far_too && FAR_CELL(addr))) {
        fprintf(stderr, "\n%s outside memory, at %llx, by the instruction at %llx\n", what,
                (unsigned long long)ucell(addr), (unsigned long long)strict_pc - 1);
        exit(3);
    }
}
#endif

// Returns a pointer to the cell at an address.  An address is a cell
// number.  An address outside memory is undefined: here it wraps around,
// unless the machine has an extra cell for it (FAR_CELL).
static inline uint8_t *cell_at(context *ctx, int64_t addr) {
    if (ucell(addr) >> ADDR_BITS) {
        uint8_t *far = FAR_CELL(addr);
        if (far) return far;
    }
    return (uint8_t *)ctx->memory + ((ucell(addr) & ((1ull << ADDR_BITS) - 1)) << BYTE_SHIFT);
}

// Reads the cell at a pointer: CELL_BYTES bytes, lowest byte first.
static inline int64_t cell_from(const uint8_t *at) {
    switch (CELL_BITS) {
        case 64: { int64_t v; memcpy(&v, at, sizeof(v)); return v; }
        case 32: { int32_t v; memcpy(&v, at, sizeof(v)); return v; }
        default: { int16_t v; memcpy(&v, at, sizeof(v)); return v; }
    }
}

static inline void cell_to(uint8_t *at, int64_t v) {
    switch (CELL_BITS) {
        case 64: memcpy(at, &v, sizeof(v)); break;
        case 32: { int32_t low = (int32_t)v; memcpy(at, &low, sizeof(low)); break; }
        default: { int16_t low = (int16_t)v; memcpy(at, &low, sizeof(low)); break; }
    }
}

static inline int64_t mem_load(context *ctx, int64_t addr) {
    strict("fetch", addr);
    return cell_from(cell_at(ctx, addr));
}

static inline void mem_store(context *ctx, int64_t addr, int64_t v) {
    strict("store", addr);
#ifdef ROM_BYTES
    if (ucell(addr) >= ROM_FROM >> BYTE_SHIFT && ucell(addr) < ROM_BYTES >> BYTE_SHIFT) {
        fprintf(stderr, "\nstore to the kernel at %llx by the instruction at %llx\n",
                (unsigned long long)ucell(addr), (unsigned long long)strict_pc - 1);
        exit(3);
    }
#endif
    if (STOPS.on) {
        if (STOPS.put && !(ucell(addr) >> ADDR_BITS)) stops_put((int64_t)ucell(addr), 1);
        if ((int64_t)ucell(addr) == STOPS.break_store) STOPS.stored = 1;
    }
    cell_to(cell_at(ctx, addr), v);
}

// The arithmetic.  Returns the result of an operation on its two inputs,
// and depends on nothing else.  The step function chooses the inputs
// (vm_step.h).
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
        case ALU_RSHIFT: return ucell(a) >> (b & 63);       // the count is modulo 64: from the cell width up to 63 it gives 0
        case ALU_LSHIFT: return ucell(a) << (b & 63);
        default:         return 0;
    }
}

#ifdef CELL_IS
// The interpreter loop.  Runs the machine from its current state until it
// halts or is stopped (vm_stopped.h).
static int vm_loop(context *ctx) {
    register int64_t EIP = ctx->EIP;        // EIP = execution pointer
    register int16_t SP = 0;                // SP = data stack pointer
    register int16_t RSP = 0;               // RSP = return stack pointer
    register int64_t T = ctx->DSTACK[SP];   // T = Top Of Stack / TOS
    register int64_t R = ctx->RSTACK[RSP];  // R = Top Of Return Stack / TOR
    register int64_t IN = 0;                // I = input to ALU
    register int64_t N;                     // N = Next on Stack / NOS
    register int64_t OUT = 0;               // OUT - result from ALU
    register uint64_t cycles = ctx->CYCLES; // how many instructions processed

    STOPS.reason = STOP_HALT;
    for (; ctx->memory[EIP] != 0; ++cycles, strict_run(EIP)) {
#ifdef STRICT_ALIGN
        if (strict_steps && cycles - ctx->CYCLES >= strict_steps) break;
#endif
        if (STOPS.on) {         // the other reasons to stop (vm_stopped.h)
            if (STOPS.ended) {
                STOPS.ended = 0;
                STOPS.reason = STOP_END_OF_INPUT;
                break;
            }
            if (EIP == STOPS.counted_at) STOPS.counted++;
            if (STOPS.stored) {
                STOPS.stored = 0;
                if (!STOPS.pass) { STOPS.reason = STOP_AT_STORE; break; }
                STOPS.pass--;
            }
            if (EIP == STOPS.break_at) {
                if (!STOPS.pass) { STOPS.reason = STOP_AT_BREAK; break; }
                STOPS.pass--;
            }
            if (STOPS.budget && ++STOPS.since > STOPS.budget) { STOPS.reason = STOP_RAN_ON; break; }
            if (STOPS.put && !(STOPS.put[EIP >> 3] & (1 << (EIP & 7)))) { STOPS.reason = STOP_NO_CODE; break; }
            STOPS.ring[STOPS.ran++ % STOP_RING] = EIP;
            if (STOPS.trace) trace_step(ctx, EIP, SP, T, RSP, R);
        }
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
        // A literal (the MSB is set) or an ALU instruction: run the step
        // function.
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
                // Unconditional call.  The return address is truncated to a
                // cell, like everything else on the return stack.
                ctx->RSTACK[RSP-1] = R;
                R = cell(EIP);
                ctx->RSTACK[RSP] = R;
                RSP++;
                EIP = branch(EIP, ins.jmp.target);
                break;
            default:                // an ALU instruction, which the step function handled
                break;
        }
        // print_stack(SP,T, ctx, false);
    }
#ifdef DEBUG
    print_state(ctx, RSP, SP, EIP, R, T);
#endif
    // A halt at an address that was never written means the machine ran
    // into memory that holds no code.
    if (STOPS.on && STOPS.reason == STOP_HALT && STOPS.put && !(STOPS.put[EIP >> 3] & (1 << (EIP & 7)))) {
        STOPS.reason = STOP_NO_CODE;
    }
    STOPS.at = EIP;
    ctx->CYCLES = cycles;
    ctx->DSTACK[SP-1] = T;
    ctx->RSTACK[RSP-1] = R;
    ctx->SP = SP;
    ctx->RSP = RSP;
    ctx->EIP = EIP;
    fflush(ctx->OUT);
    return 1;
}

#pragma pop_macro("CELL_BITS")
#undef cell
#undef ucell
#undef strict_at
#undef cell_at
#undef cell_from
#undef cell_to
#undef mem_load
#undef mem_store
#undef alu
#undef vm_loop
#undef WIDTH
#undef WIDTH_
#endif
