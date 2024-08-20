//
// Created by Wes Brown on 1/12/21.
//

#include <stdio.h>
#include "../vm_opcodes.h"

void generate_basewords_fs(const char* path) {
    init_opcodes(FORTH_WORDS);
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

    word_node* curr_word = &FORTH_WORDS[0];
    while(curr_word->repr && strlen(curr_word->repr)) {
        char* decoded = instruction_to_str(curr_word->ins[0]);
        fprintf(out, ":: %-9s %s",
            curr_word->repr,
            decoded);
        free(decoded);
        if (curr_word->type == CODE) {
            for(int idx=1; idx < curr_word->num_ins; idx++) {
                fprintf(out, "\n");
                decoded = instruction_to_str(curr_word->ins[idx]);
                fprintf(out, "   %-9s %s", "", decoded);
                free(decoded);
            }
        }
        fprintf(out, " ;\n");
        curr_word++;
    }
    fclose(out);
}

int main(int argc, char *argv[]) {
    generate_basewords_fs(argv[1]);
    return(0);
}