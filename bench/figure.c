//
// figure.c - a line of what the benchmarks print, for the tools that have
// this in them: what was done, on which machine, and what it came to.
//
// A figure is printed by what checked the work it is of, and is not
// printed for work that was not checked.
//

#ifndef HEXAFORTH_FIGURE_C
#define HEXAFORTH_FIGURE_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../util/room.c"

// A line: the work, and a piece of it if it is one of several; the
// machine; and the figure.
static void figure(const char* what, const char* piece, const char* on, const char* is) {
    char* work = NULL;
    text_more(&work, piece && piece[0] ? "%s %s" : "%s", what, piece);
    printf("%-22s %-17s %s", work, on, is);
    if (!is[0] || is[strlen(is) - 1] != '\n') printf("\n");
    free(work);
}

// What the Verilog's test bench said passed between a mark the machine
// made and the next (rtl/tb.cpp): the cycles, and the rest.  The marks
// are counted from one.  The text is made here; nothing, if the test
// bench said nothing of that mark.
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

#endif //HEXAFORTH_FIGURE_C
