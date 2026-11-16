//
// difftest KERNEL.bin CASES.bin ADDR_BITS START IMAGE NEXT HALT
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
// The kernel is loaded where it is linked (START) and its boot stub run
// once, to where it takes its first instruction (NEXT); every case then
// starts from the machine as that left it.  IMAGE is where the kernel has
// the machine's memory, which the stacks follow, and HALT where it stops.
// The four are the kernel's own symbols, which the Makefile asks of it.
//

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unicorn/unicorn.h>

#define RAM         0x40000000ull       // where QEMU's virt machine has its memory,
#define UART        0x09000000ull       // and its UART
#define UART_DATA   0x00                // the UART's registers: a character,
#define UART_FLAGS  0x18                // and its flags,
#define RX_EMPTY    0x10                // of which this says there is nothing to read
#define STEPS       100000              // the most instructions a case is given
#define SHOWN       40                  // the most failures that are said

static uc_engine* uc;
static const uint8_t* rx;               // what the case is to read,
static size_t rx_len, rx_at;            // how much, and how much of it has been read;
static uint8_t tx[1 << 16];             // and what it wrote
static size_t tx_len;

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
    } else if (type == UC_MEM_WRITE && at == UART_DATA && tx_len < sizeof(tx)) {
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
// deep it is.
static int stack_of(uint64_t base, int pointer, int top, int64_t* cells, int most) {
    int depth = (int)((int64_t)(reg(pointer) - base) / 8);
    if (depth > most) depth = most;
    if (depth > 1) uc_mem_read(uc, base + 8, cells, 8 * (size_t)(depth - 1));
    if (depth > 0) cells[depth - 1] = (int64_t)reg(top);
    return(depth < 0 ? 0 : depth);
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
    if (argc != 8) {
        fprintf(stderr, "Usage: %s KERNEL.bin CASES.bin ADDR_BITS START IMAGE NEXT HALT\n", argv[0]);
        return(1);
    }
    size_t code_len;
    uint8_t* code = file(argv[1], &code_len);
    cases = file(argv[2], &cases_len);
    uint64_t stacks = 8ull << atoi(argv[3]);            // the stacks follow memory:
    uint64_t dstack = stacks, rstack = stacks + 0x800;  // the data stack, and then the return stack
    uint64_t start = strtoull(argv[4], NULL, 16), image = strtoull(argv[5], NULL, 16);
    uint64_t next = strtoull(argv[6], NULL, 16), halt = strtoull(argv[7], NULL, 16);

    must(uc_open(UC_ARCH_ARM64, UC_MODE_ARM, &uc), "no AArch64");
    must(uc_mem_map(uc, RAM, 0x200000 + stacks, UC_PROT_ALL), "its memory");
    must(uc_mem_map(uc, UART, 0x1000, UC_PROT_ALL), "its UART");
    must(uc_mem_write(uc, start, code, code_len), "the kernel");
    uc_hook hook;
    must(uc_hook_add(uc, &hook, UC_HOOK_MEM_READ | UC_HOOK_MEM_WRITE, uart, NULL, UART, UART + 0xfff), "its UART");
    must(uc_emu_start(uc, start, next, 0, 0), "the kernel's start");       // the boot stub, once
    uc_context* booted;
    must(uc_context_alloc(uc, &booted), "the machine as it starts");
    must(uc_context_save(uc, booted), "the machine as it starts");

    static uint8_t nothing[0x1000];
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

        static int64_t got[512];
        static uint8_t got_mem[1 << 22];
        char why[512] = "";
        size_t n = 0;
        if (!halted) n += snprintf(why + n, sizeof(why) - n, "did not halt%s%s; ", ran ? ": " : "", ran ? uc_strerror(ran) : "");
        int depth = stack_of(image + dstack, UC_ARM64_REG_X24, UC_ARM64_REG_X22, got, 512);
        if (depth != want_d || memcmp(got, d_cells, 8 * (size_t)depth)) {
            n += snprintf(why + n, sizeof(why) - n, "the data stack is %d deep and is to be %d, or its cells differ; ", depth, want_d);
        }
        depth = stack_of(image + rstack, UC_ARM64_REG_X25, UC_ARM64_REG_X23, got, 512);
        if (depth != want_r || memcmp(got, r_cells, 8 * (size_t)depth)) {
            n += snprintf(why + n, sizeof(why) - n, "the return stack is %d deep and is to be %d, or its cells differ; ", depth, want_r);
        }
        if (mem_len > sizeof(got_mem)) {
            fprintf(stderr, "difftest: a case has more memory than %d bytes\n", (int)sizeof(got_mem));
            return(1);
        }
        uc_mem_read(uc, image, got_mem, mem_len);
        for(uint32_t idx=0; idx < mem_len; idx++) {
            if (got_mem[idx] != want_mem[idx]) {
                n += snprintf(why + n, sizeof(why) - n, "memory differs at 0x%x; ", idx);
                break;
            }
        }
        if (tx_len != out_len || memcmp(tx, out, out_len)) {
            n += snprintf(why + n, sizeof(why) - n, "it wrote %d bytes and is to write %d, or they differ; ", (int)tx_len, (int)out_len);
        }
        if (n) {
            failed++;
            if (failed <= SHOWN) printf("FAIL %.*s: %s\n", (int)label_len, label, why);
        }
    }
    printf("%d/%d cases match vm.c\n", total - failed, total);
    return(failed || !total);
}
