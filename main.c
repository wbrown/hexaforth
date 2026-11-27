//
// hexaforth [IMAGE.hex | --machine BITS] [--typed FILE.fs] [--evaluate TEXT]
//           [DISK.img [MEMORY.out]]
//
// Runs an image on the reference VM (vm.c).
//
//   IMAGE.hex        The image to run.  An argument that ends in .hex is
//                    the image.  The default is the nucleus that CMake
//                    built for the first machine in util/machines.
//   --machine BITS   Run the built nucleus of the machine with BITS-bit
//                    cells: 16, 32 or 64.
//   --typed FILE.fs  Use FILE.fs as the console's input, followed by `bye`,
//                    which halts the machine.  The default input is stdin.
//   --evaluate TEXT  Use TEXT as the console's input, followed by `bye`.
//                    With --typed, TEXT is typed after the file.
//   DISK.img         The disk image, or several separated by commas, for
//                    disk 0, disk 1 and so on.
//   MEMORY.out       Write the machine's memory to this file when the
//                    machine stops.
//
// With --typed or --evaluate, the program prints the data stack and the
// reason the machine stopped, and for a machine that stopped without
// halting, the line it was running.  Its exit status is 0 only if the
// machine halted and every throw was caught.
//
// After `trace-on`, the program prints a line for each instruction that
// runs in a definition made since the image was loaded, with the
// instruction's fields and both stacks, until `trace-off` (vm_stopped.h).
//

#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "vm.h"
#include "vm_stopped.h"
#include "vm_debug.h"
#include "util/image.h"
#include "util/words.h"

// Loads an image into the machine's memory.  If `verbose` is set, it also
// prints the image's path and sizes.
void load_hex(const char* filepath, const char* lstpath, context *ctx, bool verbose) {
    if (verbose) {
        printf("Loading %s\n", filepath);
        fflush(stdout);
    }
    FILE* hexfile = fopen(filepath, "r");
    FILE* lstfile = fopen(lstpath, "r");
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    if (hexfile == NULL) {
        perror(filepath);
        exit(EXIT_FAILURE);
    }
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
        dprintf("  $%0.4X @ %s => $%0.4X\n", head, word, (unsigned)xt);
        head = before;
    }
    dprintf(" ... %d words scanned!\n", WORDCT);

#ifdef DEBUG
    for(int idx=0; idx < ctx->HERE; idx+=1) {
        char out[160];
        uint16_t cell = ctx->memory[idx];
        if (cell) {
            debug_address(out, ctx, idx);
            dprintf("IMG [$%0.4X]: %s\n", idx, out);
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
    if (!verbose) return;
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

static void usage(const char* self) {
    fprintf(stderr, "usage: %s [IMAGE.hex | --machine BITS] [--typed FILE.fs] [--evaluate TEXT] [DISK.img [MEMORY.out]]\n", self);
    exit(EXIT_FAILURE);
}

static bool is_image(const char* name) {
    size_t len = strlen(name);
    return(len > 4 && !strcmp(name + len - 4, ".hex"));
}

// Returns a stream of the file's contents, then `more`, then the line
// `bye`.  Either of `path` and `more` may be NULL.
static FILE* typed_from(const char* path, const char* more) {
    static const char bye[] = "bye\n";
    size_t len = 0, extra = more ? strlen(more) : 0;
    FILE* file = path ? fopen(path, "rb") : NULL;
    if (path && !file) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    if (file) {
        fseek(file, 0, SEEK_END);
        len = (size_t)ftell(file);
        rewind(file);
    }
    char* text = malloc(len + 1 + extra + 1 + sizeof(bye));
    if (!text || (file && fread(text, 1, len, file) != len)) {
        perror(path ? path : "--evaluate");
        exit(EXIT_FAILURE);
    }
    if (file) fclose(file);
    if (len && text[len - 1] != '\n') text[len++] = '\n';      // end the file's last line, so that what follows starts a new one
    if (extra) memcpy(text + len, more, extra);
    len += extra;
    if (len && text[len - 1] != '\n') text[len++] = '\n';
    memcpy(text + len, bye, sizeof(bye));
    return(fmemopen(text, len + sizeof(bye) - 1, "r"));
}

// Prints the cells of a stack in hex, the top last.  A hex number is
// printed the way the machine prints one with `.x`: after the `$` that the
// nucleus reads a hex number by, and in upper case, so a cell can be typed
// back as it is printed.  The stack is `depth` cells: the cells in `cells`
// and, above them, `top`.  At most `most` are printed, the ones nearest
// the top.
static void say_stack(FILE* to, const int64_t* cells, int depth, int64_t top, int most) {
    const uint64_t mask = CELL_BITS == 64 ? ~0ull : (1ull << CELL_BITS) - 1;
    if (depth > STACK_CELLS) depth = STACK_CELLS;
    if (depth > most) fprintf(to, " ...");
    for(int at = depth > most ? depth - most : 0; at < depth - 1; at++) {
        fprintf(to, " $%llX", (unsigned long long)((uint64_t)cells[at] & mask));
    }
    if (depth > 0) fprintf(to, " $%llX", (unsigned long long)((uint64_t)top & mask));
}

// ---- the trace (vm_stopped.h): one line for each instruction reported

static const char* TABLES;      // the names file beside the image, which has the labels of its headerless words

// Returns the word an instruction is in, or NULL if it is in none.  The
// words are the image's labels and every header now in the machine's
// memory (util/words.h).  They are listed again when the dictionary has a
// new first header, which it has after each definition.
static const word* word_around(context *ctx, int64_t at) {
    static word* words;
    static int n;
    static uint32_t first;
    const uint8_t *bytes = (const uint8_t *)ctx->memory;
    uint32_t head = first_header(bytes, MEM_BYTES);
    if (!words || head != first) {
        free(words);
        n = words_of(bytes, MEM_BYTES, TABLES, &words);
        first = head;
    }
    const word* in = NULL;
    for(int idx=0; idx < n && words[idx].at <= at; idx++) if (words[idx].size) in = &words[idx];
    return(in && at < in->at + in->size ? in : NULL);
}

// Writes a place in the code as the word it is in and the distance into
// it: `name`, `name+3`, or the address if it is in no word.
static void say_place(char* text, size_t room, context *ctx, int64_t at) {
    const word* in = word_around(ctx, at);
    if (!in) snprintf(text, room, "$%llX", (unsigned long long)at);
    else if (at > in->at) snprintf(text, room, "%s+%ld", in->name, (long)(at - in->at));
    else snprintf(text, room, "%s", in->name);
}

// Returns true if the instruction at `at` is one of the three that
// `trace-on` or `trace-off` is where it is compiled inline: a flag pushed,
// the trace's port pushed, and `io!`.  The trace leaves them out.  The
// flag may be any one instruction, because the nucleus compiles a 0 as
// `false`.
static bool switches_trace(context *ctx, int64_t at) {
    instruction writes[8], port = { 0 };
    port.lit.lit_f = true;
    port.lit.lit_v = IO_TRACE;
    if (lookup_word(ctx->words, "io!", writes) != 1) return(false);
    for(int64_t io = at; io <= at + 2; io++) {          // the `io!` is this instruction or one of the next two
        if (io < 2 || !ins_eq(*(const instruction*)&ctx->memory[io], writes[0])) continue;
        if (ins_eq(*(const instruction*)&ctx->memory[io - 1], port)) return(true);
    }
    return(false);
}

// Reports an instruction that has just run (STOPS.trace_say): where it
// is, what it is called, its fields as vm_opcodes.h writes them, and both
// stacks as it left them.  A literal is called by its number, a variable
// instruction by its cell, a call by the word it calls, and a jump or a
// branch by where it goes.  An instruction with a return folded into it
// is called by its name and `;`.
static void trace_line(context *ctx, int64_t at, int sp, int64_t t, int rsp, int64_t r) {
    const uint16_t code = ctx->memory[at];
    instruction ins = *(const instruction*)&code;
    char where[NAME_ROOM + 24], called[NAME_ROOM + 24];
    if (switches_trace(ctx, at)) return;
    say_place(where, sizeof(where), ctx, at);
    if (ins.lit.lit_f && ins.lit.lit_shifts == 3) {
        snprintf(called, sizeof(called), "var%s $%llX", ins.lit.lit_add ? "!" : "@", (unsigned long long)(VAR_BASE + ins.lit.lit_v));
    } else if (ins.lit.lit_f) {
        snprintf(called, sizeof(called), "%s%llu", ins.lit.lit_add ? "+" : "",
                 (unsigned long long)ins.lit.lit_v << (ins.lit.lit_shifts * LIT_BITS));
    } else if (ins.alu.op_type != OP_TYPE_ALU) {
        say_place(called, sizeof(called), ctx, branch(at + 1, ins.jmp.target));
    } else {
        const char* name = lookup_opcode(ctx->words, ins);
        instruction plain = ins;
        plain.alu.r_eip = 0;
        plain.alu.rstack = 0;
        const char* returning = !name && ins.alu.r_eip && ins.alu.rstack == -1 ? lookup_opcode(ctx->words, plain) : NULL;
        snprintf(called, sizeof(called), "%s%s", name ? name : returning ? returning : "", returning ? " ;" : "");
    }
    char* fields = instruction_to_str(ins);
    if (STOPS.mid_line) fputc('\n', ctx->OUT);
    STOPS.mid_line = 0;
    fprintf(ctx->OUT, "%-16s %-12s %s  data:", where, called, fields);
    say_stack(ctx->OUT, ctx->DSTACK, sp, t, 8);
    fprintf(ctx->OUT, "  return:");
    say_stack(ctx->OUT, ctx->RSTACK, rsp, r, 4);
    fputc('\n', ctx->OUT);
    free(fields);
}

// Prints the result of a typed run: the data stack and "halted" if the
// machine halted, and otherwise the reason it stopped.  The stack is
// printed only after a halt, because at any other stop it also holds the
// interpreter's working values.  `text` is the --evaluate text, which is
// the end of the input if there is any, and `file` is the --typed file.
// Returns true if the machine halted and every throw was caught.
static bool typed_report(context *ctx, const char* file, const char* text, const char* image, const char* memory_to) {
    int cells = ctx->SP < 0 ? 0 : ctx->SP > STACK_CELLS ? STACK_CELLS : ctx->SP;
    printf("\n");
    switch (STOPS.reason) {
        case STOP_HALT:
            if (cells) {
                printf("data stack (%d cell%s, top last):", ctx->SP, ctx->SP == 1 ? "" : "s");
                say_stack(stdout, ctx->DSTACK, cells, ctx->DSTACK[cells - 1], 16);      // at most 16 cells, the ones nearest the top
                printf("\n");
            } else {
                printf("data stack: empty\n");
            }
            // STOPS.counted is the number of uncaught throws (main, below).
            if (!STOPS.counted) {
                printf("halted\n");
                return(true);
            }
            printf("halted after %llu uncaught error%s\n",
                   (unsigned long long)STOPS.counted, STOPS.counted == 1 ? "" : "s");
            return(false);
        case STOP_END_OF_INPUT:
            if (text) printf("--evaluate: end of the text inside an unfinished definition or [if]\n");
            else printf("%s: end of file inside an unfinished definition or [if]\n", file);
            return(false);
        default:
            fflush(stdout);
            post_mortem(stderr, ctx, image, memory_to);
            stops_say_line(stderr);
            return(false);
    }
}

int main(int argc, char *argv[]) {
    const char *image = NULL, *file = NULL, *text = NULL, *bits = NULL, *disks = NULL, *memory_to = NULL;
    for(int idx=1; idx < argc; idx++) {
        const char* arg = argv[idx];
        if (!strcmp(arg, "--typed") && idx + 1 < argc) file = argv[++idx];
        else if (!strcmp(arg, "--evaluate") && idx + 1 < argc) text = argv[++idx];
        else if (!strcmp(arg, "--machine") && idx + 1 < argc) bits = argv[++idx];
        else if (arg[0] == '-') usage(argv[0]);
        else if (!image && !disks && is_image(arg)) image = arg;
        else if (!disks) disks = arg;
        else if (!memory_to) memory_to = arg;
        else usage(argv[0]);
    }
    if (image && bits) {
        fprintf(stderr, "give an image or --machine, one of the two: %s is already the image of one machine\n", image);
        exit(EXIT_FAILURE);
    }
    if (!image) image = built_nucleus(bits);
    bool typed = file || text;          // the console's input is a file, text or both, in place of stdin

    context *ctx = calloc(sizeof(context), 1);
    if (getenv("STOP_AFTER")) {
        stopped = ctx;
        stopped_to = memory_to;
        signal(SIGALRM, stop_it);
        alarm((unsigned)atoi(getenv("STOP_AFTER")));
    }
    ctx->words = FORTH_WORDS;
    init_opcodes(ctx->words);           // the names of the instructions, for the trace
    STOPS.trace_say = trace_line;
    // The names file is beside the image, with the same name (cross.fs).
    size_t named = strlen(image);
    char* tables = malloc(named + sizeof(".tables"));
    if (!tables) exit(EXIT_FAILURE);
    strcpy(tables, image);
    strcpy(tables + (is_image(image) ? named - 4 : named), ".tables");
    TABLES = tables;
    machine_from(image);                // use the machine the image was built for (util/image.h)
    load_hex(image, NULL, ctx, !typed);
    stops_mark_image(ctx);              // the trace reports the code that is defined from here on
    // Line-buffer the machine's output, so that everything it printed is
    // already written if it is stopped from outside.
    setvbuf(stdout, NULL, _IOLBF, 0);
    ctx->OUT=stdout;
    ctx->IN = typed ? typed_from(file, text) : stdin;
    // DISK.img is one file, or several separated by commas, for disk 0,
    // disk 1 and so on (vm.c).  Each is opened read-write if possible, and
    // read-only otherwise.
    if (disks) {
        char* files = strdup(disks);
        int drive = 0;
        for(char* file = strtok(files, ","); file; file = strtok(NULL, ","), drive++) {
            if (drive == DISK_DRIVES) {
                fprintf(stderr, "%s: more disks than the %d a machine has\n", disks, DISK_DRIVES);
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
    if (typed) {
        // A typed run stops on a halt, and also when the machine:
        //
        //   - reads a key at the end of its input.  The input ends with
        //     `bye`, so this happens only when `bye` was compiled or
        //     skipped: the input ended inside a definition or an [if].
        //   - runs more instructions than its budget without reading a
        //     key, which is an infinite loop.
        //   - runs an address that was never written.
        //
        // Uncaught throws are counted at instruction 1.  `throw` restarts
        // the prompt with `1 execute` (nuc.fs), and that is the only way
        // instruction 1 runs: when the machine starts, `main` falls
        // through into `quit`.
        STOPS.on = 1;
        STOPS.at_end = 1;
        STOPS.counted_at = 1;
        if (!STOPS.budget) STOPS.budget = STOP_BUDGET_DEFAULT;
        stops_watch_code(ctx);
    }
    vm(ctx);
    bool ok = true;
    if (typed) {
        ok = typed_report(ctx, file, text, image, memory_to);
    } else {
        printf("\n[%lld instructions executed]\n", ctx->CYCLES);
        if (STOPS.on && STOPS.reason != STOP_HALT) {
            post_mortem(stderr, ctx, image, memory_to);
        }
    }
    // Write the machine's memory to MEMORY.out.  rtl/tb.cpp writes the same
    // file, and the benchmarks compare the two (bench/).
    if (memory_to) {
        FILE* memory = fopen(memory_to, "wb");
        if (!memory) {
            perror(memory_to);
            exit(EXIT_FAILURE);
        }
        fwrite(ctx->memory, 1, MEM_BYTES, memory);
        fclose(memory);
    }
    return(ok ? EXIT_SUCCESS : EXIT_FAILURE);
}
