#include <stdlib.h>
#include "vm.h"
#include "compiler.h"

void generate_test_program(context *ctx) {
    insert_literal(ctx, -20);
    insert_literal(ctx, 1024);
    insert_literal(ctx, 2048);
    insert_literal(ctx, 4096);
    insert_literal(ctx, 8192);
    insert_literal(ctx, 16384);
    insert_literal(ctx, 16385);
    insert_literal(ctx, 24923);
    insert_literal(ctx, -24923);
    insert_literal(ctx, 32768);
    insert_literal(ctx, 65533);
    compile_word(ctx, "+");
    insert_literal(ctx, 20);
    insert_literal(ctx, 4293918720);
    insert_literal(ctx, -4293918720);
    compile_word(ctx, "swap");
}

int main() {
    context *ctx = malloc(sizeof(context));
    ctx->HERE = 0;
    generate_test_program(ctx);
    vm(ctx);
}
