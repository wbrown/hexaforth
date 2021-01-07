//
// Created by Wes Brown on 12/31/20.
//

#include "vm_debug.h"
#include "vm_opcodes.h"

// Basic equivalency operation, leveraging the fact that our encoded
// instructions can be compared as unsigned integers.
bool ins_eq(instruction a, instruction b) {
    if (*(uint16_t *)&a ==
                      *(uint16_t *)&b) {
        return true;
    } else {
        return false;
    }
}

// Given a string representing a `word` opcode,  search our opcodes for a
// match.  If matched, return the corresponding instruction structure, else
// null.
instruction* lookup_field(const char* word) {
    int idx = 0;
    while (strlen(INS_FIELDS[idx].repr)) {
        // printf("LOOKUP_FIELD: %s\n", INS_FIELDS[idx].repr);
        char* repr = INS_FIELDS[idx].repr;
        if (strcmp(repr, word) == 0 &&
                (INS_FIELDS[idx].type == FIELD ||
                 INS_FIELDS[idx].type == INPUT ||
                 INS_FIELDS[idx].type == TERM)) {
            return(&INS_FIELDS[idx].ins[0]);
        }
        idx++;
    }
    return NULL;
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

instruction* lookup_word(const char* word) {
    int idx = 0;
    while (FORTH_WORDS[idx].repr && strlen(FORTH_WORDS[idx].repr)) {
        char *repr = FORTH_WORDS[idx].repr;
        if (strcmp(repr, word) == 0) {
            return (&FORTH_WORDS[idx].ins[0]);
        }
        idx++;
    }
    return NULL;
}

instruction* interpret_imm(const char* word) {
    char* decode_end;
    int64_t num = strtoll(word, &decode_end, 10);
    if (*decode_end) {
        printf("ERROR: '%s' not found!\n", word);
        return(NULL);
    } else if (num>=0 && num<=4096) {
        instruction *literal = calloc(sizeof(instruction), 2);
        literal->lit.lit_f = true;
        literal->lit.lit_v = llabs(num);
        return(literal);
    } else {
        printf("ERROR: Only positive literals under 4096 are supported.");
        return(NULL);
    }
}

instruction* lookup_op_word(char* word) {
    instruction* lookup = lookup_field(word);
    if (lookup) return(lookup);
    lookup = lookup_word(word);
    if (lookup) return(lookup);
    lookup = interpret_imm(word);
    return(lookup);
}

bool init_opcodes() {
    int num_words = 0;
    int last_reported = -1;
    forth_define* op = &FORTH_OPS[0];
    uint16_t instruction_acc;
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
                    instruction* lookup_ref = lookup_op_word(word);
                    if (lookup_ref) {
                        instruction lookup = *lookup_ref;
                        instruction_acc = instruction_acc | *(uint16_t*)&lookup;
                        if (lookup.lit.lit_f && lookup.lit.lit_v) free(lookup_ref);
                        // if we're a `term` or `code` field, we need to flush our
                        // accumulated instruction.
                        if (is_term(word) || lookup_word(word)) {
                            instruction* flush = (instruction *)&instruction_acc;
                            FORTH_WORDS[num_words].repr = op->repr;
                            FORTH_WORDS[num_words].ins[op_idx] = *flush;
#ifdef DEBUG
                            if (num_words != last_reported) {
                                last_reported = num_words;
                                printf("OPCODE[%4d]: ", num_words);
                            } else {
                                printf("              ");
                            }
                            debug_instruction(*(instruction*)&instruction_acc);
#endif // DEBUG
                            op_idx++;
                            instruction_acc = 0;
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
const char* lookup_opcode(instruction ins) {
    int idx = 0;
    while (FORTH_WORDS[idx].repr && strlen(FORTH_WORDS[idx].repr)) {
        if (ins_eq(ins, FORTH_WORDS[idx].ins[0]) &&
                *(uint16_t*)(&FORTH_WORDS[idx].ins[1]) == 0) {
            return FORTH_WORDS[idx].repr;
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
                 "%6d  %-7s %-8s %25s",
                 ins.lit.lit_v,
                 LIT_SHIFT_REPR[(uint8_t)ins.lit.lit_shifts],
                 ins.lit.lit_add ? "imm+" : "",
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
                     "        %-7s %-8s %-6s %-4s %-4s %-4s %-4s",
                     input_mux,
                     alu_ops_repr,
                     output_mux,
                     dstack_repr,
                     rstack_repr,
                     ins.alu.r_eip ? "RET" : "",
                     class_repr);
        } else {
            asprintf(&ret_str,
                     "0x%0.4x  %38s %-4s",
                     ins.jmp.target,
                     "",
                     OP_TYPE_REPR[ins.jmp.op_type]);
        }
    }
    return(ret_str);
}

