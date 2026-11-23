//
// Runs the nucleus on vm.c and types at its prompt.
//
// The machine is vm.c, with the sizes of the machine the image was built
// for, and the image is the nucleus.  A test starts the machine from the
// image, queues the lines to type at its prompt, and types them: the
// machine runs until it has read the last line.  The test can then examine
// what the lines left on the data stack, what the machine printed, and the
// machine's memory.
//

#ifndef HEXAFORTH_NUCLEUS_H
#define HEXAFORTH_NUCLEUS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "../vm.h"

extern context machine;         // the machine: its memory, its stacks and its disk
extern char* said;              // everything the machine printed the last time it was typed at; valid until the next time

// Sets the image the machine starts from.  Call it once, before anything
// else.
void nucleus_is(const char* path);

// Called when the machine stops without halting (vm_stopped.h: it ran past
// its budget, was about to run an address that was never written, or
// reached a break set in the environment).  The test does not continue
// from there: what the machine printed and where it stopped are printed,
// and the test fails.  A test that has more to report about its machine
// sets this function, which is called first and may end the test itself.
extern void (*stopped_short)(void);

// Starts the machine from its image, with no input queued and no disk.
void start(void);

// Queues a line to type at the machine.  The arguments are as for printf.
void line(const char* format, ...);

// Types the queued lines at the machine's prompt, followed by a line that
// stops the machine, and runs the machine until it stops.
void type(void);

// Types the queued lines at the machine in the state the last run left it
// in.  The machine restarts at its prompt, as it does after anything that
// throws: its stacks are empty, and its memory and dictionary are
// unchanged.
void type_more(void);

// These functions are for a machine that must be stopped in the middle of
// its work, examined, and resumed.  The desktop is one: it stays in its
// event loop and never returns to the prompt.
//
//   going_on()   Call it after start() and before anything else is typed.
//                It defines in the machine what go_on() needs.
//   stopping()   Returns an address.  A word the test types stops the
//                machine by running `stopping() execute`.
//   keys()       Queues bytes for the machine to read that are not a line.
//   set_off()    Runs the machine from its prompt on the queued input,
//                until it stops.
//   go_on()      Resumes the machine where it stopped, on the input queued
//                since, with its stacks and everything else as they were.
//
// vm.c starts a machine with empty stacks.  So go_on() resumes the machine
// through a word that restores the two stack depths through the depth
// ports, as a task does when its turn comes, and then returns to where the
// machine stopped.  That word pushes two numbers before it sets the
// depths, which overwrites the two cells at the bottom of the data stack.
// So a test that resumes its machine has the machine keep two cells it
// does not need at the bottom of that stack.
void going_on(void);
int stopping(void);
void keys(const void* bytes, size_t count);
void set_off(void);
void go_on(void);

// Returns the nth cell from the top of the data stack as the machine left
// it.  The top is 0.
uint64_t left(int n);

// Returns a number truncated to this machine's cell size, which is the
// form left() and fetch() return.
uint64_t as_cell(int64_t x);

// Access to the machine's memory.  in_memory() returns true if `cells`
// cells starting at an address are all inside memory.  cell_at() returns a
// pointer to a cell, fetch() returns its value, and store() sets it.
bool in_memory(uint64_t addr, uint64_t cells);
uint8_t* cell_at(uint64_t addr);
uint64_t fetch(uint64_t addr);
void store(uint64_t addr, uint64_t x);

// Returns true if a pixel of the machine's display is black.
// vm_constants.h gives the display's address and which bit of a cell is
// which pixel.
bool black_at(int x, int y);

#endif //HEXAFORTH_NUCLEUS_H
