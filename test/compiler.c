//
// Created by Wes Brown on 12/31/20.
//

#include "compiler.h"
#include "../vm_debug.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Lowest level compiler call that writes an instruction and increments the
// HERE pointer to the next CELL.
void insert_opcode(context *ctx, instruction op) {
  *(instruction *)&(ctx->memory[ctx->HERE]) = op;
#ifdef DEBUG
  char decoded[160];
  debug_address(decoded, ctx, ctx->HERE);
  dprintf("HERE[0x%0.4x]: %s\n", ctx->HERE, decoded);
#endif
  ctx->HERE++;
}

// This function is used by `compile_word` to determine when it's reached
// the end of an instruction stream that it needs to write out.
bool is_null_instruction(instruction ins) {
  return (*(uint16_t *)(void *)&ins == 0);
}

// Look up a string in our opcodes table, and if found, write the opcodes
// associated with the word into the image.
bool compile_word(context *ctx, const char *word) {
  int ins_ct = 0;
  instruction instructions[64] = {};
  if ((ins_ct = lookup_word(ctx->words, word, instructions))) {
    for (int idx = 0; idx < ins_ct; idx++) {
      instruction ins = instructions[idx];
      insert_opcode(ctx, ins);
    }
  } else {
    // Are we a literal?
    char *decode_end;
    // Use strtoull for parsing to handle large unsigned values
    // then cast to signed for consistency with cross.fs
    uint64_t unum = strtoull(word, &decode_end, 10);
    if (*decode_end) {
      printf("ERROR: '%s' not found!\n", word);
      return (false);
    } else {
      // Cast to signed to match cross.fs behavior
      insert_literal(ctx, (int64_t)unum);
    }
  }
  return (true);
}

void insert_uint16(context *ctx, uint16_t n) {
  // dprintf("INSERT_UINT16: %d @ %d\n", n, ctx->HERE);
  ctx->memory[ctx->HERE] = n;
  ctx->HERE++;
}

void insert_uint32(context *ctx, uint32_t n) {
  // dprintf("INSERT_UINT32: %d @ %d\n", n, ctx->HERE);
  *(uint32_t *)&(ctx->memory[ctx->HERE]) = n;
  ctx->HERE += 2;
}

void insert_uint64(context *ctx, uint64_t n) {
  // dprintf("INSERT_UINT64: %lld @ %d\n", n, ctx->HERE);
  *(uint64_t *)&(ctx->memory[ctx->HERE]) = n;
  ctx->HERE += 4;
}

void insert_int64(context *ctx, int64_t n) {
  // dprintf("INSERT_INT64: %lld @ %d\n", n, ctx->HERE);
  *(int64_t *)&(ctx->memory[ctx->HERE]) = n;
  ctx->HERE += 4;
}

void insert_char_literal(context *ctx, char c) {
  // For single characters, just push the ASCII value
  // This matches standard Forth CHAR/[CHAR] behavior
  insert_literal(ctx, (uint64_t)c);
}

void insert_string_literal(context *ctx, char *str) {
  uint64_t len = strlen(str);

  // Special case: single character should just push the value
  // to match cross.fs behavior with [char]
  if (len == 1) {
    insert_char_literal(ctx, str[0]);
    return;
  }

  uint64_t octabytes = len / 8;
  uint64_t bytes_rem = len % 8;
  uint64_t num_cells = octabytes + (bytes_rem ? 1 : 0);
  uint64_t octabyte;
  int idx = octabytes - (bytes_rem ? 0 : 1);

  if (bytes_rem) {
    uint64_t shifts = 64 - (8 * bytes_rem);
    octabyte = *(uint64_t *)&(str[idx * 8]) << shifts >> shifts;
    // dprintf("MSB: %hhu\n", clz(octabyte));
    // dprintf("STRING_LITERAL: '%.*s' -> %lld\n", (int)bytes_rem,
    // (char*)&octabyte, octabyte);
    insert_literal(ctx, octabyte);
    idx = idx - 1;
  }
  for (; idx >= 0; idx--) {
    octabyte = *(uint64_t *)&(str[idx * 8]);
    // dprintf("MSB: %hhu\n", clz(octabyte));
    // dprintf("STRING_LITERAL: '%.*s' -> %lld\n", 8, (char*)&octabyte,
    // octabyte);
    insert_literal(ctx, octabyte);
  }
  // Number of cells for the string.
  insert_literal(ctx, num_cells);
}

void insert_string(context *ctx, char *str) {
  uint64_t len = strlen(str);
  uint64_t octabytes = len / 8;
  uint64_t bytes_rem = len % 8;
  uint64_t octabyte = 0;

  insert_int64(ctx, len);
  int idx;
  for (idx = 0; idx < octabytes; idx++) {
    octabyte = *(uint64_t *)&(str[idx * 8]);
    insert_uint64(ctx, octabyte);
  }
  if (bytes_rem) {
    switch (bytes_rem) {
    case 1:
      octabyte = (uint8_t)str[idx * 8];
      octabyte = octabyte << 8;
      insert_uint16(ctx, octabyte);
      break;
    case 2:
      octabyte = (uint16_t)str[idx * 8];
      insert_uint16(ctx, octabyte);
      break;
    case 3:
      octabyte = (uint32_t)str[idx * 8];
      octabyte = octabyte << 8;
      insert_uint32(ctx, octabyte);
      break;
    case 4:
      octabyte = (uint32_t)str[idx * 8];
      insert_uint32(ctx, octabyte);
      break;
    case 5:
      octabyte = (uint64_t)str[idx * 8];
      octabyte = octabyte << 24;
      insert_uint64(ctx, octabyte);
      break;
    case 6:
      octabyte = (uint64_t)str[idx * 8];
      octabyte = octabyte << 16;
      insert_uint64(ctx, octabyte);
      break;
    case 7:
      octabyte = (uint64_t)str[idx * 8];
      octabyte = octabyte << 8;
      insert_uint64(ctx, octabyte);
      break;
    default:
      printf("!! %lld bytes_rem -- this should never happen!", bytes_rem);
    }
  }
}

// This function takes a 64-bit signed integer and compiles it as a sequence
// of literal, shift, and invert instructions needed to reconstruct the
// literal on top of the stack.
void insert_literal(context *ctx, int64_t n) {
  dprintf("HERE[0x%0.4d]: COMPILE_LITERAL: %lld\n", ctx->HERE, n);
  
  uint64_t acc; // Our accumulator, really a deccumlator
  bool negative = n < 0;
  if (negative) {
    acc = (uint64_t)(llabs(n)) - 1;
  } else {
    acc = (uint64_t)n;
  }
  bool first_ins = true;

  // Create our template literal instruction.  We may produce one or more
  // literal instructions for numbers that are larger than LIT_BITS.
  instruction literal = {};
  literal.lit.lit_f = true;
  literal.lit.lit_add = false;
  uint8_t shifts = 0;

  if (acc == 0 && !negative) {
    literal.lit.lit_v = 0;
    insert_opcode(ctx, literal);
    return; // Don't process further
  }

  // Handle literals > 48 bits (cross.fs doesn't support this)
  // We split into two parts: upper bits shifted left 48, then add lower 48
  // bits
  if (acc > 0xfffffffffff) {
    uint64_t acc1 = acc >> 48;
    if (acc1) { // Only generate if upper bits are non-zero (bug fix)
      insert_literal(ctx, acc1);
      insert_literal(ctx, 48);
      compile_word(ctx, "lshift");
      acc = acc << 16 >> 16; // Sign-extend lower 48 bits
      first_ins = false;
    }
  }
  
  // Check if we can use the invert optimization
  // This works for both negative numbers and large positive numbers
  // that have mostly 1 bits (like 0xffffffffffffff00)
  // Do this after handling >48 bit values
  uint64_t inverted = ~acc;
  if (inverted <= LIT_UMASK && first_ins) {
    // Use the optimization: push inverted value then invert
    literal.lit.lit_v = inverted;
    literal.lit.lit_shifts = 0;
    insert_opcode(ctx, literal);
    compile_word(ctx, "invert");
    return;
  }

  // Process literal in 12-bit chunks from MSB to LSB to match cross.fs
  // First figure out how many 12-bit chunks we need
  int shift_count = 0;
  uint64_t temp = acc;
  while (temp > LIT_UMASK) {
    temp >>= LIT_BITS;
    shift_count++;
  }

  // Now process from MSB to LSB
  for (int shifts = shift_count; shifts >= 0; shifts--) {
    uint16_t chunk = (acc >> (shifts * LIT_BITS)) & LIT_UMASK;

    // Skip zero chunks unless it's the only chunk (literal 0)
    if (chunk != 0 || (shifts == 0 && shift_count == 0)) {
      literal.lit.lit_v = chunk;
      literal.lit.lit_shifts = shifts;
      if (!first_ins)
        literal.lit.lit_add = true;
      first_ins = false;
      insert_opcode(ctx, literal);
    } else {
      dprintf("HERE[0x%0.4d]: Skipping LIT instruction as lit_v == 0, shift "
              "== %d\n",
              ctx->HERE, shifts);
    }
  }
  // Handle negative numbers (same as cross.fs approach)
  if (negative)
    compile_word(ctx, "invert");
}

// Given a string, this compiles it into VM instructions.

bool compile(context *ctx, const char *input) {
  // Size our input buffer, and copy it, as it could be a constant string
  // that can't be modified.
  uint64_t input_len = strlen(input) + 1;
  char *buffer = malloc(input_len);
  memcpy(buffer, input, input_len);
  // Our word point into the buffer and are adjusted as we go.
  char *word = buffer;
  // Are we scanning a string literal?
  bool string = false;

  // Loop through the characters in our buffer.
  for (int i = 0; i <= input_len; i++) {
    if (input[i] == '\'' && string) {
      buffer[i] = '\0';
      if (strlen(word)) {
        insert_string_literal(ctx, word);
      }
      string = false;
      word = &buffer[i + 1];
    } else if (input[i] == '\'') {
      string = true;
      word = &buffer[i + 1];
    } else if (input[i] == ' ' || input[i] == '\0') {
      // We replace spaces with null bytes to indicate to C that it's
      // the termination of the string.
      buffer[i] = '\0';
      if (strlen(word)) {
        if (!compile_word(ctx, word)) {
          free(buffer);
          return (false);
        };
      }
      // Adjust our word pointer to the beginning of the next word.
      word = &buffer[i + 1];
    }
  }
  // Only insert halt if the last instruction isn't already an exit
  if (ctx->HERE > 0) {
    uint16_t last_insn = ctx->memory[ctx->HERE - 1];
#ifdef DEBUG
    dprintf("Last instruction at HERE-1 (0x%04x): 0x%04x\n", ctx->HERE - 1,
            last_insn);
#endif
    if (last_insn != 0x7403) {
      insert_opcode(ctx, (instruction){});
    }
  } else {
    insert_opcode(ctx, (instruction){});
  }
  free(buffer);
  return (true);
}
