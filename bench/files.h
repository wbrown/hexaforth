//
// files.h - the source files an image was compiled from, for the tools
// that include this file.
//
// cross.fs writes a .sizes file beside an image.  It has one line for each
// source file, and a last line for the whole image.  The fields of a line
// that are used here are:
//
//   1  the file's name
//   2  the bytes of code the file compiled
//   3  the bytes of data it compiled
//   4  how many of those data bytes are dictionary headers
//   6  the instruction address where its code starts
//   7  its include depth, which is 1 for a file the program includes
//      directly
//
// A file's code is one run on one page.  The run includes the code of the
// files it includes that are on the same page, and its data figure
// includes their data.  A file that only lists other files, and says which
// page each goes on, has no code of its own.
//
// A file can have some of its words placed in the kernel's page, where
// every page reaches them with one instruction (nearby, cross.fs).  Each
// stretch of such words has a line of its own, before the file's line: the
// file's name, the bytes of that code, its starting address, and a depth
// of 0, which is what identifies such a line.  An address in such a
// stretch belongs to the file, as an address in the file's main run does
// (file_holding()).  This is the only C code that reads the .sizes file.
//

#ifndef HEXAFORTH_FILES_H
#define HEXAFORTH_FILES_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../util/room.h"

typedef struct {
    long from, to;              // its code: the first instruction address, and the address after the last
    char* file;                 // its name, allocated
    long data, headers;         // the bytes of its data, and how many of them are headers, as cross.fs counted them
    long deep;                  // its include depth: 1 if the program includes it directly
} span;

// Returns 1 if a line is the one for the whole image, not a file.
static int is_all_of_it(const span* line) { return(line->file[0] == '('); }

// Returns 1 if a line is a stretch of a file's code placed in the kernel's
// page, not the file's own line.
static int is_laid_near(const span* line) { return(!line->deep && !is_all_of_it(line)); }

// Reads the lines of a .sizes file into an allocated list at `*list`, and
// returns how many there are.  With `every` set it reads every line.
// Otherwise it reads only the files that compiled code.  The program exits
// if the .sizes file cannot be opened.
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
        if (!every && (name[0] == '(' || !bytes)) continue;     // skip the line for the whole image, and any file that compiled no code
        ONE_MORE(out, n, room);
        out[n].file = room_for(line + named - name + 1, 1);
        memcpy(out[n].file, name, line + named - name);
        out[n].from = from, out[n].to = from + bytes / (long)sizeof(uint16_t);
        out[n].data = data, out[n].headers = headers, out[n].deep = deep;
        n++;
    }
    fclose(in);
    free(line);
    ONE_MORE(out, n, room);                             // make sure the list is allocated even if it is empty
    *list = out;
    return((int)n);
}

// Reads the files that compiled code, each with the addresses of its code.
static int files_of(const char* sizes, span** list) { return(sizes_lines(sizes, list, 0)); }

// Reads every line of the .sizes file, including the files that compiled
// no code and the line for the whole image, which is_all_of_it()
// identifies.
static int every_file_of(const char* sizes, span** list) { return(sizes_lines(sizes, list, 1)); }

// Returns the index of the file an address's code belongs to: the file
// with the smallest run that contains the address.  For an address in a
// stretch placed in the kernel's page, it returns the index of the file's
// own line.  Returns -1 if no file in the list contains the address.
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

// Returns the name of the file an address's code belongs to, or NULL if no
// file in the list contains it.
static const char* file_at(const span* spans, int n, long at) {
    int is = file_holding(spans, n, at);
    return(is < 0 ? NULL : spans[is].file);
}

#endif // HEXAFORTH_FILES_H
