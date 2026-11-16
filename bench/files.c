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

typedef struct { long from, to; char file[64]; } span;

// The files that compiled code, each with where its code is: how many, and
// the list, which is made here and is as long as that.  It stops if there
// is no such file of sizes.
static int files_of(const char* sizes, span** list) {
    span* out = NULL;
    int n = 0;
    for(int filling=0; filling < 2; filling++) {        // they are counted, and then listed
        FILE* in = fopen(sizes, "r");
        if (in == NULL) {
            perror(sizes);
            exit(EXIT_FAILURE);
        }
        if (filling) out = calloc(n ? n : 1, sizeof(span));
        if (filling && !out) { fprintf(stderr, "there is no room for %d files\n", n); exit(EXIT_FAILURE); }
        n = 0;
        char row[256];
        while (fgets(row, sizeof(row), in)) {
            span is;
            long bytes = 0, a, b, c, from = 0;
            if (sscanf(row, "%63s %ld %ld %ld %ld %ld", is.file, &bytes, &a, &b, &c, &from) != 6) continue;
            if (is.file[0] == '(' || !bytes) continue;
            is.from = from, is.to = from + bytes / (long)sizeof(uint16_t);
            if (filling) out[n] = is;
            n++;
        }
        fclose(in);
    }
    *list = out;
    return(n);
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
