//
// memimage IMAGE.hex OUTDIR
//
// An image as what the Verilog memory is loaded from, for the machine this
// is built for (-DCELL_BITS, -DADDR_BITS).  Writes OUTDIR/mem.hex, a cell on
// each line, no more of them than the memory has.  Also writes
// OUTDIR/krom.vh, the kernel's ROM as a Verilog function and how big it is,
// and says so if the kernel's code does not fit in it.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../vm_constants.h"
#include "image.h"

// Every cell of the image, the highest byte first as $readmemh wants it.
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

// The instructions of the ROM that are not zero, each a case of a function
// of its number.  The function is declared where KB is, so the ROM's size is
// said to the Verilog here and nowhere in it.
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
    uint8_t* bytes;
    size_t len = load_image(argv[1], &bytes);
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
