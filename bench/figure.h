//
// figure.h - prints one line of the benchmarks' output: what was done, on
// which machine, and the result.  The benchmark tools include this file.
//
// A figure is printed only by the code that checked the work it measures.
// No figure is printed for work that was not checked.
//

#ifndef HEXAFORTH_FIGURE_H
#define HEXAFORTH_FIGURE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../util/room.h"

// Prints one line: the work, with the name of the piece if the work has
// several; the machine; and the figure.
static void figure(const char* what, const char* piece, const char* on, const char* is) {
    char* work = NULL;
    text_more(&work, piece && piece[0] ? "%s %s" : "%s", what, piece);
    printf("%-22s %-17s %s", work, on, is);
    if (!is[0] || is[strlen(is) - 1] != '\n') printf("\n");
    free(work);
}

// Returns what the Verilog test bench reported between one mark the
// machine made and the next (rtl/tb.cpp): the cycles, and the rest of the
// line.  Marks are numbered from one.  The text is allocated.  Returns
// NULL if the test bench reported nothing for that mark.
static char* between(const char* timed, int mark) {
    FILE* in = fopen(timed, "r");
    if (in == NULL) {
        perror(timed);
        exit(EXIT_FAILURE);
    }
    char* begins = NULL;
    text_more(&begins, "mark %d to %d: ", mark, mark + 1);
    char* line = NULL;
    char* is = NULL;
    long room = 0;
    while (!is && line_of(in, &line, &room)) {
        if (!strncmp(line, begins, strlen(begins))) text_more(&is, "%s", line + strlen(begins));
    }
    fclose(in);
    free(line);
    free(begins);
    return(is);
}

#endif //HEXAFORTH_FIGURE_H
