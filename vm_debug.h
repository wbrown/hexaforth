//
// Created by Wes Brown on 12/31/20.
//

#ifndef HEXAFORTH_VM_DEBUG_H
#define HEXAFORTH_VM_DEBUG_H

#include "vm_opcodes.h"
#include "vm.h"

#ifdef DEBUG
#define dprintf(...) printf(__VA_ARGS__)
#else
#define dprintf(...)
#endif

void debug_instruction(instruction ins);
void show_registers(int64_t T, int64_t N, int16_t R,
                    int16_t EIP, int16_t SP, int16_t RSP,
                    context *ctx);
void print_stack(int16_t SP, int64_t T, int64_t N, context *ctx);

#endif //HEXAFORTH_VM_DEBUG_H
