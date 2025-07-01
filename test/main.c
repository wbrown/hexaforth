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
  uint16_t start_addr;
  uint16_t *bytes;
  int count;
} test_bytecode;

// Load hex file into memory
static uint16_t *load_hex_file(const char *filename, int *total_words) {
  FILE *f = fopen(filename, "r");
  if (!f)
    return NULL;

  uint16_t *memory = calloc(65536, sizeof(uint16_t));
  if (!memory) {
    fclose(f);
    return NULL;
  }

  int addr = 0;
  char line[100];
  while (fgets(line, sizeof(line), f) && addr < 65536) {
    // Remove newline
    line[strcspn(line, "\n")] = 0;

    // Each line has 8 hex chars = 4 bytes = 2 16-bit words
    // But they're in big-endian order in the file
    if (strlen(line) == 8) {
      uint32_t value;
      if (sscanf(line, "%8x", &value) == 1) {
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

// Parse dictionary header to find code pointer
static uint16_t get_code_pointer(uint16_t *memory, uint16_t dict_addr) {
  // Convert byte address to word index
  // The .lst addresses are absolute byte addresses in the image
  uint16_t word_idx = dict_addr / 2;
  uint16_t start_idx = word_idx;

  // Dictionary format:
  // - link (2 bytes = 1 word)
  // - name length (1 byte)
  // - name chars (length bytes)
  // - padding to align to word boundary
  // - code pointer (2 bytes = 1 word)

  // Skip link field
  word_idx++;

  // Get name length (it's in the low byte of the word)
  uint8_t name_len = memory[word_idx] & 0xFF;

  // The name length byte is in the low byte, so we need to account for that
  // when calculating how many words to skip
  // We have: [length_byte + name_bytes], and need to round up to word
  // boundary
  int name_bytes = name_len;
  int total_name_bytes = 1 + name_bytes;       // 1 for the length byte itself
  int name_words = (total_name_bytes + 1) / 2; // Round up to word boundary

  word_idx += name_words;

  // Now we're at the code field pointer
  // Return the value at this location (the pointer to the actual code)
  return memory[word_idx];
}

// Dictionary parsing structure
typedef struct dict_entry {
  char name[64];
  uint16_t dict_addr; // Word address of dictionary entry
  uint16_t code_addr; // Word address of code
  uint16_t link;      // Link to previous entry
} dict_entry;

// Parse dictionary starting at word 0x2000
static int parse_dictionary(uint16_t *memory, dict_entry *entries,
                            int max_entries) {
  int count = 0;
  uint16_t dict_addr = 0x2000;

  while (dict_addr < 0x2200 && count < max_entries) {
    uint16_t link = memory[dict_addr];
    uint8_t name_len = memory[dict_addr + 1] & 0xFF;

    if (name_len == 0 || name_len > 63)
      break;

    // Extract name
    char name[64] = {0};
    // First character is in high byte of length word
    if (name_len > 0) {
      name[0] = (memory[dict_addr + 1] >> 8) & 0xFF;
    }
    // Rest of name continues in following words
    for (int i = 1; i < name_len; i++) {
      uint16_t word_offset = 2 + (i - 1) / 2;
      uint16_t word = memory[dict_addr + word_offset];
      if ((i - 1) & 1) {
        name[i] = (word >> 8) & 0xFF;
      } else {
        name[i] = word & 0xFF;
      }
    }

    // Calculate code pointer location
    uint16_t header_bytes = 2 + 1 + name_len;
    if (header_bytes & 1)
      header_bytes++;
    uint16_t code_ptr_addr = dict_addr + header_bytes / 2;

    // Read code pointer (already a word address)
    uint16_t code_word_addr = memory[code_ptr_addr];

    // Store entry
    strcpy(entries[count].name, name);
    entries[count].dict_addr = dict_addr;
    entries[count].code_addr = code_word_addr;
    entries[count].link = link;
    count++;

    // Find next entry
    uint16_t next_addr = code_ptr_addr + 1;
    dict_addr = 0;

    // Scan forward for next dictionary entry
    while (next_addr < 0x2200) {
      if (memory[next_addr] >= 0x4000 || memory[next_addr] == 0) {
        dict_addr = next_addr;
        break;
      }
      next_addr++;
    }

    if (dict_addr == 0)
      break;
  }

  return count;
}

// Extract bytecode for each test using dictionary
static void extract_test_bytecode(uint16_t *memory, test_bytecode *tests,
                                  int test_count, dict_entry *dict_entries,
                                  int dict_count) {
  // For each test, find its dictionary entry and extract code
  for (int i = 0; i < test_count; i++) {
    char test_name[32];
    sprintf(test_name, "test_%d", i);

    // Find this test in dictionary
    for (int j = 0; j < dict_count; j++) {
      if (strcmp(dict_entries[j].name, test_name) == 0) {
        // Found it - extract code until we hit exit/halt
        tests[i].bytes = calloc(200, sizeof(uint16_t));
        tests[i].count = 0;

        uint16_t addr = dict_entries[j].code_addr;
        while (addr < 0x4000) {
          uint16_t word = memory[addr];
          tests[i].bytes[tests[i].count++] = word;

          // Check for exit (0x7403) or halt (0x0000)
          if (word == 0x7403 || word == 0x0000) {
            break;
          }

          // Safety limit
          if (tests[i].count >= 200)
            break;
          addr++;
        }
        break;
      }
    }
  }
}

int main() {
  context ctx;
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

  // Parse dictionary to find all test words
  dict_entry dict_entries[200];
  int dict_count = parse_dictionary(hex_memory, dict_entries, 200);
  printf("Found %d dictionary entries\n", dict_count);

  // Count how many test_N entries we found
  int cross_test_count = 0;
  for (int i = 0; i < dict_count; i++) {
    if (strncmp(dict_entries[i].name, "test_", 5) == 0) {
      cross_test_count++;
    }
  }
  printf("Found %d test words in dictionary\n", cross_test_count);

  // Extract bytecode for tests
  test_bytecode cross_tests[100];
  extract_test_bytecode(hex_memory, cross_tests, cross_test_count,
                        dict_entries, dict_count);

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

  for (int i = 0; i < active_tests && i < cross_test_count; i++) {
    printf("Test %d: %-30s", i, TESTS[i].label);

    // Compile with C compiler
    context c_ctx = {0};
    c_ctx.words = calloc(1024, sizeof(word_node));

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
      // cross.fs adds exit (0x7403) at the end
      if (cross_len > 0 && cross_bytes[cross_len - 1] == 0x7403 ) {
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
    for (int i = 0; i < cross_test_count; i++) {
      free(cross_tests[i].bytes);
    }
    return 1;
  }
  printf("All compiler comparisons PASSED\n");

  // Clean up
  free(hex_memory);
  for (int i = 0; i < cross_test_count; i++) {
    free(cross_tests[i].bytes);
  }

  // Now run the VM execution tests
  printf("\nRunning VM execution tests:\n");
  printf("===========================\n");

  if (ret) {
    ret = execute_tests(&ctx, TESTS);
  }
  return (!ret);
}
