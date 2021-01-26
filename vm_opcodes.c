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

void report_opcode(instruction* ins, word_node* opcodes, int num_words) {
    static int last_reported = -1;
    char out[160];
    decode_instruction(out,
                       *ins,
                       opcodes);
    if (num_words != last_reported) {
        last_reported = num_words;
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
        for(int i=0; i<=input_len; i++) {
           if (input[i]==' ' || input[i] == '\0') {
                // We replace spaces with null bytes to indicate to C that it's
                // the termination of the string.
                buffer[i]='\0';
                if (strlen(word)) {
                    instruction lookup[8];
                    if (lookup_op_word(opcodes, word, (instruction*)&lookup)) {
                        // if we're a `term` or `code` field, we need to flush our
                        // accumulated instruction.
                        uint8_t num_ins = lookup_word(opcodes, word, (instruction*)&lookup);
                        if (num_ins) {
                            for(int ins_idx=0; ins_idx < num_ins; ins_idx++) {
                                instruction idxed_ins = lookup[ins_idx];
                                opcodes[num_words].ins[op_idx+ins_idx] = idxed_ins;
                                report_opcode(&idxed_ins, opcodes, num_words);
                            }
                            op_idx += num_ins;
                            opcodes[num_words].num_ins = op_idx;
                            opcodes[num_words].repr = op->repr;
                            opcodes[num_words].type = op->type;
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
    if (ins.lit.lit_f) {
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

