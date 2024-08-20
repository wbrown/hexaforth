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
void debug_address(char* out, context* ctx, uint64_t addr);
void show_registers(int64_t T, int16_t R,
                    int16_t EIP, int16_t SP, int16_t RSP,
                    context *ctx);
void mini_stack(int16_t P, int64_t TOS, int64_t* stack, char* buf);

// Debug monitor commands

typedef struct {
    const char* repr;
    const char* notation;
    void* fn;
} debug_monitor_op;

void show_stack(context* ctx, int64_t T, int64_t R);
void hex_dump(context* ctx, int64_t T, int64_t R);
void set_eip(context* ctx, int64_t T, int64_t R);
void get_eip(context* ctx, int64_t T, int64_t R);

static debug_monitor_op debug_vm_ops[] = {
    {".s", "( -- )", &show_stack},
    {"xd", "( -- )", &hex_dump},
    {"eip!", "( addr -- EIP: addr)", &set_eip},
    {"eip@", "( -- addr )", &get_eip},
};
#endif

#endif //HEXAFORTH_VM_DEBUG_H
