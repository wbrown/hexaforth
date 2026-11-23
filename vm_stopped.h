//
// vm_stopped.h - why the reference VM stopped, and the report printed for
// a machine that stopped where it was not meant to.
//
// vm.c stops a machine at a zero instruction: a halt, or memory that was
// never written.  With STOPS.on set it also stops a machine for these
// reasons:
//
//   ran on        it has run more than STOPS.budget instructions since it
//                 last read a key: it is neither waiting nor halting
//   no code       it is about to run an instruction at an address that was
//                 never written: not by the image, a disk block or a store
//   a break       it is about to run the instruction at STOPS.break_at
//   a store       it has stored to the cell STOPS.break_store
//
// It also keeps the addresses of the last STOP_RING instructions it ran.
// The program driving the machine (main.c, test/nucleus.c, test/deskvm.c)
// chooses which of these it wants.  When the machine stops somewhere
// unexpected, post_mortem() prints the same report whatever the reason:
// the address, both stacks, the instructions run before it, and the name
// of a file with its memory, for `disasm_hex --memory`.
//
// Environment variables set the two breaks and the budget for any of those
// programs: STOP_AT and STOP_ON_STORE, each a hex number, and STOP_BUDGET.
// STOP_PASS is the number of times a break is reached and passed before
// the machine stops at one.  STOP_MEMORY names the file the memory is
// written to when the driving program names none.
//

#ifndef HEXAFORTH_VM_STOPPED_H
#define HEXAFORTH_VM_STOPPED_H

#include "vm.h"

enum { STOP_HALT, STOP_RAN_ON, STOP_NO_CODE, STOP_AT_BREAK, STOP_AT_STORE };

#define STOP_RING 64

typedef struct {
    int on;                     // set if any of these checks is active
    int reason;                 // why the machine last stopped
    int64_t at;                 // the address of the instruction it stopped at, which it has not run
    uint64_t budget;            // the most instructions it may run between two keys; 0 for no limit
    uint64_t since;             // instructions run since it last read a key
    int64_t break_at;           // the address of an instruction to stop before, or -1
    int64_t break_store;        // a cell to stop at after a store to it, or -1
    int stored;                 // set when that store has happened and the machine has not yet stopped for it
    uint64_t pass;              // the number of breaks to reach and pass before stopping at one
    int64_t ring[STOP_RING];    // the addresses of the last instructions run
    uint64_t ran;               // the total number of instructions run
    uint8_t *put;               // one bit per instruction: set if something was written there; NULL if this is not checked
} vm_stops;

extern vm_stops STOPS;

void stops_from_env(void);                  // reads the breaks and the budget from the environment; sets STOPS.on if any is given
void stops_watch_code(context *ctx);        // from now on, stop before running an address that was never written
void stops_put(int64_t cell, int64_t cells);// marks cells as written
const char* stop_reason(void);

// Prints the report of where the machine stopped.  Its memory is written
// to memory_path, or to the file STOP_MEMORY names if memory_path is NULL.
// `image` is the image's path, for the disasm_hex command the report
// suggests.
void post_mortem(FILE *to, context *ctx, const char *image, const char *memory_path);

#endif
