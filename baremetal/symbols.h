//
// symbols.h - finds the addresses of a kernel's own symbols, for the tools
// that include it.
//
// The build keeps the output of llvm-nm beside a kernel (KERNEL.symbols):
// one line for each symbol, with its address in hexadecimal, a letter for
// its kind, and its name.
//

#ifndef HEXAFORTH_SYMBOLS_H
#define HEXAFORTH_SYMBOLS_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../util/room.h"

// Returns the address of a symbol.  Exits if the kernel has no such
// symbol, because no caller can continue without it.
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
        if (after == line) continue;                    // an undefined symbol, which has no address on its line
        after += strspn(after, " \t");
        if (!*after) continue;
        after += 1 + strspn(after + 1, " \t");          // skip the letter for its kind
        if (!strncmp(after, name, len) && strcspn(after, " \t\r\n") == len) {
            fclose(in);
            free(line);
            return(at);
        }
    }
    fprintf(stderr, "%s has no symbol %s\n", symbols, name);
    exit(EXIT_FAILURE);
}

// A symbol: its address and its name.
typedef struct {
    uint64_t at;
    char* name;
} symbol;

static int by_where(const void* a, const void* b) {
    const symbol* x = a;
    const symbol* y = b;
    if (x->at != y->at) return(x->at < y->at ? -1 : 1);
    return(strcmp(x->name, y->name));
}

// Finds the symbols a kernel has at addresses from `from` up to, but not
// including, `to`.  `*list` receives them, sorted by address, in allocated
// memory.  Returns their number.
static long symbols_between(const char* symbols, uint64_t from, uint64_t to, symbol** list) {
    FILE* in = fopen(symbols, "r");
    if (in == NULL) {
        perror(symbols);
        exit(EXIT_FAILURE);
    }
    symbol* out = NULL;
    long n = 0, have = 0;
    char* line = NULL;
    long room = 0;
    while (line_of(in, &line, &room)) {
        char* after;
        uint64_t at = strtoull(line, &after, 16);
        if (after == line || at < from || at >= to) continue;
        after += strspn(after, " \t");
        if (!*after) continue;
        after += 1 + strspn(after + 1, " \t");          // skip the letter for its kind
        size_t len = strcspn(after, " \t\r\n");
        ONE_MORE(out, n, have);
        out[n].at = at;
        out[n].name = room_for(len + 1, 1);
        memcpy(out[n++].name, after, len);
    }
    fclose(in);
    free(line);
    ONE_MORE(out, n, have);                             // allocate the list even if no symbol was found
    qsort(out, n, sizeof(symbol), by_where);
    *list = out;
    return(n);
}

#endif //HEXAFORTH_SYMBOLS_H
