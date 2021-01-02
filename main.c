#include <stdlib.h>
#include "vm.h"
#include "compiler.h"

void generate_test_program(context *ctx) {
    compile(ctx, "1 2 3 4 + 5 dup 6 drop drop 7 over nip 1+");
}

int main() {
    context *ctx = malloc(sizeof(context));
    ctx->HERE = 0;
    generate_test_program(ctx);
    vm(ctx);
}
