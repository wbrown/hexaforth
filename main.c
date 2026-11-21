#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "vm.h"
#include "vm_debug.h"

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
        *(uint32_t*)&(ctx->memory[ctx->HERE]) = (uint32_t)strtol(line, NULL, 16);
        if (first_gap <= 0 && !*(uint64_t*)&(ctx->memory[ctx->HERE])) {
            first_gap = ctx->HERE;
        }
        ctx->HERE+=2;
    }

    // Where the code ends, in instructions.  A line of the image is two of
    // them, and the gap is the first line that is nothing: the code ends
    // there, or an instruction before if the line before has only its first.
    int code_end = first_gap;
    if (code_end > 0 && !ctx->memory[code_end - 1]) code_end--;

    // The words of the image's dictionary, from the newest (read_header in
    // vm_opcodes.c), each noted at the instruction its code starts at.
    const uint8_t *bytes = (const uint8_t *)ctx->memory;
    size_t image_len = (size_t)ctx->HERE * 2;
    uint32_t head = newest_header(bytes, image_len);
    char word[256];
    dprintf("WORDS:\n");

    // Where the headers start and where they end, in instructions.
    uint32_t DP0 = code_end;
    uint32_t DP1 = DP0;
    if (head) {
        DP1 = header_end(bytes, image_len, head) / 2;
    }
    uint16_t WORDCT=0;
    while (head) {
        uint64_t xt;
        uint32_t before = read_header(bytes, image_len, head, &xt, word);
        if (!word[0]) break;
        if (xt < 65536) asprintf(&ctx->meta[xt], "%s", word);
        WORDCT++;
        dprintf("  0x%0.4x @ %s => 0x%0.4x\n", head, word, (unsigned)xt);
        DP0 = head / 2;
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

// A machine that does not stop is stopped, if STOP_AFTER in the environment
// says after how many seconds, and its memory is written where it was to
// be written when it stopped: so what it was at can be looked at
// (disasm_hex --memory, memory-map).
static context* stopped;
static const char* stopped_to;
static void stop_it(int signal) {
    (void)signal;
    if (stopped_to) {
        FILE* memory = fopen(stopped_to, "wb");
        if (memory) {
            fwrite(stopped->memory, 1, sizeof(stopped->memory), memory);
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
    load_hex(argv[1], NULL, ctx);
    // What the machine says is written a line at a time, so that a machine
    // that is stopped from outside has said what it said until then.
    setvbuf(stdout, NULL, _IOLBF, 0);
    ctx->OUT=stdout;
    ctx->IN=stdin;
    // A second argument is a file to be the disk (vm.c), or several with
    // commas between them to be disk 0, disk 1 and so on: each one that
    // can be written, if it can be opened so.
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
    // ctx->EIP=0x462C / 2;
    vm(ctx);
    printf("\n[%lld instructions executed]\n", ctx->CYCLES);
    // A third argument is a file to write the machine's memory to when it
    // has stopped, as rtl/tb.cpp does: the two are then compared (bench/).
    if (argc > 3) {
        FILE* memory = fopen(argv[3], "wb");
        if (!memory) {
            perror(argv[3]);
            exit(EXIT_FAILURE);
        }
        fwrite(ctx->memory, 1, sizeof(ctx->memory), memory);
        fclose(memory);
    }

    // context *ctx = malloc(sizeof(context));
    // ctx->HERE = 0;
    // generate_test_program(ctx);
    // vm(ctx);
}
