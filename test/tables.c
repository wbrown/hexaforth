//
// The system's tables, read from its source: tables.h says what this is for.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tables.h"

static const char* forth;       // where the Forth is

void tables_are(const char* dir) {
    forth = dir;
}

// A file of the Forth, whole; and where it is, for what is said of it.
// Both are made here.
static char* source_of(const char* file, char** where) {
    char* path = malloc(strlen(forth) + strlen(file) + 2);
    if (path == NULL) {
        fprintf(stderr, "there is no room for the name of %s\n", file);
        exit(EXIT_FAILURE);
    }
    sprintf(path, "%s/%s", forth, file);
    *where = path;
    FILE* in = fopen(path, "rb");
    if (in == NULL) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    fseek(in, 0, SEEK_END);
    size_t len = ftell(in);
    fseek(in, 0, SEEK_SET);
    char* text = calloc(len + 1, 1);
    if (fread(text, 1, len, in) != len) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    fclose(in);
    return(text);
}

// A word as a number, if it is one.
static bool number(const char* word, int64_t* n) {
    char* end;
    int base = *word == '$' ? 16 : 10;
    if (*word == '$' || *word == '#') word++;
    bool less = *word == '-';
    if (less) word++;
    if (!*word) return(false);
    uint64_t much = strtoull(word, &end, base);     // all of a cell of 64 bits, the top bit too
    *n = less ? -(int64_t)much : (int64_t)much;
    return(!*end);
}

int table(const char* file, const char* name, int64_t** numbers, int count) {
    char* path;
    char* source = source_of(file, &path);
    size_t whole = strlen(source);
    char* word = malloc(2 * (whole + 1));           // a word of it is no longer than all of it; and the word after
    int64_t* out = malloc((whole / 2 + 1) * sizeof(int64_t));   // a number is a word and a space, at the least
    if (word == NULL || out == NULL) {
        fprintf(stderr, "there is no room for the numbers of %s\n", path);
        exit(EXIT_FAILURE);
    }
    char* next = word + whole + 1;
    const char* at = NULL;
    int used = 0;
    for(const char* from = source; !at && sscanf(from, "%s%n", word, &used) == 1; ) {
        from += used;
        if (!strcmp(word, "create") && sscanf(from, "%s%n", next, &used) == 1 && !strcmp(next, name)) {
            at = from + used;
        }
    }
    if (!at) {
        fprintf(stderr, "%s has no table called %s\n", path, name);
        exit(EXIT_FAILURE);
    }
    int n = 0;
    while (sscanf(at, "%s%n", word, &used) == 1) {
        at += used;
        if (!strcmp(word, "font[")) continue;       // what a font's numbers are between
        int64_t x;
        if (!number(word, &x) || sscanf(at, "%s%n", next, &used) != 1 || next[strlen(next) - 1] != ',') break;
        at += used;
        out[n++] = x;
    }
    if (count && n != count) {
        fprintf(stderr, "%s: %s is %d numbers, and is to be %d\n", path, name, n, count);
        exit(EXIT_FAILURE);
    }
    free(source);
    free(word);
    free(path);
    *numbers = out;
    return(n);
}

font font_named(const char* name) {
    int64_t* numbers;
    char file[strlen(name) + sizeof(".fs")];        // a font is in the file named for it
    sprintf(file, "%s.fs", name);
    int count = table(file, name, &numbers, 0);
    font f = { 0 };
    if (count >= 5) {
        f.rows = (int)numbers[0], f.first = (int)numbers[1], f.count = (int)numbers[2], f.across = (int)numbers[3];
        f.starts = numbers + 5;
        f.strike = f.starts + f.count + 1;
    }
    if (count < 5 || count != 5 + f.count + 1 + f.rows * f.across) {
        fprintf(stderr, "%s/%s: %s is not a font as that file says one is\n", forth, file, name);
        exit(EXIT_FAILURE);
    }
    return(f);
}

int glyph_of(const font* f, int c, int* width) {
    int code = c - f->first;
    if (code < 0 || code >= f->count) code = 0;
    *width = (int)(f->starts[code + 1] - f->starts[code]);
    return((int)f->starts[code]);
}

bool strike_pixel(const font* f, int x, int row) {
    return((uint64_t)f->strike[row * f->across + x / 64] >> (63 - x % 64) & 1);
}

pattern pattern_named(const char* file, const char* name) {
    pattern p;
    int64_t* rows;
    const int count = sizeof(p.rows) / sizeof(p.rows[0]);
    table(file, name, &rows, count);
    memcpy(p.rows, rows, sizeof(p.rows));
    free(rows);
    return(p);
}

bool pattern_pixel(const pattern* p, int x, int y) {
    return(p->rows[y & 7] >> (7 - (x & 7)) & 1);
}
