//
// Created by Wes Brown on 1/12/21.
//

#include <stdio.h>
#include "../vm_opcodes.h"

void generate_basewords_fs(const char* path) {
    FILE* out = fopen(path, "w");

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

int main(int argc, char *argv[]) {
    generate_basewords_fs(argv[1]);
    return(0);
}