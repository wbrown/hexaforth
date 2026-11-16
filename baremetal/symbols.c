//
// symbols.c - where a kernel's own symbols are, for the tools that have
// this in them.
//
// The build keeps beside a kernel what llvm-nm says of it (KERNEL.symbols):
// a line a symbol, with where it is in hexadecimal, a letter for its kind,
// and its name.
//

#ifndef HEXAFORTH_SYMBOLS_C
#define HEXAFORTH_SYMBOLS_C

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../util/room.c"

// Where a symbol is.  It stops if the kernel has no such symbol: whoever
// asks cannot go on without it.
static uint64_t symbol_at(const char* symbols, const char* name) {
    FILE* in = fopen(symbols, "r");
    if (in == NULL) {
        perror(symbols);
        exit(EXIT_FAILURE);
    }
    char* line = NULL;
    long room = 0;
    size_t len = strlen(name);
    while (line_of(in, &line, &room)) {
        char* after;
        uint64_t at = strtoull(line, &after, 16);
        if (after == line) continue;                    // a symbol that is nowhere, which has no place written
        after += strspn(after, " \t");
        if (!*after) continue;
        after += 1 + strspn(after + 1, " \t");          // past the letter for its kind
        if (!strncmp(after, name, len) && strcspn(after, " \t\r\n") == len) {
            fclose(in);
            free(line);
            return(at);
        }
    }
    fprintf(stderr, "%s has no symbol %s\n", symbols, name);
    exit(EXIT_FAILURE);
}

#endif //HEXAFORTH_SYMBOLS_C
