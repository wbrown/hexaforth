//
// Runs the nucleus on vm.c and types at its prompt.  nucleus.h describes
// the interface.
//

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nucleus.h"
#include "../util/image.h"
#include "../util/room.h"
#include "../vm_stopped.h"

context machine;                // a global, because it holds the machine's memory and is too big for the stack
char* said = "";                // everything the machine printed, of any length
void (*stopped_short)(void);

static const char* nucleus_path;
static uint8_t* nucleus;        // the image, as cross.fs wrote it
static size_t nucleus_len;      // its length in bytes
static int stop_at;             // the address of a zero instruction, where the machine stops
static int prompt_at;           // the address of `quit`, the machine's prompt, as the machine itself reports it
static char* typed;             // the queued input, in a buffer that grows as needed
static long typed_room;
static size_t typed_len;        // the length of the queued input
static char* was_said;          // the buffer for the machine's output, allocated again on each run
static bool has_run;            // true once the machine has run since start(); it then continues from its prompt

uint64_t as_cell(int64_t x) {
    const int spare = 64 - CELL_BITS;
    return(((uint64_t)x << spare) >> spare);
}

// Returns the address of a zero instruction in the kernel's ROM.  The
// machine stops at a zero instruction.  The ROM has some after the
// kernel's code, and nothing stores into the ROM, so they stay zero.
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
    if (nucleus_len > MEM_BYTES) {
        fprintf(stderr, "%s is %d bytes, more than this machine's memory\n", path, (int)nucleus_len);
        exit(EXIT_FAILURE);
    }
    stop_at = nothing_at();
    nucleus_path = path;
    stops_from_env();
    start();
    line("' quit");
    type();
    if (machine.SP != 1) {
        fprintf(stderr, "%s did not say where its prompt is.  It said:\n%s\n", path, said);
        exit(EXIT_FAILURE);
    }
    prompt_at = (int)left(0);
}

// A context has room for the memory of the largest machine.  This machine
// uses the first MEM_BYTES of it, and nothing touches the rest.  So
// start() clears the whole context except the unused part of the memory.
void start(void) {
    uint8_t* const memory = (uint8_t*)machine.memory;
    uint8_t* const after = memory + sizeof(machine.memory);
    memset(&machine, 0, (size_t)(memory - (uint8_t*)&machine) + MEM_BYTES);
    memset(after, 0, (size_t)((uint8_t*)(&machine + 1) - after));
    memcpy(machine.memory, nucleus, nucleus_len);
    typed_len = 0;
    has_run = false;
}

void line(const char* format, ...) {
    va_list args, again;
    va_start(args, format);
    va_copy(again, args);
    int more = vsnprintf(NULL, 0, format, args);        // measure the text first, then format it
    va_end(args);
    AS_MANY(typed, (long)typed_len + more + 2, typed_room);
    typed_len += vsnprintf(typed + typed_len, more + 1, format, again);
    va_end(again);
    typed[typed_len++] = '\n';
    typed[typed_len] = '\0';
}

void keys(const void* bytes, size_t count) {
    AS_MANY(typed, (long)(typed_len + count) + 1, typed_room);
    memcpy(typed + typed_len, bytes, count);
    typed_len += count;
}

// Runs the machine on the queued input, and empties the queue.  Input
// queued afterwards is read by the next run.
static void read_it(void) {
    size_t said_len = 0;
    free(was_said);
    AS_MANY(typed, 1, typed_room);                      // make sure the buffer exists even if nothing was queued
    machine.IN = fmemopen(typed, typed_len, "r");
    machine.OUT = open_memstream(&was_said, &said_len);
    vm(&machine);
    fclose(machine.IN);
    fclose(machine.OUT);
    said = was_said;
    typed_len = 0;
    has_run = true;
    if (!STOPS.on || STOPS.reason == STOP_HALT) return;
    // The machine stopped without halting (vm_stopped.h).  The test cannot
    // continue from there.
    if (stopped_short) stopped_short();
    fprintf(stderr, "%s\n", said);
    post_mortem(stderr, &machine, nucleus_path, NULL);
    exit(EXIT_FAILURE);
}

// After type(), the machine's data stack holds what the lines left there.
// A machine that has already run is restarted at its prompt, because the
// address where it stopped is not a place to continue from.
void type(void) {
    if (has_run) machine.EIP = prompt_at;
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

// The two stack depths are saved in two cells of the machine's memory,
// and the word that resumes the machine fetches them from there.  That
// word is made only of literals and instructions.  It calls nothing, so it
// has nothing on the return stack before it sets that stack's depth.
static uint64_t depths_at;      // the address of the two cells
static int goes_on_at;          // the address of the word

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
    return((uint8_t*)machine.memory + (addr << BYTE_SHIFT));
}

// A cell is 8, 4 or 2 bytes, lowest byte first.
uint64_t fetch(uint64_t addr) {
    const uint8_t* at = cell_at(addr);
    switch (CELL_BITS) {
        case 64: { uint64_t v; memcpy(&v, at, sizeof(v)); return(v); }
        case 32: { uint32_t v; memcpy(&v, at, sizeof(v)); return(v); }
        default: { uint16_t v; memcpy(&v, at, sizeof(v)); return(v); }
    }
}

void store(uint64_t addr, uint64_t x) {
    uint8_t* at = cell_at(addr);
    switch (CELL_BITS) {
        case 64: memcpy(at, &x, sizeof(x)); break;
        case 32: { uint32_t low = (uint32_t)x; memcpy(at, &low, sizeof(low)); break; }
        default: { uint16_t low = (uint16_t)x; memcpy(at, &low, sizeof(low)); break; }
    }
}

// The number of bits in a cell is a power of two, so a pixel's cell and
// its bit within the cell are found with a shift and a mask.
bool black_at(int x, int y) {
    const int bits = CELL_BITS;
    uint64_t cell = fetch(DISP_ROW(y) + (x >> (BYTE_SHIFT + 3)));
    int bit = x & (bits - 1);
    bool set = cell >> (DISP_LEFT_LOW ? bit : bits - 1 - bit) & 1;
    return(DISP_INK_BLACK ? set : !set);
}
