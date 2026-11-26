//
// glossary.c - lists the words that have headers, each with the stack
// comment and the description that the Forth source gives it.
//
//   glossary PUBLIC DIR MAIN.fs                  prints the glossary as Markdown
//   glossary PUBLIC DIR MAIN.fs --check DOC.md   checks the stack comments in DOC.md's tables
//
// PUBLIC is the .public file that cross.fs writes beside an image.  It
// lists the names that have headers, by the file that defines them.  DIR
// is the directory of the source files, and MAIN.fs is the file that
// PUBLIC calls "(the program)".
//
// How the source is read:
//
//   - `header NAME` or `header-imm NAME` gives a word its header.
//   - The word's stack comment is the first comment in parentheses that
//     contains `--`, between the header and the end of the first line of
//     the definition.
//   - Its description is the backslash comment directly after the stack
//     comment.
//   - A word made with `create` or `variable` has the stack comment
//     `( -- a )`, and one made with `constant` has `( -- x )`.  The
//     description of such a word is the backslash comment on its line.
//
// --check reads the table rows of DOC.md that have a name in backticks in
// the first cell and a stack comment in backticks in the second.  It fails
// if a row's stack comment differs from the source's, if a row's name has
// no header, or if DOC.md has no such row.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "room.h"
#include "lines.h"

typedef enum { WORD, VARIABLE, CONSTANT } word_kind;

typedef struct {
    char* name;
    const char* file;       // the source file that gives it its header
    char* stack;            // its stack comment, or NULL
    char* says;             // its description, or NULL
    int immediate;
    word_kind kind;
} word;

typedef struct {
    char* file;
    char** names;
    long n, room;
} public_file;

static word* words;             // the words the source gives headers to, in the order they were read
static long n_words, words_room;
static public_file* files;      // the files of PUBLIC, in its order
static long n_files, files_room;

// The Forth-2012 core word set, which a reader who knows Forth expects.
static const char* const CORE[] = {
    "!", "#", "#>", "#s", "'", "(", "*", "*/", "*/mod", "+", "+!", "+loop", ",", "-", ".", ".\"", "/", "/mod",
    "0<", "0=", "1+", "1-", "2!", "2*", "2/", "2@", "2drop", "2dup", "2over", "2swap", ":", ";", "<", "<#", "=",
    ">", ">body", ">in", ">number", ">r", "?dup", "@", "abort", "abort\"", "abs", "accept", "align", "aligned",
    "allot", "and", "base", "begin", "bl", "c!", "c,", "c@", "cell+", "cells", "char", "char+", "chars",
    "constant", "count", "cr", "create", "decimal", "depth", "do", "does>", "drop", "dup", "else", "emit",
    "environment?", "evaluate", "execute", "exit", "fill", "find", "fm/mod", "here", "hold", "i", "if",
    "immediate", "invert", "j", "key", "leave", "literal", "loop", "lshift", "m*", "max", "min", "mod", "move",
    "negate", "or", "over", "postpone", "quit", "r>", "r@", "recurse", "repeat", "rot", "rshift", "s\"", "s>d",
    "sign", "sm/rem", "source", "space", "spaces", "state", "swap", "then", "type", "u.", "u<", "um*", "um/mod",
    "unloop", "until", "variable", "while", "word", "xor", "[", "[']", "[char]", "]", NULL
};

static char* copy_of(const char* text, int len) {
    char* copy = room_for((size_t)len + 1, 1);
    memcpy(copy, text, (size_t)len);
    return(copy);
}

// Returns 1 if a comment in parentheses is a stack comment.
static int is_stack(const char* line, const piece* p) {
    for(int at = p->at; at + 1 < p->at + p->len; at++) {
        if (line[at] == '-' && line[at + 1] == '-') return(1);
    }
    return(0);
}

// Returns the text of a backslash comment, without the backslash.
static char* said(const char* line, const piece* p) {
    int at = p->at + 1, end = p->at + p->len;
    while (at < end && line[at] == ' ') at++;
    return(copy_of(line + at, end - at));
}

// Reads one source file and records the words it gives headers to.
// `open_from` is the first word whose definition has not started yet, or
// -1.  Several headers can come before one definition, and they share its
// stack comment.
static void read_source(const char* dir, const char* file) {
    char* path = NULL;
    text_more(&path, "%s/%s", dir, file);
    FILE* in = fopen(path, "r");
    if (!in) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    char* line = NULL;
    piece* pieces = NULL;
    long line_room = 0, piece_room = 0, open_from = -1;
    int brackets = 0;
    while (line_of(in, &line, &line_room)) {
        int n = pieces_of(line, &brackets, &pieces, &piece_room), defined = 0, commented = 0;
        for(int idx=0; idx < n; idx++) {
            const piece* p = &pieces[idx];
            if (p->kind == PLAIN && (is_word(line, p, "header") || is_word(line, p, "header-imm"))) {
                int at = p->at + p->first;
                while (at < p->at + p->len && line[at] == ' ') at++;
                ONE_MORE(words, n_words, words_room);
                word* w = &words[n_words];
                memset(w, 0, sizeof(*w));
                w->name = copy_of(line + at, p->at + p->len - at);
                w->file = file;
                w->immediate = is_word(line, p, "header-imm");
                if (open_from < 0) open_from = n_words;
                n_words++;
            } else if (open_from < 0) {
                continue;
            } else if (p->kind == PLAIN) {
                word_kind kind = WORD;
                if (is_word(line, p, "create") || is_word(line, p, "variable") || is_word(line, p, "2variable")) {
                    kind = VARIABLE;
                } else if (is_word(line, p, "constant")) {
                    kind = CONSTANT;
                } else if (!is_word(line, p, ":") && !is_word(line, p, ":noname") && !is_word(line, p, "callable")) {
                    continue;
                }
                if (!defined) for(long at = open_from; at < n_words; at++) words[at].kind = kind;
                defined = 1;
            } else if (p->kind == BRACKETS && !commented && is_stack(line, p)) {
                const piece* next = idx + 1 < n ? &pieces[idx + 1] : NULL;
                for(long at = open_from; at < n_words; at++) {
                    if (words[at].stack) continue;
                    words[at].stack = copy_of(line + p->at, p->len);
                    if (next && next->kind == TO_END) words[at].says = said(line, next);
                }
                commented = 1;
            } else if (p->kind == TO_END && defined) {
                for(long at = open_from; at < n_words; at++) {
                    if (words[at].kind != WORD && !words[at].says) words[at].says = said(line, p);
                }
            }
        }
        if (defined) open_from = -1;
    }
    fclose(in);
    free(line);
    free(pieces);
    free(path);
}

// Reads PUBLIC's first list: a line `FILE: COUNT` for each file, and then
// indented lines of that file's names.  The list ends at an empty line.
static void read_public(const char* path, const char* main_file) {
    FILE* in = fopen(path, "r");
    if (!in) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    char* line = NULL;
    long room = 0;
    line_of(in, &line, &room);                  // the heading
    while (line_of(in, &line, &room)) {
        int len = line_length(line);
        if (!len) break;
        if (line[0] != ' ') {
            const char* colon = strrchr(line, ':');
            if (!colon) break;
            ONE_MORE(files, n_files, files_room);
            public_file* f = &files[n_files++];
            memset(f, 0, sizeof(*f));
            f->file = line[0] == '(' ? copy_of(main_file, (int)strlen(main_file)) : copy_of(line, (int)(colon - line));
            continue;
        }
        if (!n_files) break;
        public_file* f = &files[n_files - 1];
        for(int at = 0; at < len;) {
            while (at < len && line[at] == ' ') at++;
            int end = at;
            while (end < len && line[end] != ' ') end++;
            if (end > at) {
                ONE_MORE(f->names, f->n, f->room);
                f->names[f->n++] = copy_of(line + at, end - at);
            }
            at = end;
        }
    }
    fclose(in);
    free(line);
    if (!n_files) {
        fprintf(stderr, "%s lists no file: it is not a .public file that cross.fs wrote\n", path);
        exit(EXIT_FAILURE);
    }
}

// Returns the word a file gives a header of that name, or NULL.  A name
// that a file defines twice, in the two branches of an [if], is the one
// that has a stack comment.
static const word* word_of(const char* file, const char* name) {
    const word* found = NULL;
    for(long idx=0; idx < n_words; idx++) {
        if (strcmp(words[idx].file, file) || strcasecmp(words[idx].name, name)) continue;
        if (words[idx].stack) return(&words[idx]);
        if (!found) found = &words[idx];
    }
    return(found);
}

static const char* stack_of(const word* w) {
    if (!w) return(NULL);
    if (w->stack) return(w->stack);
    return(w->kind == VARIABLE ? "( -- a )" : w->kind == CONSTANT ? "( -- x )" : NULL);
}

// Prints text as code in a table's cell.  A | in a cell is written \|.
static void cell(const char* text) {
    if (!text) return;
    putchar('`');
    for(const char* at = text; *at; at++) {
        if (*at == '|') putchar('\\');
        putchar(*at);
    }
    putchar('`');
}

static int has_header(const char* name) {
    for(long f = 0; f < n_files; f++) {
        for(long idx=0; idx < files[f].n; idx++) if (!strcasecmp(files[f].names[idx], name)) return(1);
    }
    return(0);
}

static void print_glossary(void) {
    long all = 0, with_stack = 0;
    printf("# The words\n\n");
    printf("Every word that has a header in the first machine's system, by the file that defines it and in "
           "the order the file defines them. The stack comment and the description are the ones in the "
           "source. `util/glossary.c` writes this file, and the target `words` runs it.\n\n");
    printf("An address `a` is the number of a cell, and `c-addr` is the address of a byte. A word marked "
           "immediate runs while a definition is being compiled, and its stack comment is what it does then.\n");
    for(long f = 0; f < n_files; f++) {
        printf("\n## %s\n\n| Word | Stack | Description |\n|---|---|---|\n", files[f].file);
        for(long idx=0; idx < files[f].n; idx++) {
            const word* w = word_of(files[f].file, files[f].names[idx]);
            const char* stack = stack_of(w);
            printf("| ");
            cell(files[f].names[idx]);
            printf(" | ");
            cell(stack);
            printf(" | %s%s%s |\n", w && w->immediate ? "immediate" : "",
                   w && w->immediate && w->says ? ": " : "", w && w->says ? w->says : "");
            all++;
            with_stack += stack != NULL;
        }
    }
    printf("\n## Forth-2012 core words that this system lacks\n\n");
    int lacks = 0;
    for(int idx=0; CORE[idx]; idx++) {
        if (has_header(CORE[idx])) continue;
        printf("%s`%s`", lacks++ ? " " : "", CORE[idx]);
    }
    printf("\n");
    fprintf(stderr, "glossary: %ld words have headers, and %ld of them have a stack comment\n", all, with_stack);
}

// Copies one cell of a table row: the text between backticks, with \|
// written back as |.  Returns NULL if the cell is not code.
static char* code_in(const char* from, const char* to) {
    while (from < to && *from == ' ') from++;
    while (to > from && to[-1] == ' ') to--;
    if (to - from < 2 || *from != '`' || to[-1] != '`') return(NULL);
    char* text = room_for((size_t)(to - from), 1);
    int len = 0;
    for(const char* at = from + 1; at < to - 1; at++) {
        if (at[0] == '\\' && at[1] == '|') continue;
        text[len++] = *at;
    }
    return(text);
}

// Returns 1 if two stack comments are the same once runs of spaces are
// taken as one space.
static int same_comment(const char* a, const char* b) {
    for(;;) {
        while (*a == ' ' && a[1] == ' ') a++;
        while (*b == ' ' && b[1] == ' ') b++;
        if (*a != *b) return(0);
        if (!*a) return(1);
        a++, b++;
    }
}

// Checks DOC's tables and returns the number of rows that are wrong.
static int check(const char* doc) {
    FILE* in = fopen(doc, "r");
    if (!in) {
        perror(doc);
        exit(EXIT_FAILURE);
    }
    char* line = NULL;
    long room = 0, rows = 0;
    int wrong = 0;
    while (line_of(in, &line, &room)) {
        if (line[0] != '|') continue;
        const char* bars[3];                    // the three unescaped bars that bound the first two cells
        int n = 0;
        for(const char* at = line; *at && n < 3; at++) {
            if (*at == '|' && (at == line || at[-1] != '\\')) bars[n++] = at;
        }
        if (n < 3) continue;
        char* name = code_in(bars[0] + 1, bars[1]);
        char* stack = code_in(bars[1] + 1, bars[2]);
        if (name && stack && strstr(stack, "--")) {
            const char* source = NULL;
            int found = 0, same = 0;
            for(long idx=0; idx < n_words; idx++) {
                if (strcasecmp(words[idx].name, name)) continue;
                found = 1;
                if (stack_of(&words[idx])) source = stack_of(&words[idx]);
                same |= source && same_comment(source, stack);
            }
            rows++;
            if (!found) {
                fprintf(stderr, "%s: %s has no header in the source\n", doc, name);
                wrong++;
            } else if (!same) {
                fprintf(stderr, "%s: %s is %s here, and %s in the source\n", doc, name, stack,
                        source ? source : "has no stack comment");
                wrong++;
            }
        }
        free(name);
        free(stack);
    }
    fclose(in);
    free(line);
    if (!rows) {
        fprintf(stderr, "%s has no table row with a word and its stack comment\n", doc);
        return(1);
    }
    printf("%s: %ld stack comments checked against the source, %d wrong\n", doc, rows, wrong);
    return(wrong);
}

int main(int argc, char *argv[]) {
    if (argc != 4 && !(argc == 6 && !strcmp(argv[4], "--check"))) {
        fprintf(stderr, "usage: %s PUBLIC DIR MAIN.fs [--check DOC.md]\n", argv[0]);
        return(EXIT_FAILURE);
    }
    read_public(argv[1], argv[3]);
    for(long f = 0; f < n_files; f++) read_source(argv[2], files[f].file);
    if (argc == 6) return(check(argv[5]) ? EXIT_FAILURE : EXIT_SUCCESS);
    print_glossary();
    return(EXIT_SUCCESS);
}
