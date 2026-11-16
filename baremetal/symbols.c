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

// The symbols a kernel has from one place up to another, in the order they
// are there: how many, and the list, which is made here.
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
        after += 1 + strspn(after + 1, " \t");          // past the letter for its kind
        size_t len = strcspn(after, " \t\r\n");
        ONE_MORE(out, n, have);
        out[n].at = at;
        out[n].name = room_for(len + 1, 1);
        memcpy(out[n++].name, after, len);
    }
    fclose(in);
    free(line);
    ONE_MORE(out, n, have);                             // so that there is a list, if there are none
    qsort(out, n, sizeof(symbol), by_where);
    *list = out;
    return(n);
}

#endif //HEXAFORTH_SYMBOLS_C
