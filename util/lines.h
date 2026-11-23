//
// lines.h - parses a line of Forth source, for the tools that lay source
// out.  It splits the line into pieces and marks which are comments or
// strings, so that a tool never breaks a line inside one, or between a
// word that reads the next word and that word.  util/wrap.c fits source to
// the lines of a block, and mkdisk puts it on a disk.  Both read lines
// with this.
//
// A piece is one of:
//
//   a word
//   a word that reads the next word, together with that word
//     (`create cst`, `[char] (`, `d# 15`)
//   a string, together with the word that starts it (`," IN->"`, `s" dup"`)
//   a comment in parentheses
//   a backslash and the rest of its line
//
// A comment in parentheses can continue over several lines.  A caller that
// reads lines in order keeps track of whether a line starts inside one.
//

#ifndef HEXAFORTH_LINES_H
#define HEXAFORTH_LINES_H

#include <ctype.h>
#include <string.h>
#include <strings.h>
#include "room.h"

typedef enum { PLAIN, STRING, BRACKETS, TO_END } piece_kind;

typedef struct {
    int at, len;            // its offset in the line, and its length
    int first;              // the length of its first word
    int gap;                // the number of spaces before it
    piece_kind kind;
} piece;

// The words that read the next word, so that the two are kept on one line.
// If a word is missing from this list, the mistake shows when the
// rewrapped source fails to build.
static const char* const READS_NEXT[] = {
    ":", "::", "header", "header-imm", "create", "variable", "constant", "value",
    "to", "is", "defer", "char", "[char]", "'", "[']", "t'", "postpone",
    "d#", "h#", "c#", "include", "hdr,", "callable", "in-place", ";into", NULL
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

// Returns the length of a line without trailing spaces or a newline.
static int line_length(const char* line) {
    int len = (int)strlen(line);
    while (len && isspace((unsigned char)line[len - 1])) len--;
    return(len);
}

// Splits a line into pieces and returns how many there are.  The list is
// allocated or grown here.  `*brackets` must be true if the line starts
// inside a comment in parentheses, and is set to whether the line ends
// inside one.
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
        if (*brackets) {                            // the rest of a comment that began on an earlier line
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

// Returns how a word changes the nesting depth of if, begin, do and case:
// 1 if it opens a level, -1 if it closes one, 2 if it comes between the
// two (else, while), and 0 otherwise.
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

// The parsing state carried from one line to the next.  read_line()
// updates it and returns the number of pieces in the line.  It records
// whether the source is inside a definition, after a `header` and before
// the definition it names, inside a comment in parentheses, between [if]
// and [then], or inside a table that a word ending in [ starts and a word
// starting with ] ends (font[ ]font, picture[ ]picture).  When none of
// these holds, the source is between one item and the next.
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

// Returns 1 between two entries of a table, or between two items.  A
// table is read a line at a time and may continue from one block to the
// next, so a block can end inside a table, though not inside a definition.
static int between_entries(const where* w) {
    return(!w->defining && !w->named && !w->brackets && w->conditions <= 0);
}

// Returns 1 if a line contains only comments.  The lines after such a line
// are read as if it were absent.  An empty line does not count.
static int only_comments(const char* line, int brackets) {
    piece* pieces = NULL;
    long room = 0;
    int n = pieces_of(line, &brackets, &pieces, &room), all = n > 0;
    for(int idx=0; idx < n; idx++) all &= is_comment(&pieces[idx]);
    free(pieces);
    return(all);
}

// Returns 1 if the line's last piece is a backslash comment that starts
// before the last column.  In a block such a comment ends with its line,
// so the next line is not read as part of it.
static int ends_in_comment(const char* line, int columns, int brackets) {
    piece* pieces = NULL;
    long room = 0;
    int n = pieces_of(line, &brackets, &pieces, &room);
    int in = n && pieces[n - 1].kind == TO_END && pieces[n - 1].at < columns - 1;
    free(pieces);
    return(in);
}

#endif // HEXAFORTH_LINES_H
