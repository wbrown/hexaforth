//
// Created by Wes Brown on 12/31/20.
//

#include "compiler.h"
#include "vm_opcodes.h"
#include "vm_debug.h"
#include "vm.h"

void debug_instruction(instruction ins) {
    if(*(uint16_t*)&ins == 0) return;
    const char* forth_word = lookup_opcode(ins);
    char* ins_r = instruction_to_str(ins);
    printf("0x%04hx => %-10s => %s\n",
           *(uint16_t*)&ins,
           forth_word ? forth_word : "",
           ins_r);
    free(ins_r);
}

void generate_basewords_fs() {
    FILE* out = fopen("../basewords.fs", "w");


    //context *ctx = calloc(sizeof(context), 1);
    forth_op* curr_op = &INS_FIELDS[0];

    while(strlen(curr_op->repr)) {
        if (curr_op->type == COMMT) {
            fprintf(out, "\n\\ %s\n", curr_op->repr);
        } else {
            fprintf(out, ": %-10s h# %04x %s%s;\n",
                   curr_op->repr,
                   *(uint16_t*)&curr_op->ins,
                   (curr_op->type != INPUT) ? "or " : "",
                   (curr_op->type != FIELD &&
                    curr_op->type != INPUT) ? "tcode, " : "" );
        }
        curr_op++;
    }
    fprintf(out, "\n\\ words\n");

    forth_define* curr_word = &FORTH_OPS[0];
    while(strlen(curr_word->repr)) {
        if (curr_word->type != CODE) {
            fprintf(out, ":: %-9s %s ;\n",
                   curr_word->repr,
                   curr_word->code);
        }
        curr_word++;
    }
    fclose(out);
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
