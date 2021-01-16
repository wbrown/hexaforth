//
// Created by Wes Brown on 12/31/20.
//

#ifndef HEXAFORTH_VM_DEBUG_H
#define HEXAFORTH_VM_DEBUG_H

#include <stdio.h>
#include "vm_opcodes.h"
#include "vm.h"

static bool DEBUG_ENABLED = true;

#ifdef DEBUG
#define dprintf(...) fprintf(stderr, __VA_ARGS__);
#else
#define dprintf(...)
#endif

#if defined(DEBUG) || defined(TEST)
void decode_instruction(char* out, instruction ins, word_node words[]);
void debug_address(char* out, context* ctx, uint64_t addr);
void show_registers(int64_t T, int16_t R,
                    int16_t EIP, int16_t SP, int16_t RSP,
                    context *ctx);
void print_stack(int16_t SP, int64_t T, context *ctx, bool rstack);
void mini_stack(int16_t P, int64_t TOS, int64_t* stack, char* buf);

#endif

#endif //HEXAFORTH_VM_DEBUG_H
