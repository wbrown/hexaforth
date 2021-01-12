#include <stdlib.h>
#include "vm.h"
#include "vm_test.h"
#include "vm_debug.h"
#include "compiler.h"

static hexaforth_test TESTS[] = {
        {.label = "8-bit literals",
         .input = "1 2 3 4",
         .dstack = "1 2 3 4"},
        {.label = "Context cleared after tests",
         .input = "6 7 8 9 10",
         .dstack = "6 7 8 9 10"},
        {.label = "invert ( a -- ~a )",
         .input = "1 2 1024 invert",
         .dstack = "1 2 -1024"},
        {.label = "negative literals",
         .input = "-1 -2 -3 -4 -5",
         .dstack = "-1 -2 -3 -4 -5"},
        {.label = "16-bit literals",
         .input = "32768 65535 -32768 -65535 49151 -49151",
         .dstack = "32768 65535 -32768 -65535 49151 -49151"},
        {.label = "32-bit literals",
         .input = "4294967295 -4294967295 "
                  "4294967040 -4294967040 "
                  "2147483520 -2147483520",
         .dstack = "4294967295 -4294967295 "
                   "4294967040 -4294967040 "
                   "2147483520 -2147483520"},
        {.label = "48-bit literals",
         .input = "140737488355328 -140737488355328",
         .dstack = "140737488355328 -140737488355328"},
        {.label = "swap ( a b -- b a )",
         .input = "5 4 2 1 swap",
         .dstack = "5 4 1 2"},
        {.label = "over ( a b -- a b a )",
         .input = "5 4 3 2 over",
         .dstack = "5 4 3 2 3"},
        {.label = "nip ( a b -- b )",
         .input = "5 4 3 2 nip",
         .dstack = "5 4 2"},
        {.label = "drop ( a b -- a )",
         .input = "5 4 3 2 drop",
         .dstack = "5 4 3"},
        {.label = "+ ( a b -- a+b )",
         .input = "10 15 +",
         .dstack = "25"},
        {.label = "1+ ( a -- a+1 )",
         .input = "1 2 3 1+",
         .dstack = "1 2 4"},
        {.label = "2+ ( a -- a+2 )",
         .input = "1 2 3 2+",
         .dstack = "1 2 5"},
        {.label = "lshift ( a b -- a << b )",
         .input = "2 1 lshift",
         .dstack = "4"},
        {.label = "rshift ( a b -- a >> b )",
         .input = "2 1 rshift",
         .dstack = "1"},
        {.label = "rshift ( a b -- a >> b )",
         .input = "4 1 rshift",
         .dstack = "2"},
        {.label = "- ( a b -- b-a )",
         .input = "500 300 -",
         .dstack = "200"},
        {.label = "2* ( a -- a << 1 )",
         .input = "512 2*",
         .dstack = "1024"},
        {.label = "2/ ( a -- a >> 1 )",
         .input = "512 2/",
         .dstack = "256"},
        {.label = "@ ( a -- [a] )",
         .init = "1024 -1024 2048 -2048 4096 -4096",
         .input = "0 @ 4 @ 8 @ 12 @ 16 @ 20 @",
         .dstack = "1024 -1024 2048 -2048 4096 -4096"},
        {.label = "! ( a b -- )",
         .init = "0 0",
         .input = "1024 0 ! -1024 4 ! 0 @ 4 @",
         .dstack = "1024 -1024"},
        {.label = ">r ( a -- R: a )",
         .input = "1024 >r",
         .dstack = "",
         .rstack = "1024"},
        {.label = "r@ ( R:a -- a R: a )",
         .input = "1024 >r r@ r@",
         .dstack  = "1024 1024",
         .rstack = "1024" },
        {.label = "r> ( R:a -- a )",
         .input = "1024 >r r@ 2* r>",
         .dstack = "2048 1024",
         .rstack = ""},
        {.label = "48-bit string stack literal",
         .input = "'deadbe'",
         .dstack = "111473265304932 1"},
        {.label = "64-bit string stack literal",
         .input = "'deadbeef'",
         .dstack = "7378415037781730660 1"},
        {.label = "96-bit string stack literals",
         .input = "'deadbeefdead'",
         .dstack = "1684104548 7378415037781730660 2"},
        {.label = "8emit",
         .input = "'deadbe' drop 8emit",
         .io_expected = "deadbe" },
        {.label = "8emit",
         .input = "'deadbeef' drop 8emit",
         .io_expected = "deadbeef" },
        {.label = "8emit",
         .input = "'deadbeefdead' drop 8emit swap 8emit",
         .io_expected = "deadbeefdead"},
        {.label = "key",
         .input = "key emit key emit key emit key emit",
         .io_input = "dead",
         .io_expected = "dead"},
        {.input = ""},
};

uint16_t read_counted_string(const uint8_t* ptr, char* str) {
    uint8_t len = *ptr;
    memcpy(str, ptr+1, len);
    str[len] = '\0';
    return(len+1 + ((len+1) % 2));
}

void load_hex(const char* filepath, const char* lstpath, context *ctx) {
    FILE* hexfile = fopen(filepath, "r");
    FILE* lstfile = fopen(lstpath, "r");
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    uint32_t header_begin = 0;
    uint32_t last_addr = 0;
    uint16_t next_ptr = 0;

    if (hexfile == NULL)
        exit(EXIT_FAILURE);
    ctx->HERE=0;

    while ((read = getline(&line, &len, hexfile)) != -1) {
        *(uint32_t*)&(ctx->memory[ctx->HERE]) = (uint32_t)strtol(line, NULL, 16);
        if (ctx->memory[ctx->HERE+1]) {
            last_addr = ctx->HERE + 1;
        } else if (ctx->memory[ctx->HERE]) {
            last_addr = ctx->HERE;
        }
        ctx->HERE+=2;
    }

    dprintf("Last address: [0x%0.4x] = 0x%0.4x\n", last_addr, ctx->memory[last_addr]);
    next_ptr = ctx->memory[last_addr] / 2;
    dprintf("WORDS: ");

    while(next_ptr) {
        char word[80];
        uint16_t target=next_ptr;
        // How many aligned cells our word string was.
        uint16_t text_cells = read_counted_string((uint8_t*)&ctx->memory[target+1],
                                     word) / 2;
        // Where our dictionary entry's code pointer is.
        uint16_t code_ptr=target + text_cells + 1;
        // Dereference code pointer, get the address of where our code is.
        uint16_t code_addr=ctx->memory[code_ptr] / 2;
        // Tag the code address with our word's string
        asprintf(&ctx->meta[code_addr], "%s", word);
        dprintf("0x%0.4x @ %s => 0x%0.4x", target, word, code_addr);
        next_ptr = ctx->memory[next_ptr] / 2;
        if (next_ptr) {
            dprintf(", ");
        }
    }
    dprintf("\n");

/*    if (lstfile != NULL) {
        uint16_t prior = 0;
        uint16_t name = 0;
        while ((read = getline(&line, &len, lstfile)) != -1) {
            uint32_t addr = (uint32_t) strtol(line, NULL, 16);
             if (!header_begin) {
                header_begin = ctx->memory[addr / 2];
                name = header_begin + 1;
                prior = header_begin;
            } else {
                prior =
                name = + 1;


                len = read_counted_string((uint8_t *)&ctx->memory[addr/2],
                                          ctx->meta[addr/2]);
                printf("%s\n", ctx->meta[addr/2]);
            }
            line[strlen(line)-1] = '\0';
            asprintf(&ctx->meta[addr/2], "%s", line + 5);
        }
    }
    */

    for(int idx=0; idx < ctx->HERE; idx+=1) {
        char out[160];
        uint16_t cell = ctx->memory[idx];
        if (cell) {
            debug_address(out, ctx, idx);
        }
    };
    /*
        } else {
            if (cell) {
                char* meta = ctx->meta[ctx->HERE];
                char* codeptr = ctx->meta[cell/2];
                printf("[0x%0.4x] %-19s 0x%04hx => %-10s =>\n",
                       (ctx->HERE),
                       meta ? meta : "",
                       cell,
                       codeptr ? codeptr : "");
            }
            if (next_cell) {
                char* meta = ctx->meta[ctx->HERE+1];
                char* codeptr = ctx->meta[next_cell/2];
                printf("[0x%0.4x] %-19s 0x%04hx => %-10s =>\n",
                       (ctx->HERE+1),
                       meta ? meta : "",
                       next_cell,
                       codeptr ? codeptr : "");
            }

        }

    } */




    fclose(hexfile);
    if (line)
        free(line);
}

int main() {
    if(init_opcodes()) {
        execute_tests(TESTS);
    };
    generate_basewords_fs();
    context *ctx = calloc(sizeof(context), 1);
    load_hex("../build/nuc.hex", "../build/nuc.lst", ctx);
    // ctx->EIP=0x462C / 2;
    vm(ctx);

    // context *ctx = malloc(sizeof(context));
    // ctx->HERE = 0;
    // generate_test_program(ctx);
    // vm(ctx);
}
