//
// Created by Wes Brown on 1/1/21.
//

#include "vm_test.h"
#include <stdbool.h>
#include "compiler.h"
#include "vm.h"
#include "vm_debug.h"

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

bool generate_expected(hexaforth_test test, expected_results* expected) {
    char* results = (char*)test.results;
    expected->sz = 0;
    int64_t* res_n = malloc(sizeof(uint64_t));
    int64_t* res = res_n;

    while (next_literal(&results, res)) {
        if (!res) break;
        expected->sz++;
        int64_t* resized = realloc(expected->results, sizeof(int64_t)*expected->sz);
        expected->results = resized;
        expected->results[expected->sz-1] = *res;
    }

    free(res_n);
    if (!res) {
        printf("Error decoding expected results: %s\n", test.results);
        return(false);
    } else {
        return(true);
    }
}

bool stack_match(context* ctx, expected_results* expected) {
    if (ctx->SP != expected->sz) {
        printf("Stack size (%d) does not match expected size! (%llu)\n",
               ctx->SP, expected->sz);
        return(false);
    }
    for(int idx=0; idx<ctx->SP; idx++) {
        if (ctx->DSTACK[idx] != expected->results[idx]) {
            printf("DSTACK[%d]: %lld != EXPECTED[%d]: %lld\n", idx,
                   ctx->DSTACK[idx], idx, expected->results[idx]);
            return(false);
        }
    }
    return(true);
}

bool execute_test(hexaforth_test test) {
    expected_results* expected =  malloc(sizeof(expected_results));
    expected->results = malloc(0);
    if (!generate_expected(test, expected)) {
        free(expected->results);
        free(expected);
        return(false);
    }
    bool vm_results = false;
    // We use calloc rather than malloc, as malloc will often return memory of a
    // previously free'd but non-zero'ed context.
    context *ctx = calloc(sizeof(context), 1);
    if (compile(ctx, (char*)test.input)) {
        vm(ctx);
        printf("TEST: {Input=\"%s\", Expected=[%s] => ", test.input, test.results);
        vm_results = stack_match(ctx, expected);
        if (vm_results) {
            printf("PASSED\n");
        } else {
            print_stack(ctx->SP, ctx->DSTACK[ctx->SP-1], ctx->DSTACK[ctx->SP-2], ctx);
        }
    } else {
        printf("TEST: Failed to compile: \"%s\"\n", test.input);
    };
    free(ctx);
    free(expected->results);
    free(expected);
    return(vm_results);
}

bool execute_tests(hexaforth_test* tests) {
    int64_t idx = 0;
    while (strlen((tests[idx]).input)) {
        if (!execute_test(tests[idx])) {
            return false;
        } else {
            idx++;
        };
    }
    return(true);
}
