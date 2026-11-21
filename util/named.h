//
// named.h - a word of vm_opcodes.h by its name, for whatever has this in
// it.  The words are made ready first (init_opcodes).
//

#ifndef HEXAFORTH_NAMED_H
#define HEXAFORTH_NAMED_H

#include <stdint.h>
#include <string.h>
#include "../vm_opcodes.h"

// The word of that name, or none.
static const word_node* word_named(const char* name) {
    for(const word_node* w = FORTH_WORDS; w->repr && strlen(w->repr); w++) if (!strcmp(w->repr, name)) return(w);
    return(NULL);
}

// The name of an instruction: the word that is that one instruction, or
// none.
static const char* word_of(uint16_t ins) {
    for(const word_node* w = FORTH_WORDS; w->repr && strlen(w->repr); w++) {
        if (w->num_ins == 1 && *(const uint16_t*)&w->ins[0] == ins) return(w->repr);
    }
    return(NULL);
}

// The one instruction a word is, or nothing if there is no such word or
// it is more than one.
static uint16_t ins_named(const char* name) {
    const word_node* w = word_named(name);
    return(w && w->num_ins == 1 ? *(const uint16_t*)&w->ins[0] : 0);
}

#endif // HEXAFORTH_NAMED_H
