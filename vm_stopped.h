//
// vm_stopped.h - why the reference VM stopped, and what is said of a
// machine that stopped where it was not meant to.
//
// vm.c stops a machine at an instruction that is nothing: a halt, or
// memory nothing was ever put in.  With STOPS.on it also stops one that
//
//   ran on        more than STOPS.budget instructions since it last took a
//                 key: a machine that neither waits nor halts
//   no code       is about to run an instruction where nothing was ever
//                 put: not the image, not a block of a disk, not a store
//   a break       is about to run the instruction STOPS.break_at
//   a store       has stored to the cell STOPS.break_store
//
// and keeps the last STOP_RING instructions it ran.  What drives a machine
// (main.c, test/nucleus.c, test/deskvm.c) says which it wants, and when the
// machine stops where it was not sent, post_mortem says the same things of
// it whatever the reason: where, both stacks, the instructions before, and
// its memory in a file for disasm_hex --memory to name.
//
// The environment sets the two breaks and the budget for any of them:
// STOP_AT and STOP_ON_STORE, each a number in hex, and STOP_BUDGET.
// STOP_PASS is how many times a break is come to and passed before the
// machine stops at one, and STOP_MEMORY the file its memory is written to
// where what drives it names none.
//

#ifndef HEXAFORTH_VM_STOPPED_H
#define HEXAFORTH_VM_STOPPED_H

#include "vm.h"

enum { STOP_HALT, STOP_RAN_ON, STOP_NO_CODE, STOP_AT_BREAK, STOP_AT_STORE };

#define STOP_RING 64

typedef struct {
    int on;                     // whether any of this is looked at
    int reason;                 // why the machine last stopped
    int64_t at;                 // the instruction it stopped at, which it has not run
    uint64_t budget;            // the instructions it may run between two keys, or 0 for as many as it likes
    uint64_t since;             // how many it has run since it last took one
    int64_t break_at;           // an instruction it is to stop before, or -1
    int64_t break_store;        // a cell it is to stop after a store to, or -1
    int stored;                 // a store to that cell has been made, and not yet stopped for
    uint64_t pass;              // the breaks it is to come to and pass before it stops at one
    int64_t ring[STOP_RING];    // the instructions it last ran,
    uint64_t ran;               // and how many it has run in all
    uint8_t *put;               // a bit an instruction: something was put there; NULL if that is not looked at
} vm_stops;

extern vm_stops STOPS;

void stops_from_env(void);                  // the breaks and the budget the environment says, and STOPS.on if any
void stops_watch_code(context *ctx);        // from now, nothing is run where nothing was put
void stops_put(int64_t cell, int64_t cells);// cells were put in memory
const char* stop_reason(void);

// What the machine was at.  Its memory is written to memory_path, or if
// that is NULL to the file STOP_MEMORY says, and image is in the command
// that names what is there.
void post_mortem(FILE *to, context *ctx, const char *image, const char *memory_path);

#endif
