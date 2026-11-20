//
// lines.h - a line of Forth source, as what lays source out reads it: its
// pieces, and which of them are comments or strings, so that nothing cuts
// a line inside one, or between a word that reads the next and that word.
// util/wrap.c fits source to the lines of a block, and mkdisk lays it on
// the disk; both read a line here.
//
// A piece is a word; or a word that reads the word after it, with that
// word (`create cst`, `[char] (`, `d# 15`); or a string with the word that
// starts it (`," IN->"`, `s" dup"`); or a comment in brackets; or a \ and
// the rest of its line.  A comment in brackets may go on over lines, and
// whoever reads lines one after another says whether a line starts in one.
//

#ifndef HEXAFORTH_LINES_H
#define HEXAFORTH_LINES_H

#include <ctype.h>
#include <string.h>
#include <strings.h>
#include "room.h"

typedef enum { PLAIN, STRING, BRACKETS, TO_END } piece_kind;

typedef struct {
    int at, len;            // where it starts in the line, and how long it is
    int first;              // how long its first word is
    int gap;                // the spaces before it
    piece_kind kind;
} piece;

// The words that read the word after them, so that the two stay on one
// line.  A word left out of this is found when what was cut does not
// build.
static const char* const READS_NEXT[] = {
    ":", "::", "header", "header-imm", "create", "variable", "constant", "value",
    "to", "is", "defer", "char", "[char]", "'", "[']", "t'", "postpone",
    "d#", "h#", "c#", "include", "hdr,", "callable", "in-place", NULL
};

static int is_word(const char* line, const piece* p, const char* word) {
    return((int)strlen(word) == p->first && !strncasecmp(line + p->at, word, p->first));
}

static int in_list(const char* line, int at, int len, const char* const* list) {
    for(int idx=0; list[idx]; idx++) {
        if ((int)strlen(list[idx]) == len && !strncasecmp(line + at, list[idx], len)) return(1);
    }
    return(0);
}

static int is_comment(const piece* p) {
    return(p->kind == BRACKETS || p->kind == TO_END);
}

// How long a line is without the spaces at its end, or a newline.
static int line_length(const char* line) {
    int len = (int)strlen(line);
    while (len && isspace((unsigned char)line[len - 1])) len--;
    return(len);
}

// The pieces of a line, made here, and how many.  `*brackets` is true if
// the line starts in a comment in brackets, and is left true if it ends in
// one.
static int pieces_of(const char* line, int* brackets, piece** pieces, long* room) {
    int len = line_length(line), n = 0, idx = 0;
    while (idx < len) {
        int gap = 0;
        while (idx < len && line[idx] == ' ') { idx++; gap++; }
        if (idx >= len) break;
        ONE_MORE(*pieces, n, *room);
        piece* p = &(*pieces)[n++];
        p->at = idx;
        p->gap = gap;
        p->kind = PLAIN;
        if (*brackets) {                            // the rest of a comment from a line before
            const char* shut = memchr(line + idx, ')', len - idx);
            p->kind = BRACKETS;
            p->len = p->first = shut ? (int)(shut - line) + 1 - idx : len - idx;
            *brackets = !shut;
            idx += p->len;
            continue;
        }
        int end = idx;
        while (end < len && line[end] != ' ') end++;
        p->first = p->len = end - idx;
        if (in_list(line, idx, p->first, (const char* const[]){ "(", ".(", NULL })) {
            const char* shut = memchr(line + end, ')', len - end);
            p->kind = BRACKETS;
            p->len = shut ? (int)(shut - line) + 1 - idx : len - idx;
            *brackets = !shut;
        } else if (p->first == 1 && line[idx] == '\\') {
            p->kind = TO_END;
            p->len = len - idx;
        } else if (p->first > 1 && line[end - 1] == '"') {
            const char* shut = end + 1 < len ? memchr(line + end + 1, '"', len - end - 1) : NULL;
            p->kind = STRING;
            p->len = shut ? (int)(shut - line) + 1 - idx : len - idx;
        } else if (in_list(line, idx, p->first, READS_NEXT)) {
            int next = end;
            while (next < len && line[next] == ' ') next++;
            while (next < len && line[next] != ' ') next++;
            p->len = next - idx;
        }
        idx += p->len;
    }
    return(n);
}

// What a word does to how deep the line is in if, begin, do and case:
// 1 opens, -1 closes, 2 is between the two (else, while), 0 is none.
static int control(const char* line, const piece* p) {
    if (p->kind != PLAIN || p->first != p->len) return(0);
    static const char* const OPENS[] = { "if", "begin", "do", "?do", "case", "of", NULL };
    static const char* const CLOSES[] = { "then", "repeat", "until", "again", "loop", "+loop", "endof", "endcase", NULL };
    static const char* const BETWEEN[] = { "else", "while", NULL };
    if (in_list(line, p->at, p->len, OPENS)) return(1);
    if (in_list(line, p->at, p->len, CLOSES)) return(-1);
    if (in_list(line, p->at, p->len, BETWEEN)) return(2);
    return(0);
}

// Where a line's code is, from line to line (read_line, which says how
// many pieces the line is): in a definition, after a
// header and before what it names, in a comment in brackets, between [if]
// and [then], or in a table that a word ending in [ starts and one
// starting with ] ends (font[ ]font, picture[ ]picture).  Nowhere, it is
// between one thing and the next.
typedef struct {
    int defining, named, brackets, conditions, tables;
} where;

static int read_line(const char* line, where* w, piece** pieces, long* room) {
    static const char* const NAMES[] = { ":", "::", ":noname", "create", "variable", "2variable",
                                         "constant", "value", NULL };
    int n = pieces_of(line, &w->brackets, pieces, room);
    for(int idx=0; idx < n; idx++) {
        const piece* p = &(*pieces)[idx];
        if (p->kind != PLAIN) continue;
        const char* word = line + p->at;
        if (in_list(line, p->at, p->first, NAMES)) w->named = 0;
        if (is_word(line, p, ":") || is_word(line, p, "::") || is_word(line, p, ":noname")) w->defining = 1;
        else if (is_word(line, p, "header") || is_word(line, p, "header-imm")) w->named = 1;
        else if (is_word(line, p, ";")) w->defining = 0;
        else if (is_word(line, p, "[if]")) w->conditions++;
        else if (is_word(line, p, "[then]")) w->conditions--;
        else if (p->first > 1 && word[p->first - 1] == '[' && word[0] != '[') w->tables++;
        else if (p->first > 1 && word[0] == ']' && word[p->first - 1] != '[') w->tables--;
    }
    return(n);
}

static int between_things(const where* w) {
    return(!w->defining && !w->named && !w->brackets && w->conditions <= 0 && w->tables <= 0);
}

// Between two entries of a table, or between two things: a table is read
// a line at a time and goes on from one block to the next, so a block may
// end inside one, where it may not end inside a definition.
static int between_entries(const where* w) {
    return(!w->defining && !w->named && !w->brackets && w->conditions <= 0);
}

// A line that has nothing but comments: those after it are read as if it
// were not there.  A line of nothing is not one.
static int only_comments(const char* line, int brackets) {
    piece* pieces = NULL;
    long room = 0;
    int n = pieces_of(line, &brackets, &pieces, &room), all = n > 0;
    for(int idx=0; idx < n; idx++) all &= is_comment(&pieces[idx]);
    free(pieces);
    return(all);
}

// Whether the last character of a line is in a comment that runs to the
// end of the line and started before it, so that what comes next in a
// block, on the line after, is not read with it.
static int ends_in_comment(const char* line, int columns, int brackets) {
    piece* pieces = NULL;
    long room = 0;
    int n = pieces_of(line, &brackets, &pieces, &room);
    int in = n && pieces[n - 1].kind == TO_END && pieces[n - 1].at < columns - 1;
    free(pieces);
    return(in);
}

#endif // HEXAFORTH_LINES_H
