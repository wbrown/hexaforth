//
// Created by Wes Brown on 1/1/21.
//

#include "compiler.h"
#include "vm_test.h"
#include <stdbool.h>
#include "../vm_debug.h"
#include "../util/stack.h"

bool decode_literal(const char* begin, const char* end, int64_t* num) {
    char *decode_end;
    int64_t literal_len = end - begin;
    char *literal_s = malloc(literal_len + 1);
    memcpy(literal_s, begin, literal_len);
    literal_s[literal_len] = '\0';
    *num = strtoll(literal_s, &decode_end, 10);
    if (*decode_end) {
        printf("ERROR: '%s' is not a literal!\n", literal_s);
        num = NULL;
        free(literal_s);
        return (false);
    }
    free(literal_s);
    return(true);
}

bool next_literal(char** buffer, int64_t* num) {
    char *begin = *buffer;
    char *end = *buffer;
    // Loop through the characters in our buffer.
    while (*end != '\0') {
        if (*end == ' ' && end == begin) {
            begin++;
            end++;
        } else if (*end == ' ') {
            bool res = decode_literal(begin, end, num);
            end++;
            *buffer = end;
            return(res);
        } else {
            end++;
        }
    }
    if (begin != end) {
        bool res = decode_literal(begin, end, num);
        *buffer = end;
        return(res);
    } else {
        num = NULL;
        return(false);
    }
}

bool generate_expected(const char* input, counted_array* expected_stack) {
    char* results = (char*)input;
    expected_stack->sz = 0;
    expected_stack->elems = malloc(0);

    if (input == NULL) {
        return(true);
    }

    int64_t* res_n = malloc(sizeof(uint64_t));
    int64_t* res = res_n;

    while (next_literal(&results, res)) {
        if (!res) break;
        expected_stack->sz++;
        expected_stack->elems =
                (int64_t*)realloc(expected_stack->elems,
                                  sizeof(int64_t)*expected_stack->sz);
        expected_stack->elems[expected_stack->sz-1] = *res;
    }

    free(res_n);
    if (!res) {
        printf("Error decoding input: %s\n", results);
        return(false);
    } else {
        return(true);
    }
}

bool init_image(context *ctx, hexaforth_test test) {
    if (test.init && strlen(test.init)) {
        counted_array* parsed = calloc(sizeof(counted_array), 1);
        bool results = generate_expected(test.init, parsed);
        if (results) {
            for (int i=0; i<parsed->sz; i++) {
                insert_int64(ctx, parsed->elems[i]);
            }
            ctx->EIP = ctx->HERE;
        }
        free(parsed->elems);
        free(parsed);
        return(results);
    } else {
        return(true);
    }
}

bool stack_match(context* ctx, bool rstack, counted_array* expected) {
    int P;
    const char *P_label;
    int64_t *stack;
    if (rstack) {
        P=ctx->RSP;
        P_label="RSTACK";
        stack=ctx->RSTACK;
    } else {
        P=ctx->SP;
        P_label="DSTACK";
        stack=ctx->DSTACK;
    }

    if (P != expected->sz) {
        printf("%s size (%d) does not match expected size! (%llu)\n",
               P_label, P, expected->sz);
        return(false);
    }
    for(int idx=0; idx<P; idx++) {
        if (stack[idx] != expected->elems[idx]) {
            printf("%s[%d]: %lld != EXPECTED[%d]: %lld\n", P_label, idx,
                   stack[idx], idx, expected->elems[idx]);
            return(false);
        }
    }
    return(true);
}

bool execute_test(context *in_ctx, hexaforth_test test) {
    bool dstack_results = false;
    counted_array* expected_dstack =  calloc(sizeof(counted_array), sizeof(int64_t));
    expected_dstack->elems = malloc(0);
    if (!generate_expected(test.dstack, expected_dstack)) {
        free(expected_dstack->elems);
        free(expected_dstack);
        return(false);
    }

    bool rstack_results = false;
    counted_array* expected_rstack =  calloc(sizeof(counted_array), sizeof(int64_t));
    expected_rstack->elems = malloc(0);
    if (!generate_expected(test.rstack, expected_rstack)) {
        free(expected_dstack->elems);
        free(expected_dstack);
        return(false);
    }

    bool eip_match = false;
    char* eip_s;
    counted_array* expected_eip = calloc(sizeof(counted_array), sizeof(int64_t));
    expected_eip->elems = malloc(0);
    if (!generate_expected(test.eip_expected, expected_eip)) {
        free(expected_eip->elems);
        free(expected_eip);
        return(false);
    }
    if (expected_eip->sz > 1) {
        printf("expected_eip should not have more than 1 integer: '%s'!\n",
               test.eip_expected);
        free(expected_eip->elems);
        free(expected_eip);
        return(false);
    } else if (expected_eip->sz == 1) {
        asprintf(&eip_s, "%lld", expected_eip->elems[0]);
    }

    bool io_match;

    // We use calloc rather than malloc, as malloc will often return memory of a
    // previously free'd but non-zero'ed context.
    context *ctx = calloc(1, sizeof(context));
    if (in_ctx) {
        ctx->words = in_ctx->words;
    } else {
        init_opcodes(ctx->words);
    }

    // Initialize our image with memory values if requried.
    if (!init_image(ctx, test)) {
        free(expected_dstack->elems);
        free(expected_dstack);
        free(expected_rstack->elems);
        free(expected_rstack);
        free(expected_eip->elems);
        free(expected_eip);
        return(false);
    }

    // Allocate our input and output buffer.
    const char* input = test.io_input ? test.io_input : "";
    ctx->IN = fmemopen((void *)input, strlen(input), "r");
    char* output = calloc(4096, 1);
    ctx->OUT = fmemopen(output, 4096, "w");

    // Compile our input program.
    if (compile(ctx, (char*)test.input)) {
        vm(ctx);
        // Write out a null byte to our output to terminate a string.
        fputc('\0', ctx->OUT);
        fclose(ctx->OUT);
        fclose(ctx->IN);
        printf("TEST: %-28s INPUT=%-42s%s%s%s%s%s%s EXPECTED={stack: [%s] rstack: [%s]%s%s%s%s%s}} => ",
               test.label,
               test.input,
               test.init ? " MEMORY=[" : "",
               test.init ? test.init : "",
               test.init ? "]" : "",
               test.io_input ? " IO_INPUT=\"" : "",
               test.io_input ? test.io_input : "",
               test.io_input ? "\"" : "",
               test.dstack ? test.dstack : "",
               test.rstack ? test.rstack : "",
               test.eip_expected ? " eip: " : "",
               test.eip_expected ? eip_s : "",
               test.io_expected ? " output: \"" : "",
               test.io_expected ? test.io_expected : "",
               test.io_expected ? "\"" : "");
        if (test.io_input && (strcmp(test.io_input, output) != 0)) {
            io_match = false;
        } else {
            io_match = true;
        }
        if (expected_eip->sz) {
            eip_match =  (expected_eip->elems[0] == ctx->EIP);
        } else {
            eip_match = true;
        }
        dstack_results = stack_match(ctx, false, expected_dstack);
        rstack_results = stack_match(ctx, true, expected_rstack);
        if (dstack_results && rstack_results && io_match && eip_match) {
            printf("PASSED\n");
        } else {
            printf("FAILED: ");
        }
        if (!io_match) {
            printf("\"%s\" != \"%s\" ", test.io_input, output);
        }
        if (!eip_match) {
            printf(" eip: %d != expected_eip: %lld", ctx->EIP,
                   expected_eip->elems[0]);
        }
        if (!dstack_results) {
            print_stack(ctx->SP, ctx->DSTACK[ctx->SP-1], ctx, false);
        }
        if (!rstack_results) {
            print_stack(ctx->RSP, ctx->RSTACK[ctx->RSP-1], ctx, true);
        }
    } else {
        printf("TEST: Failed to compile: \"%s\"\n", test.input);
    };
    free(output);
    free(ctx);
    free(expected_dstack->elems);
    free(expected_dstack);
    free(expected_rstack->elems);
    free(expected_rstack);
    if(expected_eip->sz) {
        free(eip_s);
    }
    free(expected_eip->elems);
    free(expected_eip);

    return(dstack_results && rstack_results && io_match && eip_match );
}

bool execute_tests(context* ctx, hexaforth_test* tests) {
    int64_t idx = 0;
    while (strlen((tests[idx]).input)) {
        if (!execute_test(ctx, tests[idx])) {
            return false;
        } else {
            idx++;
        };
    }
    return(true);
}
