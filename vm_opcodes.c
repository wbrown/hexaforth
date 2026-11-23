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

// Returns true if a name is a phrase: a name with a space in it
// (vm_opcodes.h).
bool is_phrase(const char* repr) {
    return(strchr(repr, ' ') != NULL);
}

// Looks up the words starting at `words` in the table: the longest phrase
// they begin, or failing that the first word alone.  The instructions are
// written to `lookup`, and their number is returned.  `took` is set to the
// number of words used.  Returns 0 if the first word is not in the table.
#define PHRASE_WORDS 4                  // more than the number of words in any phrase of the table
uint8_t lookup_said(word_node* nodes, char* const* words, int count, instruction* lookup, int* took) {
    for(int n = count < PHRASE_WORDS ? count : PHRASE_WORDS; n > 0; n--) {
        char said[160];
        size_t len = 0;
        said[0] = '\0';
        for(int idx=0; idx < n && len < sizeof(said); idx++) {
            len += snprintf(said + len, sizeof(said) - len, "%s%s", idx ? " " : "", words[idx]);
        }
        if (len >= sizeof(said)) continue;
        uint8_t num_ins = lookup_word(nodes, said, lookup);
        if (num_ins) {
            *took = n;
            return(num_ins);
        }
    }
    *took = 0;
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

// Unpacks the name in a dictionary header into the string `out`.  `cells`
// points to the name's first cell, and `room` is the number of bytes from
// there to the end of the image.  Returns the number of bytes the name's
// cells take.  If the name does not end inside the image, or is longer
// than NAME_MOST characters, it is not a name: `out` is then empty and the
// result is 0.  `NAME_CODES_REPR` describes the codes.
int decode_name(const uint8_t* cells, size_t room, char* out) {
    const char* lot = NULL;     // the group the next code selects a character from, or NULL
    int len = 0;
    int bytes = 0;
    bool last = false;

    while (!last) {
        if (bytes + CELL_BYTES > room || len > NAME_MOST) {
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

// The dictionary of an image of `len` bytes.  A header is a sequence of
// cells: a link, which is the address of the next header to search, or 0
// in the last header; the word's execution token, with the top bit set if
// the word is immediate; and the name.  A search starts at the header
// whose address is in the first cell of the image's data, the cell at
// VAR_BASE, where cross.fs puts it.  An address is a cell number, but
// these functions identify a header by the byte offset where it starts.
//
// The links follow the order of a search, which is not the order of the
// headers in memory.  cross.fs links an image's headers by how often the
// source uses each name (headers-ordered), and a header created on the
// machine is linked in front of those.  So a link can point forward in
// memory as well as back.  A chain with more headers than the image has
// room for is not a dictionary: a header is at least three cells.
static size_t headers_read;     // headers read since first_header() was called

// Returns the byte offset of the header a search starts at, or 0 if the
// image has none.
uint32_t first_header(const uint8_t* image, size_t len) {
    uint64_t head = 0;
    headers_read = 0;
    if ((VAR_BASE + 1) * CELL_BYTES > len) return(0);
    memcpy(&head, image + VAR_BASE * CELL_BYTES, CELL_BYTES);
    return(head < len / CELL_BYTES ? head * CELL_BYTES : 0);
}

// Reads the header at byte offset `head`: its name, and its execution
// token with the top bit cleared.  Returns the byte offset of the next
// header in the chain, or 0 after the last.  A header that does not fit in
// the image, or whose link points to itself or outside the image, is not a
// header: the name is then empty and the result is 0.  The result is also
// 0 once more headers have been read, since first_header() was called,
// than the image has room for, so a chain that loops is walked only once.
uint32_t read_header(const uint8_t* image, size_t len, uint32_t head,
                     uint64_t* xt, char* name) {
    uint64_t link = 0;
    name[0] = '\0';
    *xt = 0;
    if (head + 2 * CELL_BYTES > len) return(0);
    memcpy(&link, image + head, CELL_BYTES);
    memcpy(xt, image + head + CELL_BYTES, CELL_BYTES);
    *xt &= ~(1ull << (CELL_BITS - 1));
    if (link == head / CELL_BYTES || link >= len / CELL_BYTES) return(0);
    decode_name(image + head + 2 * CELL_BYTES, len - head - 2 * CELL_BYTES, name);
    if (++headers_read > len / (3 * CELL_BYTES)) return(0);
    return(link * CELL_BYTES);
}

// Returns the byte offset just past the header at `head`: the byte after
// the last cell of its name.  This function and read_header() are the only
// code that knows a header is two cells followed by the name.
uint32_t header_end(const uint8_t* image, size_t len, uint32_t head) {
    char name[NAME_ROOM];
    if (head + 2 * CELL_BYTES > len) return(head);
    return(head + 2 * CELL_BYTES + decode_name(image + head + 2 * CELL_BYTES, len - head - 2 * CELL_BYTES, name));
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
        // Split the definition into words, each a pointer into the copy.
        char* words[sizeof(op->code) / 2];
        int count = 0;
        for(char* word = strtok(buffer, " "); word; word = strtok(NULL, " ")) words[count++] = word;

        for(int at=0; at < count; ) {
            instruction lookup[128];
            // If this is a word defined earlier in the table, or a phrase
            // of such words, append its instructions to this word.
            int took;
            uint8_t num_ins = lookup_said(opcodes, words + at, count - at, (instruction*)&lookup, &took);
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
                at += took;
                continue;
            }
            char* word = words[at++];
            if (!lookup_op_word(opcodes, word, (instruction*)&lookup)) {
                printf("ERROR: Lookup of %s failed!", word);
                return(false);
            }
            if (is_term(word)) {
                // a `term` field flushes our accumulated instruction.
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

