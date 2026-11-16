//
// The reference VM that counts (counting.c), for what has it in it and
// reads the counts itself.
//

#ifndef HEXAFORTH_COUNTING_H
#define HEXAFORTH_COUNTING_H

#include <stdint.h>

#define PLACES 0x20000                  // the instructions of memory that are counted: where code is

// How often the instruction at each place has been run.
extern uint64_t RAN[PLACES];

// The counts are written to the file that the environment's RUN_COUNTS
// names, if it names one.
void counts_written(void);

#endif //HEXAFORTH_COUNTING_H
