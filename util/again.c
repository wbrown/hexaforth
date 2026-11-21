//
// again NAME FILE.shadows [NAME FILE.shadows ...]
//
// The names that a source defines again, of several images of it, each
// said once.
//
// cross.fs writes beside every image the names it found defined when there
// was one of that name already (cross.fs, names defined again): two lines
// that say what the file is, and then for each kind of them a line that
// says the kind and a line a name, which starts with spaces.  The machines
// of util/machines are built of one source, and what one has another mostly
// has.  This prints the first file's two lines, and each kind once with
// every name that any image has under it.  A name that not every image has
// is followed by the images that have it: the NAME given with each file.
//
// It is what is shown when the tests are run (the targets quick and check).
// A name defined again is not an error, and Forth takes the newest; one
// that was not meant is to be read here, and not found by debugging.
//
// It fails if a file is not there, since then an image was not built.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "room.h"

typedef struct {
    char* text;                 // a name's line
    long kind;                  // the kind it is under,
    unsigned long in;           // and the files that have it, a bit a file
} entry;

static char** KINDS;            // the line that says each kind, in the order they are first read
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
            if (n < 2) {                                // what the file is
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
