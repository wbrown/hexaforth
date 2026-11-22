//
// The nucleus on vm.c, typed at: nucleus.h says what this is for.
//

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nucleus.h"
#include "../util/image.h"
#include "../util/room.h"
#include "../vm_stopped.h"

context machine;                // too big for the stack: it has the machine's memory
char* said = "";                // all it printed, however much
void (*stopped_short)(void);

static const char* nucleus_path;
static uint8_t* nucleus;        // the image, as cross.fs wrote it,
static size_t nucleus_len;      // and how many bytes it is
static int stop_at;             // an instruction that is nothing: the machine stops at one
static int prompt_at;           // where the machine is at its prompt: quit, as the machine says
static char* typed;             // what is typed at it, which has room for as much as is typed,
static long typed_room;
static size_t typed_len;        // and how much that is
static char* was_said;          // the room for what it printed, which is made anew each time it is run
static bool has_run;            // it has been run since it was started: it goes on from its prompt

uint64_t as_cell(int64_t x) {
    const int spare = 64 - CELL_BITS;
    return(((uint64_t)x << spare) >> spare);
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

// A context has room for the largest machine's memory.  This machine's is
// the first MEM_BYTES of that room, and nothing touches the rest of it: so
// what is cleared is all of the context but the rest of the room.
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
    int more = vsnprintf(NULL, 0, format, args);        // how long it is, and then it is written
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

// What has been said has then been read: what is said next is read after.
static void read_it(void) {
    size_t said_len = 0;
    free(was_said);
    AS_MANY(typed, 1, typed_room);                      // so that there is something to read from, if nothing was typed
    machine.IN = fmemopen(typed, typed_len, "r");
    machine.OUT = open_memstream(&was_said, &said_len);
    vm(&machine);
    fclose(machine.IN);
    fclose(machine.OUT);
    said = was_said;
    typed_len = 0;
    has_run = true;
    if (!STOPS.on || STOPS.reason == STOP_HALT) return;
    // It stopped, and did not halt (vm_stopped.h): nothing goes on from there.
    if (stopped_short) stopped_short();
    fprintf(stderr, "%s\n", said);
    post_mortem(stderr, &machine, nucleus_path, NULL);
    exit(EXIT_FAILURE);
}

// What is then on the machine's data stack is what the lines left there.
// A machine that has stopped once is where it stopped, which is nowhere to
// go on from: it goes on from its prompt.
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
    return((uint8_t*)machine.memory + (addr << BYTE_SHIFT));
}

// A cell is 8, 4 or 2 bytes, the lowest first.
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

// A cell's bits are a power of two, so a pixel's cell and its bit of it
// are a shift and a mask.
bool black_at(int x, int y) {
    const int bits = CELL_BITS;
    uint64_t cell = fetch(DISP_ROW(y) + (x >> (BYTE_SHIFT + 3)));
    int bit = x & (bits - 1);
    bool set = cell >> (DISP_LEFT_LOW ? bit : bits - 1 - bit) & 1;
    return(DISP_INK_BLACK ? set : !set);
}
