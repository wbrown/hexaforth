//
// memimage IMAGE.hex OUTDIR ADDR_BITS
//
// Converts an image to the files the Verilog memory is loaded from.  The
// cell size is that of the machine the image was built for, which is
// recorded beside the image (util/image.h).  The memory is 2^ADDR_BITS
// cells: the size the Verilog is built with, which may be smaller than
// that machine's.  It writes OUTDIR/mem.hex, one cell per line, with no
// more cells than the memory has.  It also writes OUTDIR/krom.vh, the
// kernel's ROM as a Verilog function together with the ROM's size, and
// prints a warning if the kernel's code does not fit in the ROM.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../vm_constants.h"
#include "image.h"

// Writes every cell of the image, highest byte first, which is the order
// $readmemh expects.
void write_cells(const char* path, const uint8_t* bytes, size_t len) {
    FILE* out = fopen(path, "w");
    for(size_t at=0; at < len; at += CELL_BYTES) {
        for(int idx=CELL_BYTES - 1; idx >= 0; idx--) {
            fprintf(out, "%02x", bytes[at + idx]);
        }
        fprintf(out, "\n");
    }
    fclose(out);
}

// Writes the ROM's non-zero instructions, each as one case of a function
// of its address.  The function is declared together with KB, so this file
// is the only place the Verilog gets the ROM's size from.
void write_krom(const char* path, const uint8_t* bytes, size_t len) {
    int bits = 0;
    while ((2 << bits) < KROM_BYTES) bits++;

    FILE* out = fopen(path, "w");
    fprintf(out, "// The first %d bytes of the kernel, for the CPU's kernel ROM.  Written by memimage.\n",
            KROM_BYTES);
    fprintf(out, "localparam KB = %d;             // log2 of the ROM's size in instructions\n", bits);
    fprintf(out, "function [15:0] kromf(input [%d:0] a);\n    case (a)\n", bits - 1);
    for(size_t at=0; at + 1 < len && at < KROM_BYTES; at += 2) {
        uint16_t ins = bytes[at] | bytes[at + 1] << 8;
        if (ins) {
            fprintf(out, "        %d'd%d: kromf = 16'h%04x;\n", bits, (int)(at / 2), ins);
        }
    }
    fprintf(out, "        default: kromf = 16'h0000;\n    endcase\nendfunction\n");
    fclose(out);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "usage: %s IMAGE.hex OUTDIR ADDR_BITS\n", argv[0]);
        return(1);
    }
    uint8_t* bytes;
    size_t len = load_image(argv[1], &bytes);   // the cell size comes from the image's machine
    THIS_MACHINE.addr_bits = atoi(argv[3]);     // the memory size comes from the argument, and may be less than that machine's
    if (len > MEM_BYTES) len = MEM_BYTES;
    while (len % CELL_BYTES) len++;

    static const char cells[] = "/mem.hex", krom[] = "/krom.vh";
    char* path = malloc(strlen(argv[2]) + sizeof(cells) + sizeof(krom));    // room for the longer of the two
    if (path == NULL) {
        fprintf(stderr, "memimage: there is no room for the name of a file in %s\n", argv[2]);
        return(1);
    }
    sprintf(path, "%s%s", argv[2], cells);
    write_cells(path, bytes, len);
    sprintf(path, "%s%s", argv[2], krom);
    write_krom(path, bytes, len);
    free(path);

    // The kernel's page is 8KB of code, of which the ROM is the first part.
    for(size_t at=KROM_BYTES; at < len && at < 0x2000; at++) {
        if (bytes[at]) {
            fprintf(stderr, "memimage: kernel code runs past %d bytes; it will not work from the kernel ROM\n",
                    KROM_BYTES);
            break;
        }
    }
    free(bytes);
    return(0);
}
