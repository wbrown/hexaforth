//
// Created by Wes Brown on 12/31/20.
//

#include "vm_debug.h"
#include "vm.h"

void debug_instruction(instruction ins) {
    const char* forth_word = lookup_opcode(ins);
    char* ins_r = instruction_to_str(ins);
    printf("0x%04hx", *(uint16_t*)&ins);
    if (forth_word) printf(" => %s", forth_word);
    printf(" => %s\n", ins_r);
    free(ins_r);
}

void show_registers(int64_t T, int16_t R,
                    int16_t EIP, int16_t SP, int16_t RSP,
                    context *ctx) {
    printf("\nREGS: {T=%lld R=%d} PTRS: {EIP=%d SP=%d RSP=%d}\n",
           T, R, EIP, SP, RSP);
}

void print_stack(int16_t SP, int64_t T, context *ctx, bool rstack) {
    int64_t* stack;

    if (rstack) {
        stack = ctx->RSTACK;
        printf("RSTACK[%d]:", SP);
    } else {
        stack = ctx->DSTACK;
        printf("DSTACK[%d]:", SP);
    }

    switch (SP) {
        case 0:
            break;
        default:
            for (int i = 0; i < SP - 1; i++) {
                printf(" %lld", stack[i]);
            }
        case 1:
            printf(" %lld", T);
            break;
    }
    printf("\n");
}
