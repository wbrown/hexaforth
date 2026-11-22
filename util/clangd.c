//
// clangd_said LOG...
//
// What clangd says of the C, from what it printed when it was asked of a
// file at a time (clangd --check=FILE: the target `clangd`, which
// cmake/clangd.cmake says).  A language server says what it finds wrong with a
// file each time the file is changed.  What it says of the tree as it is
// would be said again after every change, of what was not changed: so
// there is to be none of it.
//
// Each LOG is what clangd printed of one file: the file it read, how it
// took the file to be compiled, a line that begins with E for each thing
// it finds wrong, and that it finished.  This prints each of those lines
// after the file's name, and then how many files there were.  It fails if
// anything was said of a file; if how a file is compiled was not had from
// what CMake wrote (compile_commands.json), since then clangd read some
// other file than the one that is compiled; if clangd did not finish; and
// if there were no files.
//
// A header is not compiled by itself, so CMake does not say how it is:
// clangd takes it to be compiled as a file near it is, and reads it by
// itself, which is how it reads one in a session.  So a header is to
// include what it needs.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "room.h"

// What clangd says of itself as it goes, after the time it says it at
#define READS    "Testing on source file "
#define COMPILED "Compile command from CDB is: "
#define AS_NEAR  "Compile command inferred from "
#define FINISHED "All checks completed"

// A line clangd begins is a letter for its kind and the time: "E[11:16:13.342] "
static int begins(const char* line) {
    return(line[0] && line[1] == '[');
}

static int says(const char* text, const char* what) {
    return(!strncmp(text, what, strlen(what)));
}

// What clangd said of one file, each line of it printed after the file's
// name: how many things.  A file under `here` is named from there.
static long said_of(const char* log, const char* here) {
    FILE* in = fopen(log, "r");
    if (in == NULL) {
        perror(log);
        exit(EXIT_FAILURE);
    }
    char* line = NULL;
    long room = 0;
    char* file = NULL;
    text_more(&file, "%s", log);                // until it says what it read
    int compiled = 0, finished = 0, wrong = 0;
    long said = 0;
    while (line_of(in, &line, &room)) {
        line[strcspn(line, "\n")] = '\0';
        if (!begins(line)) {                    // more of the line before
            if (wrong) printf("%s:     %s\n", file, line);
            continue;
        }
        const char* text = strstr(line, "] ");
        text = text ? text + 2 : line;
        wrong = 0;
        if (says(text, READS)) {
            const char* path = text + strlen(READS);
            size_t under = strlen(here);
            if (!strncmp(path, here, under) && path[under] == '/') path += under + 1;
            file[0] = '\0';
            text_more(&file, "%s", path);
        } else if (says(text, COMPILED)) {
            compiled = 1;
        } else if (says(text, AS_NEAR)) {
            size_t len = strlen(file);
            compiled = len > 2 && !strcmp(file + len - 2, ".h");
        } else if (says(text, FINISHED)) {
            finished = 1;
        } else if (line[0] == 'E') {
            printf("%s: %s\n", file, text);
            wrong = 1;
            said++;
        }
    }
    fclose(in);
    if (!compiled) {
        printf("%s: clangd did not have how it is compiled from compile_commands.json (%s)\n", file, log);
        said++;
    }
    if (!finished) {
        printf("%s: clangd did not finish (%s)\n", file, log);
        said++;
    }
    free(line);
    free(file);
    return(said);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s LOG...\n", argv[0]);
        printf("FAIL clangd was asked of no files\n");
        return(1);
    }
    char* here = getcwd(NULL, 0);
    if (!here) {
        perror("getcwd");
        return(1);
    }
    long files = 0, of = 0, said = 0;
    for(int idx=1; idx<argc; idx++) {
        long these = said_of(argv[idx], here);
        files++;
        if (these) of++;
        said += these;
    }
    free(here);
    if (said) {
        printf("FAIL clangd: %ld files of C, and it says %ld things of %ld of them\n", files, said, of);
        return(1);
    }
    printf("clangd: %ld files of C, and it says nothing of any\n", files);
    return(0);
}
