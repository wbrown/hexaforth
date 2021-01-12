//
// Created by Wes Brown on 12/31/20.
//

#include "vm_opcodes.h"
#include "vm_debug.h"
#include "vm.h"

void decode_instruction(char* out, instruction ins) {
    if(*(uint16_t*)&ins == 0) return;
    const char* forth_word = lookup_opcode(ins);
    char* ins_r = instruction_to_str(ins);
    sprintf(out,
            "0x%04hx => %-10s => %s",
            *(uint16_t*)&ins,
            forth_word ? forth_word : "",
            ins_r);
    free(ins_r);
}

void debug_address(char* decoded, context* ctx, uint64_t addr) {
    char* meta = ctx->meta[addr];
    char* target_meta = NULL;
    char decoded_ins[160];

    instruction ins = *(instruction*)&ctx->memory[addr];
    if (!ins.lit.lit_f && ins.alu.op_type != OP_TYPE_ALU) {
        target_meta = ctx->meta[ins.jmp.target];
    }
    decode_instruction(decoded_ins, ins);
    sprintf(decoded,
            "%-20s %-77s %-20s",
            meta ? meta : "",
            decoded_ins,
            target_meta ? target_meta : "");
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
        printf("RSTACK[%4d]:", SP);
    } else {
        stack = ctx->DSTACK;
        printf("DSTACK[%4d]:", SP);
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
