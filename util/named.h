//
// named.h - looks up the words of vm_opcodes.h by name, for any file that
// includes it.  Call init_opcodes() first.
//

#ifndef HEXAFORTH_NAMED_H
#define HEXAFORTH_NAMED_H

#include <stdint.h>
#include <string.h>
#include "../vm_opcodes.h"

// Returns the word with a name, or NULL if there is none.
static const word_node* word_named(const char* name) {
    for(const word_node* w = FORTH_WORDS; w->repr && strlen(w->repr); w++) if (!strcmp(w->repr, name)) return(w);
    return(NULL);
}

// Returns the name of an instruction: the word that is exactly that one
// instruction, or NULL if there is none.
static const char* word_of(uint16_t ins) {
    for(const word_node* w = FORTH_WORDS; w->repr && strlen(w->repr); w++) {
        if (w->num_ins == 1 && *(const uint16_t*)&w->ins[0] == ins) return(w->repr);
    }
    return(NULL);
}

// Returns the one instruction a word is, or 0 if there is no such word or
// the word is more than one instruction.
static uint16_t ins_named(const char* name) {
    const word_node* w = word_named(name);
    return(w && w->num_ins == 1 ? *(const uint16_t*)&w->ins[0] : 0);
}

// Reduces an instruction to the form the decompiler's table stores
// (forth/fused.fs): an ALU instruction without the return that an exit
// folded into it, which see.fs also removes.  Returns 0 for an instruction
// the table never has: a literal, a jump, a branch, a call, an instruction
// that returns any other way, or a no-op.
static uint16_t as_tabled(uint16_t w) {
    instruction ins = *(const instruction*)&w;
    if (ins.lit.lit_f || ins.alu.op_type != OP_TYPE_ALU) return(0);
    if (ins.alu.r_eip) {
        const uint16_t leaves = ins_named("exit");
        if (ins.alu.rstack != ((const instruction*)&leaves)->alu.rstack) return(0);
        ins.alu.r_eip = 0;
        ins.alu.rstack = 0;
    }
    const uint16_t is = *(const uint16_t*)&ins;
    return(is == ins_named("noop") ? 0 : is);
}

#endif // HEXAFORTH_NAMED_H
