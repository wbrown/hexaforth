//
// session_test VM.out CPU.out SOC.out [CPU.err]
//
// What the Verilog sent when a session was typed at it, against what vm.c
// sent, which is the machine.
//
// One file (rtl/session.fs: arithmetic, definitions, recursion, loops, a
// task and its pauses) is typed at the nucleus on the reference VM, on the
// Verilog CPU with a plain memory (rtl/tb.cpp) and on the whole system,
// the CPU with its own memory (rtl/tb_soc.cpp), and what each sent is kept
// by the build.  Here the three are compared, a line at a time, without
// the ends of the lines; a line with nothing in it is left out.
//
// The VM writes three lines of its own with what the machine sent: two of
// the image it loaded, before anything, and one of how many instructions
// it executed, after.  Those are not the machine's, and are left out.
//
// It says how many lines it compared, and what they were, and fails if
// that is none: then it compared nothing.  With CPU.err, what the test
// bench said of the run, it ends with the last line of that, which is the
// cycles the session took.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../util/room.c"

#define OWN_FIRST   2                           // the lines the VM writes before the machine sends anything,
static const char OWN_LAST[] = " instructions executed]";       // and what the one it writes after ends with

typedef struct {
    char** line;
    long n, room;
} said;

static int nothing_in(const char* line) { return(!line[0] || line[0] == '\n'); }

// What a file has, a line at a time, without the lines that have nothing
// in them: and, of the VM's, without its own.
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
        line[strcspn(line, "\n")] = '\0';               // the last line of a file may not have its end
        ONE_MORE(all.line, all.n, all.room);
        all.line[all.n] = room_for(strlen(line) + 1, 1);
        strcpy(all.line[all.n++], line);
    }
    fclose(in);
    free(line);
    return(all);
}

// Are two the same?  Where they first are not is said.
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
