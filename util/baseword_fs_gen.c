//
// Created by Wes Brown on 1/12/21.
//

#include <stdio.h>
#include "../vm_opcodes.h"

// The machine's sizes and what is where on its disk, from vm_constants.h:
// a constant each for cross.fs, and (sizes-words, below) a word each for
// the nucleus.  A name with no value ends a group.
static const struct { const char* name; long value; } SIZES[] = {
    {"\\ machine sizes, from vm_constants.h"},
    {"cell-bits", CELL_BITS}, {"addr-bits", ADDR_BITS}, {"stack-cells", STACK_CELLS},
    {"var-base", (long)VAR_BASE}, {"disp-w", DISP_W}, {"disp-h", DISP_H}, {"disp-base", DISP_BASE},
    {"disp-left-low", DISP_LEFT_LOW}, {"disp-ink-black", DISP_INK_BLACK}, {"disp-stride", DISP_STRIDE},
    {"disp-banks", DISP_BANKS}, {"disp-bank-step", DISP_BANK_STEP}, {"krom-bytes", KROM_BYTES},
    {"\\ what is where on the disk, from vm_constants.h"},
    {"disk-block", DISK_BLOCK}, {"disk-line", DISK_LINE}, {"disk-source-block", DISK_SOURCE_BLOCK},
    {"disk-picture-block", DISK_PICTURE_BLOCK}, {"disk-names-block", DISK_NAMES_BLOCK},
    {"disk-canvas-block", DISK_CANVAS_BLOCK}, {"disk-masks-block", DISK_MASKS_BLOCK},
    {"disk-mask32-block", DISK_MASK32_BLOCK}, {"disk-bayer-block", DISK_BAYER_BLOCK},
    {"disk-mask-width", DISK_MASK_WIDTH}, {"disk-mask-planes", DISK_MASK_PLANES},
    {"disk-view-block", DISK_VIEW_BLOCK}, {"disk-view-blocks", DISK_VIEW_BLOCKS},
    {"disk-gray-block", DISK_GRAY_BLOCK}, {"disk-gray-blocks", DISK_GRAY_BLOCKS},
    {"disk-entry", DISK_ENTRY}, {"disk-entry-type", DISK_ENTRY_TYPE}, {"disk-entry-first", DISK_ENTRY_FIRST},
    {"disk-entry-blocks", DISK_ENTRY_BLOCKS}, {"disk-entry-where", DISK_ENTRY_WHERE},
    {"disk-binary", DISK_BINARY}, {"disk-source", DISK_SOURCE}, {"disk-index", DISK_INDEX},
    {NULL}};

void generate_basewords_fs(const char* path) {
    init_opcodes(FORTH_WORDS);
    FILE* out = fopen(path, "w");

    for(int n=0; SIZES[n].name; n++) {
        if (SIZES[n].name[0] == '\\') fprintf(out, "%s%s\n", n ? "\n" : "", SIZES[n].name);
        else fprintf(out, "#%ld constant %s\n", SIZES[n].value, SIZES[n].name);
    }

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

    // Every baseword as a word of the nucleus too, so that source the
    // nucleus compiles can say it: a header of its name on a word that is
    // the baseword, which the nucleus compiles in place.  cross.fs lets no
    // word of the program take a macro's name, so the word is bw- and the
    // name, and the header is given the name (forth/cross-words.fs runs
    // this).
    fprintf(out, "\n\\ the words as the nucleus's\n:: baseword-words\n");
    for(curr_word = &FORTH_WORDS[0]; curr_word->repr && strlen(curr_word->repr); curr_word++) {
        fprintf(out, "    s\" bw-header %s\" evaluate\n", curr_word->repr);
    }
    // and the sizes, each what cross.fs has (HOST)
    for(int n=0; SIZES[n].name; n++) {
        if (SIZES[n].name[0] == '\\') continue;
        fprintf(out, "    s\" bw-size %s\" evaluate\n", SIZES[n].name);
    }
    fprintf(out, ";\n");
    fclose(out);
}

int main(int argc, char *argv[]) {
    generate_basewords_fs(argv[1]);
    return(0);
}