//
// wrap-source FILE...
//
// Reformats Forth source to fit the lines of a block, following
// doc/formatting.md.  The rules are in that document, and this program
// applies them and does nothing else.  It changes only lines that are too
// long: a line that already fits stays as it was written.  It rewrites a
// file only if something in it changed, and reports each thing it could
// not fit.  Read the changes in git's diff before keeping them.
//
// Where the rules leave a choice, it does this:
//
// - Comments after code that start in the same column on consecutive
//   lines, together with their continuation lines, are a group.  A group
//   moves left as one, as far as the code allows.
// - A comment continues under itself, in its own column (which must leave
//   it at least ROOM characters beside the code), if it is a narrative
//   comment, or one of several field comments in a record.  A comment on
//   a definition's own line is its purpose comment, and goes where
//   doc/formatting.md puts a purpose comment.
// - A line of code is broken only where a phrase ends.  A line with no
//   such place is reported and left alone.  This program does not know
//   what the words do together, so that line is broken by hand.
// - A run of lines longer than a block gets a blank line where one thing
//   ends and the next starts, or failing that, between a comment and the
//   code after it.  A definition longer than a block cannot be split that
//   way.  It is reported, and is factored by hand.
//
// It changes no word of the source.  It refills the words of a comment,
// and breaks and indents code.  It adds only spaces, the \ that starts
// each line of a comment, and blank lines.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../vm_constants.h"
#include "lines.h"

#define WIDTH DISK_LINE
#define LINES (DISK_BLOCK / DISK_LINE)
#define ROOM 24         // the least width, in characters, of a comment beside code

typedef struct {
    char** at;
    long n, room;
} text;

static const char* file;        // the file being reformatted, for messages
static int left;                // the number of lines it could not fit
static int trying;              // nonzero while a trial layout is made, whose failures are not reported
static int at_top;              // 1 if the current line is outside every definition and table

static void add_line(text* t, const char* from, int len) {
    ONE_MORE(t->at, t->n, t->room);
    char* line = room_for(len + 1, 1);
    memcpy(line, from, len);
    t->at[t->n++] = line;
}

static void forget(text* t) {
    for(long i=0; i < t->n; i++) free(t->at[i]);
    free(t->at);
    *t = (text){ 0 };
}

static int indent_of(const char* line) {
    int at = 0;
    while (line[at] == ' ') at++;
    return(at);
}

// Counts the last output line as left too long, and reports it with the
// reason `why` and its first characters.  Its line number is not printed,
// because blank lines may still be inserted before it.
static void too_long(const text* out, const char* why) {
    left++;
    if (trying) return;
    const char* line = out->at[out->n - 1];
    fprintf(stderr, "%s: %s, and is left longer than a line: %.40s...\n", file, why, line + indent_of(line));
}

// ---- comments

typedef struct {
    const char* at;
    int len, after;             // `after` is the number of spaces that follow it when another word comes after it on the line
} said;

typedef struct {
    said* at;
    long n, room;
} saying;

static int ends_sentence(const char* word, int len) {
    char last = word[len - 1];
    if (len > 1 && strchr(")\"'", last)) last = word[len - 2];
    return(strchr(".?!", last) != NULL);
}

// Appends the words of some comment text to `s`.  The spacing after each
// word is kept as one space or two.  A word at the end of the text gets
// two spaces after it if it ends a sentence, and one otherwise.
static void words_of(saying* s, const char* from, int len) {
    int idx = 0;
    while (idx < len) {
        while (idx < len && from[idx] == ' ') idx++;
        if (idx >= len) break;
        int end = idx, next;
        while (end < len && from[end] != ' ') end++;
        for(next = end; next < len && from[next] == ' '; next++) ;
        ONE_MORE(s->at, s->n, s->room);
        said* w = &s->at[s->n++];
        w->at = from + idx;
        w->len = end - idx;
        w->after = next >= len ? 1 + ends_sentence(w->at, w->len) : next - end >= 2 ? 2 : 1;
        idx = next;
    }
}

// Returns the length of a line that has `start` characters, then a \ and
// a space, then words a to b-1 of a comment.
static int said_span(const saying* s, long a, long b, int start) {
    int len = start + 2;
    for(long k=a; k < b; k++) len += (k > a ? s->at[k - 1].after : 0) + s->at[k].len;
    return(len);
}

// Writes a comment's words as lines no wider than WIDTH, each line a \ and
// then words.  The first line starts with the text `head`, if there is
// one, and the other lines start at `indent`.  Each line takes as many
// words as fit, except that the last line is not left with a single word
// when the line before it can give up a word.
static void fill_after(text* out, const char* head, int head_len, int indent, const saying* s) {
    long* first = room_for(s->n + 2, sizeof(long));     // the index of the first word of each line
    long lines = 0;
    for(long k=0; k < s->n; k++) {
        int start = lines == 1 && head ? head_len : indent;     // the column where the current line starts
        if (!lines || said_span(s, first[lines - 1], k + 1, start) > WIDTH) first[lines++] = k;
    }
    first[lines] = s->n;
    if (lines >= 2 && first[lines] - first[lines - 1] == 1 && first[lines - 1] - first[lines - 2] >= 3 &&
        said_span(s, first[lines - 1] - 1, s->n, indent) <= WIDTH) first[lines - 1]--;
    for(long l=0; l < lines; l++) {
        int start = !l && head ? head_len : indent;
        int len = said_span(s, first[l], first[l + 1], start);
        char* line = room_for(len + 1, 1);
        if (!l && head) memcpy(line, head, head_len);
        else memset(line, ' ', indent);
        memcpy(line + start, "\\ ", 2);
        int at = start + 2;
        for(long k=first[l]; k < first[l + 1]; k++) {
            int gap = k > first[l] ? s->at[k - 1].after : 0;
            memset(line + at, ' ', gap);
            memcpy(line + at + gap, s->at[k].at, s->at[k].len);
            at += gap + s->at[k].len;
        }
        add_line(out, line, len);
        free(line);
        if (len > WIDTH) too_long(out, "a word of a comment is longer than a line");
    }
    free(first);
}

static void fill(text* out, int indent, const saying* s) {
    fill_after(out, NULL, 0, indent, s);
}

// Refills the lines of one comment paragraph, which all start at `indent`.
static void fill_lines(text* out, char** lines, long n, int indent) {
    saying s = { 0 };
    for(long k=0; k < n; k++) words_of(&s, lines[k] + indent + 2, line_length(lines[k]) - indent - 2);
    fill(out, indent, &s);
    free(s.at);
}

// Classifies a line that is only a \ comment.  PROSE is ordinary text and
// HEADING starts with dashes.  LAID_OUT has more than one space after its
// \ and was formatted by hand.  ALONE is a \ with nothing after it.
// Returns NOT_ONE for any other line.
enum { NOT_ONE, PROSE, HEADING, LAID_OUT, ALONE };

static int comment_line(const char* line, int brackets) {
    int at = indent_of(line), len = line_length(line);
    if (brackets || at >= len || line[at] != '\\' || (at + 1 < len && line[at + 1] != ' ')) return(NOT_ONE);
    if (at + 1 >= len) return(ALONE);
    if (at + 2 >= len || line[at + 2] == ' ') return(LAID_OUT);     // formatted by hand
    if (!strncmp(line + at + 2, "----", 4)) return(HEADING);
    return(PROSE);
}

// Returns 1 if a line continues the \ comment of the line above it, which
// starts in `column`.
static int goes_on(const char* line, int column) {
    return(column > 0 && indent_of(line) == column && line[column] == '\\' &&
           (line[column + 1] == ' ' || !line[column + 1]));
}

// ---- code

typedef struct {
    const char* line;
    const piece* p;
} code;

// Returns the length of pieces a to b-1 on a line of their own.
static int span(const code* c, int a, int b) {
    int len = 0;
    for(int k=a; k < b; k++) len += (k > a ? c->p[k].gap : 0) + c->p[k].len;
    return(len);
}

// Returns the most characters a line that ends with piece k can have.  A
// line's last column must stay blank unless the line ends in a comment.
static int room_to(const code* c, int k) {
    return(is_comment(&c->p[k]) ? WIDTH : WIDTH - 1);
}

// Returns 1 if pieces a to b-1 fit on one line that starts at `indent`.
static int fits(const code* c, int a, int b, int indent) {
    return(indent + span(c, a, b) <= room_to(c, b - 1));
}

// Returns 1 if a line, as written, fits a block's line.
static int line_fits(const char* line, int brackets) {
    int len = line_length(line);
    if (len != WIDTH) return(len < WIDTH);
    piece* p = NULL;
    long room = 0;
    int n = pieces_of(line, &brackets, &p, &room), ends_comment = n && is_comment(&p[n - 1]);
    free(p);
    return(ends_comment);
}

// Writes pieces a to b-1 as one line that starts at `indent`.
static void put(text* out, const code* c, int a, int b, int indent) {
    char* line = room_for(indent + span(c, a, b) + 1, 1);
    int len = indent;
    memset(line, ' ', indent);
    for(int k=a; k < b; k++) {
        int gap = k > a ? c->p[k].gap : 0;
        memset(line + len, ' ', gap);
        memcpy(line + len + gap, c->line + c->p[k].at, c->p[k].len);
        len += gap + c->p[k].len;
    }
    add_line(out, line, len);
    free(line);
}

// Returns 1 if piece k is a word that begins a definition.
static int defines(const code* c, int k) {
    const piece* p = &c->p[k];
    return(p->kind == PLAIN && (is_word(c->line, p, ":") || is_word(c->line, p, "::") ||
                                is_word(c->line, p, ":noname") || is_word(c->line, p, "header") ||
                                is_word(c->line, p, "header-imm")));
}

// Returns how many pieces at the start of a line begin a definition.  If
// a stack comment follows them, `*stack_end` is set to the column where it
// ends, and otherwise to 0.
static int head_of(const code* c, int n, int* stack_end) {
    int head = 0;
    while (head < n && defines(c, head)) head++;
    *stack_end = head && head < n && c->p[head].kind == BRACKETS ? c->p[head].at + c->p[head].len : 0;
    return(head);
}

// Returns the nesting depth of `if`, `begin` and `do` after a word, given
// the depth before it and what the word does (`how`: 1 opens a structure,
// -1 closes one).  A word that closes a structure opened on an earlier
// line leaves the depth at 0.
static int deeper(int depth, int how) {
    depth += how == 1 ? 1 : how == -1 ? -1 : 0;
    return(depth < 0 ? 0 : depth);
}

// Tests whether a phrase ends after piece k, among the pieces up to b.
// Returns 0 inside an `if`, a `begin` or a `do` (`depth` nonzero).
// Returns 2 where the author marked the end with two spaces, and after a
// word that compiles a cell or a string (`,` `prow,` `,"`), which is how
// the entries of a table end.  Returns 1 at the end of an `if`, a `begin`
// or a `do`.  A line is broken at a 1 only if it has no place that
// returns 2.
static int phrase_ends(const code* c, int k, int depth, int b) {
    const piece* p = &c->p[k];
    const char* word = c->line + p->at;
    if (depth) return(0);
    if (k + 1 < b && c->p[k + 1].gap >= 2) return(2);
    if (p->kind == PLAIN && word[p->first - 1] == ',') return(2);
    if (p->kind == STRING && p->first == 2 && !strncmp(word, ",\"", 2)) return(2);
    return(control(c->line, p) == -1);
}

// Returns 1 if a piece starts a new definition.  Such a piece goes at the
// left margin, not indented under the code before it.
static int starts_one(const code* c, int k) {
    static const char* const STARTS[] = { ":", "::", ":noname", "create", "variable", "2variable",
                                          "header", "header-imm", "defer", NULL };
    const piece* p = &c->p[k];
    return(p->kind == PLAIN && in_list(c->line, p->at, p->first, STARTS));
}

// Writes pieces a to b-1 of a line of code on as many lines as they need.
// The first line starts at `indent`.
static void cut(text* out, const code* c, int a, int b, int indent) {
    if (a >= b) return;
    if (fits(c, a, b, indent)) {
        put(out, c, a, b, indent);
        return;
    }
    // A definition: the name and stack comment go on the first line, and
    // the code on the following lines, indented.
    int head = a;
    while (head < b && defines(c, head)) head++;
    if (head > a && head + 1 < b && c->p[head].kind == BRACKETS) {
        cut(out, c, a, head + 1, indent);
        cut(out, c, head + 1, b, indent + 4);
        return;
    }
    // Continuation lines are indented four columns after a definition's
    // first line and after a line at the left margin.  After an indented
    // line they keep that line's indent.
    int more = head > a ? indent + 4 : indent ? indent : 4;
    // The data after a `create` continues in the column where it started,
    // if it can be broken that way and nothing else is defined after it on
    // the line.
    int under = indent + c->p[a].len + (a + 1 < b ? c->p[a + 1].gap : 0), alone = 1;
    for(int k=a + 1; k < b; k++) alone &= !starts_one(c, k);
    if (head == a && c->p[a].kind == PLAIN && is_word(c->line, &c->p[a], "create") && a + 1 < b &&
        alone && under <= WIDTH - ROOM) {
        text rest = { 0 };
        int had = left;
        trying++;
        cut(&rest, c, a + 1, b, under);
        trying--;
        if (left == had) {
            memcpy(rest.at[0] + indent, c->line + c->p[a].at, c->p[a].len);
            for(long l=0; l < rest.n; l++) add_line(out, rest.at[l], (int)strlen(rest.at[l]));
        }
        forget(&rest);
        if (left == had) return;
        left = had;
    }
    // Break at the last phrase end that fits on the line.  A place the
    // author marked is preferred over any other.  A definition's name is
    // never left on a line by itself.  If the piece after the break starts
    // a definition of its own, it goes at the left margin.
    int best = -1;
    for(int said=2; said >= 1 && best < 0; said--) {
        int depth = 0;
        for(int j=a + 1; j < b; j++) {
            depth = deeper(depth, control(c->line, &c->p[j - 1]));
            if (j > head && phrase_ends(c, j - 1, depth, b) >= said && fits(c, a, j, indent)) best = j;
        }
    }
    if (best > 0) {
        put(out, c, a, best, indent);
        cut(out, c, best, b, !indent && starts_one(c, best) ? 0 : more);
        return;
    }
    int depth;
    // An `if`, a `begin` or a `do` that is too long for the line is spread
    // over several lines, with its contents indented.
    depth = 0;
    for(int k=a; k < b; k++) {
        int how = control(c->line, &c->p[k]);
        if (!depth && how == 1) {
            int in = 1, end = -1;
            for(int j=k + 1; j < b && end < 0; j++) {
                int then = control(c->line, &c->p[j]);
                if (then == 1) in++;
                else if (then == -1 && !--in) end = j;
            }
            if (end < 0) break;
            // A `begin` keeps its test and its `while` on its own line, if
            // they fit there.
            int from = k + 1, test = -1;
            in = 1;
            for(int j=k + 1; j < end && test < 0; j++) {
                int then = control(c->line, &c->p[j]);
                if (then == 1) in++;
                else if (then == -1) in--;
                else if (then == 2 && in == 1) test = j;
            }
            if (is_word(c->line, &c->p[k], "begin") && test > 0 && fits(c, a, test + 1, indent)) {
                put(out, c, a, test + 1, indent);
                from = test + 1;
            } else {
                cut(out, c, a, k + 1, indent);
            }
            in = 1;
            for(int j=from; j < end; j++) {
                int then = control(c->line, &c->p[j]);
                if (then == 1) in++;
                else if (then == -1) in--;
                else if (then == 2 && in == 1) {
                    cut(out, c, from, j, indent + 4);
                    put(out, c, j, j + 1, indent);
                    from = j + 1;
                }
            }
            cut(out, c, from, end, indent + 4);
            cut(out, c, end, b, indent);
            return;
        }
        depth = deeper(depth, how);
    }
    put(out, c, a, b, indent);
    too_long(out, "code has no end of a phrase to be cut at");
}

// Returns the index of the first of the comments that follow a line's
// code, and sets `*code_end` to the column where the code ends.  Those
// comments are the pieces at the end of the line that are comments,
// except a definition's stack comment, which counts as code.  Returns n
// if there are none, and 0 if the line is only comments.
static int comments_after(const code* c, int n, int* code_end) {
    int s = n;
    while (s > 0 && is_comment(&c->p[s - 1])) s--;
    int only_defines = s > 0;
    for(int k=0; k < s; k++) only_defines &= defines(c, k);
    if (only_defines && s < n && c->p[s].kind == BRACKETS) s++;
    *code_end = s > 0 ? c->p[s - 1].at + c->p[s - 1].len : 0;
    return(s);
}

// Writes the first n pieces of a line of code.  None of them is a \ comment.
// A ( ) comment after the code stays beside the code if it fits once the
// gap between them is reduced to two spaces.  Otherwise it goes on a line
// of its own after the code.
static void fit_rest(text* out, const code* c, int n, int indent) {
    const piece* p = c->p;
    int end = p[n - 1].at + p[n - 1].len, code_end;
    int s = comments_after(c, n, &code_end);
    if (end <= room_to(c, n - 1)) {
        add_line(out, c->line, end);
    } else if (s > 0 && s < n && p[s].gap >= 2) {
        int comment = end - p[s].at;
        if (code_end + 2 + comment <= WIDTH) {
            char* both = room_for(code_end + 2 + comment + 1, 1);
            memcpy(both, c->line, code_end);
            memcpy(both + code_end, "  ", 2);
            memcpy(both + code_end + 2, c->line + p[s].at, comment);
            add_line(out, both, code_end + 2 + comment);
            free(both);
        } else {
            cut(out, c, 0, s, indent);
            char* alone = room_for(indent + comment + 1, 1);
            memset(alone, ' ', indent);
            memcpy(alone + indent, c->line + p[s].at, comment);
            add_line(out, alone, indent + comment);
            free(alone);
            if (indent + comment > WIDTH) too_long(out, "a comment in brackets is longer than a line");
        }
    } else {
        cut(out, c, 0, n, indent);
    }
}

// Handles a definition whose stack comment is at the start of the \ comment
// after its code, instead of after its name.  Returns a new line with the
// stack comment moved to follow the name, which is where Forth puts it.
// Returns NULL if the line is not such a definition.
static char* stack_after_name(const char* line, int brackets) {
    piece* p = NULL;
    long room = 0;
    int n = pieces_of(line, &brackets, &p, &room), stack_end;
    code c = { line, p };
    int head = head_of(&c, n, &stack_end);
    char* made = NULL;
    if (head && head < n && !stack_end && p[n - 1].kind == TO_END) {
        const char* said = line + p[n - 1].at + 1;
        int len = p[n - 1].len - 1;
        while (len && *said == ' ') said++, len--;
        const char* shut = len && *said == '(' ? memchr(said, ')', len) : NULL;
        int effect = 0;                                 // 1 if the ( ) text contains --, which makes it a stack effect
        for(const char* at=said; shut && at + 1 < shut; at++) effect |= at[0] == '-' && at[1] == '-';
        if (effect) {
            const char* rest = shut + 1;
            int rest_len = (int)(said + len - rest);
            while (rest_len && *rest == ' ') rest++, rest_len--;
            int name_end = p[head - 1].at + p[head - 1].len;
            text_more(&made, "%.*s %.*s", name_end, line, (int)(shut - said + 1), said);
            if (head < n - 1) {                         // copy the code after the name, with the gap it had before it
                int code_end = p[n - 2].at + p[n - 2].len;
                text_more(&made, "%*s%.*s", p[head].gap, "", code_end - p[head].at, line + p[head].at);
            }
            if (rest_len) text_more(&made, " \\ %.*s", rest_len, rest);
        }
    }
    free(p);
    return(made);
}

// Returns 1 if a line is only `header NAME`.  Such a line belongs to the
// definition on the line after it.
static int lone_header(const char* line) {
    piece* p = NULL;
    long room = 0;
    int brackets = 0, n = pieces_of(line, &brackets, &p, &room);
    int lone = n == 1 && p[0].kind == PLAIN && p[0].first < p[0].len &&
               (is_word(line, &p[0], "header") || is_word(line, &p[0], "header-imm"));
    free(p);
    return(lone);
}

// Writes a comment on the lines before the code it was beside.  If the
// comment belongs to a definition whose `header` line is just above, the
// comment goes before that `header` line.  If a comment paragraph is just
// above at the left margin, a blank line is written first, to keep the two
// paragraphs apart.  No blank line is written inside a definition.
static void before_code(text* out, int indent, const saying* s, int definition) {
    char* header = definition && out->n && lone_header(out->at[out->n - 1]) ? out->at[--out->n] : NULL;
    if ((at_top || header) && !indent && out->n) {
        int above = comment_line(out->at[out->n - 1], 0);
        if (above == PROSE || above == HEADING || above == LAID_OUT) add_line(out, "", 0);
    }
    fill(out, indent, s);
    if (header) {
        add_line(out, header, (int)strlen(header));
        free(header);
    }
}

// Writes a definition's line and its purpose comment.  The comment's words
// are in `s` and have been taken off the end of the line, which leaves n
// pieces.  It uses the first of three layouts that fits:
//
//   the purpose comment after the stack comment on the first line, and
//   the code on the following lines
//   the purpose comment on the line after the name, indented like the
//   code
//   the purpose comment before the definition, when it is longer than one
//   line
static void purpose(text* out, const code* c, int n, const saying* s, int indent) {
    int stack_end, head = head_of(c, n, &stack_end);
    int body = stack_end ? head + 1 : head;
    int name_end = stack_end ? stack_end : c->p[head - 1].at + c->p[head - 1].len;
    if (said_span(s, 0, s->n, name_end + 1) <= WIDTH) {
        char* first = room_for(name_end + 2, 1);
        memcpy(first, c->line, name_end);
        first[name_end] = ' ';
        fill_after(out, first, name_end + 1, 0, s);
        free(first);
        cut(out, c, body, n, indent + 4);
    } else if (stack_end && said_span(s, 0, s->n, indent + 4) <= WIDTH) {
        cut(out, c, 0, body, indent);
        fill(out, indent + 4, s);
        cut(out, c, body, n, indent + 4);
    } else {
        before_code(out, indent, s, 1);
        fit_rest(out, c, n, indent);
    }
}

// How the comment after a line's code is handled:
//
//   AS_IT_IS   by the rule for its kind
//   UNDER      continued under itself, as one of a group
//   ABOVE      moved to the line above the code, because there is no room
//              beside the code
enum { AS_IT_IS, UNDER, ABOVE };

// Reformats one line of code together with the lines after it that
// continue its \ comment.  Returns the index of the line after those.
static long fit_code_line(text* out, const text* in, long i, int brackets, int how) {
    const char* line = in->at[i];
    piece* p = NULL;
    long room = 0;
    int starts = brackets;
    int n = pieces_of(line, &brackets, &p, &room), indent = p[0].at;
    int tail = n > 1 && p[n - 1].kind == TO_END ? p[n - 1].at : -1;
    // When a group's comments go above their code, every comment in the
    // group goes above, even one that would fit where it is.
    int all_fit = line_fits(line, starts) && !(how == ABOVE && tail >= 0);
    long j = i + 1;
    for(; tail >= 0 && j < in->n && goes_on(in->at[j], tail); j++) all_fit &= line_length(in->at[j]) <= WIDTH;
    if (all_fit) {
        for(long k=i; k < j; k++) add_line(out, in->at[k], line_length(in->at[k]));
        free(p);
        return(j);
    }
    saying s = { 0 };
    for(long k=i; tail >= 0 && k < j; k++) words_of(&s, in->at[k] + tail + 1, line_length(in->at[k]) - tail - 1);
    char* stacked = j == i + 1 ? stack_after_name(line, starts) : NULL;
    if (stacked) {                                      // parse the line again in its new form
        line = stacked;
        brackets = starts;
        n = pieces_of(line, &brackets, &p, &room);
        tail = n > 1 && p[n - 1].kind == TO_END ? p[n - 1].at : -1;
        s.n = 0;
        if (tail >= 0) words_of(&s, line + tail + 1, line_length(line) - tail - 1);
        how = AS_IT_IS;
    }
    code c = { line, p };
    int stack_end, head = head_of(&c, n, &stack_end);
    if (tail < 0) {
        fit_rest(out, &c, n, indent);
    } else if ((how == UNDER || (how == AS_IT_IS && !head)) && tail <= WIDTH - ROOM) {
        fill_after(out, line, tail, tail, &s);          // continue the comment under itself
    } else if (head && how != ABOVE) {
        purpose(out, &c, n - 1, &s, indent);
    } else {
        // A narrative comment with no room beside its code goes on the
        // line above the code, or on the line after it if the code only
        // closes an `if` or a loop.
        int closes = 1;
        for(int k=0; k < n - 1; k++) {
            int then = control(line, &p[k]);
            closes &= then == -1 || then == 2;
        }
        if (!closes) before_code(out, indent, &s, head);
        fit_rest(out, &c, n - 1, indent);
        if (closes) fill(out, indent, &s);
    }
    free(s.at);
    free(stacked);
    free(p);
    return(j);
}

// Finds each group of comments after code, moves the group left as far as
// the code allows, and records in `how` what fit_code_line() is to do with
// each line's comment.  A group continues across lines of code that have
// no comment after them, and leaves those lines unchanged.
static void align(text* lines, char* how) {
    long n = lines->n;
    int* at = room_for(n, sizeof(int));                 // the column where the comments after the code start, or -1
    // The column where the code ends.  On a line that continues the
    // comment above, it is -1 if that comment is in a group and -2 if it
    // is not.
    int* ends = room_for(n, sizeof(int));
    int* plain = room_for(n, sizeof(int));              // the column where the line ends, not counting a \ comment
    char* defining = room_for(n, 1);                    // 1 if the line starts a definition
    char* fits_now = room_for(n, 1);
    int brackets = 0, tail = -1, grouped = 0;
    piece* p = NULL;
    long room = 0;
    for(long i=0; i < n; i++) {
        const char* line = lines->at[i];
        int starts = brackets;
        int k = pieces_of(line, &brackets, &p, &room);
        at[i] = -1;
        fits_now[i] = line_fits(line, starts);
        if (!starts && goes_on(line, tail)) {
            ends[i] = grouped ? -1 : -2;
            continue;
        }
        code c = { line, p };
        int s = k ? comments_after(&c, k, &ends[i]) : 0;
        if (s > 0 && s < k && p[s].gap >= 2) at[i] = p[s].at;
        tail = s > 0 && p[k - 1].kind == TO_END ? p[k - 1].at : -1;
        grouped = at[i] >= 0;
        plain[i] = tail >= 0 && k > 1 ? p[k - 2].at + p[k - 2].len : line_length(line);
        defining[i] = k && defines(&c, 0);
    }
    free(p);
    for(long i=0; i < n; ) {
        if (at[i] < 0) {
            i++;
            continue;
        }
        long j = i + 1;
        while (j < n && (at[j] == at[i] || ends[j] < 0 || (at[j] < 0 && ends[j] > 0))) j++;
        int column = at[i], to = 0, over = 0, still = 0, commented = 0, fit = 1, definitions = 0;
        for(long k=i; k < j; k++) {
            if (at[k] != column && ends[k] != -1) continue;
            if (ends[k] >= 0 && ends[k] + 2 > to) to = ends[k] + 2;
            over |= !fits_now[k];
            still |= ends[k] >= 0 && plain[k] > WIDTH - 1;
            commented += ends[k] >= 0;
            definitions += ends[k] >= 0 && defining[k];
        }
        for(long k=i; k < j; k++) {
            if (at[k] == column || ends[k] == -1) fit &= line_length(lines->at[k]) - (column - to) <= WIDTH;
        }
        // Once moved left, the comments may all fit.  If they do not, a
        // narrative comment, or one of several field comments in a record,
        // continues under itself if there is room beside the code, and
        // goes above its code if there is not.  A single comment on a
        // definition is its purpose comment, and goes where a purpose
        // comment goes.  The group moves left if its comments then fit or
        // continue under themselves.  Otherwise it moves only if some line
        // of the group is too long even without its \ comment.
        int table = commented > 1, beside = to <= WIDTH - ROOM;
        int moving = fit || (beside && (table || !definitions)) || still;
        for(long k=i; over && k < j; k++) {
            if (at[k] != column && ends[k] != -1) continue;
            if (fit || (defining[k] && !table)) how[k] = AS_IT_IS;
            else if (beside) how[k] = UNDER;
            else how[k] = defining[k] ? AS_IT_IS : ABOVE;
            if (to == column || !moving) continue;
            char* line = lines->at[k];
            int len = line_length(line);
            char* moved = room_for(len + 1, 1);
            if (ends[k] < 0) {                          // a continuation line moves left by the same distance
                memcpy(moved, line + (column - to), len - (column - to));
            } else {
                memcpy(moved, line, ends[k]);
                memset(moved + ends[k], ' ', to - ends[k]);
                memcpy(moved + to, line + column, len - column);
            }
            free(line);
            lines->at[k] = moved;
        }
        i = j;
    }
    free(at);
    free(ends);
    free(plain);
    free(defining);
    free(fits_now);
}

// Reformats every line of `in` to fit the width, and writes the result to
// `out`.
static void fit_lines(text* out, const text* in, const char* how) {
    int brackets = 0;
    where w = { 0 };                                    // the parse state after the lines read so far
    long read = 0;
    piece* seen = NULL;
    long seen_room = 0;
    for(long i=0; i < in->n; ) {
        while (read < i) read_line(in->at[read++], &w, &seen, &seen_room);
        at_top = between_things(&w);
        const char* line = in->at[i];
        int len = line_length(line), kind = comment_line(line, brackets);
        if (kind == PROSE) {                            // handle it together with the rest of its paragraph
            long j = i + 1;
            int indent = indent_of(line), longest = len;
            while (j < in->n && comment_line(in->at[j], 0) == PROSE && indent_of(in->at[j]) == indent) {
                if (line_length(in->at[j]) > longest) longest = line_length(in->at[j]);
                j++;
            }
            if (longest > WIDTH) fill_lines(out, in->at + i, j - i, indent);
            else for(long k=i; k < j; k++) add_line(out, in->at[k], line_length(in->at[k]));
            i = j;
            continue;
        }
        if (kind == ALONE && at_top && !indent_of(line)) {      // a blank line separates paragraphs, not a \ by itself
            add_line(out, "", 0);
            i++;
            continue;
        }
        int starts = brackets, only;
        piece* p = NULL;
        long room = 0;
        int n = pieces_of(line, &brackets, &p, &room);
        only = n > 0;
        for(int k=0; k < n; k++) only &= is_comment(&p[k]);
        free(p);
        if (n && !only) {
            i = fit_code_line(out, in, i, starts, how[i]);
            continue;
        }
        if (len <= WIDTH) add_line(out, line, len);
        else if (kind == HEADING) fill_lines(out, in->at + i, 1, indent_of(line));
        else {
            add_line(out, line, len);
            too_long(out, "a comment laid out by hand is too long");
        }
        i++;
    }
    free(seen);
}

// Writes to `name` the name of the first definition in lines `from` to
// `to`-1, for use in a message.
static void name_from(const text* t, long from, long to, char* name, int room) {
    snprintf(name, room, "no definition");
    piece* p = NULL;
    long had = 0;
    for(long i=from; i < to; i++) {
        int brackets = 0, n = pieces_of(t->at[i], &brackets, &p, &had);
        code c = { t->at[i], p };
        for(int k=0; k < n; k++) {
            if (defines(&c, k) && p[k].first < p[k].len) {
                snprintf(name, room, "%.*s", p[k].len, t->at[i] + p[k].at);
                free(p);
                return;
            }
        }
    }
    free(p);
}

// Returns the first line after j and before e where a new thing starts
// right after another has ended, or e if there is none.
static long next_thing(const char* between, const char* only, long j, long e) {
    for(long k=j + 1; k < e; k++) if (between[k - 1] && !only[k - 1]) return(k);
    return(e);
}

// Copies `t` to `done`, inserting blank lines to split each run of lines
// that is longer than a block.  Adds the number of blank lines inserted to
// `*put_in`, and returns how many runs could not be split.
static int fit_runs(text* done, const text* t, long* put_in) {
    long n = t->n;
    char* nothing = room_for(n, 1);                     // 1 for a blank line
    char* only = room_for(n, 1);                        // 1 for a line that is only comments
    char* between = room_for(n, 1);                     // 1 if one thing has ended after this line and the next has not started
    char* entries = room_for(n, 1);                     // 1 if a table continues with its next entry after this line
    char* before = room_for(n + 1, 1);                  // 1 if a blank line is to be inserted before this line
    where w = { 0 };
    piece* p = NULL;
    long room = 0;
    int tail = -1;                                      // the column of the \ comment that the next line may continue
    for(long i=0; i < n; i++) {
        nothing[i] = !line_length(t->at[i]);
        only[i] = !nothing[i] && only_comments(t->at[i], w.brackets);
        int goes = only[i] && goes_on(t->at[i], tail);
        int k = read_line(t->at[i], &w, &p, &room);
        // A line that continues the comment above belongs to that line, and
        // no blank line may come between the two.  So it is treated as
        // code.
        between[i] = between_things(&w);
        entries[i] = between_entries(&w);
        if (goes) {
            only[i] = 0;
            between[i - 1] = 0;
            entries[i - 1] = 0;
        } else {
            tail = !only[i] && k > 1 && p[k - 1].kind == TO_END ? p[k - 1].at : -1;
        }
    }
    free(p);
    int long_runs = 0;
    for(long i=0; i < n; ) {
        long e = i;
        int has_code = 0;
        while (e < n && !nothing[e]) has_code |= !only[e++];
        // The places a blank line can go before line j, in order of
        // preference.  ENDS: a new thing starts at line j.  COMMENT: code
        // starts at line j after a comment, and reaches the start of the
        // next thing within a block's lines.  ENTRY: a table continues at
        // line j with its next entry.
        #define ENDS(j)     (between[(j) - 1] && !only[(j) - 1])
        #define COMMENT(j)  (between[(j) - 1] && only[(j) - 1] && !only[j] && \
                             next_thing(between, only, (j), e) - (j) <= LINES)
        #define ENTRY(j)    (entries[(j) - 1] && !only[(j) - 1])
        for(long at=i; has_code && e - at > LINES; ) {
            long cut_at = -1;
            for(long j=at + LINES; j > at && cut_at < 0; j--) if (ENDS(j)) cut_at = j;
            for(long j=at + LINES; j > at && cut_at < 0; j--) if (COMMENT(j)) cut_at = j;
            for(long j=at + LINES; j > at && cut_at < 0; j--) if (ENTRY(j)) cut_at = j;
            if (cut_at < 0) {                           // no place within a block: take the next one, and report the lines before it unless they are all comments
                for(long j=at + LINES + 1; j < e && cut_at < 0; j++) if (ENDS(j) || COMMENT(j)) cut_at = j;
                long to = cut_at < 0 ? e : cut_at;
                int comment = 1;
                for(long k=at; k < to; k++) comment &= only[k];
                if (!comment) {
                    char name[80];
                    name_from(t, at, to, name, sizeof(name));
                    fprintf(stderr, "%s:%ld: %ld lines from here (%s) have nowhere to be cut, and are longer than a block\n",
                            file, at + 1 + *put_in, to - at, name);
                    long_runs++;
                }
                if (cut_at < 0) break;
            }
            before[cut_at] = 1;
            ++*put_in;
            at = cut_at;
        }
        i = e > i ? e : i + 1;
    }
    for(long i=0; i < n; i++) {
        if (before[i]) add_line(done, "", 0);
        add_line(done, t->at[i], (int)strlen(t->at[i]));
    }
    free(nothing);
    free(only);
    free(between);
    free(entries);
    free(before);
    return(long_runs);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: wrap-source FILE...\n");
        return(1);
    }
    int failed = 0;
    for(int f=1; f < argc; f++) {
        file = argv[f];
        unsigned char* bytes;
        size_t len = file_bytes(file, &bytes);
        if (!bytes) {
            perror(file);
            failed = 1;
            continue;
        }
        text in = { 0 }, out = { 0 }, done = { 0 };
        for(size_t from=0, idx=0; idx <= len; idx++) {
            if (idx < len && bytes[idx] != '\n') continue;
            if (idx == len && idx == from) break;
            size_t end = idx > from && bytes[idx - 1] == '\r' ? idx - 1 : idx;
            add_line(&in, (const char*)bytes + from, (int)(end - from));
            from = idx + 1;
        }
        long were_long = 0, put_in = 0;
        int brackets = 0;
        piece* p = NULL;
        long room = 0;
        for(long i=0; i < in.n; i++) {
            were_long += !line_fits(in.at[i], brackets);
            pieces_of(in.at[i], &brackets, &p, &room);
        }
        free(p);
        int had_left = left;
        char* how = room_for(in.n + 1, 1);
        align(&in, how);
        fit_lines(&out, &in, how);
        int long_runs = fit_runs(&done, &out, &put_in);

        char* all = NULL;
        for(long i=0; i < done.n; i++) text_more(&all, "%s\n", done.at[i]);
        if (!all || strlen(all) != len || memcmp(all, bytes, len)) {
            FILE* write = fopen(file, "wb");
            if (!write || fputs(all ? all : "", write) == EOF || fclose(write)) {
                perror(file);
                failed = 1;
            }
        }
        printf("%s: %ld of %ld lines did not fit a block's line, and %d are left so; "
               "%ld lines of nothing put in, and %d runs left longer than a block\n",
               file, were_long, in.n, left - had_left, put_in, long_runs);
        free(how);
        free(all);
        free(bytes);
        forget(&in);
        forget(&out);
        forget(&done);
    }
    return(failed);
}
