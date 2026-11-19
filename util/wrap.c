//
// wrap-source FILE...
//
// Lays Forth source out as doc/formatting.md says, where a line of it is
// longer than a block's: the rules are there, and this does what they
// say and no more.  A line that already fits is left as it is written.
// Each file is written again if anything in it changed, and what could
// not be done is said.  What it changed is read in git's diff before it
// is kept.
//
// How it reads the rules, where they leave it something to decide:
//
// - The comments after code that are at one column on lines one after
//   another, with the lines under them that go on with them, are a
//   group, and move left together as far as the code allows.
// - A comment goes on under itself (its column, ROOM characters beside
//   the code at least) when it is a narrative comment, or a field's in a
//   record of several.  A definition's own comment is its purpose
//   comment, and goes where doc/formatting.md has a purpose comment go.
// - A line of code is cut only where a phrase ends.  A line with no such
//   place is said and left: what its words do together is not known
//   here, and it is cut by hand.
// - A run of lines longer than a block is cut with a line of nothing,
//   where one thing ends and the next starts, or failing that between a
//   comment and the code after it.  A definition longer than a block
//   cannot be cut, and is said: it is factored by hand.
//
// It changes no word of the source.  A comment's words are filled again,
// and code is cut and indented; nothing is added but spaces, \ before a
// comment's lines, and lines of nothing.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../vm_constants.h"
#include "lines.c"

#define WIDTH DISK_LINE
#define LINES (DISK_BLOCK / DISK_LINE)
#define ROOM 24         // the fewest characters a comment beside code is to have on a line

typedef struct {
    char** at;
    long n, room;
} text;

static const char* file;        // the file being fitted, for what is said of it
static int left;                // lines it could not fit
static int trying;              // a way of cutting is being tried, and what it cannot do is not said
static int at_top;              // the line being fitted is not inside a definition or a table

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

// A line left too long, said with how it starts: its number is not yet
// known, since lines of nothing may be put in before it.
static void too_long(const text* out, const char* why) {
    left++;
    if (trying) return;
    const char* line = out->at[out->n - 1];
    fprintf(stderr, "%s: %s, and is left longer than a line: %.40s...\n", file, why, line + indent_of(line));
}

// ---- comments

typedef struct {
    const char* at;
    int len, after;             // and the spaces after it, when a word comes after it on its line
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

// The words of a comment, after those there are.  The spaces after a word
// are kept, one or two; a word at the end of a line has two after it if it
// ends a sentence.
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

// How long words a to b of a comment are on a line, after `start`
// characters and the \.
static int said_span(const saying* s, long a, long b, int start) {
    int len = start + 2;
    for(long k=a; k < b; k++) len += (k > a ? s->at[k - 1].after : 0) + s->at[k].len;
    return(len);
}

// A comment's words filled to the width, each line a \ and words.  The
// first line starts with `head`, if there is one, and the rest at the
// indent.  Each line has as many words as fit, but that the last is not
// one word by itself when the line before can spare it another.
static void fill_after(text* out, const char* head, int head_len, int indent, const saying* s) {
    long* first = room_for(s->n + 2, sizeof(long));     // the word each line starts with
    long lines = 0;
    for(long k=0; k < s->n; k++) {
        int start = lines == 1 && head ? head_len : indent;     // where the line it would go on starts
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

// Lines of a paragraph of comment, at an indent, filled again.
static void fill_lines(text* out, char** lines, long n, int indent) {
    saying s = { 0 };
    for(long k=0; k < n; k++) words_of(&s, lines[k] + indent + 2, line_length(lines[k]) - indent - 2);
    fill(out, indent, &s);
    free(s.at);
}

// A line that is only a \ comment: what it is.  ALONE is a \ by itself.
enum { NOT_ONE, PROSE, HEADING, LAID_OUT, ALONE };

static int comment_line(const char* line, int brackets) {
    int at = indent_of(line), len = line_length(line);
    if (brackets || at >= len || line[at] != '\\' || (at + 1 < len && line[at + 1] != ' ')) return(NOT_ONE);
    if (at + 1 >= len) return(ALONE);
    if (at + 2 >= len || line[at + 2] == ' ') return(LAID_OUT);     // laid out by hand
    if (!strncmp(line + at + 2, "----", 4)) return(HEADING);
    return(PROSE);
}

// A line that goes on with the \ comment of the line above, which is at
// this column.
static int goes_on(const char* line, int column) {
    return(column > 0 && indent_of(line) == column && line[column] == '\\' &&
           (line[column + 1] == ' ' || !line[column + 1]));
}

// ---- code

typedef struct {
    const char* line;
    const piece* p;
} code;

// How long pieces a to b are, on a line by themselves.
static int span(const code* c, int a, int b) {
    int len = 0;
    for(int k=a; k < b; k++) len += (k > a ? c->p[k].gap : 0) + c->p[k].len;
    return(len);
}

// How long a line may be that ends with piece k: its last column is blank,
// but for the end of a comment.
static int room_to(const code* c, int k) {
    return(is_comment(&c->p[k]) ? WIDTH : WIDTH - 1);
}

static int fits(const code* c, int a, int b, int indent) {
    return(indent + span(c, a, b) <= room_to(c, b - 1));
}

// Whether a line as it is written fits a block's line.
static int line_fits(const char* line, int brackets) {
    int len = line_length(line);
    if (len != WIDTH) return(len < WIDTH);
    piece* p = NULL;
    long room = 0;
    int n = pieces_of(line, &brackets, &p, &room), ends_comment = n && is_comment(&p[n - 1]);
    free(p);
    return(ends_comment);
}

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

static int defines(const code* c, int k) {
    const piece* p = &c->p[k];
    return(p->kind == PLAIN && (is_word(c->line, p, ":") || is_word(c->line, p, "::") ||
                                is_word(c->line, p, ":noname") || is_word(c->line, p, "header") ||
                                is_word(c->line, p, "header-imm")));
}

// How many pieces a line starts with that start a definition, and where
// its name and stack comment end, if it has one after its name.
static int head_of(const code* c, int n, int* stack_end) {
    int head = 0;
    while (head < n && defines(c, head)) head++;
    *stack_end = head && head < n && c->p[head].kind == BRACKETS ? c->p[head].at + c->p[head].len : 0;
    return(head);
}

// How deep in if, begin and do after a word, from how deep before it.
// What ends one that started on a line before leaves it at nothing.
static int deeper(int depth, int how) {
    depth += how == 1 ? 1 : how == -1 ? -1 : 0;
    return(depth < 0 ? 0 : depth);
}

// Whether a phrase ends after piece k of those up to b, outside an if, a
// begin or a do (depth): 2 where the writer said so, with two spaces after
// it, or after a word that lays down a cell or a string (`,` `prow,` `,"`),
// as a table's entries are; 1 at the end of an if, a begin or a do, which
// is cut at only where the writer said nothing.
static int phrase_ends(const code* c, int k, int depth, int b) {
    const piece* p = &c->p[k];
    const char* word = c->line + p->at;
    if (depth) return(0);
    if (k + 1 < b && c->p[k + 1].gap >= 2) return(2);
    if (p->kind == PLAIN && word[p->first - 1] == ',') return(2);
    if (p->kind == STRING && p->first == 2 && !strncmp(word, ",\"", 2)) return(2);
    return(control(c->line, p) == -1);
}

// A piece that starts a definition of its own: which goes at the left
// edge, and not on under what is before it.
static int starts_one(const code* c, int k) {
    static const char* const STARTS[] = { ":", "::", ":noname", "create", "variable", "2variable",
                                          "header", "header-imm", "defer", NULL };
    const piece* p = &c->p[k];
    return(p->kind == PLAIN && in_list(c->line, p->at, p->first, STARTS));
}

// Pieces a to b of a line of code, over as many lines as they need, the
// first at the indent.
static void cut(text* out, const code* c, int a, int b, int indent) {
    if (a >= b) return;
    if (fits(c, a, b, indent)) {
        put(out, c, a, b, indent);
        return;
    }
    // A definition: its name and stack comment on the first line, and its
    // code on the lines after, indented.
    int head = a;
    while (head < b && defines(c, head)) head++;
    if (head > a && head + 1 < b && c->p[head].kind == BRACKETS) {
        cut(out, c, a, head + 1, indent);
        cut(out, c, head + 1, b, indent + 4);
        return;
    }
    // What goes on from a line is indented: from a definition's first
    // line, and from a line at the left edge.
    int more = head > a ? indent + 4 : indent ? indent : 4;
    // What a create lays down goes on under itself, if it can be cut so
    // and nothing else is defined after it.
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
    // Where a phrase ends, as late as the line has room: where the writer
    // said, if anywhere; but a definition's name is not left on a line by
    // itself.  A definition of its own that follows goes at the left edge.
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
    // An if, a begin or a do too long for the line: over lines, with what
    // it holds indented.
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
            // A begin keeps what it tests, and its while, on its line if
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

// Where the comments after a line's code start, and where its code ends:
// the pieces at its end that are comments, but for a definition's stack
// comment.  The start is the number of pieces if there are none, and
// nothing if the line is only comments.
static int comments_after(const code* c, int n, int* code_end) {
    int s = n;
    while (s > 0 && is_comment(&c->p[s - 1])) s--;
    int only_defines = s > 0;
    for(int k=0; k < s; k++) only_defines &= defines(c, k);
    if (only_defines && s < n && c->p[s].kind == BRACKETS) s++;
    *code_end = s > 0 ? c->p[s - 1].at + c->p[s - 1].len : 0;
    return(s);
}

// The first n pieces of a line of code, which has no \ comment among them.
// A comment in brackets after the code stays beside it if the gap closed
// is enough, or goes on a line of its own after it.
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

// A definition whose stack comment is the start of the \ comment after
// its code, and not after its name: the line with it after the name, where
// Forth has it, made here.  Nothing if the line is not one.
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
        int effect = 0;                                 // it says what is taken and what is left
        for(const char* at=said; shut && at + 1 < shut; at++) effect |= at[0] == '-' && at[1] == '-';
        if (effect) {
            const char* rest = shut + 1;
            int rest_len = (int)(said + len - rest);
            while (rest_len && *rest == ' ') rest++, rest_len--;
            int name_end = p[head - 1].at + p[head - 1].len;
            text_more(&made, "%.*s %.*s", name_end, line, (int)(shut - said + 1), said);
            if (head < n - 1) {                         // the code after the name, as far apart as it was
                int code_end = p[n - 2].at + p[n - 2].len;
                text_more(&made, "%*s%.*s", p[head].gap, "", code_end - p[head].at, line + p[head].at);
            }
            if (rest_len) text_more(&made, " \\ %.*s", rest_len, rest);
        }
    }
    free(p);
    return(made);
}

// A line that is only `header name`, which is the definition's after it.
static int lone_header(const char* line) {
    piece* p = NULL;
    long room = 0;
    int brackets = 0, n = pieces_of(line, &brackets, &p, &room);
    int lone = n == 1 && p[0].kind == PLAIN && p[0].first < p[0].len &&
               (is_word(line, &p[0], "header") || is_word(line, &p[0], "header-imm"));
    free(p);
    return(lone);
}

// A comment put before the code it was beside: before the definition's
// header, if it is a definition's and its header is on the line above;
// and after a line of nothing, if a paragraph of comment is just above at
// the left edge, which is another paragraph.  Nothing is put inside a
// definition.
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

// A definition's line, with its purpose comment, whose words are s, taken
// off its end: n pieces are left.  The purpose comment follows the stack
// comment on the first line, and the code goes on the lines after; or it
// goes on the line after the name, indented as the code is; or it is more
// than a line, and goes before the definition.
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

// What is done with the comment after a line's code: as the rules say for
// its kind (AS_IT_IS), or on under itself as one of a group (UNDER), or on
// the line above the code, since there is no room beside it (ABOVE).
enum { AS_IT_IS, UNDER, ABOVE };

// A line of code, and the lines after it that go on with its \ comment.
// Returns the line after them.
static long fit_code_line(text* out, const text* in, long i, int brackets, int how) {
    const char* line = in->at[i];
    piece* p = NULL;
    long room = 0;
    int starts = brackets;
    int n = pieces_of(line, &brackets, &p, &room), indent = p[0].at;
    int tail = n > 1 && p[n - 1].kind == TO_END ? p[n - 1].at : -1;
    // A comment of a group whose comments go above their code goes above
    // with the rest of them, though it would fit.
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
    if (stacked) {                                      // read again, as it now is
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
        fill_after(out, line, tail, tail, &s);          // on under itself
    } else if (head && how != ABOVE) {
        purpose(out, &c, n - 1, &s, indent);
    } else {
        // A narrative comment with no room beside its code: on the line
        // above the code, or after it if the code only ends an if or a
        // loop.
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

// The comments after code that are a group: moved left together as far
// as the code allows, and what is to be done with each line's said in
// `how`.  A group goes on over lines of code with no comment after them,
// which it leaves as they are.
static void align(text* lines, char* how) {
    long n = lines->n;
    int* at = room_for(n, sizeof(int));                 // where the comments after the code start, or -1
    // Where the code ends; or on a line that goes on with the comment
    // above, -1 if that comment is one of a group's and -2 if not.
    int* ends = room_for(n, sizeof(int));
    int* plain = room_for(n, sizeof(int));              // where the line ends but for a \ comment
    char* defining = room_for(n, 1);                    // the line starts a definition
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
        // Moved left, the comments may all fit.  If not, a narrative
        // comment, or a field's in a record of several, goes on under
        // itself if there is room beside the code, and above its code if
        // there is not.  A definition's comment by itself is its purpose
        // comment, and goes where a purpose comment goes.  The comments of
        // the group move left together if then they fit or go on under
        // themselves, and otherwise only if what is left beside the code
        // does not fit where it is.
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
            if (ends[k] < 0) {                          // a line that goes on, moved as far
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

// Each line fitted to the width.
static void fit_lines(text* out, const text* in, const char* how) {
    int brackets = 0;
    where w = { 0 };                                    // where the lines read so far have left the code
    long read = 0;
    piece* seen = NULL;
    long seen_room = 0;
    for(long i=0; i < in->n; ) {
        while (read < i) read_line(in->at[read++], &w, &seen, &seen_room);
        at_top = between_things(&w);
        const char* line = in->at[i];
        int len = line_length(line), kind = comment_line(line, brackets);
        if (kind == PROSE) {                            // and the rest of its paragraph
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
        if (kind == ALONE && at_top && !indent_of(line)) {      // paragraphs are apart by a line of nothing
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

// The name of the first thing defined from a line on, for what is said.
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

// The first line after j, before e, that starts a thing after one that
// ended on the line before; or e.
static long next_thing(const char* between, const char* only, long j, long e) {
    for(long k=j + 1; k < e; k++) if (between[k - 1] && !only[k - 1]) return(k);
    return(e);
}

// Runs longer than a block cut with a line of nothing, into `done`.
// Returns how many runs could not be cut.
static int fit_runs(text* done, const text* t, long* put_in) {
    long n = t->n;
    char* nothing = room_for(n, 1);                     // a line of nothing
    char* only = room_for(n, 1);                        // a line of nothing but comments
    char* between = room_for(n, 1);                     // after this line, one thing has ended and the next is to start
    char* entries = room_for(n, 1);                     // or a table goes on with its next entry
    char* before = room_for(n + 1, 1);                  // a line of nothing goes before this one
    where w = { 0 };
    piece* p = NULL;
    long room = 0;
    int tail = -1;                                      // the column of the \ comment a line may go on with
    for(long i=0; i < n; i++) {
        nothing[i] = !line_length(t->at[i]);
        only[i] = !nothing[i] && only_comments(t->at[i], w.brackets);
        int goes = only[i] && goes_on(t->at[i], tail);
        int k = read_line(t->at[i], &w, &p, &room);
        // A line that goes on with the comment above is that line's, and
        // nothing is put between them: it is read as if it were code.
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
        // Before line j a thing starts (ENDS), or code starts after a
        // comment and runs to where the next thing starts in a block's
        // lines (COMMENT), or failing both, a table goes on with its next
        // entry (ENTRY).
        #define ENDS(j)     (between[(j) - 1] && !only[(j) - 1])
        #define COMMENT(j)  (between[(j) - 1] && only[(j) - 1] && !only[j] && \
                             next_thing(between, only, (j), e) - (j) <= LINES)
        #define ENTRY(j)    (entries[(j) - 1] && !only[(j) - 1])
        for(long at=i; has_code && e - at > LINES; ) {
            long cut_at = -1;
            for(long j=at + LINES; j > at && cut_at < 0; j--) if (ENDS(j)) cut_at = j;
            for(long j=at + LINES; j > at && cut_at < 0; j--) if (COMMENT(j)) cut_at = j;
            for(long j=at + LINES; j > at && cut_at < 0; j--) if (ENTRY(j)) cut_at = j;
            if (cut_at < 0) {                           // the next place, and what is before it is too long, unless it is comment
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
