//
// screen IMAGE.hex MEMORY.out SCREEN.pbm
//
// Extracts a machine's display from the memory file the reference VM
// writes when it stops (`hexaforth IMAGE.hex DISK.img MEMORY.out`), and
// writes it as a binary PBM.  In the PBM the leftmost pixel is the highest
// bit of a byte and a set bit is black, the same format the desktop's test
// saves its screens in.  util/png.c converts one to a PNG.  The reference
// VM has no window for the display, so this is how to see what a program
// drew.
//
// The machine is the one the image was built for, which is recorded beside
// the image (util/image.h).  That gives the display's address and which
// bit of a cell is which pixel.  It fails if the image has no machine
// recorded, or if the memory file ends before the display does.
//

#include "../vm_constants.h"
#include "image.h"
#include "room.h"

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "usage: screen IMAGE.hex MEMORY.out SCREEN.pbm\n");
        return(1);
    }
    if (!machine_of(argv[1])) {
        fprintf(stderr, "%s has no machine said beside it\n", argv[1]);
        return(1);
    }
    argv++;
    unsigned char* memory;
    size_t len = file_bytes(argv[1], &memory);
    size_t row = (size_t)(DISP_W + 7) / 8;
    unsigned char* bits = room_for(row * DISP_H + 1, 1);
    for(long y=0; y < DISP_H; y++) {
        for(long x=0; x < DISP_W; x++) {
            size_t at = ((size_t)DISP_ROW(y) + x / CELL_BITS) * CELL_BYTES;
            if (at + CELL_BYTES > len) {
                fprintf(stderr, "%s is not this machine's memory: it has %zu bytes, and the display is past them\n", argv[1], len);
                return(1);
            }
            uint64_t cell = 0;
            memcpy(&cell, memory + at, CELL_BYTES);
            int bit = x % CELL_BITS;
            int set = cell >> (DISP_LEFT_LOW ? bit : CELL_BITS - 1 - bit) & 1;
            if (DISP_INK_BLACK ? set : !set) bits[(size_t)y * row + x / 8] |= 0x80 >> (x % 8);
        }
    }
    FILE* out = fopen(argv[2], "wb");
    if (!out) { perror(argv[2]); return(1); }
    fprintf(out, "P4\n%d %d\n", (int)DISP_W, (int)DISP_H);
    fwrite(bits, 1, row * DISP_H, out);
    if (fclose(out)) { perror(argv[2]); return(1); }
    return(0);
}
