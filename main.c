#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "vm.h"
#include "vm_stopped.h"
#include "vm_debug.h"
#include "util/image.h"

void load_hex(const char* filepath, const char* lstpath, context *ctx) {
    printf("Loading %s\n", filepath);
    fflush(stdout);
    FILE* hexfile = fopen(filepath, "r");
    FILE* lstfile = fopen(lstpath, "r");
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    if (hexfile == NULL)
        exit(EXIT_FAILURE);
    ctx->HERE=0;

    int first_gap = -1;

    while ((read = getline(&line, &len, hexfile)) != -1) {
        *(uint32_t*)&(ctx->memory[ctx->HERE]) = (uint32_t)strtoul(line, NULL, 16);
        if (first_gap <= 0 && !*(uint64_t*)&(ctx->memory[ctx->HERE])) {
            first_gap = ctx->HERE;
        }
        ctx->HERE+=2;
    }

    // Find where the code ends, as an instruction address.  A line of the
    // image holds two instructions, and `first_gap` is the first line that
    // is zero.  The code ends there, or one instruction earlier if the
    // line before it has only its first instruction.
    int code_end = first_gap;
    if (code_end > 0 && !ctx->memory[code_end - 1]) code_end--;

    // Walk the image's dictionary in search order (read_header,
    // vm_opcodes.c), and record each word's name at the address of its
    // code.
    const uint8_t *bytes = (const uint8_t *)ctx->memory;
    size_t image_len = (size_t)ctx->HERE * 2;
    uint32_t head = first_header(bytes, image_len);
    char word[256];
    dprintf("WORDS:\n");

    // DP0 and DP1 are where the headers start and end in memory, as
    // instruction addresses.  They are the lowest and the highest header
    // in memory, not the first and the last in the chain (read_header).
    uint32_t DP0 = code_end;
    uint32_t DP1 = DP0;
    uint16_t WORDCT=0;
    while (head) {
        uint64_t xt;
        uint32_t before = read_header(bytes, image_len, head, &xt, word);
        if (!word[0]) break;
        if (xt < 65536) asprintf(&ctx->meta[xt], "%s", word);
        uint32_t end = header_end(bytes, image_len, head) / 2;
        if (!WORDCT || head / 2 < DP0) DP0 = head / 2;
        if (!WORDCT || end > DP1) DP1 = end;
        WORDCT++;
        dprintf("  0x%0.4x @ %s => 0x%0.4x\n", head, word, (unsigned)xt);
        head = before;
    }
    dprintf(" ... %d words scanned!\n", WORDCT);

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
           code_end * 2,
           (DP0 - code_end) * 2,
           (DP1 - DP0) * 2,
           WORDCT);
    fflush(stdout);
}

// If STOP_AFTER is set in the environment, a machine still running after
// that many seconds is stopped, and its memory is written to the file it
// would have been written to at a normal stop.  The memory can then be
// examined (disasm_hex --memory, memory-map).
static context* stopped;
static const char* stopped_to;
static void stop_it(int signal) {
    (void)signal;
    if (stopped_to) {
        FILE* memory = fopen(stopped_to, "wb");
        if (memory) {
            fwrite(stopped->memory, 1, MEM_BYTES, memory);
            fclose(memory);
        }
    }
    fprintf(stderr, "\nstopped after the %s seconds STOP_AFTER says\n", getenv("STOP_AFTER"));
    _exit(4);
}

int main(int argc, char *argv[]) {
    context *ctx = calloc(sizeof(context), 1);
    if (getenv("STOP_AFTER")) {
        stopped = ctx;
        stopped_to = argc > 3 ? argv[3] : NULL;
        signal(SIGALRM, stop_it);
        alarm((unsigned)atoi(getenv("STOP_AFTER")));
    }
    ctx->words = FORTH_WORDS;
#ifdef DEBUG
    init_opcodes(ctx->words);
#endif
    machine_from(argv[1]);              // use the machine the image was built for (util/image.h)
    load_hex(argv[1], NULL, ctx);
    // Line-buffer the machine's output, so that everything it printed is
    // already written if it is stopped from outside.
    setvbuf(stdout, NULL, _IOLBF, 0);
    ctx->OUT=stdout;
    ctx->IN=stdin;
    // The second argument is a file to use as the disk (vm.c), or several
    // files separated by commas, for disk 0, disk 1 and so on.  Each is
    // opened for writing if that is possible, and read-only otherwise.
    if (argc > 2) {
        char* files = strdup(argv[2]);
        int drive = 0;
        for(char* file = strtok(files, ","); file; file = strtok(NULL, ","), drive++) {
            if (drive == DISK_DRIVES) {
                fprintf(stderr, "%s: more disks than the %d a machine has\n", argv[2], DISK_DRIVES);
                exit(EXIT_FAILURE);
            }
            FILE* disk = fopen(file, "r+b");
            if (!disk) disk = fopen(file, "rb");
            if (!disk) {
                perror(file);
                exit(EXIT_FAILURE);
            }
            fseek(disk, 0, SEEK_END);
            ctx->DISKS[drive] = disk;
            ctx->DISKBLOCKS[drive] = ftell(disk) / DISK_BLOCK;
        }
        free(files);
    }
    // Take the stop conditions from the environment (vm_stopped.h).  If
    // the machine stops for one of them, print the report of where it was.
    stops_from_env();
    vm(ctx);
    printf("\n[%lld instructions executed]\n", ctx->CYCLES);
    if (STOPS.on && STOPS.reason != STOP_HALT) {
        post_mortem(stderr, ctx, argv[1], argc > 3 ? argv[3] : NULL);
    }
    // The third argument is a file to write the machine's memory to once
    // it has stopped.  rtl/tb.cpp writes the same file, and the benchmarks
    // compare the two (bench/).
    if (argc > 3) {
        FILE* memory = fopen(argv[3], "wb");
        if (!memory) {
            perror(argv[3]);
            exit(EXIT_FAILURE);
        }
        fwrite(ctx->memory, 1, MEM_BYTES, memory);
        fclose(memory);
    }

    // context *ctx = malloc(sizeof(context));
    // ctx->HERE = 0;
    // generate_test_program(ctx);
    // vm(ctx);
}
