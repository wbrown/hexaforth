//
// keep GIT KEPT LABEL IMAGE.hex FILE...
//
// Saves the results of a measurement of an image where the next
// measurement will not overwrite them.
//
// A measuring target always writes its results to the same place.  So once
// a change has been measured, the results from before the change are gone,
// and there is nothing to compare with.  This copies each FILE into KEPT,
// a directory under build/, filed by the image that was measured, the
// measurement's LABEL, and a run number:
//
//   KEPT/IMAGE-ID/LABEL/RUN/FILE
//
// IMAGE-ID is the first twelve hex digits of a hash of the image's bytes.
// Two images with the same bytes are the same image, whatever the tree was
// called when each was built.  So results are filed by what was measured,
// not by when.
//
// Nothing that has been kept is overwritten.  Runs are numbered from 1.
// If every FILE is byte for byte the same as in the last run, nothing is
// copied, and the log records that this run repeated that one.  If any
// file differs, which means the same image was given other work or was
// measured another way, the results become a new run beside the last.
//
// KEPT/IMAGE-ID/image/ holds the image itself and the files cross.fs wrote
// beside it (its .tables, .sizes, .lst and .machine).  With those, counts
// by address can still be reported by word long after build/ holds another
// image, by a tool that takes its machine from the image (util/image.h).
// The thing measured need not be an image: it can be a kernel, or the
// CPU's Verilog.  A file that is not a .hex is still what was measured,
// and is kept there on its own.
//
// KEPT/IMAGE-ID/of.txt describes the image: its file, the commit the tree
// was at when it was first kept (GIT is run to find out), whether the tree
// had uncommitted changes, and the time.  KEPT/log.txt has one line for
// every measurement, in order: the time, the image's id, the commit, the
// label and run number, and the image's file.  The log is where to look to
// find the run from before a change.
//
// It fails if the image or a file is missing, because then nothing was
// measured.
//

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "room.h"

// Creates a directory, and its parents, if they do not exist.
static void directory(const char* path) {
    char* part = NULL;
    text_more(&part, "%s", path);
    for(char* at = part + 1; ; at++) {
        if (*at && *at != '/') continue;
        char was = *at;
        *at = 0;
        if (mkdir(part, 0777) && errno != EEXIST) { perror(part); exit(EXIT_FAILURE); }
        *at = was;
        if (!was) break;
    }
    free(part);
}

// Returns a hash of some bytes (FNV-1a).
static uint64_t number_of(const unsigned char* bytes, size_t len) {
    uint64_t h = 1469598103934665603ull;
    for(size_t at=0; at < len; at++) h = (h ^ bytes[at]) * 1099511628211ull;
    return(h);
}

// Runs a git command in the tree that contains a file, and returns the
// first line of its output, without the newline, as an allocated string.
// The string is empty if git printed nothing.  The command runs in the
// image's tree, which is not always the tree this program was started in:
// an image built in a copy of the tree belongs to that copy's commit.
static char* git_says(const char* git, const char* in_tree_of, const char* asked) {
    char* command = NULL;
    char* line = NULL;
    long room = 0;
    const char* slash = strrchr(in_tree_of, '/');
    if (slash) text_more(&command, "\"%s\" -C \"%.*s\" %s 2>/dev/null", git, (int)(slash - in_tree_of), in_tree_of, asked);
    else text_more(&command, "\"%s\" %s 2>/dev/null", git, asked);
    FILE* said = popen(command, "r");
    if (said && line_of(said, &line, &room)) line[strcspn(line, "\r\n")] = 0;
    if (said) pclose(said);
    free(command);
    if (!line) text_more(&line, "%s", "");
    return(line);
}

static const char* file_of(const char* path) {
    const char* slash = strrchr(path, '/');
    return(slash ? slash + 1 : path);
}

static int there(const char* path) {
    struct stat is;
    return(!stat(path, &is));
}

// Returns 1 if two files both exist and have the same bytes.
static int same(const char* one, const char* other) {
    unsigned char* a;
    unsigned char* b;
    if (!there(one) || !there(other)) return(0);
    size_t la = file_bytes(one, &a);
    size_t lb = file_bytes(other, &b);
    int is = a && b && la == lb && !memcmp(a, b, la);
    free(a);
    free(b);
    return(is);
}

// Copies a file into a directory, keeping its name.
static void copied(const char* from, const char* dir) {
    unsigned char* file;
    size_t size = file_bytes(from, &file);
    char* to = NULL;
    if (!file) { fprintf(stderr, "keep: there is no %s to keep\n", from); exit(EXIT_FAILURE); }
    text_more(&to, "%s/%s", dir, file_of(from));
    if (!write_whole(to, (const char*)file, size)) { perror(to); exit(EXIT_FAILURE); }
    free(to);
    free(file);
}

// Saves the image, and the files written beside it, the first time
// anything is kept for that image.
static void image_kept(const char* image, const char* of_image) {
    static const char* const beside[] = { ".hex", ".tables", ".sizes", ".lst", ".machine" };
    char* dir = NULL;
    text_more(&dir, "%s/image", of_image);
    if (there(dir)) { free(dir); return; }
    directory(dir);
    size_t stem = strlen(image);
    if (stem > 4 && !strcmp(image + stem - 4, ".hex")) stem -= 4;
    else copied(image, dir);                            // the thing measured is not an image: keep the file itself
    for(size_t b=0; stem < strlen(image) && b < sizeof(beside) / sizeof(beside[0]); b++) {
        char* from = NULL;
        text_more(&from, "%.*s%s", (int)stem, image, beside[b]);
        if (there(from)) copied(from, dir);
        free(from);
    }
    free(dir);
}

int main(int argc, char *argv[]) {
    if (argc < 6) {
        fprintf(stderr, "usage: keep GIT KEPT LABEL IMAGE.hex FILE...\n");
        return(1);
    }
    const char* git = argv[1];
    const char* kept = argv[2];
    const char* label = argv[3];
    const char* image = argv[4];

    unsigned char* bytes;
    size_t len = file_bytes(image, &bytes);
    if (!bytes || !len) { fprintf(stderr, "keep: there is no image %s\n", image); return(1); }
    char as_it_is[13];
    snprintf(as_it_is, sizeof(as_it_is), "%012llx", (unsigned long long)(number_of(bytes, len) >> 16));
    free(bytes);

    char when[32];
    time_t now = time(NULL);
    strftime(when, sizeof(when), "%Y-%m-%d %H:%M:%S", localtime(&now));
    char* commit = git_says(git, image, "rev-parse --short=12 HEAD");
    char* changed = git_says(git, image, "status --porcelain --untracked-files=no");

    char* of_image = NULL;
    char* labelled = NULL;
    char* into = NULL;
    char* said = NULL;
    text_more(&of_image, "%s/%s", kept, as_it_is);
    text_more(&labelled, "%s/%s", of_image, label);
    directory(labelled);
    image_kept(image, of_image);

    // Find the last run of this measurement for this image, and whether
    // this run repeats it.
    int last = 0;
    for(;;) {
        char* next = NULL;
        text_more(&next, "%s/%d", labelled, last + 1);
        int is = there(next);
        free(next);
        if (!is) break;
        last++;
    }
    int again = last > 0;
    for(int a=5; again && a < argc; a++) {
        char* had = NULL;
        text_more(&had, "%s/%d/%s", labelled, last, file_of(argv[a]));
        again = same(argv[a], had);
        free(had);
    }
    int time_of = again ? last : last + 1;
    text_more(&into, "%s/%d", labelled, time_of);

    text_more(&said, "%s/of.txt", of_image);
    if (!there(said)) {
        FILE* out = fopen(said, "w");
        if (!out) { perror(said); return(1); }
        fprintf(out, "%s\nfirst kept %s, the tree at %s%s\n", image, when, commit[0] ? commit : "no commit",
                changed[0] ? ", with changes not committed" : "");
        fclose(out);
    }

    for(int a=5; a < argc; a++) {
        if (!there(argv[a])) { fprintf(stderr, "keep: there is no %s to keep\n", argv[a]); return(1); }
    }
    if (!again) {
        directory(into);
        for(int a=5; a < argc; a++) copied(argv[a], into);
    }

    char* log = NULL;
    text_more(&log, "%s/log.txt", kept);
    FILE* out = fopen(log, "a");
    if (!out) { perror(log); return(1); }
    fprintf(out, "%s  %s  %s%s  %s/%d%s  %s\n", when, as_it_is, commit[0] ? commit : "no commit",
            changed[0] ? "+" : "", label, time_of, again ? " again" : "", image);
    fclose(out);
    printf("%s: %s of %s, which is %s as it is, in %s\n", again ? "as it was kept" : "kept", label, image, as_it_is, into);
    return(0);
}
