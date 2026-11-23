//
// vm_step.h - what one instruction does, if it is a literal or an ALU
// instruction.
//
// This is the machine's step function, and it is written only here.  vm.c
// includes it in its loop and runs it on cells.  util/fuse.h includes it
// too and runs it on terms.  A term is a name for the value a cell would
// hold, so stepping on terms gives the result of a run of instructions for
// every starting state at once.  Nothing in this file examines a value
// except through the macros below.  It only says what moves where.
//
// A file that includes this must first define:
//
//   VALUE                  the type of a cell's contents
//   T R IN N OUT           the registers, each a VALUE
//   SP RSP                 the stack depths, which are plain numbers
//   ins                    the instruction
//   DS(i) RS(i)            a cell of the data stack, and of the return stack
//   NUMBER(n)              the VALUE for a number
//   WHOLE(x)               x truncated to a cell
//   ALU(op, a, b)          the result of an operation on two inputs
//   FETCH(a)  STORE(a, x)  memory access
//   RETURN(r)              continue at the address r
//   IO_IN(port)            the value read from a port
//   IO_OUT()               store OUT to the port T, then go to
//                          resolve_dstack or resolve_rstack
//   DEPTHS()               called after the stack depths have changed
//
// Jumps, conditional jumps and calls are not here.  vm.c handles them.
//

if (ins.lit.lit_f) {
    if (ins.lit.lit_shifts == 3) {          // a variable: fetch it, or store T in it
        VALUE addr = NUMBER(VAR_BASE + ins.lit.lit_v);
        if (!ins.lit.lit_add) {
            DS(SP - 1) = T;
            SP++;
            T = FETCH(addr);
        } else {
            STORE(addr, T);
            SP--;
            T = DS(SP - 1);
        }
    } else {
        VALUE lit = NUMBER((uint64_t)ins.lit.lit_v << (ins.lit.lit_shifts * LIT_BITS));
        if (!ins.lit.lit_add) {
            DS(SP - 1) = T;
            SP++;
            T = lit;
        } else {
            T = WHOLE(ALU(ALU_ADD, lit, T));
        }
    }
} else {
    N = DS(SP - 2);
    // Which source is the input `IN`:
    switch (ins.alu.in_mux) {
        case INPUT_N:           // `N->IN`
            IN = N;
            break;
        case INPUT_T:           // `T->IN`
            IN = T;
            break;
        case INPUT_LOAD_T:      // `[T]->IN`
            IN = FETCH(T);
            break;
        case INPUT_R:           // `R->IN`
            IN = R;
            break;
        default:
            break;
    }
    // The operation.  For the arithmetic operations, this chooses the two
    // inputs.
    switch (ins.alu.alu_op) {
        case ALU_IN:            // `IN->OUT`
            OUT = IN;
            break;
        case ALU_T_N:           // `T->N, IN->OUT`
            DS(SP - 2) = T;
            OUT = IN;
            break;
        case ALU_SWAP_IN:       // `T<->N, IN->OUT`
            OUT = DS(SP - 2);
            DS(SP - 2) = T;
            T = OUT;
            OUT = IN;
            break;
        case ALU_LOAD:          // `[IN]->OUT`
            OUT = FETCH(IN);
            break;
        case ALU_IO_READ:       // `io[IN]->OUT`
            OUT = IO_IN(IN);
            break;
        case ALU_INVERT:        // IN only
            OUT = ALU(ALU_INVERT, IN, IN);
            break;
        case ALU_RSHIFT:        // IN, shifted by T
        case ALU_LSHIFT:
            OUT = ALU(ins.alu.alu_op, IN, T);
            break;
        default:                // IN and N
            OUT = ALU(ins.alu.alu_op, IN, N);
            break;
    }
    OUT = WHOLE(OUT);
    // R->EIP flag
    if (ins.alu.r_eip) RETURN(R);
    // Adjust stack depths
    SP += ins.alu.dstack;
    RSP += ins.alu.rstack;
    DEPTHS();
    // Update NOS (Next On Stack) if stack size was incremented
    if (ins.alu.dstack > 0) {
        DS(SP - 2) = T;
    }
    if (ins.alu.rstack > 0) {
        RS(RSP - 2) = R;
    }
    // == Where does `OUT` go?
    switch (ins.alu.out_mux) {
        case OUTPUT_T:          // `OUT->T`
            T = OUT;
            goto resolve_rstack;
        case OUTPUT_R:          // `OUT->R`
            R = OUT;
            if (ins.alu.dstack < 0) {
                T = DS(SP - 1);
            }
            break;
        case OUTPUT_IO_T:       // `OUT->io[T]`
            IO_OUT();
        case OUTPUT_MEM_T:      // `OUT->memory[T]`
            STORE(T, OUT);
        default:
        resolve_dstack:
            if (ins.alu.dstack < 0) {
                T = DS(SP - 1);
            }
        resolve_rstack:
            if (ins.alu.rstack < 0) {
                R = RS(RSP - 1);
            }
            break;
    }
}
