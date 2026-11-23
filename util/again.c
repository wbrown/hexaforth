//
// again NAME FILE.shadows [NAME FILE.shadows ...]
//
// Prints the names that a source defines more than once, merged across
// several images built from it, with each name listed once.
//
// Beside every image, cross.fs writes the names it found being defined
// when a definition of that name already existed (cross.fs, names defined
// again).  The file is two lines that describe it, and then, for each kind
// of redefinition, a line naming the kind followed by one line per name,
// each starting with spaces.  The machines in util/machines are built from
// one source, so their files are mostly the same.  This prints the first
// file's two lines, and then each kind once, with every name that any
// image has under it.  A name that some images lack is followed by the
// images that have it, using the NAME given with each file.
//
// The targets quick and check print this before their tests.  A name
// defined again is not an error, since Forth uses the newest definition.
// This list is where an unintended redefinition gets noticed, so that it
// does not have to be found by debugging.
//
// It fails if a file is missing, because that means an image was not
// built.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "room.h"

typedef struct {
    char* text;                 // a name's line
    long kind;                  // the index of its kind
    unsigned long in;           // the files that have it, one bit per file
} entry;

static char** KINDS;            // the line naming each kind, in the order first read
static long kinds, kinds_room;
static entry* ENTRIES;
static long entries, entries_room;

static char* kept(const char* line) {
    char* is = room_for(strlen(line) + 1, 1);
    strcpy(is, line);
    return(is);
}

static long kind_of(const char* line) {
    for(long k=0; k < kinds; k++) if (!strcmp(KINDS[k], line)) return(k);
    ONE_MORE(KINDS, kinds, kinds_room);
    KINDS[kinds] = kept(line);
    return(kinds++);
}

static void note(const char* line, long kind, int file) {
    for(long e=0; e < entries; e++) {
        if (ENTRIES[e].kind == kind && !strcmp(ENTRIES[e].text, line)) {
            ENTRIES[e].in |= 1ul << file;
            return;
        }
    }
    ONE_MORE(ENTRIES, entries, entries_room);
    ENTRIES[entries].text = kept(line);
    ENTRIES[entries].kind = kind;
    ENTRIES[entries++].in = 1ul << file;
}

int main(int argc, char *argv[]) {
    int files = (argc - 1) / 2;
    if (argc < 3 || argc % 2 != 1 || files > 32) {
        fprintf(stderr, "usage: again NAME FILE.shadows [NAME FILE.shadows ...]\n");
        return(1);
    }
    char* line = NULL;
    long room = 0;
    for(int f=0; f < files; f++) {
        const char* path = argv[2 + 2 * f];
        FILE* in = fopen(path, "r");
        if (!in) {
            perror(path);
            return(1);
        }
        long kind = -1;
        for(long n=0; line_of(in, &line, &room); n++) {
            line[strcspn(line, "\r\n")] = 0;
            if (n < 2) {                                // the two lines that describe the file
                if (!f) puts(line);
            } else if (line[0] == ' ') {
                if (kind >= 0) note(line, kind, f);
            } else if (line[0]) {
                kind = kind_of(line);
            }
        }
        fclose(in);
    }

    const unsigned long all = files == 32 ? 0xfffffffful : (1ul << files) - 1;
    for(long k=0; k < kinds; k++) {
        printf("\n%s\n", KINDS[k]);
        for(long e=0; e < entries; e++) {
            if (ENTRIES[e].kind != k) continue;
            printf("%s", ENTRIES[e].text);
            if (ENTRIES[e].in != all) {
                printf("   (only");
                for(int f=0; f < files; f++) if (ENTRIES[e].in >> f & 1) printf(" %s", argv[1 + 2 * f]);
                printf(")");
            }
            printf("\n");
        }
    }
    if (!entries) printf("\nNo name is defined again.\n");
    printf("\n");
    return(0);
}
