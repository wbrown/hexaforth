//
// files.c - the source files an image's code is of, for the tools that
// have this in them.
//
// cross.fs writes beside an image its .sizes: a line a file, with the
// bytes of code the file compiled second and the instruction its code
// starts at sixth.  A file that includes others has their code in its own.
//

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../util/room.c"

typedef struct { long from, to; char* file; } span;     // where its code is, and its name, which is as long as it is

// The files that compiled code, each with where its code is: how many, and
// the list, which is made here and has room for them all.  It stops if
// there is no such file of sizes.
static int files_of(const char* sizes, span** list) {
    FILE* in = fopen(sizes, "r");
    if (in == NULL) {
        perror(sizes);
        exit(EXIT_FAILURE);
    }
    span* out = NULL;
    char* line = NULL;
    long n = 0, room = 0, line_room = 0;
    while (line_of(in, &line, &line_room)) {
        long bytes = 0, a, b, c, from = 0;
        int named = 0;
        if (sscanf(line, "%*s%n %ld %ld %ld %ld %ld", &named, &bytes, &a, &b, &c, &from) != 5) continue;
        const char* name = line + strspn(line, " \t");
        if (name[0] == '(' || !bytes) continue;         // the line for all of it, and a file that compiled no code
        ONE_MORE(out, n, room);
        out[n].file = room_for(line + named - name + 1, 1);
        memcpy(out[n].file, name, line + named - name);
        out[n].from = from, out[n].to = from + bytes / (long)sizeof(uint16_t);
        n++;
    }
    fclose(in);
    free(line);
    ONE_MORE(out, n, room);                             // so that there is a list, if there are none
    *list = out;
    return((int)n);
}

// The file a place's code is of: the smallest file's that has it.  None,
// if no file of the list has it.
static const char* file_at(const span* spans, int n, long at) {
    const char* is = NULL;
    long least = -1;
    for(int idx=0; idx < n; idx++) {
        long size = spans[idx].to - spans[idx].from;
        if (at >= spans[idx].from && at < spans[idx].to && (least < 0 || size < least)) least = size, is = spans[idx].file;
    }
    return(is);
}
