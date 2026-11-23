//
// typed.h - reads the output of `typed read` (bench/typed.c, which
// describes a line of it) and collects the ALU instructions the nucleus
// compiled from the typed code.
//

#ifndef HEXAFORTH_TYPED_H
#define HEXAFORTH_TYPED_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "room.h"

#define EVERY_MADE (1 << 16)            // one entry for each instruction

typedef struct {
    long times[EVERY_MADE];             // the number of times the nucleus compiled each instruction
    char* in_run[EVERY_MADE];           // the first run each instruction was in: the word and the address
} made_typed;

// Collects the ALU instructions the nucleus compiled, which follow the
// third bar of each line.  Returns the number of different instructions,
// or 0 if the file cannot be read.
static long made_of_typed(const char* typed, made_typed* m) {
    FILE* in = fopen(typed, "r");
    if (!in) { perror(typed); return(0); }
    char* line = NULL;
    long room = 0, made = 0;
    while (line_of(in, &line, &room)) {
        size_t all = strcspn(line, " ");
        const char* part = line + all;          // search after the word, because a word's name can have a bar in it
        for(int n=0; n < 3 && part; n++) part = strchr(part + 1, '|');
        const char* end = part ? strchr(part + 1, '|') : NULL;
        for(const char* at = part ? part + 1 : NULL; at; ) {
            char* after;
            long ins = strtol(at, &after, 16);
            if (after == at || (end && after > end)) break;
            at = after;
            if ((ins & 0xe000) != 0x6000) continue;     // skip a literal, a jump, a branch or a call
            if (!m->times[ins]++) {
                made++;
                text_more(&m->in_run[ins], "%.*s", (int)all, line);
            }
        }
    }
    fclose(in);
    free(line);
    return(made);
}

#endif // HEXAFORTH_TYPED_H
