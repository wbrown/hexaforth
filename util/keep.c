//
// keep GIT KEPT LABEL IMAGE.hex FILE...
//
// What was measured of an image, kept where the next measurement does not
// write over it.
//
// A target that measures writes what it found where it always does, so a
// change that is to be compared with what was there before has nothing to
// be compared with once it has been measured: the one before is gone.
// This copies each FILE into KEPT, a directory of build/, under the image
// it was measured of, the measurement's LABEL, and which time it is:
//
//   KEPT/IMAGE-AS-IT-IS/LABEL/TIME/FILE
//
// An image as it is, is the first twelve digits of a number made of its
// bytes: two images that are the same bytes are the same image, whatever
// the tree was called when each was built.  So what is kept is kept by
// what was measured, and not by when.
//
// Nothing that is kept is written over.  The times are numbered from 1.
// If every FILE is byte for byte what the last time has, nothing is
// copied, and the log says it is as that time was; if any is not, which
// is the same image given other work or measured another way, it is
// another time, beside the last.
//
// KEPT/IMAGE-AS-IT-IS/image/ has the image itself and what was written
// beside it (its .tables, .sizes and .lst), so that counts by place can be
// said by word long after build/ has another image in it.  What is
// measured need not be an image (a kernel, the CPU's Verilog): a file that
// is not a .hex is what was measured all the same, and is kept there by
// itself.
// KEPT/IMAGE-AS-IT-IS/of.txt says what the image is: its file, the commit
// the tree was at when it was first kept (GIT is asked) and whether the
// tree had changes not committed, and when that was.  KEPT/log.txt has a
// line for every time something was measured, in order: when, the image
// as it is, the commit, the label and its time, and the image's file.  It
// is what is read to find the one before.
//
// It fails if the image or a file is not there: then nothing was
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

// A directory, and those it is in, made if they are not there.
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

// A number made of some bytes (FNV-1a), which is the same for the same
// bytes and all but never for others.
static uint64_t number_of(const unsigned char* bytes, size_t len) {
    uint64_t h = 1469598103934665603ull;
    for(size_t at=0; at < len; at++) h = (h ^ bytes[at]) * 1099511628211ull;
    return(h);
}

// The first line of what git says when it is asked something of the tree
// a file is in, without its end: made here, and empty if it said nothing.
// It is the image's tree that is asked, which is not always the tree this
// is run in: an image built in a copy of the tree is of that copy's commit.
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

// Whether two files are there and are the same bytes.
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

// A file copied to a directory, under its own name.
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

// The image, and what was written beside it, kept the first time anything
// is kept of it.
static void image_kept(const char* image, const char* of_image) {
    static const char* const beside[] = { ".hex", ".tables", ".sizes", ".lst" };
    char* dir = NULL;
    text_more(&dir, "%s/image", of_image);
    if (there(dir)) { free(dir); return; }
    directory(dir);
    size_t stem = strlen(image);
    if (stem > 4 && !strcmp(image + stem - 4, ".hex")) stem -= 4;
    else copied(image, dir);                            // what was measured is not an image: it is kept as it is
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

    // The last time this was measured of this image, and whether this is
    // that again.
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
