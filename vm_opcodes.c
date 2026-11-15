//
// Created by Wes Brown on 12/31/20.
//

#include "vm_debug.h"
#include "vm_opcodes.h"

// Basic equivalency operation, leveraging the fact that our encoded
// instructions can be compared as unsigned integers.
bool ins_eq(instruction a, instruction b) {
    uint16_t a_int = *(uint16_t *)&a;
    uint16_t b_int = *(uint16_t *)&b;
    if (a_int == b_int) {
        return true;
    } else {
        return false;
    }
}

// Given a string representing a `word` opcode,  search our opcodes for a
// match.  If matched, return the corresponding instruction structure, else
// null.
bool lookup_field(const char* word, instruction* lookup) {
    int idx = 0;
    while (strlen(INS_FIELDS[idx].repr)) {
        // printf("LOOKUP_FIELD: %s\n", INS_FIELDS[idx].repr);
        char* repr = INS_FIELDS[idx].repr;
        if (strcmp(repr, word) == 0 &&
                (INS_FIELDS[idx].type == FIELD ||
                 INS_FIELDS[idx].type == INPUT ||
                 INS_FIELDS[idx].type == TERM)) {
            *lookup = INS_FIELDS[idx].ins[0];
            return(true);
        }
        idx++;
    }
    return(false);
}

bool is_term(const char* word) {
    int idx = 0;
    while (strlen(INS_FIELDS[idx].repr)) {
        char* repr = INS_FIELDS[idx].repr;
        if (strcmp(repr, word) == 0 &&
            (INS_FIELDS[idx].type == TERM)) {
            return(true);
        }
        idx++;
    }
    return(false);
}

uint8_t lookup_word(word_node* nodes, const char* word, instruction* lookup) {
    int idx = 0;
    while (nodes[idx].repr && strlen(nodes[idx].repr)) {
        word_node forth_word = nodes[idx];
        char *repr = forth_word.repr;
        if (strcmp(repr, word) == 0) {
            int ins_idx;
            for(ins_idx=0; ins_idx < nodes[idx].num_ins; ins_idx++) {
                lookup[ins_idx] = nodes[idx].ins[ins_idx];
            }
            for(; ins_idx < 8; ins_idx++) {
                *(uint16_t*)&lookup[ins_idx] = 0;
            }
            return(nodes[idx].num_ins);
        }
        idx++;
    }
    return(0);
}

bool interpret_imm(const char* word, instruction* literal) {
    char* decode_end;
    int64_t num = strtoll(word, &decode_end, 10);
    if (*decode_end) {
        printf("ERROR: '%s' not found!\n", word);
        return(false);
    } else if (num>=0 && num<=4096) {
        *literal = (instruction){};
        literal->lit.lit_f = true;
        literal->lit.lit_v = llabs(num);
        return(true);
    } else {
        printf("ERROR: Only positive literals under 4096 are supported.");
        return(false);
    }
}

bool lookup_op_word(word_node* nodes, char* word, instruction* lookup) {
    if (lookup_field(word, lookup)) return true;
    if (lookup_word(nodes, word, lookup)) return true;
    return(interpret_imm(word, lookup));
}

// Given where the name in a dictionary header starts, and how many bytes
// there are from there to the end of the image, write the name out as a
// string, and return how many bytes of the header the name's cells are.
// What does not end inside the image, or goes on longer than a name does,
// is not a name: then the string is empty and nothing is returned.
// See `NAME_CODES_REPR` for what the codes of a name are.
int decode_name(const uint8_t* cells, size_t room, char* out) {
    const char* lot = NULL;     // which lot the next code is one of, if any
    int len = 0;
    int bytes = 0;
    bool last = false;

    while (!last) {
        if (bytes + CELL_BYTES > room || len > 128) {
            out[0] = '\0';
            return(0);
        }
        uint64_t cell = 0;
        memcpy(&cell, cells + bytes, CELL_BYTES);
        bytes += CELL_BYTES;
        last = (cell >> (CELL_BITS - 1)) & 1;
        for (int idx = 0; idx < CELL_BITS / 5; idx++) {
            uint8_t code = (cell >> (5 * idx)) & 31;
            if (lot) {
                out[len++] = (code && code <= strlen(lot)) ? lot[code - 1] : '?';
                lot = NULL;
            } else if (code >= 30) {
                lot = NAME_CODES_REPR[code - 29];
            } else if (code >= 27) {
                out[len++] = NAME_CODES_REPR[0][code - 27];
            } else if (code) {
                out[len++] = 'a' + code - 1;
            }
        }
    }
    out[len] = '\0';
    return(bytes);
}

// The dictionary of an image of `len` bytes.  A header is cells: the address
// of the header before it, which is nothing for the first; the word's
// execution token, with the top bit set if the word is immediate; and the
// name.  The address of the newest header is the first cell of the image's
// data, the cell at VAR_BASE, where cross.fs puts it.  An address is the
// number of a cell, and a header is said here by the byte it starts at.
uint32_t newest_header(const uint8_t* image, size_t len) {
    uint64_t head = 0;
    if ((VAR_BASE + 1) * CELL_BYTES > len) return(0);
    memcpy(&head, image + VAR_BASE * CELL_BYTES, CELL_BYTES);
    return(head < len / CELL_BYTES ? head * CELL_BYTES : 0);
}

// Read the header at byte `head`: its name, and its execution token without
// the top bit.  Returns the byte the header before it starts at, which is
// nothing for the first.  What does not fit in the image, or has a link
// that does not go back, is not a header: then the name is empty and
// nothing is returned.
uint32_t read_header(const uint8_t* image, size_t len, uint32_t head,
                     uint64_t* xt, char* name) {
    uint64_t link = 0;
    name[0] = '\0';
    *xt = 0;
    if (head + 2 * CELL_BYTES > len) return(0);
    memcpy(&link, image + head, CELL_BYTES);
    memcpy(xt, image + head + CELL_BYTES, CELL_BYTES);
    *xt &= ~(1ull << (CELL_BITS - 1));
    if (link >= head / CELL_BYTES) return(0);
    decode_name(image + head + 2 * CELL_BYTES, len - head - 2 * CELL_BYTES, name);
    return(link * CELL_BYTES);
}

void decode_instruction(char* out, instruction ins, word_node words[]) {
    const char* forth_word = lookup_opcode(words, ins);
    char* ins_r = instruction_to_str(ins);
    sprintf(out,
            HX "%04hx => %-10s => %s",
            *(uint16_t*)&ins,
            forth_word ? forth_word : "",
            ins_r);
    free(ins_r);
}

// Global flag to suppress opcode output
int suppress_opcode_output = 1;

void report_opcode(const instruction* ins, word_node* opcodes, int num_words) {
    if (suppress_opcode_output) return;

    static int last_reported = -1;
    char out[640];
    decode_instruction(out,
                       *ins,
                       opcodes);
    if (num_words != last_reported) {
        last_reported = num_words;
        printf("%s\n", opcodes[num_words].repr);
        printf("OPCODE[%4d]: %s\n", num_words, out);
    } else {
        printf("              %s\n", out);
    }
}

bool init_opcodes(word_node* opcodes) {
    int num_words = 0;
    int last_reported = -1;
    forth_define* op = &FORTH_OPS[0];
    uint16_t instruction_acc = 0;

    // size our opcode list
    /* int opcode_ct = 0;
    while(strlen(op->repr)) {
        opcode_ct++;
        op++;
    } */
    //FORTH_WORDS=(word_node*)malloc(sizeof(word_node) * opcode_ct);
    // op = &FORTH_OPS[0];

    while(strlen(op->repr)) {
        int curr_word = num_words;
        uint8_t op_idx = 0;
        const char* input = op->code;
        // Size our input buffer, and copy it, as it could be a constant string
        // that can't be modified.
        uint64_t input_len = strlen(input) + 1;
        char* buffer = malloc(input_len);
        memcpy(buffer, input, input_len);
        // Our word point into the buffer and are adjusted as we go.
        char* word = buffer;
        // Are we scanning a string literal?
        bool string = false;

        // Loop through the characters in our buffer.
        for(int i=0; i<input_len; i++) {
           if (input[i]==' ' || input[i] == '\0') {
                // We replace spaces with null bytes to indicate to C that it's
                // the termination of the string.
                buffer[i]='\0';
                if (strlen(word)) {
                    instruction lookup[128];
                    if (lookup_op_word(opcodes, word, (instruction*)&lookup)) {
                        // if we're a `term` or `code` field, we need to flush our
                        // accumulated instruction.
                        uint8_t num_ins = lookup_word(opcodes, word, (instruction*)&lookup);
                        if (num_ins) {
                            opcodes[num_words].repr = op->repr;
                            opcodes[num_words].type = op->type;
                            for(int ins_idx=0; ins_idx < num_ins; ins_idx++) {
                                instruction idxed_ins = lookup[ins_idx];
                                opcodes[num_words].ins[op_idx+ins_idx] = idxed_ins;
                                report_opcode(&idxed_ins, opcodes, num_words);
                            }
                            op_idx += num_ins;
                            opcodes[num_words].num_ins = op_idx;
                            instruction_acc = 0;
                        } else if (is_term(word)) {
                            instruction_acc = instruction_acc | *(uint16_t*)&lookup;
                            instruction flush = *(instruction *)&instruction_acc;
                            opcodes[num_words].ins[op_idx] = flush;
                            opcodes[num_words].num_ins = ++op_idx;
                            opcodes[num_words].repr = op->repr;
                            opcodes[num_words].type = op->type;
                            instruction_acc = 0;
                            report_opcode(&flush, opcodes, num_words);
                        } else {
                            instruction_acc = instruction_acc | *(uint16_t*)&lookup;
                        }
                    } else {
                        printf("ERROR: Lookup of %s failed!", word);
                        return(false);
                    }
                }
                // Adjust our word pointer to the beginning of the next word.
                word = &buffer[i+1];
            }
        }
        free(buffer);
        num_words++;
        op = &(FORTH_OPS[num_words]);
    }
    return(true);
}


// Given an instruction, look up our table of instructions, and if a match
// is found, return the Forth representation of the opcode, else null.
const char* lookup_opcode(word_node words[], instruction ins) {
    int idx = 0;
    while (words[idx].repr && strlen(words[idx].repr)) {
        if (ins_eq(ins, words[idx].ins[0]) &&
                *(uint16_t*)(&words[idx].ins[1]) == 0) {
            return words[idx].repr;
        }
        idx++;
    }
    return false;
}

// Given an instruction, generates a human readable decoding of it as a
// string and return it.
char* instruction_to_str(instruction ins) {
    char *ret_str;
    if (ins.lit.lit_f && ins.lit.lit_shifts == 3) {
        asprintf(&ret_str, "%6d  %-7s %-8s %25s %-7s",
                 ins.lit.lit_v, "var", ins.lit.lit_add ? "!" : "@", "", "imm");
    } else if (ins.lit.lit_f) {
        asprintf(&ret_str,
                 "%6d  %-7s %-8s %25s %-7s",
                 ins.lit.lit_v,
                 LIT_SHIFT_REPR[(uint8_t)ins.lit.lit_shifts],
                 ins.lit.lit_add ? "imm+" : "",
                 "",
                 "imm");
    } else {
        if (ins.alu.op_type == OP_TYPE_ALU) {
            int8_t dstack = ins.alu.dstack;
            int8_t rstack = ins.alu.rstack;
            uint8_t dstack_idx = dstack < 0 ? abs(dstack) + 1 : dstack ;
            uint8_t rstack_idx = rstack < 0 ? abs(rstack) + 1 : rstack ;
            const char* input_mux = INPUT_MUX_REPR[ins.alu.in_mux];
            const char* output_mux = OUTPUT_MUX_REPR[ins.alu.out_mux];
            const char* alu_ops_repr = ALU_OPS_REPR[ins.alu.alu_op];
            const char* dstack_repr = DSTACK_REPR[dstack_idx];
            const char* rstack_repr = RSTACK_REPR[rstack_idx];
            const char* class_repr = OP_TYPE_REPR[ins.alu.op_type];

            asprintf(&ret_str,
                     "        %-7s %-9s %-7s %-6s %-4s %-4s %-7s",
                     input_mux,
                     alu_ops_repr,
                     output_mux,
                     dstack_repr,
                     rstack_repr,
                     ins.alu.r_eip ? "RET" : "",
                     class_repr);
        } else {
            asprintf(&ret_str,
                     "$%0.4x   %42s %-7s",
                     ins.jmp.target,
                     "",
                     OP_TYPE_REPR[ins.jmp.op_type]);
        }
    }
    return(ret_str);
}

