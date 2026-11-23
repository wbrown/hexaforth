//
// run68k PROGRAM.bin AT PICTURE.tiff gray|colour own|half
//
// Runs the machine's dither on a 68000 and reports the number of clocks it
// took.  It is a comparison for the machine's own figures.  The machine's
// dither is timed on its own CPU (rtl/tb.cpp), and this program times the
// same picture and the same work on the processor of the first Macintosh,
// at the same clock rate.
//
// The 68000 is Musashi, which counts the clocks of each instruction from
// Motorola's tables.  Its source is not in this tree: CMake fetches it.
// PROGRAM.bin is glue.s, atk.s and gray.s, linked to run from the address
// AT.  The whole picture is put in the 68000's memory, as its three
// samples per pixel (colour) or as its grays (gray).  The registers are
// set to what glue.s expects, and the program runs until it signals that
// it is done.  The bitmap it leaves has the first pixel in the highest bit
// of each byte, as the Macintosh's bitmaps have.  Every pixel of it is
// compared with the reference dither, which works one pixel at a time
// (test/dither.c).  The machine's bitmap is compared with the same
// dither.
//
// The clocks are for the whole run.  The 68000 has the picture in memory
// when it starts, and reads no file.  The result is printed in the form of
// the machine's own figures (figure.h), and it says FAIL if the bitmap is
// not the dither's.
//

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "m68k.h"
#include "../../test/dither.h"
#include "../figure.h"
#include "../tiff.h"

// The size of the 68000's memory, and the addresses things are put at.
// The program is below the bitmap, and its stack grows down from STACK.
#define MEMORY  (1 << 20)
#define BITMAP  0x10000
#define TABLES  0x1f000
#define PICTURE 0x20000
#define STACK   0xf0000
#define DONE    0xffffe         // the program writes to this address when it is done

static uint8_t memory[MEMORY];
static bool done;

// Musashi calls the functions below to read and write the 68000's memory.
// A write to DONE ends the run.
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

// Stores a 32-bit value in the 68000's memory, high byte first.
static void put_long(unsigned at, unsigned v) {
    for(int idx=0; idx < 4; idx++) memory[at + idx] = v >> (8 * (3 - idx));
}

// Writes one of gray.s's tables.  For each sample value from 0 to 255 it
// stores parts * value as 16 bits, high byte first: the sample weighted by
// `parts` parts of 256.
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

    // Put the picture in memory: its samples as the file has them, or its
    // grays.
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

    // Compare the bitmap the program made with the reference dither, pixel
    // by pixel.
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
    bool failed = differ || !(dw * dh);
    char* work = NULL;
    char* is = NULL;
    text_more(&work, "dither %s %s", argv[4], argv[5]);
    text_more(&is, "%s%llu clocks; %d by %d: %d pixels compared, %d differ (program %d bytes)",
              failed ? "FAIL " : "", clocks, dw, dh, dw * dh, differ, (int)len);
    figure(work, NULL, "68000", is);
    return(0);                  // the line printed says whether it failed; `check table` turns that into an exit status (check.c)
}
