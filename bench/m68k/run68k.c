//
// run68k PROGRAM.bin AT PICTURE.tiff gray|colour own|half
//
// The same dither as the machine's, on a 68000, and how many clocks it
// took.  It is here to be compared with: the machine's dither is timed on
// its own CPU (rtl/tb.cpp), and this is the same picture and the same
// work on the processor of the first Macintosh, at the same clock.
//
// The 68000 is Musashi, which counts the clocks of each instruction as
// Motorola's tables give them.  It is not kept here: bench.sh fetches it.
// PROGRAM.bin is glue.s, atk.s and gray.s, linked to run from the address
// AT.  The picture is put in the 68000's memory, whole, as its three
// samples (colour) or as its grays (gray), with what glue.s says it wants
// in the registers, and the program is run until it says it is done.  The
// bitmap it leaves, whose bytes have their first pixel highest as the
// Macintosh's do, is then compared with the dither a pixel at a time
// (test/dither.c), every pixel: the same one the machine's is compared
// with.
//
// The clocks are of the whole run.  The 68000 has the picture in memory
// when it starts, and reads no file.
//

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "m68k.h"
#include "../../test/dither.h"
#include "../tiff.h"

// The 68000's memory, and where things are put in it.  The program is
// below the bitmap, and its stack comes down from STACK.
#define MEMORY  (1 << 20)
#define BITMAP  0x10000
#define TABLES  0x1f000
#define PICTURE 0x20000
#define STACK   0xf0000
#define DONE    0xffffe

static uint8_t memory[MEMORY];
static bool done;

unsigned int m68k_read_memory_8(unsigned int a) {
    return(memory[a & (MEMORY - 1)]);
}

unsigned int m68k_read_memory_16(unsigned int a) {
    return(m68k_read_memory_8(a) << 8 | m68k_read_memory_8(a + 1));
}

unsigned int m68k_read_memory_32(unsigned int a) {
    return(m68k_read_memory_16(a) << 16 | m68k_read_memory_16(a + 2));
}

void m68k_write_memory_8(unsigned int a, unsigned int v) {
    a &= MEMORY - 1;
    if (a == DONE) {
        done = true;
        m68k_end_timeslice();
    }
    memory[a] = v;
}

void m68k_write_memory_16(unsigned int a, unsigned int v) {
    m68k_write_memory_8(a, v >> 8);
    m68k_write_memory_8(a + 1, v);
}

void m68k_write_memory_32(unsigned int a, unsigned int v) {
    m68k_write_memory_16(a, v >> 16);
    m68k_write_memory_16(a + 2, v);
}

// A long as the 68000 has it: the high byte first.
static void put_long(unsigned at, unsigned v) {
    for(int idx=0; idx < 4; idx++) memory[at + idx] = v >> (8 * (3 - idx));
}

// A table of gray.s: what each value of a sample comes to, of 256 parts.
static void table(unsigned at, int parts) {
    for(int idx=0; idx < 256; idx++) {
        memory[at + 2 * idx] = (parts * idx) >> 8;
        memory[at + 2 * idx + 1] = parts * idx;
    }
}

int main(int argc, char *argv[]) {
    if (argc != 6) {
        fprintf(stderr, "Usage: %s PROGRAM.bin AT PICTURE.tiff gray|colour own|half\n", argv[0]);
        return(1);
    }
    unsigned at = strtoul(argv[2], NULL, 0);
    bool colour = !strcmp(argv[4], "colour");
    bool half = !strcmp(argv[5], "half");

    FILE* in = fopen(argv[1], "rb");
    if (in == NULL) {
        perror(argv[1]);
        return(1);
    }
    size_t len = fread(memory + at, 1, BITMAP - at, in);
    if (!feof(in)) {
        fprintf(stderr, "%s runs into the bitmap\n", argv[1]);
        return(1);
    }
    fclose(in);
    put_long(0, STACK);
    put_long(4, at);

    // The picture: its samples as the file has them, or its grays.
    tiff picture = read_tiff(argv[3]);
    int w, h;
    int* gray = grays_of(picture.bytes, picture.w, picture.h, picture.samples, false, &w, &h);
    size_t bytes = (size_t)w * h * (colour ? picture.samples : 1);
    if (colour && picture.samples != 3) {
        fprintf(stderr, "%s is not of three samples\n", argv[3]);
        return(1);
    }
    if (PICTURE + bytes > STACK - 0x1000) {
        fprintf(stderr, "%s is too big for the 68000's memory\n", argv[3]);
        return(1);
    }
    uint8_t* grays = malloc((size_t)w * h);
    for(int idx=0; idx < w * h; idx++) grays[idx] = gray[idx];
    memcpy(memory + PICTURE, colour ? picture.bytes : grays, bytes);
    table(TABLES, 77);
    table(TABLES + 512, 150);
    table(TABLES + 1024, 29);

    m68k_init();
    m68k_set_cpu_type(M68K_CPU_TYPE_68000);
    m68k_pulse_reset();
    m68k_set_reg(M68K_REG_A0, PICTURE);
    m68k_set_reg(M68K_REG_A1, BITMAP);
    m68k_set_reg(M68K_REG_A2, TABLES);
    m68k_set_reg(M68K_REG_A3, DONE);
    m68k_set_reg(M68K_REG_D0, w);
    m68k_set_reg(M68K_REG_D1, h);
    unsigned long long clocks = 0;
    while (!done) {
        int used = m68k_execute(100000000);
        if (used <= 0) {
            fprintf(stderr, "the 68000 stopped\n");
            return(1);
        }
        clocks += used;
        if (clocks > 100000000000ull) {
            fprintf(stderr, "the 68000 did not finish\n");
            return(1);
        }
    }

    // What it made, against the dither a pixel at a time.
    int dw, dh;
    int* wanted = grays_of(grays, w, h, 1, half, &dw, &dh);
    uint8_t* black = calloc((size_t)dw * dh + 1, 1);
    dither(wanted, dw, dh, black);
    int differ = 0;
    for(int y=0; y < dh; y++) {
        for(int x=0; x < dw; x++) {
            int got = memory[BITMAP + y * (dw / 8) + x / 8] >> (7 - (x & 7)) & 1;
            if (got != black[y * dw + x]) differ++;
        }
    }
    printf("%llu clocks; %d by %d: %d pixels compared, %d differ (program %d bytes)\n",
           clocks, dw, dh, dw * dh, differ, (int)len);
    return(differ || !(dw * dh));
}
