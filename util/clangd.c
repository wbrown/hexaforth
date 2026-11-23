//
// clangd_said LOG...
//
// Reports what clangd finds wrong with the C, from the output of checking
// one file at a time (clangd --check=FILE: the target `clangd`, defined in
// cmake/clangd.cmake).  A language server reports its findings for a file
// every time the file changes.  Findings in files that were not changed
// would be repeated after every edit, so the tree is kept free of them.
//
// Each LOG is clangd's output for one file: the file it read, the compile
// command it used, a line starting with E for each problem, and a line
// when it finished.  This prints each problem line after the file's name,
// and then the number of files.  It fails if:
//
//   clangd reported a problem in any file
//   a file's compile command did not come from what CMake wrote
//     (compile_commands.json), because clangd then checked something other
//     than what is compiled
//   clangd did not finish
//   there were no files
//
// A header is not compiled on its own, so CMake has no compile command for
// it.  clangd checks it alone, using the command of a nearby file, which
// is also how it treats a header in an editing session.  So a header must
// include everything it needs.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "room.h"

// The messages clangd prints about its own progress.  Each follows a
// timestamp.
#define READS    "Testing on source file "
#define COMPILED "Compile command from CDB is: "
#define AS_NEAR  "Compile command inferred from "
#define FINISHED "All checks completed"

// Returns 1 if a line starts a clangd message: a letter for its kind, then
// the time, as in "E[11:16:13.342] ".
static int begins(const char* line) {
    return(line[0] && line[1] == '[');
}

static int says(const char* text, const char* what) {
    return(!strncmp(text, what, strlen(what)));
}

// Prints the problems clangd reported for one file, each after the file's
// name, and returns how many there were.  A file under the directory
// `here` is named relative to it.
static long said_of(const char* log, const char* here) {
    FILE* in = fopen(log, "r");
    if (in == NULL) {
        perror(log);
        exit(EXIT_FAILURE);
    }
    char* line = NULL;
    long room = 0;
    char* file = NULL;
    text_more(&file, "%s", log);                // the log's name stands in until clangd names the file it read
    int compiled = 0, finished = 0, wrong = 0;
    long said = 0;
    while (line_of(in, &line, &room)) {
        line[strcspn(line, "\n")] = '\0';
        if (!begins(line)) {                    // a continuation of the previous message
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
