//
// files.h - the source files an image is made of, for the tools that have
// this in them.
//
// cross.fs writes beside an image its .sizes: a line a file, with the
// bytes of code the file compiled second, the bytes of data it laid down
// third, how many of those are dictionary headers fourth, the instruction
// its code starts at sixth, and how deep it is included seventh, 1 if the
// program includes it; and a last line for the whole image.  A file's
// code is one run of one page, with that of the files it includes that
// are on its page, and their data is in its own.  A list of files that
// says which page each goes on has no code: its files have.
//
// A file may have some of its words laid in the kernel's page, where every
// page reaches them with one instruction (nearby, cross.fs).  Each stretch
// of those is a line of its own, before the file's: the file's name, the
// bytes of that code, where it starts, and a depth of nothing, which is
// how such a line is told.  A place there is the file's, as a place of its
// run is (file_holding).  This is the one place in the C that reads that
// file.
//

#ifndef HEXAFORTH_FILES_H
#define HEXAFORTH_FILES_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../util/room.h"

typedef struct {
    long from, to;              // where its code is,
    char* file;                 // its name, which is as long as it is,
    long data, headers;         // the bytes of its data, and of that its headers, as cross.fs counted them,
    long deep;                  // and how deep it is included: 1 if the program includes it
} span;

// Is a line of the list the one for the whole image, and not a file?
static int is_all_of_it(const span* line) { return(line->file[0] == '('); }

// Is a line a stretch of a file's code that is laid in the kernel's page,
// and not the file's own line?
static int is_laid_near(const span* line) { return(!line->deep && !is_all_of_it(line)); }

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
        long bytes = 0, data = 0, headers = 0, labels, from = 0, deep = 0;
        int named = 0;
        if (sscanf(line, "%*s%n %ld %ld %ld %ld %ld %ld", &named, &bytes, &data, &headers, &labels, &from, &deep) != 6) continue;
        const char* name = line + strspn(line, " \t");
        if (!every && (name[0] == '(' || !bytes)) continue;     // the line for all of it, and a file that compiled no code
        ONE_MORE(out, n, room);
        out[n].file = room_for(line + named - name + 1, 1);
        memcpy(out[n].file, name, line + named - name);
        out[n].from = from, out[n].to = from + bytes / (long)sizeof(uint16_t);
        out[n].data = data, out[n].headers = headers, out[n].deep = deep;
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

// Which file a place's code is of: the smallest file's that has it, and
// for a place in a stretch laid in the kernel's page, the file's own line.
// Less than nothing, if no file of the list has it.
static int file_holding(const span* spans, int n, long at) {
    int is = -1;
    for(int idx=0; idx < n; idx++) {
        if (is_all_of_it(&spans[idx]) || at < spans[idx].from || at >= spans[idx].to) continue;
        if (is < 0 || spans[idx].to - spans[idx].from < spans[is].to - spans[is].from) is = idx;
    }
    if (is >= 0 && is_laid_near(&spans[is])) {
        for(int idx=0; idx < n; idx++) {
            if (!is_laid_near(&spans[idx]) && !strcmp(spans[idx].file, spans[is].file)) return(idx);
        }
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
