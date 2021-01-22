//
// Created by Wes Brown on 1/22/21.
//

#include "stack.h"

void print_stack(int16_t SP, int64_t T, context *ctx, bool rstack) {
    int64_t* stack;

    if (rstack) {
        stack = ctx->RSTACK;
        fprintf(stderr, "RSTACK[%4d]:", SP);
    } else {
        stack = ctx->DSTACK;
        fprintf(stderr, "DSTACK[%4d]:", SP);
    }

    switch (SP) {
        case 0:
            break;
        default:
            for (int i = 0; i < SP - 1; i++) {
                fprintf(stderr, " %lld (" HX "%llx)", stack[i], stack[i]);
            }
        case 1:
            fprintf(stderr, " %lld (" HX "%llx)", T, T);
            break;
    }
    fprintf(stderr, "\n");
}

