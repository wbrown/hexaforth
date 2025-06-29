//
// Created by Wes Brown on 1/12/21.
//

#include "../vm.h"
#include "../vm_opcodes.h"
#include "compiler.h"
#include "vm_test.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static hexaforth_test TESTS[] = {
    {.label = "8-bit literals", .input = "1 2 3 4", .dstack = "1 2 3 4"},
    // saw issues due to the use of `malloc`, which re-used a prior
    // context's memory and didn't clear.  `calloc` solves this.
    {.label = "Context cleared after tests",
     .input = "6 7 8 9 10",
     .dstack = "6 7 8 9 10"},
    {.label = "halt ( -- )", .init = "0", .input = "1 halt", .dstack = "1"},
    {.label = "noop ( -- )", .input = "1 2 noop 3 noop", .dstack = "1 2 3"},
    // `invert` is required for negative literals to be pushed onto stack.
    {.label = "invert ( a -- ~a )",
     .input = "1 2 1024 invert",
     .dstack = "1 2 -1025"},
    {.label = "negative literals",
     .input = "-1 -2 -3 -4 -5",
     .dstack = "-1 -2 -3 -4 -5"},
    {.label = "lit16",
     .input = "65535 -65535 49151 -49151",
     .dstack = "65535 -65535 49151 -49151"},
    {.label = "lit32 max/min",
     .input = "4294967295 -4294967295 ",
     .dstack = "4294967295 -4294967295 "},
    {.label = "lit48 max/min",
     .input = "140737488355328 -140737488355328",
     .dstack = "140737488355328 -140737488355328"},
    {.label = "dup ( a -- a a )",
     .input = "1 2 3 dup 4 dup",
     .dstack = "1 2 3 3 4 4"},
    {.label = "swap ( a b -- b a )",
     .input = "5 4 2 1 swap",
     .dstack = "5 4 1 2"},
    {.label = "over ( a b -- a b a )",
     .input = "5 4 3 2 over",
     .dstack = "5 4 3 2 3"},
    {.label = "nip ( a b -- b )", .input = "5 4 3 2 nip", .dstack = "5 4 2"},
    {.label = "tuck ( a b -- b a b )",
     .input = "1024 2048 tuck",
     .dstack = "2048 1024 2048"},
    {.label = "rot ( a b c -- b c a )",
     .input = "1024 2048 4096 rot",
     .dstack = "2048 4096 1024"},
    {.label = "-rot ( a b c -- c a b )",
     .input = "1024 2048 4096 -rot",
     .dstack = "4096 1024 2048"},
    {.label = "drop ( a b -- a )",
     .input = "5 4 3 2 drop",
     .dstack = "5 4 3"},
    {.label = "2drop ( a b -- )", .input = "5 4 3 2 2drop", .dstack = "5 4"},
    {.label = "rdrop ( R: a -- R: )",
     .input = "1024 >r 2048 >r rdrop",
     .rstack = "1024"},
    {.label = "+ ( a b -- a+b )", .input = "10 15 +", .dstack = "25"},
    {.label = "* ( a b -- a*b )",
     .input = "10 15 * -520 10 *",
     .dstack = "150 -5200"},
    {.label = "1+ ( a -- a+1 )", .input = "1 2 3 1+", .dstack = "1 2 4"},
    {.label = "2+ ( a -- a+2 )", .input = "1 2 3 2+", .dstack = "1 2 5"},
    {.label = "xor ( a b -- a^b )",
     .input = "65535 32768 xor 65535 65535 xor -1 -1 xor",
     .dstack = "32767 0 0"},
    {.label = "and ( a b -- a&b )",
     .input = "65535 32768 and -1 65535 and",
     .dstack = "32768 65535"},
    {.label = "or ( a b -- a|b )",
     .input = "65535 32768 or -1 -1 or",
     .dstack = "65535 -1"},
    {
        .label = "lshift ( a b -- a << b )",
        .input = "2 1 lshift",
        .dstack = "4",
    },
    {
        .label = "48-bit literal test",
        .input = "20015998343868",
        .dstack = "20015998343868",
    },
    {
        .label = "edge case: 4096",
        .input = "4096",
        .dstack = "4096",
    },
    {
        .label = "lshift64 ( a b -- a << b )",
        .input = "140737488355328 16 lshift",
        .dstack = "18446744073709486080",
    },
    {.label = "rshift ( a b -- a >> b )",
     .input = "2 1 rshift",
     .dstack = "1"},
    {.label = "rshift ( a b -- a >> b )",
     .input = "4 1 rshift",
     .dstack = "2"},
    {.label = "- ( a b -- b-a )", .input = "500 300 -", .dstack = "200"},
    {.label = "2* ( a -- a << 1 )", .input = "512 2*", .dstack = "1024"},
    {.label = "2/ ( a -- a >> 1 )", .input = "512 2/", .dstack = "256"},
    {.label = "@ ( a -- [a] )",
     .init = "1024 -1024 2048 -2048 4096 -4096",
     .input = "0 @ 8 @ 16 @ 24 @ 32 @ 40 @",
     .dstack = "1024 -1024 2048 -2048 4096 -4096"},
    {.label = "! ( a b -- )",
     .init = "0 0",
     .input = "1024 0 ! -1024 8 ! 0 @ 8 @",
     .dstack = "1024 -1024"},
    {.label = ">r ( a -- R: a )",
     .input = "1024 >r",
     .dstack = "",
     .rstack = "1024"},
    {.label = "r@ ( R:a -- a R: a )",
     .input = "1024 >r r@ r@",
     .dstack = "1024 1024",
     .rstack = "1024"},
    {.label = "r> ( R:a -- a )",
     .input = "1024 >r r@ 2* r>",
     .dstack = "2048 1024",
     .rstack = ""},
    {.label = "= ( a b -- f )",
     .input = "1024 1024 = "
              "-1024 1024 = "
              "-1024 -1024 = ",
     .dstack = "-1 0 -1"},
    {.label = "u< ( aU bU -- f )",
     .input = "-1024 1024 u< 1024 -1024 u<",
     .dstack = "0 -1"},
    {.label = "< ( a b -- f )",
     .input = "-1024 0 < "
              "0 1024 < "
              "0 -1024 < "
              "2048 1024 < ",
     .dstack = "-1 -1 0 0"},
    {.label = "exit ( -- )",
     .init = "0",
     .input = "1024 >r 0 >r exit",
     .rstack = "1024"},
    {.label = "2dup< ( a b -- a b f )",
     .input = "1024 2048 2dup<",
     .dstack = "1024 2048 -1"},
    {.label = "dup@ ( addr -- addr n )",
     .init = "1024 2048",
     .input = "8 dup @",
     .dstack = "8 2048"},
    {.label = "overand ( a b -- a f )",
     .input = "1024 2048 overand 4096 4096 overand",
     .dstack = "1024 0 4096 4096"},
    {.label = "dup>r ( a -- a R: a )",
     .input = "1024 dup>r",
     .dstack = "1024",
     .rstack = "1024"},
    {.label = "2dup< ( a b -- a b f )",
     .input = "1024 2048 2dup< 2048 1024 2dup<",
     .dstack = "1024 2048 -1 2048 1024 0"},
    {.label = "2dupxor ( a b -- a b a^b )",
     .input = "1024 2048 2dupxor",
     .dstack = "1024 2048 3072"},
    {.label = "over+ ( a b -- a a+b )",
     .input = "1024 4096 over+",
     .dstack = "1024 5120"},
    {.label = "over= ( a b -- a a=b? )",
     .input = "1024 4096 over= 2048 2048 over=",
     .dstack = "1024 0 2048 -1"},
    {.label = "@@ ( addr -- n )",
     .init = "8 1024 24 2048",
     .input = "0 @@ 16 @@",
     .dstack = "1024 2048"},
    {.label = "! ( n addr -- )",
     .init = "0 0 0 0",
     .input = "16 0 32 8 ! ! 8 @ 0 @",
     .dstack = "32 16"},
    {.label = "dup@ ( addr -- n )",
     .init = "0 0 24",
     .input = "16 dup@",
     .dstack = "16 24"},
    {.label = "dup@@ ( addr -- n )",
     .init = " 0 0 24 1024",
     .input = "16 dup@@",
     .dstack = "16 1024"},
    {.label = "over@ ( addr a -- addr a n )",
     .init = "1024",
     .input = "0 2048 over@",
     .dstack = "0 2048 1024"},
    {.label = "@r ( R: addr -- n )",
     .init = "1024 2048",
     .input = "8 >r @r",
     .dstack = "2048",
     .rstack = "8"},
    {.label = "swap>r ( a b -- b R: a )",
     .input = "1024 2048 swap>r",
     .dstack = "2048",
     .rstack = "1024"},
    {.label = "swapr> ( a b R: c -- b a c )",
     .input = "1024 2048 4096 >r swapr>",
     .dstack = "2048 1024 4096"},
    /* {
    .label = "48-bit string stack literal",
    .input = "'deadbe'",
    .dstack = "111473265304932 1"
    },
    {
    .label = "64-bit string stack literal",
    .input = "'deadbeef'",
    .dstack = "7378415037781730660 1"
    },
    {
    .label = "96-bit string stack literals",
    .input = "'deadbeefdead'",
    .dstack = "1684104548 7378415037781730660 2"
    }, */
    {.label = "8emit ( n -- )",
     .input = "'deadbe' drop 8emit",
     .io_expected = "deadbe"},
    {.label = "8emit ( n -- )",
     .input = "'deadbeef' drop 8emit",
     .io_expected = "deadbeef"},
    {.label = "8emit ( n -- )",
     .input = "'deadbeefdead' drop 8emit 8emit",
     .io_expected = "deadbeefdead"},
    {.label = "key ( -- ch )",
     .input = "key emit key emit key emit key emit",
     .io_input = "dead",
     .io_expected = "dead"},
    {.label = "+! ( n addr -- )",
     .init = "8",
     .input = "0 >r "
              "4  r@ +! r@ @ "
              "16 r@ +! r@ @ ",
     .dstack = "12 28",
     .rstack = "0"},
    {.label = "r@; ( R: addr -- addr )",
     .init = "0 0 0 0",
     .input = "4 >r r@;",
     .dstack = "4",
     .eip_expected = "4"},
    {.label = "@and ( n addr -- n )",
     .init = "32768",
     .input = "65535 0 @and",
     .dstack = "32768"},
    {.label = "2dup ( a b -- a b a b )",
     .input = "1024 2048 2dup",
     .dstack = "1024 2048 1024 2048"},
    {.label = "2swap ( ab cd -- cd ab )",
     .input = "1024 2048 4096 8192 2swap",
     .dstack = "4096 8192 1024 2048"},
    {.label = "2over ( ab cd - ab cd ab )",
     .input = "1024 2048 4096 8192 2over",
     .dstack = "1024 2048 4096 8192 1024 2048"},
    {.label = "3rd ( abc -- abc a )",
     .input = "1024 2048 4096 3rd",
     .dstack = "1024 2048 4096 1024"},
    {
        .label = "3dup ( abc -- abc abc )",
        .input = "1024 2048 4096 3dup",
        .dstack = "1024 2048 4096 1024 2048 4096",
    },
    {.label = "> ( a b -- f )",
     .input = "1024 2048 > 2048 1024 >",
     .dstack = "0 -1"},
    {
        .label = "u> ( a b -- f )",
        .input = "1024 2048 u> 2048 1024 u>",
        .dstack = "0 -1",
    },
    {
        .label = "0= ( a -- f )",
        .input = "0 0= 1 0= -1 0= ",
        .dstack = "-1 0 0",
    },
    {
        .label = "0< ( a -- f )",
        .input = "0 0< 1 0< -1 0< ",
        .dstack = "0 0 -1",
    },
    {
        .label = "0> ( a -- f )",
        .input = "0 0> 1 0> -1 0> ",
        .dstack = "0 -1 0",
    },
    {
        .label = "0<> ( a -- f )",
        .input = "0 0<> 1 0<> -1 0<> ",
        .dstack = "0 -1 -1",
    },
    {
        .label = "1- ( a -- a-1 )",
        .input = "1 1- 0 1- -1 1- ",
        .dstack = "0 -1 -2",
    },
    {
        .label = "bounds ( a b -- a+b a )",
        .input = "1024 2048 bounds",
        .dstack = "3072 1024",
    },
    {
        .label = "s>d ( s -- s f )",
        .input = "1024 s>d",
        .dstack = "1024 0",
    },
    {
        .label = "0xfffffffffffffff00",
        .input = "18446744073709551360",
        .dstack = "18446744073709551360",
    },
    {.label = "char literal ( -- c )", .input = "'a'", .dstack = "97"},
    {
        .label = "c@ ( addr -- c )",
        .input = "0 c@ 1 c@ 8 c@",
        .init = "7378415037781730660 101 0 0",
        .dstack = "100 101 101",
    },
    {.label = "c! ( c addr -- )",
     .init = "0 0 0 0",
     .input = "'d' 0 c! 'e' 1 c! 0 c@ 1 c@",
     .dstack = "100 101"},
    // Test array terminator
    {.label = "", .input = "", .dstack = ""},
    // {.label = "c@ ( addr -- c )",
    // {.label = "c! ( c addr -- )",
    // .init = "0 0 0 0",
    // .input = "'d' 0 c! 'e' 1 c! 'a' 2 c! 'd' 3 c! 'b' 4 c! 'e' 5 c! 'e' 6
    // c! 7 c! 0 @",
    //         .dstack = "7378415037781730660"},
    /* {.label = "depths ( -- n )",
            .input = "1024 2048 504 depths",
            .dstack = "1024 2048 504 3"},
    {.label = "depthr ( -- n )",
            .input = "1024 2048 >r >r depthr rdrop rdrop",
            .dstack = "2"}, */
};

void generate_test_cases_fs() {
  FILE *f = fopen("test_cases.fs", "w");
  if (!f) {
    printf("Failed to create test_cases.fs\n");
    return;
  }

  fprintf(f, "\\ Test cases generated from test/main.c\n\n");
  fprintf(f, "meta\n    4 org\ntarget\n\n");

  int test_num = 0;
  for (int i = 0; i < sizeof(TESTS) / sizeof(TESTS[0]); i++) {
    // Stop at terminator
    if (!TESTS[i].label || strlen(TESTS[i].label) == 0)
      break;

    // Create a valid word name from the label
    char word_name[100];
    snprintf(word_name, sizeof(word_name), "test_%d", test_num);

    fprintf(f, "\\ %s\n", TESTS[i].label);
    fprintf(f, "header %s : %s ", word_name, word_name);

    // Write the test input as the word body
    if (TESTS[i].input) {
      // Convert regular numbers to d# format for cross.fs
      const char *p = TESTS[i].input;
      while (*p) {
        if (*p == ' ' || *p == '\t') {
          fputc(*p, f);
          p++;
        } else if (*p == '\'' && *(p + 1) && *(p + 2) == '\'') {
          // Character literal 'x' -> [char] x
          fprintf(f, "[char] %c ", *(p + 1));
          p += 3;
        } else if (*p == '-' && isdigit(*(p + 1))) {
          // Negative number
          fprintf(f, "d# ");
          while (*p && *p != ' ' && *p != '\t') {
            fputc(*p, f);
            p++;
          }
          fprintf(f, " ");
        } else if (isdigit(*p)) {
          // Check if this is a word starting with a digit (like 2drop, 2dup,
          // etc)
          const char *start = p;
          while (*p && *p != ' ' && *p != '\t')
            p++;
          size_t len = p - start;
          char token[100];
          memcpy(token, start, len);
          token[len] = '\0';

          // Check if it's a pure number
          char *endptr;
          strtoll(token, &endptr, 10);
          if (*endptr == '\0') {
            // It's a pure number
            fprintf(f, "d# %s ", token);
          } else {
            // It's a word that starts with a digit
            fprintf(f, "%s ", token);
          }
        } else {
          // Word name
          while (*p && *p != ' ' && *p != '\t') {
            fputc(*p, f);
            p++;
          }
          fprintf(f, " ");
        }
      }
    }

    fprintf(f, ";\n\n");
    test_num++;
  }

  // Add required entry points
  fprintf(f, "header main : main halt ;\n");
  fprintf(f, "header quit : quit halt ;\n");

  fclose(f);
  printf("Generated test_cases.fs with %d test words\n", test_num);

  // Execute gforth to compile it
  printf("Compiling with cross.fs...\n");
  int ret = system("cd forth && gforth cross.fs basewords.fs "
                   "../test_cases.fs >/dev/null 2>&1");
  if (ret != 0) {
    printf("Failed to compile test_cases.fs\n");
  }
}

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

    // Read code pointer (byte address) and convert to word address
    uint16_t code_byte_addr = memory[code_ptr_addr];
    uint16_t code_word_addr = code_byte_addr / 2;

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

  // Generate test_cases.fs and compile it
  generate_test_cases_fs();

  // Load the compiled bytecode
  int total_words;
  uint16_t *hex_memory = load_hex_file("test_cases.hex", &total_words);
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
      }

      // cross.fs adds exit (0x7403) at the end
      if (cross_len > 0 && cross_bytes[cross_len - 1] == 0x7403) {
        cross_len--;
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
