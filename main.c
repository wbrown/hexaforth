#include <stdlib.h>
#include "vm.h"
#include "vm_debug.h"

uint16_t read_counted_string(const uint8_t* ptr, char* str) {
    uint8_t len = *ptr;
    memcpy(str, ptr+1, len);
    str[len] = '\0';
    return(len+1 + ((len+1) % 2));
}

void load_hex(const char* filepath, const char* lstpath, context *ctx) {
    printf("Loading %s\n", filepath);
    fflush(stdout);
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

    int first_gap = -1;

    while ((read = getline(&line, &len, hexfile)) != -1) {
        *(uint32_t*)&(ctx->memory[ctx->HERE]) = (uint32_t)strtol(line, NULL, 16);
        if (ctx->memory[ctx->HERE+1]) {
            last_addr = ctx->HERE + 1;
        } else if (ctx->memory[ctx->HERE]) {
            last_addr = ctx->HERE;
        } else if (first_gap <= 0 && !*(uint64_t*)&(ctx->memory[ctx->HERE])) {
            first_gap = ctx->HERE;
        }
        ctx->HERE+=2;
    }


    next_ptr = ctx->memory[last_addr] / 2;
    dprintf("WORDS:\n");

    uint16_t DP0;
    uint16_t WORDCT=0;
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
        WORDCT++;
        dprintf("  0x%0.4x @ %s => 0x%0.4x\n", target, word, code_addr);
        next_ptr = ctx->memory[next_ptr] / 2;
        if (!next_ptr) {
            DP0 = target;
            dprintf(" ... %d words scanned!\n", WORDCT);
        } else {
            dprintf(", ");
        }
    }

    // Scan backwards from DP0 to discover first used cell.
    uint16_t last_code = DP0 - 1;
    while(!ctx->memory[last_code]) last_code--;

    // printf("First gap:    [0x%0.4x] = 0x%0.4x\n", first_gap, ctx->memory[first_gap]);
    // printf("DP0:          [0x%0.4x] = 0x%0.4x\n", DP0, ctx->memory[DP0]);
    // printf("Last address: [0x%0.4x] = 0x%0.4x\n", last_addr, ctx->memory[last_addr]);
    // printf("Last code:    [0x%0.4x] = 0x%0.4x\n", last_code, ctx->memory[last_code]);

#ifdef DEBUG
    for(int idx=0; idx < ctx->HERE; idx+=1) {
        char out[160];
        uint16_t cell = ctx->memory[idx];
        if (cell) {
            debug_address(out, ctx, idx);
            dprintf("IMG [0x%0.4x]: %s\n", idx, out);
        }
    };
#endif
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
    printf("%s loaded: (CODE=%d bytes, CODE_FREE=%d bytes, DICT=%d bytes, WORDS=%d)\n",
           filepath,
           (first_gap-1) * 2,
           (DP0 - (first_gap-1)) * 2,
           (last_addr - DP0) * 2,
           WORDCT);
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    context *ctx = calloc(sizeof(context), 1);
    ctx->words = FORTH_WORDS;
#ifdef DEBUG
    init_opcodes(ctx->words);
#endif
    load_hex(argv[1], NULL, ctx);
    ctx->OUT=stdout;
    ctx->IN=stdin;
    // ctx->EIP=0x462C / 2;
    vm(ctx);
    printf("\n[%lld instructions executed]\n", ctx->CYCLES);

    // context *ctx = malloc(sizeof(context));
    // ctx->HERE = 0;
    // generate_test_program(ctx);
    // vm(ctx);
}
