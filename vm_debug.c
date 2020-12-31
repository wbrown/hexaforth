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

void show_registers(int64_t T, int64_t N, int16_t R,
                    int16_t EIP, int16_t SP, int16_t RSP,
                    context *ctx) {
    printf("\nREGS: T=%lld N=%lld R=%d\tPTRS: EIP=%d SP=%d RSP=%d\n",
           T, N, R, EIP, SP, RSP);
}

void print_stack(int16_t SP, int64_t T, int64_t N, context *ctx) {
    printf("STACK[%d]:", SP);
    switch (SP) {
        case 0:
            break;
        case 1:
            dprintf(" %lld", T);
            break;
        default:
            for (int i = 0; i < SP - 2; i++) {
                dprintf(" %lld", ctx->DSTACK[i]);
            }
        case 2:
            dprintf(" %lld %lld", N, T);
            break;
    }
    dprintf("\n");
}
