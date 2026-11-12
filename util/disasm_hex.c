#include "vm.h"
#include "vm_opcodes.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Label tracking for calls
typedef struct label {
  uint16_t addr;
  char name[64];
} label;

label labels[1024];
int label_count = 0;

void add_label(uint16_t addr, const char *name) {
  if (label_count < 1024) {
    labels[label_count].addr = addr;
    strncpy(labels[label_count].name, name, 63);
    labels[label_count].name[63] = '\0';
    label_count++;
  }
}

const char *find_label(uint16_t addr) {
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

  // Initialize opcodes
  word_node opcodes[1024];
  init_opcodes(opcodes);

  // Check if we have address range arguments
  int start_addr = 0;
  int end_addr = 65535;
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
      char line[256];
      while (fgets(line, sizeof(line), lst)) {
        uint16_t addr;
        char name[64];
        if (sscanf(line, "%hx %s", &addr, name) == 2) {
          add_label(addr, name);
        }
      }
      fclose(lst);
    }
  }

  // First pass: load all instructions to memory
  uint16_t memory[65536] = {0};
  FILE *f = fopen(argv[1], "r");
  if (!f) {
    perror("Cannot open hex file");
    return 1;
  }

  char line[100];
  int addr = 0;
  while (fgets(line, sizeof(line), f)) {
    // Remove newline
    line[strcspn(line, "\n")] = 0;

    // Each line has 8 hex chars = 2 16-bit words
    if (strlen(line) == 8) {
      uint32_t value;
      if (sscanf(line, "%8x", &value) == 1) {
        // Each line has two 16-bit words
        // Last 4 hex chars = first word in memory
        memory[addr++] = value & 0xFFFF;
        // First 4 hex chars = second word in memory
        memory[addr++] = (value >> 16) & 0xFFFF;
      }
    }
  }
  fclose(f);

  // Walk the dictionary from the newest header, whose address is the last
  // nonzero word of the image.  A header is cells: link, xt (top bit:
  // immediate), then the name as a count byte and characters.
  printf("Dictionary entries:\n");
  const uint8_t *bytes = (const uint8_t *)memory;
  int last = 0;
  for (int i = 0x2000; i < addr && i < 0x8000; i++)
    if (memory[i]) last = i;
  uint32_t head = last ? memory[last] : 0;
  for (int n = 0; head && n < 1000; n++) {
    uint64_t link = 0, xt = 0;
    memcpy(&link, bytes + head, CELL_BYTES);
    memcpy(&xt, bytes + head + CELL_BYTES, CELL_BYTES);
    xt &= ~(1ull << (CELL_BITS - 1));
    char name[256];
    uint8_t len = bytes[head + 2 * CELL_BYTES];
    memcpy(name, bytes + head + 2 * CELL_BYTES + 1, len);
    name[len] = '\0';
    printf("  '%s' @ 0x%04X: code at word 0x%04X (byte 0x%04X), link=0x%04X\n",
           name, head, (unsigned)xt, (unsigned)xt * 2, (unsigned)link);
    add_label((uint16_t)xt, name);
    head = (uint32_t)link;
  }
  printf("\n");

  // Second pass: disassemble with labels
  int print_zeros = 0;
  // Limit to address range
  int loop_start = (start_addr < addr) ? start_addr : 0;
  int loop_end = (end_addr < addr) ? end_addr + 1 : addr;

  for (int i = loop_start; i < loop_end; i++) {
    uint16_t value = memory[i];
    instruction ins = *(instruction *)&value;
    char decoded[200];
    decode_instruction(decoded, ins, opcodes);

    // Check instruction type and add target info for calls/jumps
    if ((value & 0x8000) == 0) {        // Not a literal (bit 15 = 0)
      if ((value & 0x6000) == 0x4000) { // scall (bits 14:13 = 10)
        uint16_t target = value & 0x1FFF;
        const char *label = find_label(target);
        char enhanced[256];
        if (label) {
          sprintf(enhanced, "%s ; CALL %s", decoded, label);
        } else {
          sprintf(enhanced, "%s ; CALL 0x%04X", decoded, target);
        }
        strcpy(decoded, enhanced);
      } else if ((value & 0x6000) == 0x0000) { // ubranch (bits 14:13 = 00)
        uint16_t target = value & 0x1FFF;
        const char *label = find_label(target);
        char enhanced[256];
        if (label) {
          sprintf(enhanced, "%s ; JMP %s", decoded, label);
        } else if (target != 0) {
          sprintf(enhanced, "%s ; JMP 0x%04X", decoded, target);
        } else {
          strcpy(enhanced, decoded); // halt is special (JMP 0)
        }
        strcpy(decoded, enhanced);
      } else if ((value & 0x6000) == 0x2000) { // 0branch (bits 14:13 = 01)
        uint16_t target = value & 0x1FFF;
        const char *label = find_label(target);
        char enhanced[256];
        if (label) {
          sprintf(enhanced, "%s ; JZ %s", decoded, label);
        } else {
          sprintf(enhanced, "%s ; JZ 0x%04X", decoded, target);
        }
        strcpy(decoded, enhanced);
      }
      // bits 14:13 = 11 is ALU, no address decoding needed
    }

    // Check if this address has a label
    const char *addr_label = find_label(i);
    if (addr_label) {
      printf("\n%s:\n", addr_label);
    }
    printf("0x%04X: %s\n", i, decoded);
  }
  
  return 0;
}