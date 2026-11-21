//
// Created by Wes Brown on 1/12/21.
//

#include "../vm.h"
#include "../vm_opcodes.h"
#include "compiler.h"
#include "vm_test.h"
#include "tests.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef struct {
  uint16_t *bytes;
  int count;
} test_bytecode;

// Load hex file into memory
static uint16_t *load_hex_file(const char *filename, int *total_words) {
  FILE *f = fopen(filename, "r");
  if (!f)
    return NULL;

  // As many words as the file has: there is room for more as it is read.
  uint16_t *memory = NULL;
  int addr = 0, room = 0;
  char line[100];
  while (fgets(line, sizeof(line), f)) {
    // Remove newline
    line[strcspn(line, "\n")] = 0;

    // Each line has 8 hex chars = 4 bytes = 2 16-bit words
    // But they're in big-endian order in the file
    if (strlen(line) == 8) {
      uint32_t value;
      if (sscanf(line, "%8x", &value) == 1) {
        if (addr + 2 > room) {
          room = 2 * room + 2;
          memory = realloc(memory, room * sizeof(uint16_t));
          if (!memory) {
            fclose(f);
            return NULL;
          }
        }
        // Split into two 16-bit words
        // The hex file has them in reverse order
        // Last 4 hex chars = first word in memory
        memory[addr++] = value & 0xFFFF;
        // First 4 hex chars = second word in memory
        memory[addr++] = (value >> 16) & 0xFFFF;
      }
    }
  }
  fclose(f);
  *total_words = addr;
  return memory;
}

// A word of a test image's dictionary: its name, and the instruction its
// code starts at.
typedef struct dict_entry {
  char name[64];
  uint16_t code_addr;
} dict_entry;

// The words of a test image's dictionary, from the first that is looked at
// (read_header in vm_opcodes.c): how many, and the entries, which are made
// here and are as many as the dictionary has.
static int parse_dictionary(uint16_t *memory, int total_words,
                            dict_entry **entries) {
  const uint8_t *bytes = (const uint8_t *)memory;
  size_t image_len = (size_t)total_words * 2;
  uint32_t head = first_header(bytes, image_len);
  dict_entry *found = NULL;
  int count = 0, room = 0;

  while (head) {
    uint64_t xt;
    char name[256];
    head = read_header(bytes, image_len, head, &xt, name);
    if (!name[0])
      break;
    if (strlen(name) < sizeof(found->name)) {
      if (count == room) {
        room = 2 * room + 1;
        found = realloc(found, room * sizeof(dict_entry));
        if (!found) {
          printf("No room for %d dictionary entries\n", room);
          exit(1);
        }
      }
      strcpy(found[count].name, name);
      found[count].code_addr = (uint16_t)xt;
      count++;
    }
  }

  *entries = found;
  return count;
}

// Extract bytecode for each test using dictionary.  A test's code is from
// where its word starts to its exit or a halt, or to the end of the image.
static void extract_test_bytecode(uint16_t *memory, int total_words,
                                  uint16_t exit_ins, test_bytecode *tests,
                                  int test_count, dict_entry *dict_entries,
                                  int dict_count) {
  // For each test, find its dictionary entry and extract code
  for (int i = 0; i < test_count; i++) {
    char test_name[sizeof("test_") + 3 * sizeof(int)];
    sprintf(test_name, "test_%d", i);

    // Find this test in dictionary
    for (int j = 0; j < dict_count; j++) {
      if (strcmp(dict_entries[j].name, test_name) == 0) {
        // Found it - its code is where the image has it, and is not copied
        int addr = dict_entries[j].code_addr;
        tests[i].bytes = memory + addr;
        tests[i].count = 0;

        while (addr < total_words) {
          uint16_t word = memory[addr++];
          tests[i].count++;

          // Check for exit or halt
          if (word == exit_ins || word == 0x0000) {
            break;
          }
        }
        break;
      }
    }
  }
}

int main() {
  static context ctx;       // too big for the stack: it holds the VM's memory
  ctx.words = FORTH_WORDS;
  int ret = init_opcodes(ctx.words);

  // Load the compiled bytecode
  int total_words;
  uint16_t *hex_memory = load_hex_file("build/test_cases.hex", &total_words);
  if (!hex_memory) {
    printf("Failed to load test_cases.hex\n");
    return 1;
  }
  printf("Loaded %d words from test_cases.hex\n", total_words);

  // The instruction that is an exit, as vm_opcodes.h has it
  uint16_t exit_ins = 0;
  for (word_node *w = ctx.words; w->repr && strlen(w->repr); w++) {
    if (w->num_ins == 1 && strcmp(w->repr, "exit") == 0) {
      exit_ins = *(uint16_t *)&w->ins[0];
    }
  }
  if (!exit_ins) {
    printf("exit is not one instruction\n");
    return 1;
  }

  // Parse dictionary to find all test words
  dict_entry *dict_entries;
  int dict_count = parse_dictionary(hex_memory, total_words, &dict_entries);
  printf("Found %d dictionary entries\n", dict_count);

  // Count how many test_N entries we found
  int cross_test_count = 0;
  for (int i = 0; i < dict_count; i++) {
    if (strncmp(dict_entries[i].name, "test_", 5) == 0) {
      cross_test_count++;
    }
  }
  printf("Found %d test words in dictionary\n", cross_test_count);

  // Extract bytecode for tests: as many as the dictionary has test words
  test_bytecode *cross_tests = calloc(cross_test_count + 1, sizeof(test_bytecode));
  extract_test_bytecode(hex_memory, total_words, exit_ins, cross_tests,
                        cross_test_count, dict_entries, dict_count);

  // Now let's compare with C compiler output
  printf("\nComparing cross.fs vs C compiler:\n");
  printf("=================================\n");

  // Only compare the active tests
  int active_tests = 0;
  for (int i = 0; i < sizeof(TESTS) / sizeof(TESTS[0]); i++) {
    if (TESTS[i].label && strlen(TESTS[i].label) > 0) {
      active_tests++;
    }
  }

  bool all_compiler_tests_passed = true;

  // A test that cross.fs's image has no word for is compared with nothing.
  if (cross_test_count < active_tests) {
    printf("%d tests, and the image has %d of them\n", active_tests,
           cross_test_count);
    all_compiler_tests_passed = false;
  }

  for (int i = 0; i < active_tests && i < cross_test_count; i++) {
    printf("Test %d: %-30s", i, TESTS[i].label);

    // Compile with C compiler
    static context c_ctx;
    memset(&c_ctx, 0, sizeof(c_ctx));
    // as many words as vm_opcodes.h has room for
    c_ctx.words = calloc(sizeof(FORTH_WORDS) / sizeof(FORTH_WORDS[0]), sizeof(word_node));

    // Suppress opcode output during comparison
    extern int suppress_opcode_output;
    suppress_opcode_output = 1;
    init_opcodes(c_ctx.words);
    suppress_opcode_output = 0;

    // Just compile the test input directly
    if (compile(&c_ctx, TESTS[i].input)) {
      // Use the bytecode we already extracted via dictionary parsing
      uint16_t *cross_bytes = cross_tests[i].bytes;
      int cross_count = cross_tests[i].count;

      // Strip trailing halt from C output and exit from cross.fs
      int c_len = c_ctx.HERE;
      int cross_len = cross_count;

      // C compiler adds halt (0x0000) at the end
      if (c_len > 0 && c_ctx.memory[c_len - 1] == 0x0000) {
        c_len--;
      // cross.fs adds exit at the end
      if (cross_len > 0 && cross_bytes[cross_len - 1] == exit_ins) {
        cross_len--;
      }
      }


      // Detailed comparison
      bool match = (c_len == cross_len);
      if (match) {
        for (int j = 0; j < c_len; j++) {
          if (c_ctx.memory[j] != cross_bytes[j]) {
            match = false;
            break;
          }
        }
      }

      if (match) {
        printf(" ✓\n");
      } else {
        printf(" MISMATCH\n");
        all_compiler_tests_passed = false;

        // Show detailed output only on mismatch
        printf("\nC compiler output (%d instructions):\n", c_ctx.HERE);
        for (int j = 0; j < c_ctx.HERE; j++) {
          char dis[200];
          decode_instruction(dis, *(instruction *)&c_ctx.memory[j],
                             ctx.words);
          printf("  %04X: %s\n", c_ctx.memory[j], dis);
        }

        printf("\ncross.fs output (%d instructions):\n", cross_count);
        for (int j = 0; j < cross_count; j++) {
          char dis[200];
          decode_instruction(dis, *(instruction *)&cross_bytes[j], ctx.words);
          printf("  %04X: %s\n", cross_bytes[j], dis);
        }

        if (c_len != cross_len) {
          printf(
              "\nLength mismatch: C has %d instructions, cross.fs has %d\n",
              c_len, cross_len);
        }

        // Show where they differ
        int min_len = c_len < cross_len ? c_len : cross_len;
        for (int j = 0; j < min_len; j++) {
          if (c_ctx.memory[j] != cross_bytes[j]) {
            printf(
                "First difference at instruction %d: C=%04X cross.fs=%04X\n",
                j, c_ctx.memory[j], cross_bytes[j]);
            break;
          }
        }
      }
    }

    free(c_ctx.words);
  }

  // Summary of compiler comparison
  printf("\n=================================\n");
  if (!all_compiler_tests_passed) {
    printf("COMPILER COMPARISON FAILED - Stopping before VM tests\n");
    free(hex_memory);
    free(cross_tests);
    free(dict_entries);
    return 1;
  }
  printf("All compiler comparisons PASSED\n");

  // Clean up: a test's bytes are the image's
  free(hex_memory);
  free(cross_tests);
  free(dict_entries);

  // Now run the VM execution tests
  printf("\nRunning VM execution tests:\n");
  printf("===========================\n");

  if (ret) {
    ret = execute_tests(&ctx, TESTS);
  }
  return (!ret);
}
