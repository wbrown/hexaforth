//
// session_test VM.out CPU.out SOC.out [CPU.err]
//
// Compares what the Verilog printed when a session was typed at it with
// what vm.c printed.  vm.c is the reference.
//
// One file (rtl/session.fs: arithmetic, definitions, recursion, loops, a
// task and its pauses) is typed at the nucleus on three machines: the
// reference VM, the Verilog CPU with a plain memory (rtl/tb.cpp), and the
// whole system, which is the CPU with its own memory (rtl/tb_soc.cpp).
// The build saves the output of each.  This program compares the three
// outputs line by line, ignoring line endings and skipping empty lines.
//
// The VM adds three lines of its own to the machine's output: two about
// the image it loaded, at the start, and one with the number of
// instructions it executed, at the end.  Those lines do not come from the
// machine, and are skipped.
//
// It prints how many lines it compared and what they were.  It fails if
// there were none, because then it compared nothing.  With CPU.err, which
// is the test bench's own report of the run, it ends by printing the last
// line of that file: the number of cycles the session took.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../util/room.h"

#define OWN_FIRST   2                           // the number of lines the VM writes before the machine's output
static const char OWN_LAST[] = " instructions executed]";       // the end of the line the VM writes after it

typedef struct {
    char** line;
    long n, room;
} said;

static int nothing_in(const char* line) { return(!line[0] || line[0] == '\n'); }

// Reads a file's lines, skipping empty lines and, for the VM's output, the
// VM's own lines.
static said lines_of(const char* path, int of_the_vm) {
    FILE* in = fopen(path, "r");
    if (in == NULL) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    said all = { NULL, 0, 0 };
    char* line = NULL;
    long room = 0, read = 0;
    while (line_of(in, &line, &room)) {
        read++;
        if (of_the_vm && (read <= OWN_FIRST || strstr(line, OWN_LAST))) continue;
        if (nothing_in(line)) continue;
        line[strcspn(line, "\n")] = '\0';               // remove the newline, which the last line of a file may lack
        ONE_MORE(all.line, all.n, all.room);
        all.line[all.n] = room_for(strlen(line) + 1, 1);
        strcpy(all.line[all.n++], line);
    }
    fclose(in);
    free(line);
    return(all);
}

// Returns 1 if two outputs are the same.  Prints the first line where
// they differ.
static int same(const said* a, const char* a_is, const said* b, const char* b_is) {
    for(long n=0; n < a->n || n < b->n; n++) {
        const char* x = n < a->n ? a->line[n] : "(nothing more)";
        const char* y = n < b->n ? b->line[n] : "(nothing more)";
        if (!strcmp(x, y)) continue;
        printf("FAIL line %ld: %s sent\n    %s\n  and %s sent\n    %s\n", n + 1, a_is, x, b_is, y);
        return(0);
    }
    return(1);
}

int main(int argc, char *argv[]) {
    if (argc < 4 || argc > 5) {
        fprintf(stderr, "Usage: %s VM.out CPU.out SOC.out [CPU.err]\n", argv[0]);
        return(1);
    }
    said vm = lines_of(argv[1], 1), cpu = lines_of(argv[2], 0), soc = lines_of(argv[3], 0);
    int failed = !same(&vm, "vm.c", &cpu, "the CPU");
    failed += !same(&cpu, "the CPU", &soc, "the whole system");
    if (!vm.n) {
        printf("FAIL vm.c sent nothing: nothing was compared\n");
        failed++;
    }
    if (!failed) {
        printf("nucleus session: CPU and whole system match vm.c, %ld lines\n", vm.n);
        for(long n=0; n < cpu.n; n++) puts(cpu.line[n]);
    }
    if (argc > 4) {
        said run = lines_of(argv[4], 0);
        if (run.n) puts(run.line[run.n - 1]);
    }
    return(failed != 0);
}
