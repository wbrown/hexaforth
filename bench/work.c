//
// work.c - the work of a profile by place, as a file, for the tools that
// have this in them: profile.c writes it, and what says what the work
// costs reads it.
//
// The file is, for each of the first WORKED places of code, how often the
// instruction there ran in the work that was profiled; and then, for each
// again, how often it runs in a turn of the event loop with nothing
// happening.  Each is a number of 64 bits with a fraction, since the work
// has what an idle turn runs taken out of it.
//

#include <stdio.h>

#define WORKED 0x10000                  // the places the work is kept of

// Write the work and a turn's to a file.  False if it could not be.
static int work_written(const char* path, const double* work, const double* a_turn) {
    FILE* out = fopen(path, "wb");
    if (out == NULL) {
        perror(path);
        return(0);
    }
    int is = fwrite(work, sizeof(double), WORKED, out) == WORKED && fwrite(a_turn, sizeof(double), WORKED, out) == WORKED;
    return(fclose(out) == 0 && is);
}

// Read them, the second if somewhere is given for it.  False, and it says
// why, if the file is not there or is not all there.
static int work_read(const char* path, double* work, double* a_turn) {
    FILE* in = fopen(path, "rb");
    int is = in && fread(work, sizeof(double), WORKED, in) == WORKED &&
             (!a_turn || fread(a_turn, sizeof(double), WORKED, in) == WORKED);
    if (!in) perror(path);
    else if (!is) fprintf(stderr, "%s: it is not the work of %d places\n", path, WORKED);
    if (in) fclose(in);
    return(is);
}
