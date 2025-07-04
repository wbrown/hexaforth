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

  // Parse dictionary by walking the chain
  // Dictionary format: link(2), name_len(1), name(n), padding, code (inline)
  printf("Dictionary entries:\n");

  // Start at the beginning of dictionary at word 0x2000
  uint16_t dict_addr = 0x2000;
  int entry_count = 0;

  while (dict_addr < addr && entry_count < 100) { // Safety limit
    uint16_t link = memory[dict_addr];
    if (dict_addr > 0x2000 && link == 0)
      break; // End of dictionary

    if (dict_addr < addr - 1) {
      uint16_t link = memory[dict_addr];
      uint8_t name_len =
          memory[dict_addr + 1] & 0xFF; // Low byte of next word

      // Extract the actual name from memory
      char extracted_name[64] = {0};
      // Name starts in high byte of word containing length
      int byte_pos = 0;
      // First character is in high byte of the length word
      if (name_len > 0) {
        extracted_name[0] = (memory[dict_addr + 1] >> 8) & 0xFF;
        byte_pos = 1;
      }
      // Rest of name continues in following words
      while (byte_pos < name_len && byte_pos < 63) {
        uint16_t word_offset = 2 + (byte_pos - 1) / 2;
        uint16_t word = memory[dict_addr + word_offset];
        if ((byte_pos - 1) & 1) {
          extracted_name[byte_pos++] = (word >> 8) & 0xFF; // High byte
        } else {
          extracted_name[byte_pos++] = word & 0xFF; // Low byte
        }
      }

      // Calculate where code starts: after link(1 word) + name_len(1 byte) +
      // name + padding Total header size in bytes: 2 + 1 + name_len + padding
      // Padding to make total bytes even (word aligned)
      uint16_t header_bytes = 2 + 1 + name_len;
      if (header_bytes & 1)
        header_bytes++; // Add padding if odd
      uint16_t code_addr = dict_addr + header_bytes / 2;
      // The value at code_addr is a word address (after execution token consistency fix)
      uint16_t code_word_addr = memory[code_addr];
      uint16_t code_byte_addr = code_word_addr * 2;

      printf(
          "  '%s' @ 0x%04X: code at word 0x%04X (byte 0x%04X), link=0x%04X\n",
          extracted_name, dict_addr, code_word_addr, code_byte_addr, link);

      // Add this as a label
      add_label(code_word_addr, extracted_name);

      // Move to next entry - follow the chain backwards
      // The first entry (test_0) has link=0x0000
      // Subsequent entries have links pointing to previous entries
      // We need to find the next entry by scanning forward

      // Calculate next dictionary entry address
      uint16_t next_addr = code_addr + 1; // Skip past code pointer

      // Skip to next aligned dictionary entry
      uint16_t old_dict_addr = dict_addr;
      dict_addr = 0; // Reset to detect if we found a new entry

      while (next_addr < addr - 10) { // Leave some margin at end
        // Check if this looks like a dictionary entry
        if (memory[next_addr] >= 0x4000 || memory[next_addr] == 0) {
          // Looks like a link field
          dict_addr = next_addr;
          break;
        }
        next_addr++;
      }

      // If we didn't find a new entry or it's the same as before, we're done
      if (dict_addr == 0 || dict_addr <= old_dict_addr) {
        break;
      }
      entry_count++;
    } else {
      break;
    }
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