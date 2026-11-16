//
// named.c - a word of vm_opcodes.h by its name, for whatever has this in
// it.  It is had after vm_opcodes.h, and the words are made ready first
// (init_opcodes).
//

#ifndef HEXAFORTH_NAMED_C
#define HEXAFORTH_NAMED_C

#include <string.h>

// The word of that name, or none.
static const word_node* word_named(const char* name) {
    for(const word_node* w = FORTH_WORDS; w->repr && strlen(w->repr); w++) if (!strcmp(w->repr, name)) return(w);
    return(NULL);
}

// The one instruction a word is, or nothing if there is no such word or
// it is more than one.
static uint16_t ins_named(const char* name) {
    const word_node* w = word_named(name);
    return(w && w->num_ins == 1 ? *(const uint16_t*)&w->ins[0] : 0);
}

#endif // HEXAFORTH_NAMED_C
