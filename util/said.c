//
// said_files GIT KIND... [--not DIRECTORY...] [--besides NAME...]
//
// Finds the file names mentioned in the tree's own text, and checks that
// each one names a file that exists: the target `said`, defined in
// cmake/said.cmake.
//
// A file is mentioned in many places besides its own path: in the files
// that include it, in CMake, in the comment at the top of another file,
// in CLAUDE.md.  When the file is renamed or removed, every one of those
// mentions is wrong, and the build catches only the ones it uses.  This
// catches the rest.
//
// Run it from the top of the tree.  The tree's files are the ones git
// tracks, plus the untracked ones that git does not ignore; it runs GIT
// to list them.  Files under a DIRECTORY are left out (worktrees, which
// holds other trees).  It reads every file that is text.  A name is a run
// of letters, digits and _ + . / - that ends in a dot and one of the
// KINDs.  The KINDs are the extensions of the tree's source: `c`, `h`,
// `fs`.  A name exists if some file's path ends with it, so it is found
// whatever directory it was written relative to.  These are not checked:
//
//   In C, a name outside a comment or a string.  A struct member is also
//   written with a dot, and a system header is between < >.
//
//   A name with `build` among its directories, which is a generated file,
//   and a name that follows a % in a string, which is a format that
//   builds a name.
//
//   A name with no lower-case letter before its extension, which is a
//   placeholder for any file (FILE.fs).  A leading directory with no
//   lower-case letter (FORTH/) is a placeholder too, and is dropped before
//   the name is looked up.
//
//   Each NAME, which is a file that is mentioned in the tree and does not
//   belong to it.  cmake/said.cmake lists them and gives the reason for
//   each.
//
// It prints each missing name after the file and line that mention it,
// and then the number of names it checked and the number of files they
// were in.  It fails if any name is missing, or if it found no names at
// all.
//
// Code that tools share is in a header, which each tool includes, and a
// .c file is compiled on its own.  A few .c files are compiled on their
// own and also included by another file, as the machine is (vm.c).  It
// lists those last.
//
// said_files GIT --figures WORD... [--not DIRECTORY...]
//
// Lists the figures in the tree's comments and documents: the target
// `figures`.  The build prints sizes, counts and costs.  A figure written
// down is a second copy of one of those, which someone then has to keep
// up to date (CLAUDE.md).  This prints every line of a comment or a
// document that has a number followed by a WORD.  The WORDs are the units
// such figures come in (`bytes`, `cycles`, `%`).  A person reads the list:
// a number that defines the machine stays, and a number that records a
// measurement from the time of writing goes.  The program cannot tell the
// two apart, so it never fails.
//
// said_files GIT --words WORD... [--not DIRECTORY...]
//
// Lists where the tree's text mentions a Forth word, for use before a
// word is renamed or removed.  It is run by hand from the top of the
// tree, with the words in question
// (`build/cmake/said_files git --words tuck --not worktrees`).  A Forth
// word is delimited by spaces.  In a comment or a document it can also be
// delimited by quotes, backticks or brackets, or be followed by a comma
// or a full stop.  It prints each line that has a WORD delimited that
// way, after the file and line number.  It also prints, marked `within`,
// each line that has the WORD only as part of a longer word (`or` in
// `or!`).  That is a different word, which a rename must leave alone.  It
// exits with status 1 if any line has a WORD as a whole word.
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

static names KINDS, SKIPPED, BESIDES, FILES, INCLUDED, FIGURES, WORDS;

// Records the .c files that a file includes.  A .c file is also compiled
// on its own; otherwise it would be a header.  Each is recorded once, by
// its file name without the directory.
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

// Returns 1 if a file is under one of the skipped directories.
static int is_skipped(const char* path) {
    for(const char* at = strchr(path, '/'); at; at = strchr(at + 1, '/'))
        if (among(&SKIPPED, path, at - path)) return(1);
    return(0);
}

// Lists the tree's files in FILES, by path relative to the current
// directory.  It runs git to list the tracked files and the untracked
// files that are not ignored.  A file that no longer exists, or that is a
// symbolic link, is left out.
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

// Returns 1 if a file's name has a C or C++ extension.
static int is_c(const char* path) {
    const char* dot = strrchr(path, '.');
    return(dot && (!strcmp(dot, ".c") || !strcmp(dot, ".h") || !strcmp(dot, ".cpp")));
}

// Blanks out everything in a C file's text except its comments and, if
// `quoted` is set, the contents of its strings.  Everything else becomes
// spaces.  So does a backslash in a string and the character after it,
// which is an escape and not part of a name ("\nfuse" is a newline and
// then "fuse").  Newlines are kept, so line numbers do not change.
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

// Returns 1 if one of the directories in a name is `build`.
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

// Returns 1 if some file's path ends with the name, starting at a
// directory boundary.
static int is_there(const char* name, size_t len) {
    for(long idx=0; idx<FILES.n; idx++) {
        size_t all = strlen(FILES.at[idx]);
        if (all < len || strncmp(FILES.at[idx] + all - len, name, len)) continue;
        if (all == len || FILES.at[idx][all - len - 1] == '/') return(1);
    }
    return(0);
}

// Finds the file names mentioned in one file.  Prints each that does not
// exist, adds their number to `*missing`, and returns the number of names
// it checked.
static long said_in(const char* path, long* missing) {
    unsigned char* text;
    size_t len = file_bytes(path, &text);
    long said = 0, line = 1;
    if (!text || memchr(text, '\0', len)) {     // a binary file
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
        if (from > 0 && text[from - 1] == '%') from++;                  // "%s.fs" is a format that builds a name
        while (to > from && strchr(".-/", text[to - 1])) to--;          // drop the punctuation that ends a sentence
        // Drop each leading directory that is empty, starts with a dot
        // (./ and ../), or has no lower-case letter (a placeholder), and
        // drop a leading -.
        for(;;) {
            size_t under = from;
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

// Blanks out everything in a file's text except its prose: the comments
// of C, of Forth (after a \ that is a word by itself) and of CMake (after
// a #), or the whole text of a Markdown document.  Returns 0 if the file
// is none of those kinds.
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

// Prints the lines of a file's prose that contain a figure, and returns
// how many there are.  A figure is a decimal number (digits, with commas
// and a decimal point) followed by one of the FIGURES words, which may be
// on the next line.  A number that is part of a word, or of a number in
// another notation ($ff, 0x4000), does not count.
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
        at = stop - 1;                          // print each line only once
    }
    free(text);
    return(said);
}

// Prints the lines of a file that contain one of the WORDS, and returns
// how many contain one as a whole word.  A Forth word is whole if it has
// on each side a space, the end of the line, or the punctuation prose puts
// around a word (BEFORE and AFTER).
static long words_in(const char* path) {
    static const char BEFORE[] = " \t`\"'([{,";
    static const char AFTER[] = " \t`\"')]},.";
    unsigned char* text;
    size_t len = file_bytes(path, &text);
    long said = 0, line = 1;
    if (!text || memchr(text, '\0', len)) {
        free(text);
        return(0);
    }
    for(size_t from=0; from < len; line++) {
        size_t to = from;
        while (to < len && text[to] != '\n') to++;
        for(long w=0; w < WORDS.n; w++) {
            const char* word = WORDS.at[w];
            size_t n = strlen(word);
            int as_word = 0, within = 0;
            for(size_t at=from; at + n <= to; at++) {
                if (strncmp((const char*)text + at, word, n)) continue;
                int before = at == from || strchr(BEFORE, text[at - 1]);
                int after = at + n == to || strchr(AFTER, text[at + n]);
                if (before && after) as_word = 1; else within = 1;
            }
            if (!as_word && !within) continue;
            printf("%s:%ld: %s%s: %.*s\n", path, line, word, as_word ? "" : " (within)", (int)(to - from), (const char*)text + from);
            said += as_word;
        }
        from = to + 1;
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
        else if (!strcmp(argv[idx], "--words")) to = &WORDS;
        else one_more(to, argv[idx]);
    }
    if (!KINDS.n && !FIGURES.n && !WORDS.n) {
        fprintf(stderr, "Usage: %s GIT KIND... [--not DIRECTORY...] [--besides NAME...]\n", argv[0]);
        fprintf(stderr, "       %s GIT --figures WORD... [--not DIRECTORY...]\n", argv[0]);
        fprintf(stderr, "       %s GIT --words WORD... [--not DIRECTORY...]\n", argv[0]);
        return(1);
    }
    files_of(argv[1]);
    if (WORDS.n) {
        long lines = 0, of = 0;
        for(long idx=0; idx<FILES.n; idx++) {
            long here = words_in(FILES.at[idx]);
            if (here) of++;
            lines += here;
        }
        printf("worded: %ld lines of %ld files say one of the words\n", lines, of);
        return(lines ? 1 : 0);
    }
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
