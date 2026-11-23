//
// work.h - the file format for a profile's work by address, for the tools
// that include this file.  profile.c writes the file, and the tools that
// report what the work costs read it.
//
// The file has two arrays.  The first has, for each code address that was
// counted, the number of times the instruction there ran in the profiled
// work.  The second has, for each of the same addresses, the number of
// times it runs in one turn of the event loop when nothing is happening.
// Each number is a double, because the work has the idle turns subtracted
// from it and so can have a fraction.  The writer decides how many
// addresses there are (profile.c writes as many as the counting VM has
// counts for), and a reader gets that number from the file's length.
//

#ifndef HEXAFORTH_WORK_H
#define HEXAFORTH_WORK_H

#include <stdio.h>
#include <stdlib.h>

// Writes the work and the idle turn for `places` addresses to a file.
// Returns 0 if the file could not be written.
static int work_written(const char* path, const double* work, const double* a_turn, long places) {
    FILE* out = fopen(path, "wb");
    if (out == NULL) {
        perror(path);
        return(0);
    }
    int is = fwrite(work, sizeof(double), places, out) == (size_t)places &&
             fwrite(a_turn, sizeof(double), places, out) == (size_t)places;
    return(fclose(out) == 0 && is);
}

// Reads a work file.  Returns the number of addresses it covers, or 0
// after printing the reason if it cannot be read.  `*work` receives an
// allocated array of that length.  If `a_turn` is not NULL, `*a_turn`
// receives the idle turn's array, which is in the same allocation.
static long work_read(const char* path, double** work, double** a_turn) {
    FILE* in = fopen(path, "rb");
    if (!in) {
        perror(path);
        return(0);
    }
    fseek(in, 0, SEEK_END);
    long bytes = ftell(in), places = bytes / (long)(2 * sizeof(double));
    rewind(in);
    double* both = places ? malloc(bytes) : NULL;
    if (bytes <= 0 || bytes % (long)(2 * sizeof(double)) || !both || fread(both, 1, bytes, in) != (size_t)bytes) {
        fprintf(stderr, "%s: it is not the work of a number of places, and what a turn runs of each\n", path);
        fclose(in);
        free(both);
        return(0);
    }
    fclose(in);
    *work = both;                           // the work is the first half of the buffer, and the idle turn is the second half
    if (a_turn) *a_turn = both + places;
    return(places);
}

#endif // HEXAFORTH_WORK_H
