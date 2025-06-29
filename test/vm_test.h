//
// Created by Wes Brown on 1/1/21.
//

#ifndef HEXAFORTH_VM_TEST_H
#define HEXAFORTH_VM_TEST_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  const char *label;
  const char *init;
  const char *input;
  const char *io_input;
  const char *io_expected;
  const char *dstack;
  const char *rstack;
  const char *eip_expected;
} hexaforth_test;

typedef struct {
  uint64_t sz;
  int64_t *elems;
} counted_array;

bool execute_test(context *ctx, hexaforth_test test);
bool execute_tests(context *ctx, hexaforth_test *tests);

#endif // HEXAFORTH_VM_TEST_H
