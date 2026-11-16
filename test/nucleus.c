//
// The nucleus on vm.c, typed at: nucleus.h says what this is for.
//

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nucleus.h"
#include "../util/image.h"

context machine;                // too big for the stack: it has the machine's memory
char said[1 << 16];

static uint8_t* nucleus;        // the image, as cross.fs wrote it,
static size_t nucleus_len;      // and how many bytes it is
static int stop_at;             // an instruction that is nothing: the machine stops at one
static int prompt_at;           // where the machine is at its prompt: quit, as the machine says
static char typed[1 << 12];     // what is typed at it,
static size_t typed_len;        // and how much that is

uint64_t as_cell(int64_t x) {
#if CELL_BITS == 64
    return((uint64_t)x);
#else
    return((uint64_t)x & ((1ull << CELL_BITS) - 1));
#endif
}

// The machine stops at an instruction that is nothing, and the kernel's
// ROM, which nothing stores into, has some after the kernel.
static int nothing_at(void) {
    const uint16_t* code = (const uint16_t*)nucleus;
    for(int idx=0; idx < KROM_BYTES / 2; idx++) {
        if (!code[idx]) return(idx);
    }
    fprintf(stderr, "the kernel's ROM is full, and there is nowhere for the machine to stop\n");
    exit(EXIT_FAILURE);
}

void nucleus_is(const char* path) {
    nucleus_len = load_image(path, &nucleus);
    if (nucleus_len > sizeof(machine.memory)) {
        fprintf(stderr, "%s is %d bytes, more than this machine's memory\n", path, (int)nucleus_len);
        exit(EXIT_FAILURE);
    }
    stop_at = nothing_at();
    start();
    line("' quit");
    type();
    if (machine.SP != 1) {
        fprintf(stderr, "%s did not say where its prompt is.  It said:\n%s\n", path, said);
        exit(EXIT_FAILURE);
    }
    prompt_at = (int)left(0);
}

void start(void) {
    memset(&machine, 0, sizeof(machine));
    memcpy(machine.memory, nucleus, nucleus_len);
    typed_len = 0;
}

void line(const char* format, ...) {
    va_list args;
    va_start(args, format);
    typed_len += vsnprintf(typed + typed_len, sizeof(typed) - typed_len, format, args);
    va_end(args);
    if (typed_len + 2 > sizeof(typed)) {
        fprintf(stderr, "there is too much to type at the machine\n");
        exit(EXIT_FAILURE);
    }
    typed[typed_len++] = '\n';
    typed[typed_len] = '\0';
}

void keys(const void* bytes, size_t count) {
    if (typed_len + count + 1 > sizeof(typed)) {
        fprintf(stderr, "there is too much for the machine to read\n");
        exit(EXIT_FAILURE);
    }
    memcpy(typed + typed_len, bytes, count);
    typed_len += count;
}

// What has been said has then been read: what is said next is read after.
static void read_it(void) {
    memset(said, 0, sizeof(said));
    machine.IN = fmemopen(typed, typed_len, "r");
    machine.OUT = fmemopen(said, sizeof(said) - 1, "w");
    vm(&machine);
    fclose(machine.IN);
    fclose(machine.OUT);
    typed_len = 0;
}

// What is then on the machine's data stack is what the lines left there.
void type(void) {
    line("%d execute", stop_at);
    read_it();
}

void type_more(void) {
    machine.EIP = prompt_at;
    type();
}

void set_off(void) {
    machine.EIP = prompt_at;
    read_it();
}

int stopping(void) {
    return(stop_at);
}

// The two depths are kept in two cells of the machine's, and the word that
// goes on fetches them from there: it is all numbers and instructions, so
// it calls nothing, and has nothing on the return stack before that
// stack's depth is set.
static uint64_t depths_at;      // the two cells,
static int goes_on_at;          // and the word

void going_on(void) {
    line("create depths 0 , 0 ,  depths");
    type();
    depths_at = machine.SP == 1 ? left(0) : 0;
    line(": go-on %d @ %d io!  %d @ %d io! ;  ' go-on",
         (int)depths_at + 1, IO_RSP, (int)depths_at, IO_SP);
    type_more();
    if (!depths_at || machine.SP != 1) {
        fprintf(stderr, "the machine was not given what goes on from where it stops.  It said:\n%s\n", said);
        exit(EXIT_FAILURE);
    }
    goes_on_at = (int)left(0);
}

void go_on(void) {
    store(depths_at, (uint64_t)machine.SP);
    store(depths_at + 1, (uint64_t)machine.RSP);
    machine.EIP = goes_on_at;
    read_it();
}

uint64_t left(int n) {
    return(as_cell(machine.DSTACK[machine.SP - 1 - n]));
}

bool in_memory(uint64_t addr, uint64_t cells) {
    return(addr <= (1ull << ADDR_BITS) && cells <= (1ull << ADDR_BITS) - addr);
}

uint8_t* cell_at(uint64_t addr) {
    return((uint8_t*)machine.memory + addr * CELL_BYTES);
}

uint64_t fetch(uint64_t addr) {
    uint64_t v = 0;
    memcpy(&v, cell_at(addr), CELL_BYTES);
    return(v);
}

void store(uint64_t addr, uint64_t x) {
    memcpy(cell_at(addr), &x, CELL_BYTES);
}
