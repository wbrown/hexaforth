//
// said_files GIT KIND... [--not DIRECTORY...] [--besides NAME...]
//
// The files that the tree's own text says by name, and whether each is
// there: the target `said`, which CMakeLists.txt says.
//
// A file is named in many places besides where it is: by what includes
// it, by CMake, in the comment at the top of another file, in CLAUDE.md.
// When it is given another name, or taken away, each of those is wrong,
// and nothing that is built finds the ones that are only said.  This does.
//
// It is run where the tree is.  The tree's files are those git has, and
// those it has not been told of and has not been told to leave out: GIT is
// asked.  What is in a DIRECTORY is not among them (worktrees, which is
// other trees).  It reads each that is text.  A name is a run of letters,
// digits and _ + . / - that ends in a dot and one of the KINDs, which are
// the endings of the tree's source: `c`, `h`, `fs`.  It is there if some
// file's path ends with it, whatever directory it was said from.  These
// are not looked for:
//
//   in C, a name that is not in a comment or between quotes: a part of a
//   structure is said with a dot too, and a header of the system's is
//   between < >;
//   a name with `build` among its directories, which is made, and one
//   after a % between quotes, which is yet to be made;
//   a name with no small letter before its ending, which stands for any
//   file (FILE.fs), and before the name a directory with none (FORTH/),
//   which stands for wherever that is;
//   and each NAME, which is a file that is said here and is not the
//   tree's: CMakeLists.txt says which and why.
//
// It prints each name that is not there after the file and the line that
// say it, and then how many names were said in how many files.  It fails
// if any is not there, and if nothing was said.
//
// Code that tools share is a header, which each includes, and a file of C
// is compiled by itself.  Some are both, as the machine is (vm.c): it says
// which, after the rest.
//
// said_files GIT --figures WORD... [--not DIRECTORY...]
//
// The figures the tree's comments and documents say: the target `figures`.
// How big a thing is, how many of a thing there are and what a thing costs
// is what the build prints, and written down it is a fact said a second
// time, which someone then has to keep in step (CLAUDE.md).  This prints
// each line of a comment, or of a document, that has a number and then a
// WORD, which is what such a figure is of (`bytes`, `cycles`, `%`), for
// someone to read: a number that says what the machine is stays, and one
// that says how much there was when it was written does not.  Nothing here
// can tell the two apart, so it fails nothing.
//

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "room.h"

typedef struct {
    char** at;
    long n, room;
} names;

static void one_more(names* list, char* name) {
    ONE_MORE(list->at, list->n, list->room);
    list->at[list->n++] = name;
}

static int among(const names* list, const char* name, size_t len) {
    for(long idx=0; idx<list->n; idx++)
        if (strlen(list->at[idx]) == len && !strncmp(list->at[idx], name, len)) return(1);
    return(0);
}

static names KINDS, SKIPPED, BESIDES, FILES, INCLUDED, FIGURES;

// The files of C that a file includes, which are compiled by themselves as
// well, or they would be headers: each is kept, once, by its name alone.
static void included_in(const unsigned char* text, size_t len) {
    for(size_t at=0; at<len; at++) {
        if (at && text[at - 1] != '\n') continue;
        const char* line = (const char*)text + at;
        size_t all = len - at;
        const char* end = memchr(line, '\n', all);
        if (end) all = end - line;
        size_t in = 0;
        while (in < all && (line[in] == ' ' || line[in] == '#')) in++;
        if (all - in < 8 || strncmp(line + in, "include", 7)) continue;
        const char* name = memchr(line + in, '"', all - in);
        if (!name) continue;
        const char* to = memchr(name + 1, '"', line + all - name - 1);
        if (!to || to - name < 3 || strncmp(to - 2, ".c", 2)) continue;
        while (memchr(name + 1, '/', to - name - 1)) name = memchr(name + 1, '/', to - name - 1);
        if (among(&INCLUDED, name + 1, to - name - 1)) continue;
        char* kept = NULL;
        text_more(&kept, "%.*s", (int)(to - name - 1), name + 1);
        one_more(&INCLUDED, kept);
    }
}

// Is a file in a directory that is skipped?
static int is_skipped(const char* path) {
    for(const char* at = strchr(path, '/'); at; at = strchr(at + 1, '/'))
        if (among(&SKIPPED, path, at - path)) return(1);
    return(0);
}

// The tree's files, by their paths from where this is run: git is asked
// for those it has and those it has not been told of or told to leave out.
// One that is no longer there, or is a link, is not among them.
static void files_of(const char* git) {
    char* asked = NULL;
    text_more(&asked, "'%s' ls-files --cached --others --exclude-standard", git);
    FILE* in = popen(asked, "r");
    if (!in) {
        perror(asked);
        exit(EXIT_FAILURE);
    }
    char* line = NULL;
    long room = 0;
    while (line_of(in, &line, &room)) {
        line[strcspn(line, "\n")] = '\0';
        struct stat is;
        if (is_skipped(line) || lstat(line, &is) || !S_ISREG(is.st_mode)) continue;
        char* path = NULL;
        text_more(&path, "%s", line);
        one_more(&FILES, path);
    }
    if (pclose(in)) {
        fprintf(stderr, "%s: it did not say what the tree's files are\n", asked);
        exit(EXIT_FAILURE);
    }
    free(line);
    free(asked);
}

// Is a file C, by its name?
static int is_c(const char* path) {
    const char* dot = strrchr(path, '.');
    return(dot && (!strcmp(dot, ".c") || !strcmp(dot, ".h") || !strcmp(dot, ".cpp")));
}

// A file of C with nothing left of it but its comments and, if they are
// wanted, what is between quotes: the rest is spaces, and so is a \ between
// quotes and the character after it, which is not part of a name ("\nfuse"
// is a new line).  Its lines are where they were.
static void only_said(unsigned char* text, size_t len, int quoted) {
    enum { CODE, LINE, BLOCK, QUOTED, CHARACTER } in = CODE;
    for(size_t at=0; at<len; at++) {
        unsigned char c = text[at];
        unsigned char next = at + 1 < len ? text[at + 1] : '\0';
        if (c == '\n') {
            if (in == LINE) in = CODE;
            continue;
        }
        switch (in) {
            case CODE:
                text[at] = ' ';
                if (c == '/' && (next == '/' || next == '*')) {
                    in = next == '/' ? LINE : BLOCK;
                    text[++at] = ' ';
                } else if (c == '"') {
                    in = QUOTED;
                } else if (c == '\'') {
                    in = CHARACTER;
                }
                break;
            case BLOCK:
                if (c == '*' && next == '/') {
                    in = CODE;
                    text[at] = text[at + 1] = ' ';
                    at++;
                }
                break;
            case QUOTED:
            case CHARACTER:
                if (c == '\\') {
                    text[at] = ' ';
                    if (next && next != '\n') text[++at] = ' ';
                } else if (c == (in == QUOTED ? '"' : '\'')) {
                    in = CODE;
                    text[at] = ' ';
                } else if (in == CHARACTER || !quoted) {
                    text[at] = ' ';
                }
                break;
            case LINE:
                break;
        }
    }
}

static int of_a_name(int c) {
    return(isalnum(c) || c == '_' || c == '+' || c == '.' || c == '/' || c == '-');
}

// Is one of a name's directories `build`?
static int is_made(const char* name, size_t len) {
    const char* at = name;
    const char* end = name + len;
    while (at < end) {
        const char* to = memchr(at, '/', end - at);
        if (!to) break;
        if (to - at == 5 && !strncmp(at, "build", 5)) return(1);
        at = to + 1;
    }
    return(0);
}

// Does some file's path end with a name, at a directory?
static int is_there(const char* name, size_t len) {
    for(long idx=0; idx<FILES.n; idx++) {
        size_t all = strlen(FILES.at[idx]);
        if (all < len || strncmp(FILES.at[idx] + all - len, name, len)) continue;
        if (all == len || FILES.at[idx][all - len - 1] == '/') return(1);
    }
    return(0);
}

// The names a file says: how many, and each that is not there printed.
static long said_in(const char* path, long* missing) {
    unsigned char* text;
    size_t len = file_bytes(path, &text);
    long said = 0, line = 1;
    if (!text || memchr(text, '\0', len)) {     // not text
        free(text);
        return(0);
    }
    if (is_c(path)) {
        included_in(text, len);
        only_said(text, len, 1);
    }
    for(size_t at=0; at<len; ) {
        if (text[at] == '\n') line++;
        if (!of_a_name(text[at])) {
            at++;
            continue;
        }
        size_t from = at;
        while (at < len && of_a_name(text[at])) at++;
        size_t to = at;
        if (from > 0 && text[from - 1] == '%') from++;                  // "%s.fs" is a name yet to be made
        while (to > from && strchr(".-/", text[to - 1])) to--;          // the end of a sentence is not the name's
        for(;;) {                                                       // nor is where it is said from,
            size_t under = from;                                        // nor a directory that stands for any
            int small = 0;
            while (under < to && text[under] != '/') small |= islower(text[under++]);
            if (under < to && (under == from || !small || text[from] == '.')) from = under + 1;
            else if (to > from && text[from] == '-') from++;
            else break;
        }
        const char* name = (const char*)text + from;
        size_t n = to - from;
        size_t dot = n, stem = 0;
        for(size_t idx=0; idx<n; idx++) {
            if (name[idx] == '.') dot = idx;
            if (name[idx] == '/') stem = idx + 1;
        }
        if (dot >= n || dot <= stem || !among(&KINDS, name + dot + 1, n - dot - 1)) continue;
        int small = 0;
        for(size_t idx=stem; idx<dot; idx++) small |= islower((unsigned char)name[idx]);
        if (!small || is_made(name, n)) continue;
        if (among(&BESIDES, name, n) || among(&BESIDES, name + stem, n - stem)) continue;
        said++;
        if (is_there(name, n)) continue;
        printf("%s:%ld: %.*s is said, and is not a file of the tree\n", path, line, (int)n, name);
        (*missing)++;
    }
    free(text);
    return(said);
}

static int ends_with(const char* path, const char* end) {
    size_t all = strlen(path), len = strlen(end);
    return(all >= len && !strcmp(path + all - len, end));
}

// A file with nothing left of it but what it says to a reader: the
// comments of C, of Forth (after a \ that is a word) and of CMake (after a
// #), and all of a document.  False if it is none of those.
static int only_prose(const char* path, unsigned char* text, size_t len) {
    if (ends_with(path, ".md")) return(1);
    if (is_c(path)) {
        only_said(text, len, 0);
        return(1);
    }
    int forth = ends_with(path, ".fs");
    if (!forth && !ends_with(path, ".cmake") && !ends_with(path, "CMakeLists.txt")) return(0);
    int said = 0;
    for(size_t at=0; at<len; at++) {
        unsigned char c = text[at];
        if (c == '\n') {
            said = 0;
            continue;
        }
        if (said) continue;
        int before = !at || text[at - 1] == '\n' || text[at - 1] == ' ';
        int after = at + 1 >= len || text[at + 1] == '\n' || text[at + 1] == ' ';
        if (forth ? c == '\\' && before && after : c == '#') said = 1;
        text[at] = ' ';
    }
    return(1);
}

// The lines of a file's prose that have a figure in them, printed: how
// many.  A figure is a number, of digits with commas and a point, that is
// not part of a word or of a number said otherwise ($ff, 0x4000), and
// then one of the words a figure is of, which may be on the next line.
static long figures_in(const char* path) {
    unsigned char* text;
    size_t len = file_bytes(path, &text);
    long said = 0, line = 1;
    if (!text || memchr(text, '\0', len) || !only_prose(path, text, len)) {
        free(text);
        return(0);
    }
    for(size_t at=0; at<len; at++) {
        if (text[at] == '\n') line++;
        if (!isdigit(text[at]) || (at && (isalnum(text[at - 1]) || strchr("_$#.-", text[at - 1])))) continue;
        size_t to = at;
        while (to < len && (isdigit(text[to]) || ((text[to] == ',' || text[to] == '.') && to + 1 < len && isdigit(text[to + 1])))) to++;
        size_t of = to;
        while (of < len && strchr(" \t\n/\\#*", text[of])) of++;
        size_t end = of;
        while (end < len && (isalpha(text[end]) || text[end] == '%')) end++;
        if (!among(&FIGURES, (const char*)text + of, end - of)) {
            at = to - 1;
            continue;
        }
        size_t from = at;
        while (from && text[from - 1] != '\n') from--;
        size_t stop = at;
        while (stop < len && text[stop] != '\n') stop++;
        while (from < stop && strchr(" \t", text[from])) from++;
        printf("%s:%ld: %.*s\n", path, line, (int)(stop - from), (const char*)text + from);
        said++;
        at = stop - 1;                          // a line is printed once
    }
    free(text);
    return(said);
}

int main(int argc, char *argv[]) {
    names* to = &KINDS;
    for(int idx=2; idx<argc; idx++) {
        if (!strcmp(argv[idx], "--not")) to = &SKIPPED;
        else if (!strcmp(argv[idx], "--besides")) to = &BESIDES;
        else if (!strcmp(argv[idx], "--figures")) to = &FIGURES;
        else one_more(to, argv[idx]);
    }
    if (!KINDS.n && !FIGURES.n) {
        fprintf(stderr, "Usage: %s GIT KIND... [--not DIRECTORY...] [--besides NAME...]\n", argv[0]);
        fprintf(stderr, "       %s GIT --figures WORD... [--not DIRECTORY...]\n", argv[0]);
        return(1);
    }
    files_of(argv[1]);
    if (FIGURES.n) {
        long lines = 0, of = 0;
        for(long idx=0; idx<FILES.n; idx++) {
            long here = figures_in(FILES.at[idx]);
            if (here) of++;
            lines += here;
        }
        printf("figures: %ld lines of %ld files have one, each to be read\n", lines, of);
        return(0);
    }
    long said = 0, missing = 0, in = 0;
    for(long idx=0; idx<FILES.n; idx++) {
        long here = said_in(FILES.at[idx], &missing);
        if (here) in++;
        said += here;
    }
    if (missing || !said) {
        printf("FAIL said: %ld names of files said in %ld files, of which %ld are not files of the tree\n",
               said, in, missing);
        return(1);
    }
    printf("said: %ld names of files said in %ld files, and each is a file of the tree\n", said, in);
    if (INCLUDED.n) {
        printf("said: %ld files of C are compiled by themselves and included in another as well:", INCLUDED.n);
        for(long idx=0; idx<INCLUDED.n; idx++) printf(" %s", INCLUDED.at[idx]);
        printf("\n");
    }
    return(0);
}
