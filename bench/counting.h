//
// The interface to the counting reference VM (counting.c), for a program
// that is built with it and reads the counts itself.
//

#ifndef HEXAFORTH_COUNTING_H
#define HEXAFORTH_COUNTING_H

#include <stdint.h>

// RAN[at] is the number of times the instruction at address `at` has run.
// It has an entry for every address below RAN_TO, which is one more than
// the highest address that has run.  The array grows as higher addresses
// run, so a pointer to it is valid only until the machine runs again.
extern uint64_t* RAN;
extern long RAN_TO;

// Writes the counts to the file named by RUN_COUNTS in the environment, if
// it is set: one 64-bit count for each address below RAN_TO.  It also
// writes them as a profile's work to the file RUN_WORK names (counting.c).
void counts_written(void);

#endif //HEXAFORTH_COUNTING_H
