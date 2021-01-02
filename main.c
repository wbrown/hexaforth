#include <stdlib.h>
#include "vm.h"
#include "vm_test.h"
#include "compiler.h"

void generate_test_program(context *ctx) {
    compile(ctx, "1 2 3 4 + 5 dup 6 drop drop 7 over nip 1+");
}

static hexaforth_test TESTS[] = {
        {.input = "1 2 3 4",
         .results = "1 2 3 4"},
        {.input = "6 7 8 9 10",
         .results = "6 7 8 9 10"},
        {.input = "5 4 2 1 swap",
         .results = "5 4 1 2"},
        {.input = "5 4 3 2 over",
         .results = "5 4 3 2 3"},
        {.input = "5 4 3 2 nip",
         .results = "5 4 2"},
        {.input = "5 4 3 2 drop",
         .results = "5 4 3"},
        {.input = "1 2 3 1+",
         .results = "1 2 4"},
        {.input = "1 2 3 2+",
         .results = "1 2 5"},
        {.input = "2 1 lshift",
         .results = "4"},
        {.input = "2 1 rshift",
         .results = "1"},
        {.input = "4 1 rshift",
         .results = "2"},
        {.input = "-1 -2 -3 -4 -5",
         .results = "-1 -2 -3 -4 -5"},
        {.input = "1 2 -1024 invert",
         .results = "1 2 1024"},
        {.input = "500 300 -",
         .results = "200"}
};

int main() {
    execute_tests(TESTS);
    // context *ctx = malloc(sizeof(context));
    // ctx->HERE = 0;
    // generate_test_program(ctx);
    // vm(ctx);
}
