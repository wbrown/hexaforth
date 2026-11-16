//
// difftest KERNEL.bin CASES.bin KERNEL.symbols
//
// An AArch64 kernel against vm.c, which is the machine.
//
// CASES.bin is what difftest_dump wrote: each case a machine's memory and
// where it starts, what it is to read, and what vm.c left when it had run
// it: the two stacks, what it wrote, and its memory.  Each is run here on
// the kernel, under Unicorn, which is an AArch64 with nothing but the
// memory it is given and a UART: and the kernel is to halt with the same
// stacks, the same output and the same memory, every byte.
//
// The kernel is loaded where it is linked (_start) and its boot stub run
// once, to where it takes its first instruction (next); every case then
// starts from the machine as that left it.  image is where the kernel has
// the machine's memory, which the stacks follow, and halt where it stops.
// The four are the kernel's own symbols, which the build keeps beside it
// (symbols.c).  This is built for the machine the kernel is built for, and
// so has how much memory that is.
//

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unicorn/unicorn.h>
#include "../vm_constants.h"
#include "symbols.c"

#define RAM        0x40000000ull       // where QEMU's virt machine has its memory,
#define UART        0x09000000ull       // and its UART
#define UART_DATA   0x00                // the UART's registers: a character,
#define UART_FLAGS  0x18                // and its flags,
#define RX_EMPTY    0x10                // of which this says there is nothing to read
#define STEPS       100000              // the most instructions a case is given: chosen, and more than any takes
#define SHOWN       40                  // the most failures that are said
#define STACK_BYTES (STACK_CELLS * sizeof(int64_t))     // a stack, as the machine says how many cells one is
#define MAPPED      0x1000              // what the emulator maps memory in

static uc_engine* uc;
static const uint8_t* rx;               // what the case is to read,
static size_t rx_len, rx_at;            // how much, and how much of it has been read;
static uint8_t* tx;                     // and what it wrote: all of it, however much
static size_t tx_len, tx_room;

// Room for so many of something, or it stops.
static void* more_room(void* had, size_t count, size_t size) {
    void* room = realloc(had, (count ? count : 1) * size);
    if (room == NULL) {
        fprintf(stderr, "difftest: there is no room for %zu of %zu bytes\n", count, size);
        exit(EXIT_FAILURE);
    }
    return(room);
}

static void must(uc_err err, const char* what) {
    if (err == UC_ERR_OK) return;
    fprintf(stderr, "difftest: %s: %s\n", what, uc_strerror(err));
    exit(EXIT_FAILURE);
}

// The UART: a read of its flags says whether there is more to read, a
// read of its data takes the next character, and a write of its data is a
// character written.
static void uart(uc_engine* on, uc_mem_type type, uint64_t address, int size, int64_t value, void* user) {
    uint64_t at = address - UART;
    uint32_t is = 0;
    if (type == UC_MEM_READ && at == UART_FLAGS) {
        is = rx_at < rx_len ? 0 : RX_EMPTY;
        uc_mem_write(on, address, &is, 4);
    } else if (type == UC_MEM_READ && at == UART_DATA) {
        is = rx_at < rx_len ? rx[rx_at++] : 0;
        uc_mem_write(on, address, &is, 4);
    } else if (type == UC_MEM_WRITE && at == UART_DATA) {
        if (tx_len == tx_room) tx = more_room(tx, tx_room = 2 * tx_room + MAPPED, 1);
        tx[tx_len++] = (uint8_t)value;
    }
}

static uint64_t reg(int which) {
    uint64_t value = 0;
    uc_reg_read(uc, which, &value);
    return(value);
}

// A stack as the kernel has it: its cells are in memory from a cell past
// its base, but for the top one, which is in a register.  Returns how
// deep it is, and its cells, which are made here.
static int stack_of(uint64_t base, int pointer, int top, int64_t** cells) {
    int depth = (int)((int64_t)(reg(pointer) - base) / (int64_t)sizeof(int64_t));
    if (depth < 0) depth = 0;
    *cells = more_room(NULL, depth, sizeof(int64_t));
    if (depth > 1) uc_mem_read(uc, base + sizeof(int64_t), *cells, sizeof(int64_t) * (size_t)(depth - 1));
    if (depth > 0) (*cells)[depth - 1] = (int64_t)reg(top);
    return(depth);
}

// What a case did that it was not to, said after what has been said of it:
// the text is as long as it has to be.
static void also(char** why, const char* format, ...) {
    va_list args, again;
    va_start(args, format);
    va_copy(again, args);
    size_t had = *why ? strlen(*why) : 0, more = vsnprintf(NULL, 0, format, args);
    va_end(args);
    *why = more_room(*why, had + more + 1, 1);
    vsnprintf(*why + had, more + 1, format, again);
    va_end(again);
}

// The cases, a field at a time.
static const uint8_t* cases;
static size_t cases_len, at;

static const uint8_t* take(size_t n) {
    if (at + n > cases_len) {
        fprintf(stderr, "difftest: the cases end in the middle of one\n");
        exit(EXIT_FAILURE);
    }
    at += n;
    return(cases + at - n);
}

static uint32_t u32(void) {
    uint32_t v;
    memcpy(&v, take(4), 4);
    return(v);
}

static uint8_t* file(const char* path, size_t* len) {
    FILE* in = fopen(path, "rb");
    if (in == NULL) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    fseek(in, 0, SEEK_END);
    *len = ftell(in);
    fseek(in, 0, SEEK_SET);
    uint8_t* bytes = malloc(*len + 1);
    if (fread(bytes, 1, *len, in) != *len) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    fclose(in);
    return(bytes);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s KERNEL.bin CASES.bin KERNEL.symbols\n", argv[0]);
        return(1);
    }
    size_t code_len;
    uint8_t* code = file(argv[1], &code_len);
    cases = file(argv[2], &cases_len);
    uint64_t stacks = sizeof(int64_t) << ADDR_BITS;     // the stacks follow memory:
    uint64_t dstack = stacks, rstack = stacks + STACK_BYTES;    // the data stack, and then the return stack
    uint64_t start = symbol_at(argv[3], "_start"), image = symbol_at(argv[3], "image");
    uint64_t next = symbol_at(argv[3], "next"), halt = symbol_at(argv[3], "halt");

    // Its memory is from where QEMU has it to the end of the kernel or of
    // the second stack, whichever is further, in what the emulator maps.
    uint64_t end = image + stacks + 2 * STACK_BYTES;
    if (start + code_len > end) end = start + code_len;
    uint64_t mapped = (end - RAM + MAPPED - 1) / MAPPED * MAPPED;
    must(uc_open(UC_ARCH_ARM64, UC_MODE_ARM, &uc), "no AArch64");
    must(uc_mem_map(uc, RAM, mapped, UC_PROT_ALL), "its memory");
    must(uc_mem_map(uc, UART, MAPPED, UC_PROT_ALL), "its UART");
    must(uc_mem_write(uc, start, code, code_len), "the kernel");
    uc_hook hook;
    must(uc_hook_add(uc, &hook, UC_HOOK_MEM_READ | UC_HOOK_MEM_WRITE, uart, NULL, UART, UART + MAPPED - 1), "its UART");
    must(uc_emu_start(uc, start, next, 0, 0), "the kernel's start");       // the boot stub, once
    uc_context* booted;
    must(uc_context_alloc(uc, &booted), "the machine as it starts");
    must(uc_context_save(uc, booted), "the machine as it starts");

    static uint8_t nothing[2 * STACK_BYTES];            // the two stacks, with nothing in them
    int total = 0, failed = 0;
    while (at < cases_len) {
        uint32_t label_len = u32();
        const char* label = (const char*)take(label_len);
        uint32_t eip = u32(), mem_len = u32();
        const uint8_t* memory = take(mem_len);
        rx_len = u32(), rx = take(rx_len), rx_at = 0;
        int32_t sp = (int32_t)u32(), rsp = (int32_t)u32();
        int want_d = sp > 0 ? sp : 0, want_r = rsp > 0 ? rsp : 0;
        const uint8_t* d_cells = take(8 * (size_t)want_d);
        const uint8_t* r_cells = take(8 * (size_t)want_r);
        uint32_t out_len = u32();
        const uint8_t* out = take(out_len);
        const uint8_t* want_mem = take(mem_len);
        total++;

        uc_context_restore(uc, booted);
        uc_mem_write(uc, image, memory, mem_len);
        uc_mem_write(uc, image + stacks, nothing, sizeof(nothing));
        uint64_t x21 = eip;
        uc_reg_write(uc, UC_ARM64_REG_X21, &x21);
        tx_len = 0;
        uc_err ran = uc_emu_start(uc, next, halt, 0, STEPS);
        bool halted = ran == UC_ERR_OK && reg(UC_ARM64_REG_PC) == halt;

        int64_t* got;
        uint8_t* got_mem = more_room(NULL, mem_len, 1);
        char* why = NULL;
        if (!halted) also(&why, "did not halt%s%s; ", ran ? ": " : "", ran ? uc_strerror(ran) : "");
        int depth = stack_of(image + dstack, UC_ARM64_REG_X24, UC_ARM64_REG_X22, &got);
        if (depth != want_d || memcmp(got, d_cells, sizeof(int64_t) * (size_t)depth)) {
            also(&why, "the data stack is %d deep and is to be %d, or its cells differ; ", depth, want_d);
        }
        free(got);
        depth = stack_of(image + rstack, UC_ARM64_REG_X25, UC_ARM64_REG_X23, &got);
        if (depth != want_r || memcmp(got, r_cells, sizeof(int64_t) * (size_t)depth)) {
            also(&why, "the return stack is %d deep and is to be %d, or its cells differ; ", depth, want_r);
        }
        free(got);
        uc_mem_read(uc, image, got_mem, mem_len);
        for(uint32_t idx=0; idx < mem_len; idx++) {
            if (got_mem[idx] != want_mem[idx]) {
                also(&why, "memory differs at 0x%x; ", idx);
                break;
            }
        }
        free(got_mem);
        if (tx_len != out_len || memcmp(tx, out, out_len)) {
            also(&why, "it wrote %d bytes and is to write %d, or they differ; ", (int)tx_len, (int)out_len);
        }
        if (why) {
            failed++;
            if (failed <= SHOWN) printf("FAIL %.*s: %s\n", (int)label_len, label, why);
            free(why);
        }
    }
    printf("%d/%d cases match vm.c\n", total - failed, total);
    return(failed || !total);
}
