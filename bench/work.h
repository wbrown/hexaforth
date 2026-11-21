//
// work.h - the work of a profile by place, as a file, for the tools that
// have this in them: profile.c writes it, and what says what the work
// costs reads it.
//
// The file is, for each place of code that was counted, how often the
// instruction there ran in the work that was profiled; and then, for each
// again, how often it runs in a turn of the event loop with nothing
// happening.  Each is a number of 64 bits with a fraction, since the work
// has what an idle turn runs taken out of it.  How many places there are
// is whoever writes it's to say (profile.c: as many as the VM that counts
// keeps counts of), and is read from how long the file is.
//

#ifndef HEXAFORTH_WORK_H
#define HEXAFORTH_WORK_H

#include <stdio.h>
#include <stdlib.h>

// Write the work and a turn's, of so many places, to a file.  False if it
// could not be.
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

// Read them: how many places they are of, or nothing, and then it says
// why.  The work is made here, as long as that, and so is a turn's if
// somewhere is given for it.
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
    *work = both;                           // the first half; and a turn's is the second
    if (a_turn) *a_turn = both + places;
    return(places);
}

#endif // HEXAFORTH_WORK_H
