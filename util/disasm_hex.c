#include "vm.h"
#include "vm_opcodes.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Label tracking for calls: as many as there are, each with its whole name
typedef struct label {
  long addr;
  char *name;
} label;

label *labels;
int label_count = 0, label_room = 0;

// More room for a list, or it stops
static void *more_room(void *list, size_t count, size_t size) {
  list = realloc(list, (count ? count : 1) * size);
  if (!list) {
    printf("No room for %zu of %zu bytes\n", count, size);
    exit(1);
  }
  return list;
}

void add_label(long addr, const char *name) {
  if (label_count == label_room) {
    label_room = 2 * label_room + 1;
    labels = more_room(labels, label_room, sizeof(label));
  }
  labels[label_count].addr = addr;
  labels[label_count].name = more_room(NULL, strlen(name) + 1, 1);
  strcpy(labels[label_count].name, name);
  label_count++;
}

const char *find_label(long addr) {
  for (int i = 0; i < label_count; i++) {
    if (labels[i].addr == addr) {
      return labels[i].name;
    }
  }
  return NULL;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Usage: %s <hexfile> [start_addr] [end_addr] [lstfile]\n",
           argv[0]);
    printf("       %s <hexfile> [lstfile]\n", argv[0]);
    printf("Disassembles a hex file produced by cross.fs\n");
    printf(
        "Optional: provide start/end addresses in hex (e.g., 0xc5a 0xc60)\n");
    printf("Optional: provide .lst file for symbol names\n");
    return 1;
  }

  // Initialize opcodes: the table vm_opcodes.h has for them
  word_node *opcodes = FORTH_WORDS;
  init_opcodes(opcodes);

  // Check if we have address range arguments: all of it, if there are none
  int start_addr = 0;
  int end_addr = -1;
  int lst_arg_index = 2;

  if (argc >= 4) {
    // Try to parse as addresses
    char *endptr;
    long start = strtol(argv[2], &endptr, 0);
    if (*endptr == '\0') {
      // Successfully parsed first argument as number
      long end = strtol(argv[3], &endptr, 0);
      if (*endptr == '\0') {
        // Both arguments are numbers
        start_addr = (int)start;
        end_addr = (int)end;
        lst_arg_index = 4;
      }
    }
  }

  // Load labels from .lst file if provided
  if (argc > lst_arg_index) {
    FILE *lst = fopen(argv[lst_arg_index], "r");
    if (lst) {
      // A line at a time, however long: a name is no longer than its line
      char *line = NULL, *name = NULL;
      size_t room = 0;
      while (getline(&line, &room, lst) > 0) {
        long addr;
        name = more_room(name, room, 1);
        if (sscanf(line, "%lx %s", &addr, name) == 2) {
          add_label(addr, name);
        }
      }
      free(line);
      free(name);
      fclose(lst);
    }
  }

  // First pass: load all instructions to memory, which has room for as
  // many as the file has
  uint16_t *memory = NULL;
  FILE *f = fopen(argv[1], "r");
  if (!f) {
    perror("Cannot open hex file");
    return 1;
  }

  char line[100];
  int addr = 0, room = 0;
  while (fgets(line, sizeof(line), f)) {
    // Remove newline
    line[strcspn(line, "\n")] = 0;

    // Each line has 8 hex chars = 2 16-bit words
    if (strlen(line) == 8) {
      uint32_t value;
      if (sscanf(line, "%8x", &value) == 1) {
        if (addr + 2 > room) {
          room = 2 * room + 2;
          memory = more_room(memory, room, sizeof(uint16_t));
        }
        // Each line has two 16-bit words
        // Last 4 hex chars = first word in memory
        memory[addr++] = value & 0xFFFF;
        // First 4 hex chars = second word in memory
        memory[addr++] = (value >> 16) & 0xFFFF;
      }
    }
  }
  fclose(f);

  // The words of the image's dictionary, from the newest (read_header in
  // vm_opcodes.c), each a label for the instruction its code starts at.
  printf("Dictionary entries:\n");
  const uint8_t *bytes = (const uint8_t *)memory;
  size_t image_len = (size_t)addr * 2;
  uint32_t head = newest_header(bytes, image_len);
  while (head) {
    uint64_t xt;
    char name[256];
    uint32_t before = read_header(bytes, image_len, head, &xt, name);
    if (!name[0]) break;
    printf("  '%s' @ 0x%04X: code at word 0x%04X (byte 0x%04X), link=0x%04X\n",
           name, head, (unsigned)xt, (unsigned)xt * 2, before);
    add_label((long)xt, name);
    head = before;
  }
  printf("\n");

  // Second pass: disassemble with labels
  int print_zeros = 0;
  // Limit to address range
  int loop_start = (start_addr < addr) ? start_addr : 0;
  int loop_end = (end_addr >= 0 && end_addr < addr) ? end_addr + 1 : addr;

  for (int i = loop_start; i < loop_end; i++) {
    uint16_t value = memory[i];
    instruction ins = *(instruction *)&value;
    char decoded[200];
    decode_instruction(decoded, ins, opcodes);

    // Check if this address has a label
    const char *addr_label = find_label(i);
    if (addr_label) {
      printf("\n%s:\n", addr_label);
    }
    printf("0x%04X: %s", i, decoded);

    // Check instruction type and add target info for calls/jumps: it is
    // printed after the instruction, however long the label is
    if ((value & 0x8000) == 0) {        // Not a literal (bit 15 = 0)
      uint16_t target = value & 0x1FFF;
      const char *label = find_label(target);
      const char *how = NULL;
      if ((value & 0x6000) == 0x4000) { // scall (bits 14:13 = 10)
        how = "CALL";
      } else if ((value & 0x6000) == 0x0000) { // ubranch (bits 14:13 = 00)
        if (label || target != 0) how = "JMP"; // halt is special (JMP 0)
      } else if ((value & 0x6000) == 0x2000) { // 0branch (bits 14:13 = 01)
        how = "JZ";
      }
      // bits 14:13 = 11 is ALU, no address decoding needed
      if (how && label) {
        printf(" ; %s %s", how, label);
      } else if (how) {
        printf(" ; %s 0x%04X", how, target);
      }
    }
    printf("\n");
  }
  
  return 0;
}