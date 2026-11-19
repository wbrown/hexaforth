//
// Created by Wes Brown on 1/12/21.
//

#include <stdio.h>
#include "../vm_opcodes.h"

void generate_basewords_fs(const char* path) {
    init_opcodes(FORTH_WORDS);
    FILE* out = fopen(path, "w");

    fprintf(out, "\\ machine sizes, from vm_constants.h\n");
    fprintf(out, "#%d constant cell-bits\n", CELL_BITS);
    fprintf(out, "#%d constant addr-bits\n", ADDR_BITS);
    fprintf(out, "#%d constant stack-cells\n", STACK_CELLS);
    fprintf(out, "#%d constant var-base\n", (int)VAR_BASE);
    fprintf(out, "#%d constant disp-w\n", DISP_W);
    fprintf(out, "#%d constant disp-h\n", DISP_H);
    fprintf(out, "#%d constant disp-base\n", DISP_BASE);
    fprintf(out, "#%d constant disp-left-low\n", DISP_LEFT_LOW);
    fprintf(out, "#%d constant disp-ink-black\n", DISP_INK_BLACK);
    fprintf(out, "#%d constant disp-stride\n", DISP_STRIDE);
    fprintf(out, "#%d constant disp-banks\n", DISP_BANKS);
    fprintf(out, "#%d constant disp-bank-step\n", DISP_BANK_STEP);

    fprintf(out, "\n\\ what is where on the disk, from vm_constants.h\n");
    fprintf(out, "#%d constant disk-block\n", DISK_BLOCK);
    fprintf(out, "#%d constant disk-line\n", DISK_LINE);
    fprintf(out, "#%d constant disk-source-block\n", DISK_SOURCE_BLOCK);
    fprintf(out, "#%d constant disk-picture-block\n", DISK_PICTURE_BLOCK);
    fprintf(out, "#%d constant disk-names-block\n", DISK_NAMES_BLOCK);
    fprintf(out, "#%d constant disk-canvas-block\n", DISK_CANVAS_BLOCK);
    fprintf(out, "#%d constant disk-masks-block\n", DISK_MASKS_BLOCK);
    fprintf(out, "#%d constant disk-mask32-block\n", DISK_MASK32_BLOCK);
    fprintf(out, "#%d constant disk-bayer-block\n", DISK_BAYER_BLOCK);
    fprintf(out, "#%d constant disk-mask-width\n", DISK_MASK_WIDTH);
    fprintf(out, "#%d constant disk-mask-planes\n", DISK_MASK_PLANES);
    fprintf(out, "#%d constant disk-view-block\n", DISK_VIEW_BLOCK);
    fprintf(out, "#%d constant disk-view-blocks\n", DISK_VIEW_BLOCKS);
    fprintf(out, "#%d constant disk-entry\n", DISK_ENTRY);
    fprintf(out, "#%d constant disk-entry-type\n", DISK_ENTRY_TYPE);
    fprintf(out, "#%d constant disk-entry-first\n", DISK_ENTRY_FIRST);
    fprintf(out, "#%d constant disk-entry-blocks\n", DISK_ENTRY_BLOCKS);
    fprintf(out, "#%d constant disk-entry-where\n", DISK_ENTRY_WHERE);
    fprintf(out, "#%d constant disk-binary\n", DISK_BINARY);
    fprintf(out, "#%d constant disk-index\n", DISK_INDEX);

    // The characters of a name that are not letters, a lot to a table: the
    // characters as their numbers, and how many there are.
    static const char* name_tables[] = { "n-direct", "n-page1", "n-page2" };
    fprintf(out, "\n\\ the codes of a name, from vm_opcodes.h\n");
    for(int lot=0; lot < 3; lot++) {
        const char* chars = NAME_CODES_REPR[lot];
        fprintf(out, "create %s ", name_tables[lot]);
        for(int idx=0; idx < strlen(chars); idx++) {
            fprintf(out, " #%d c,", chars[idx]);
        }
        fprintf(out, "\n#%d constant %ss\n", (int)strlen(chars), name_tables[lot]);
    }

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