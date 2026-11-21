//
// synth_said NEXTPNR.log [WHAT]
//
// What placing and routing the system came to, from what nextpnr said of
// it: the LUTs, the flip-flops, and how fast its clock can go.
//
// nextpnr says how many of each kind of cell the design has, each with how
// many the FPGA has after a /, and the most its clock can be: the last
// time it says each is the one that is so, since it says the clock again
// when it has routed.  It prints what it found on one line, after WHAT,
// which says what was synthesised; and fails if any of the three was not
// said: then nothing was placed.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../util/room.h"

// The number that follows some text, in the last line of a file that says
// a thing and has the text; or the number before the text, if that is
// where it is.  Less than nothing, if no line has them.
static double number_by(const char* path, const char* says, const char* text, int before) {
    FILE* in = fopen(path, "r");
    if (in == NULL) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    char* line = NULL;
    long room = 0;
    double is = -1;
    while (line_of(in, &line, &room)) {
        char* at = strstr(line, says) ? strstr(line, text) : NULL;
        if (!at) continue;
        if (before) {
            while (at > line && at[-1] == ' ') at--;
            while (at > line && at[-1] != ' ') at--;
        } else {
            at += strlen(text);
        }
        is = strtod(at, NULL);
    }
    fclose(in);
    free(line);
    return(is);
}

int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s NEXTPNR.log [WHAT]\n", argv[0]);
        return(1);
    }
    if (argc > 2) printf("%s ", argv[2]);
    double luts = number_by(argv[1], "Total LUT4s:", "Total LUT4s:", 0);
    double flops = number_by(argv[1], "TRELLIS_FF:", "TRELLIS_FF:", 0);
    double mhz = number_by(argv[1], "Max frequency for clock", " MHz", 1);
    if (luts < 0 || flops < 0 || mhz < 0) {
        printf("FAIL %s does not say how many LUTs, how many flip-flops and how fast\n", argv[1]);
        return(1);
    }
    printf("%.0f LUTs, %.0f flip-flops, %.2f MHz\n", luts, flops, mhz);
    return(0);
}
