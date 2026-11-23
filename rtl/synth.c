//
// synth_said NEXTPNR.log [WHAT]
//
// Reports the result of placing and routing the system, from nextpnr's
// log: the number of LUTs, the number of flip-flops, and the highest clock
// frequency.
//
// nextpnr prints the number of cells of each kind the design uses, each
// followed by a / and the number the FPGA has, and the highest frequency
// the clock can run at.  It prints the frequency again after routing, so
// the last line with each figure is the one that counts.  This program
// prints the three figures on one line, after WHAT, which names what was
// synthesised.  It fails if any of the three is missing from the log,
// which means nothing was placed.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../util/room.h"

// Returns a number from the last line of a file that contains both `says`
// and `text`.  The number is the one after `text`, or the one before
// `text` if `before` is not 0.  Returns -1 if no line contains both.
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
