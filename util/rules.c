//
// rules.c - the rules of the nucleus's peephole as forth/joins.fs writes
// them, for the tools that have this in them.  It is had after room.c.
//
// joins.fs says at its top how a rule is laid out.  A rule is a line that
// starts with an instruction: the instructions it is, and after a
// backslash what it is called.  One whose first instruction is nothing
// joins two instructions, and any other joins three.
//

#ifndef HEXAFORTH_RULES_C
#define HEXAFORTH_RULES_C

#include "room.c"

typedef struct {
    char* said;                         // what it is called
    int instructions;                   // how many it is written as
    int of_three;                       // it joins three instructions, and not two
} said_rule;

// The rules a file has, in its order: how many, and the list, which is
// made here.  It stops if there is no such file.
static long rules_of(const char* joins, said_rule** list) {
    FILE* in = fopen(joins, "r");
    if (!in) {
        perror(joins);
        exit(EXIT_FAILURE);
    }
    said_rule* out = NULL;
    char* line = NULL;
    long n = 0, room = 0, line_room = 0;
    while (line_of(in, &line, &line_room)) {
        if (line[0] != '$') continue;
        const char* said = strchr(line, '\\');
        ONE_MORE(out, n, room);
        out[n].said = room_for(strlen(line) + 1, 1);
        if (said) strcpy(out[n].said, said + 1 + (said[1] == ' '));
        out[n].said[strcspn(out[n].said, "\n")] = '\0';
        for(const char* c = line; *c && c != said; c++) out[n].instructions += *c == '$';
        out[n].of_three = strtol(line + 1, NULL, 16) != 0;
        n++;
    }
    fclose(in);
    free(line);
    ONE_MORE(out, n, room);             // so that there is a list, if there are none
    *list = out;
    return(n);
}

#endif // HEXAFORTH_RULES_C
