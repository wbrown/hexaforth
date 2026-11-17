//
// gates_said LABEL YOSYS.stat
//
// How big a CPU is, as gates, from what Yosys counted of it.
//
// The CPU was put through a flow that leaves it as NAND gates of two
// inputs, inverters and D flip-flops (CMakeLists.txt says the flow), and
// Yosys's `stat` says how many of each cell there are: a line a kind, with
// the number before its name or after it, as the Yosys is.  A NAND is one
// gate, an inverter half of one and a
// flip-flop four.  A memory that was left a memory, and a multiplier that
// was left out, are said and not counted.  It fails if there are no gates:
// then nothing was counted.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../util/room.c"

// The number a line has: the first of its words that is all digits.
static long count_in(const char* line) {
    for(const char* at = line + strspn(line, " \t"); *at; at += strspn(at, " \t")) {
        size_t word = strcspn(at, " \t\r\n");
        if (word && strspn(at, "0123456789") == word) return(strtol(at, NULL, 10));
        at += word ? word : 1;
    }
    return(0);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s LABEL YOSYS.stat\n", argv[0]);
        return(1);
    }
    FILE* in = fopen(argv[2], "r");
    if (in == NULL) {
        perror(argv[2]);
        return(1);
    }
    long nand = 0, not = 0, flops = 0, memories = 0, multipliers = 0, room = 0;
    char* line = NULL;
    while (line_of(in, &line, &room)) {
        long count = count_in(line);
        if (strstr(line, "$_NAND_")) nand = count;
        if (strstr(line, "$_NOT_")) not = count;
        if (strstr(line, "$_DFF_")) flops += count;     // of every kind of flip-flop
        if (strstr(line, "$mem_v2")) memories = count;
        if (strstr(line, "$mul")) multipliers = count;
    }
    fclose(in);
    printf("%-26s %6ld gates: NAND %6ld  NOT %6ld  flip-flops %5ld;  memories left out %ld",
           argv[1], (4 * nand + 2 * not + 16 * flops) / 4, nand, not, flops, memories);
    if (multipliers) printf(", multipliers left out %ld", multipliers);
    printf("\n");
    return(nand + not + flops ? 0 : 1);
}
