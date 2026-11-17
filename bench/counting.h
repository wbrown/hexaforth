//
// The reference VM that counts (counting.c), for what has it in it and
// reads the counts itself.
//

#ifndef HEXAFORTH_COUNTING_H
#define HEXAFORTH_COUNTING_H

#include <stdint.h>

// How often the instruction at each place has been run: a count for every
// place up to RAN_TO, which is one past the highest that has been run.
// There is room for more as more are run, so RAN is where it is only until
// the machine is next run.
extern uint64_t* RAN;
extern long RAN_TO;

// The counts are written to the file that the environment's RUN_COUNTS
// names, if it names one: a count of 64 bits for each place up to RAN_TO.
void counts_written(void);

#endif //HEXAFORTH_COUNTING_H
