//
// files.h - the source files an image is made of, for the tools that have
// this in them.
//
// cross.fs writes beside an image its .sizes: a line a file, with the
// bytes of code the file compiled second, the bytes of data it laid down
// third, how many of those are dictionary headers fourth, and the
// instruction its code starts at sixth; and a last line for the whole
// image.  A file that includes others has their code in its own, and their
// figures in its own.  This is the one place in the C that reads that
// file.
//

#ifndef HEXAFORTH_FILES_H
#define HEXAFORTH_FILES_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../util/room.h"

typedef struct {
    long from, to;              // where its code is,
    char* file;                 // its name, which is as long as it is,
    long data, headers;         // and the bytes of its data, and of that its headers, as cross.fs counted them
} span;

// Is a line of the list the one for the whole image, and not a file?
static int is_all_of_it(const span* line) { return(line->file[0] == '('); }

// The lines of a file of sizes: how many, and the list, which is made here
// and has room for them all.  With `every` it is every line; without, the
// files that compiled code.  It stops if there is no such file of sizes.
static int sizes_lines(const char* sizes, span** list, int every) {
    FILE* in = fopen(sizes, "r");
    if (in == NULL) {
        perror(sizes);
        exit(EXIT_FAILURE);
    }
    span* out = NULL;
    char* line = NULL;
    long n = 0, room = 0, line_room = 0;
    while (line_of(in, &line, &line_room)) {
        long bytes = 0, data = 0, headers = 0, labels, from = 0;
        int named = 0;
        if (sscanf(line, "%*s%n %ld %ld %ld %ld %ld", &named, &bytes, &data, &headers, &labels, &from) != 5) continue;
        const char* name = line + strspn(line, " \t");
        if (!every && (name[0] == '(' || !bytes)) continue;     // the line for all of it, and a file that compiled no code
        ONE_MORE(out, n, room);
        out[n].file = room_for(line + named - name + 1, 1);
        memcpy(out[n].file, name, line + named - name);
        out[n].from = from, out[n].to = from + bytes / (long)sizeof(uint16_t);
        out[n].data = data, out[n].headers = headers;
        n++;
    }
    fclose(in);
    free(line);
    ONE_MORE(out, n, room);                             // so that there is a list, if there are none
    *list = out;
    return((int)n);
}

// The files that compiled code, each with where its code is.
static int files_of(const char* sizes, span** list) { return(sizes_lines(sizes, list, 0)); }

// Every line of the file of sizes: the files that compiled no code too,
// and the line for the whole image, which is_all_of_it tells from a file.
static int every_file_of(const char* sizes, span** list) { return(sizes_lines(sizes, list, 1)); }

// Which file a place's code is of: the smallest file's that has it.  Less
// than nothing, if no file of the list has it.
static int file_holding(const span* spans, int n, long at) {
    int is = -1;
    for(int idx=0; idx < n; idx++) {
        if (is_all_of_it(&spans[idx]) || at < spans[idx].from || at >= spans[idx].to) continue;
        if (is < 0 || spans[idx].to - spans[idx].from < spans[is].to - spans[is].from) is = idx;
    }
    return(is);
}

// The file a place's code is of, by its name.  None, if no file of the
// list has it.
static const char* file_at(const span* spans, int n, long at) {
    int is = file_holding(spans, n, at);
    return(is < 0 ? NULL : spans[is].file);
}

#endif // HEXAFORTH_FILES_H
