//
// vm_step.h - what one instruction does, if it is a literal or of the ALU.
//
// This is the machine's step, and it is said here once.  vm.c has it in its
// loop and runs it on cells.  util/fuse.h has it too, and runs it on terms:
// the name of what would be in each cell, in place of the number, so that
// what a run of instructions leaves is known for every state at once.
// Nothing in it looks at a value, but through what is named below: it only
// says what goes where.
//
// Whatever has it gives, before it:
//
//   VALUE                  what is in a cell
//   T R IN N OUT           the registers, each a VALUE
//   SP RSP                 the depths, which are numbers
//   ins                    the instruction
//   DS(i) RS(i)            a cell of the data stack and of the return stack
//   NUMBER(n)              the cell that holds a number
//   WHOLE(x)               x as a cell holds it
//   ALU(op, a, b)          what the operation makes of its two inputs
//   FETCH(a)  STORE(a, x)  memory
//   RETURN(r)              go to r
//   IO_IN(port)            what a port reads as
//   IO_OUT()               store OUT to the port T, and go to resolve_dstack
//                          or resolve_rstack
//   DEPTHS()               the depths have just moved
//
// A jump, a conditional jump and a call are not here: they are vm.c's.
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
    // The operation.  Those that are arithmetic are given their two
    // inputs, and it is said here which those are.
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
        case ALU_INVERT:        // of IN alone
            OUT = ALU(ALU_INVERT, IN, IN);
            break;
        case ALU_RSHIFT:        // of IN, by T
        case ALU_LSHIFT:
            OUT = ALU(ins.alu.alu_op, IN, T);
            break;
        default:                // of IN and N
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
