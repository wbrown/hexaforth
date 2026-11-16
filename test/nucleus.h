//
// The nucleus on vm.c, typed at.
//
// The machine is vm.c, which is built into a test for the size of machine
// the image is for, and the image is the nucleus.  A test starts the
// machine from the image, says the lines to type at its prompt, and types
// them: the machine runs until it has read the last.  What the lines left
// on the data stack, what the machine printed and its memory are then
// there to be looked at.
//

#ifndef HEXAFORTH_NUCLEUS_H
#define HEXAFORTH_NUCLEUS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "../vm.h"

extern context machine;         // the machine: it has the memory, the stacks and the disk
extern char said[1 << 16];      // what it printed when it was last typed at

// The image the machine is started from.  This is said once, first.
void nucleus_is(const char* path);

// Start the machine from its image, with nothing to type and no disk.
void start(void);

// A line to type at the machine.
void line(const char* format, ...);

// Type the lines at the machine's prompt and run it until it stops, which
// is where the last line sends it.
void type(void);

// Type the lines at the machine as it was left when it was last typed at.
// It is at its prompt again, as it is after anything that throws: its
// stacks are empty, and its memory and its dictionary are as they were.
void type_more(void);

// A machine that is to be stopped in the middle of what it is doing, looked
// at, and gone on with: a desktop in its event loop, which never comes back
// to the prompt.  going_on() is said after start() and before anything
// else is typed: it gives the machine what go_on() needs.  A word the test
// types stops the machine by running `stopping() execute`; keys() are
// bytes for the machine to read that are not a line; set_off() runs it
// from its prompt on what has been said, until it stops; and go_on() runs
// it again from where it stopped on what has been said since, its stacks
// and everything else as they were.
//
// vm.c starts a machine with nothing on its stacks.  So the machine is gone
// on with by a word that sets the two depths back through the depth ports,
// as a task that takes its turn does, and returns to where it stopped.
// That word puts two numbers on the data stack before the depths are set:
// the two cells at the bottom of the data stack are not kept, and a test
// that goes on has the machine put two there that it has no use for.
void going_on(void);
int stopping(void);
void keys(const void* bytes, size_t count);
void set_off(void);
void go_on(void);

// The nth cell down from the top of what was left on the data stack.
uint64_t left(int n);

// A number as a cell of this machine holds it, which is how left() and
// fetch() give one.
uint64_t as_cell(int64_t x);

// Whether cells from an address are all in the machine's memory, where a
// cell is, and what is in it.
bool in_memory(uint64_t addr, uint64_t cells);
uint8_t* cell_at(uint64_t addr);
uint64_t fetch(uint64_t addr);
void store(uint64_t addr, uint64_t x);

#endif //HEXAFORTH_NUCLEUS_H
