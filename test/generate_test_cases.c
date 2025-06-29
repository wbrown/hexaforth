//
// generate_test_cases.c - Standalone test case generator for hexaforth
//
// This program reads the test definitions from tests.h and generates
// a Forth source file (test_cases.fs) that can be cross-compiled
// using cross.fs to create test bytecode.
//

#include "../vm.h"
#include "../vm_opcodes.h"
#include "vm_test.h"
#include "tests.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void generate_test_cases_fs() {
  FILE *f = fopen("forth/test_cases.fs", "w");
  if (!f) {
    printf("Failed to create forth/test_cases.fs\n");
    return;
  }

  fprintf(f, "\\ Test cases generated from test/tests.h\n\n");
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
  printf("Generated forth/test_cases.fs with %d test words\n", test_num);
}

int main(int argc, char *argv[]) {
  generate_test_cases_fs();
  
  // Optionally compile with cross.fs if requested
  if (argc > 1 && strcmp(argv[1], "--compile") == 0) {
    printf("Compiling with cross.fs...\n");
    int ret = system("cd forth && gforth cross.fs basewords.fs "
                     "test_cases.fs");
    if (ret != 0) {
      printf("Failed to compile test_cases.fs\n");
      return 1;
    }
    printf("Successfully generated build/test_cases.hex\n");
  }
  
  return 0;
}