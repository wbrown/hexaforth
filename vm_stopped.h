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
//   end of input  it read a key at the end of its input, and STOPS.at_end
//                 is set (main.c sets it for --typed and --evaluate)
//
// It also keeps the addresses of the last STOP_RING instructions it ran,
// counts how many times it ran the instruction at STOPS.counted_at, and
// keeps the line of input it last read, which is the line it is running
// (stops_say_line).
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
// The trace.  Code turns it on and off by writing to port 0xe0 (MODEL.md):
// `trace-on` and `trace-off` (vm_opcodes.h).  While it is on, vm.c reports
// each instruction that runs in code defined since the image was loaded,
// after the instruction has run, with both stacks as it left them.  That
// code is what was typed or read from a disk: the definitions being
// tried.  A call from it to a word of the image is one report, made when
// the machine is back in such code, and a call to another such definition
// is followed into it.  The trace stays on until `trace-off`, and stops
// after STOPS.trace_most reports in one run.  A report is made by
// STOPS.trace_say, which the driving program sets (main.c); with none set,
// the port does nothing.
//

#ifndef HEXAFORTH_VM_STOPPED_H
#define HEXAFORTH_VM_STOPPED_H

#include "vm.h"

enum { STOP_HALT, STOP_RAN_ON, STOP_NO_CODE, STOP_AT_BREAK, STOP_AT_STORE, STOP_END_OF_INPUT };

#define STOP_RING 64
#define STOP_LINE 160           // the most characters kept of a line of input

#define IO_TRACE 0xe0           // the port whose write switches the trace

#define TRACE_MOST_DEFAULT 500  // the most reports in one run, which TRACE_MOST in the environment overrides

// The default budget, which STOP_BUDGET in the environment overrides.  A
// boot that compiles the system runs about a hundred million instructions
// between two keys, and this is twenty times that.
#define STOP_BUDGET_DEFAULT 2000000000ull

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
    int at_end;                 // set to stop the machine when it reads a key at the end of its input
    int ended;                  // set by that read, and cleared when the machine stops for it
    int64_t counted_at;         // the address of the instruction to count, or -1
    uint64_t counted;           // the number of times the machine has run that instruction
    int64_t ring[STOP_RING];    // the addresses of the last instructions run
    uint64_t ran;               // the total number of instructions run
    uint8_t *put;               // one bit per instruction: set if something was written there; NULL if this is not checked
    long line;                  // the number of the line of input being read or run, from 1; 0 before any input is read
    int line_ended;             // set when the last key read was the end of that line
    int line_len;               // how many characters of that line were read
    char line_text[STOP_LINE];  // those characters
    int mid_line;               // set while the last character the machine sent to the console did not end a line
    int trace;                  // set while the trace is on
    uint8_t *image;             // one bit per instruction: set if the image had an instruction there when it was loaded
    int64_t trace_at;           // the address of the instruction to report once it has run, or -1 for none
    int trace_called;           // set if that instruction is a call, which is reported when the machine is back in traced code
    long trace_said;            // the number of reports made in this run
    long trace_most;            // the most reports to make
    // Reports the instruction at `at`, which has just run and left the
    // stacks `sp` and `rsp` deep, with `t` and `r` on top.
    void (*trace_say)(context *ctx, int64_t at, int sp, int64_t t, int rsp, int64_t r);
} vm_stops;

extern vm_stops STOPS;

void stops_from_env(void);                  // reads the breaks and the budget from the environment; sets STOPS.on if any is given
void stops_watch_code(context *ctx);        // from now on, stop before running an address that was never written
void stops_put(int64_t cell, int64_t cells);// marks cells as written
const char* stop_reason(void);
void stops_say_line(FILE *to);              // prints the number and the text of the line of input the machine is running, if it read any
void stops_mark_image(context *ctx);        // records which instructions are in memory now, as the image's: the trace reports the others

// Prints the report of where the machine stopped.  Its memory is written
// to memory_path, or to the file STOP_MEMORY names if memory_path is NULL.
// `image` is the image's path, for the disasm_hex command the report
// suggests.
void post_mortem(FILE *to, context *ctx, const char *image, const char *memory_path);

#endif
