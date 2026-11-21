//
// screen MEMORY.out SCREEN.pbm
//
// The display of a machine, from its memory as the reference VM writes it
// when it has stopped (`hexaforth IMAGE.hex DISK.img MEMORY.out`): a PBM of
// bits, the leftmost pixel the highest bit of a byte and a set bit black,
// as the desktop's test leaves its screens.  util/png.c makes a PNG of
// one.  The reference VM shows no display, and this is how what a program
// drew on it is seen.
//
// It is built for the machine whose memory it is, and has where the
// display is, and which bit of a cell is which pixel, from
// vm_constants.h.  It fails if the file is less than the display's memory.
//

#include "../vm_constants.h"
#include "room.h"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "usage: screen MEMORY.out SCREEN.pbm\n");
        return(1);
    }
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
