// difftest_dump.c - run test programs on the reference VM (vm.c) and dump
// each program image together with the machine state it halts in, for
// difftest.py to replay against the AArch64 kernel.
//
// Two sets of cases:
//   1. every test in test/tests.h, compiled with the test compiler;
//   2. every possible ALU instruction (all 8192 encodings), executed once
//      from a fixed machine state in which every stack cell and every
//      memory cell it can reach holds a valid data address;
//   3. paged code: calls, jumps and returns between the kernel and two
//      code pages;
//   4. the instructions that fetch and store a variable.
#include "../test/compiler.h"
#include "../test/tests.h"
#include "../test/vm_test.h"
#include "../vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool init_image(context *ctx, hexaforth_test test);

#define MEM_DUMP 0x2000

static void w32(FILE *f, uint32_t v) { fwrite(&v, 4, 1, f); }

static void run_and_dump_n(FILE *f, context *ctx, const char *label,
                           const char *input, uint32_t mem_len) {
  w32(f, strlen(label));
  fwrite(label, 1, strlen(label), f);
  w32(f, ctx->EIP);
  w32(f, mem_len);
  fwrite(ctx->memory, 1, mem_len, f);
  w32(f, strlen(input));
  fwrite(input, 1, strlen(input), f);

  char *output = calloc(4096, 1);
  ctx->IN = fmemopen((void *)input, strlen(input) ? strlen(input) : 1, "r");
  ctx->OUT = fmemopen(output, 4096, "w");
  vm(ctx);
  long outlen = ftell(ctx->OUT);
  fclose(ctx->OUT);
  fclose(ctx->IN);

  w32(f, ctx->SP);
  w32(f, ctx->RSP);
  fwrite(ctx->DSTACK, 8, ctx->SP > 0 ? ctx->SP : 0, f);
  fwrite(ctx->RSTACK, 8, ctx->RSP > 0 ? ctx->RSP : 0, f);
  w32(f, outlen);
  fwrite(output, 1, outlen, f);
  fwrite(ctx->memory, 1, mem_len, f);
  free(output);
}

static void run_and_dump(FILE *f, context *ctx, const char *label,
                         const char *input) {
  run_and_dump_n(f, ctx, label, input, MEM_DUMP);
}

// Kernel at word 0, pages 8 and 9 at words 0x8000 and 0x9000.  Exercises a
// far call through `execute`, near calls and jumps inside a page, calls from
// a page into the kernel, a 0branch inside a page, and returns across pages.
static void paging_case(FILE *f, word_node *words) {
  context *ctx = calloc(1, sizeof(context));
  ctx->words = words;
  uint16_t *m = ctx->memory;
  // kernel
  m[0x000] = 0x9008;                    // h# 8000
  m[0x001] = 0x4010;                    // call execute
  m[0x002] = 0x8063;                    // 99
  m[0x003] = 0x0000;                    // halt
  m[0x010] = 0x650d; m[0x011] = 0x7403; // execute: >r exit
  m[0x020] = 0x8005; m[0x021] = 0x7403; // five: 5 exit
  // page 8
  m[0x8000] = 0x802a;                   // 42
  m[0x8001] = 0x5010;                   // call 0x8010, same page
  m[0x8002] = 0x4020;                   // call five, kernel
  m[0x8003] = 0x9009;                   // h# 9000
  m[0x8004] = 0x4010;                   // call execute: far call into page 9
  m[0x8005] = 0x1020;                   // jump 0x8020, same page
  m[0x8010] = 0x8007; m[0x8011] = 0x7403; // 7 exit
  m[0x8020] = 0x8000;                   // 0
  m[0x8021] = 0x3030;                   // 0branch 0x8030, taken
  m[0x8022] = 0x8bad;                   // skipped
  m[0x8030] = 0x8008; m[0x8031] = 0x7403; // 8 exit: back to the kernel
  // page 9
  m[0x9000] = 0x8009;                   // 9
  m[0x9001] = 0x5ffe;                   // call 0x9ffe, same page
  m[0x9002] = 0x7403;                   // exit: back to page 8
  m[0x9ffe] = 0x800a; m[0x9fff] = 0x7403; // 10 exit, at the end of the page
  run_and_dump_n(f, ctx, "paged code", "", 0x14000);
  free(ctx);
}

// A literal instruction with a shift field of 3 fetches or stores one of
// the 4,096 cells from VAR_BASE.  The first and the last of them, and one
// between, stored and fetched back; one that was never stored reads zero.
static void variable_case(FILE *f, word_node *words) {
  context *ctx = calloc(1, sizeof(context));
  ctx->words = words;
  uint16_t *m = ctx->memory;
  int i = 0;
  m[i++] = 0x8123;  m[i++] = 0xf005;    // h# 123, store it in variable 5
  m[i++] = 0xb005;  m[i++] = 0xb006;    // fetch 5, fetch 6
  m[i++] = 0x8007;  m[i++] = 0xffff;    // 7, store it in variable 4095
  m[i++] = 0xbfff;                      // fetch it
  m[i++] = 0x8fff;  m[i++] = 0xf000;    // h# fff, store it in variable 0
  m[i++] = 0xb000;  m[i++] = 0xb005;    // fetch 0, fetch 5
  m[i++] = 0xc001;                      // 1+, so the top is plainly a number
  run_and_dump_n(f, ctx, "variables", "", 0x4000 + 4096 * CELL_BYTES);
  free(ctx);
}

int main(int argc, char **argv) {
  FILE *f = fopen(argv[1], "wb");
  word_node *words = calloc(256, sizeof(word_node));
  init_opcodes(words);
  int n = 0;

  // The tests in tests.h are written for 64-bit cells.
  for (int i = 0; CELL_BITS == 64 && TESTS[i].input && strlen(TESTS[i].input); i++) {
    context *ctx = calloc(1, sizeof(context));
    ctx->words = words;
    if (init_image(ctx, TESTS[i]) && compile(ctx, TESTS[i].input)) {
      run_and_dump(f, ctx, TESTS[i].label,
                   TESTS[i].io_input ? TESTS[i].io_input : "");
      n++;
    }
    free(ctx);
  }

  // Eight cells of data start at byte 0x400, each holding the address of
  // one of the eight.  The return stack holds addresses of the empty cells
  // after them: an instruction that returns goes to one of those, and on a
  // machine whose cells are 16 bits that is the cell itself, which halts.
  enum { DATA = 0x8000 | (0x400 / CELL_BYTES) };
  static const uint16_t prefix[] = {
      DATA + 8, DATA + 9, DATA + 10,        // literals destined for the return stack
      0x650d, 0x650d, 0x650d,               // >r >r >r
      DATA, DATA + 1, DATA + 2, DATA + 7,   // data stack
  };
  for (int ins = 0x6000; ins < 0x8000; ins++) {
    context *ctx = calloc(1, sizeof(context));
    ctx->words = words;
    memcpy(ctx->memory, prefix, sizeof(prefix));
    ctx->memory[sizeof(prefix) / 2] = ins;
    for (int k = 0; k < 8; k++) {
      uint64_t a = 0x400 / CELL_BYTES + (k * 3 + 1) % 8;
      memcpy((uint8_t *)ctx->memory + 0x400 + k * CELL_BYTES, &a, CELL_BYTES);
    }
    char label[32];
    snprintf(label, sizeof(label), "alu 0x%04x", ins);
    run_and_dump(f, ctx, label, "");
    free(ctx);
    n++;
  }
  if (CELL_BITS >= 32) {        // its code pages are past 64KB
    paging_case(f, words);
    n++;
  }
  variable_case(f, words);
  n++;
  fclose(f);
  fprintf(stderr, "%d cases dumped\n", n);
  return 0;
}
