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

// A picture drawn in the source (picture[, cross.fs): rows of # and ., each
// the rest of a line after row|, in pieces side by side with `beside`
// between them, up to ]picture; a line that starts with \ is a comment.
// It is the numbers px, would lay it down as: 64 pixels to a number, the
// leftmost the highest, a row after a row.  A font's strike may be drawn
// so (spin.fs).  How many numbers it came to; `from` is left after it.
static int picture(const char* path, const char** from, int64_t* out) {
    enum { DOWN = 64, ACROSS = 1024 };
    static char px[DOWN][ACROSS];
    int have[DOWN] = { 0 }, rows = -1, row = 0, used = 0;
    char word[16];
    const char* at = *from;
    for(;;) {
        if (sscanf(at, "%15s%n", word, &used) != 1) {
            fprintf(stderr, "%s: a picture has no ]picture\n", path);
            exit(EXIT_FAILURE);
        }
        at += used;
        if (!strcmp(word, "]picture")) break;
        if (!strcmp(word, "beside")) {
            if (rows < 0) rows = row;
            row = 0;
            continue;
        }
        const char* end = at + strcspn(at, "\n");
        if (!strcmp(word, "row|") && row < DOWN) {
            for(const char* c = at; c < end; c++) {
                if ((*c == '#' || *c == '.') && have[row] < ACROSS) px[row][have[row]++] = *c == '#';
            }
            row++;
        } else if (strcmp(word, "\\")) {
            fprintf(stderr, "%s: %s in a picture, which is rows and comments\n", path, word);
            exit(EXIT_FAILURE);
        }
        at = end;
    }
    if (rows < 0) rows = row;
    int n = 0;
    for(int r=0; r < rows; r++) {
        for(int x=0; x < have[0]; x += 64) {
            uint64_t pixels = 0;
            for(int i=0; i < 64; i++) pixels = pixels << 1 | (x + i < have[r] ? (uint64_t)px[r][x + i] : 0);
            out[n++] = (int64_t)pixels;
        }
    }
    *from = at;
    return(n);
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
        if (!strcmp(word, "\\")) {                  // a comment, to the end of its line
            at += strcspn(at, "\n");
            continue;
        }
        if (!strcmp(word, "font[")) continue;       // what a font's numbers are between
        if (!strcmp(word, "picture[")) {            // a strike that is drawn, and not said as numbers
            n += picture(path, &at, out + n);
            continue;
        }
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
