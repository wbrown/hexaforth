// difftest_dump.c - run test programs on the reference VM (vm.c) and dump
// each program image together with the machine state it halts in, for
// difftest.py to replay against the AArch64 kernel.
//
// Two sets of cases:
//   1. every test in test/tests.h, compiled with the test compiler;
//   2. every possible ALU instruction (all 8192 encodings), executed once
//      from a fixed machine state in which every stack cell and every
//      memory cell it can reach holds a valid data address.
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

static void run_and_dump(FILE *f, context *ctx, const char *label,
                         const char *input) {
  w32(f, strlen(label));
  fwrite(label, 1, strlen(label), f);
  w32(f, ctx->EIP);
  fwrite(ctx->memory, 1, MEM_DUMP, f);
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
  fwrite(ctx->memory, 1, MEM_DUMP, f);
  free(output);
}

int main(int argc, char **argv) {
  FILE *f = fopen(argv[1], "wb");
  word_node *words = calloc(256, sizeof(word_node));
  init_opcodes(words);
  int n = 0;

  for (int i = 0; TESTS[i].input && strlen(TESTS[i].input); i++) {
    context *ctx = calloc(1, sizeof(context));
    ctx->words = words;
    if (init_image(ctx, TESTS[i]) && compile(ctx, TESTS[i].input)) {
      run_and_dump(f, ctx, TESTS[i].label,
                   TESTS[i].io_input ? TESTS[i].io_input : "");
      n++;
    }
    free(ctx);
  }

  static const uint16_t prefix[] = {
      0x8418, 0x8420, 0x8428,         // literals destined for the return stack
      0x650d, 0x650d, 0x650d,         // >r >r >r
      0x8400, 0x8408, 0x8410, 0x8438, // data stack
  };
  for (int ins = 0x6000; ins < 0x8000; ins++) {
    context *ctx = calloc(1, sizeof(context));
    ctx->words = words;
    memcpy(ctx->memory, prefix, sizeof(prefix));
    ctx->memory[sizeof(prefix) / 2] = ins;
    int64_t *cells = (int64_t *)((uint8_t *)ctx->memory + 0x400);
    for (int k = 0; k < 8; k++)
      cells[k] = 0x400 + 8 * ((k * 3 + 1) % 8);
    char label[32];
    snprintf(label, sizeof(label), "alu 0x%04x", ins);
    run_and_dump(f, ctx, label, "");
    free(ctx);
    n++;
  }
  fclose(f);
  fprintf(stderr, "%d cases dumped\n", n);
  return 0;
}
