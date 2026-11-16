//
// check PICTURE.tiff own|half MEMORY CONSOLE
// check MEMORY MEMORY
//
// What a benchmark made, looked at: one that has not been checked has
// measured nothing.
//
// The first is a bitmap the machine made of a picture, against the dither
// a pixel at a time (test/dither.c), every cell of it.  MEMORY is the
// machine's memory when it had stopped (rtl/tb.cpp writes it), and CONSOLE
// what it printed, in which
//     b @ . b 1+ @ .
// was typed last: where the bitmap is, and the cells of a row of it.  The
// picture was dithered at its own size, or at half of it.
//
// The second is the display, in the memory of two machines that were
// typed the same at: the reference VM (main.c writes its memory) and the
// Verilog CPU.  Every cell of it is to be the same in both, and something
// is to have been drawn.
//
// It is built for the size of machine the memory is of, and for its
// display, since a bitmap has its pixels as the display has them.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../vm_constants.h"
#include "../test/dither.h"
#include "tiff.h"

// A file, whole, with nothing after it.
static char* read_all(const char* path, size_t* len) {
    FILE* in = fopen(path, "rb");
    if (in == NULL) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    fseek(in, 0, SEEK_END);
    *len = ftell(in);
    fseek(in, 0, SEEK_SET);
    char* bytes = malloc(*len + 1);
    if (fread(bytes, 1, *len, in) != *len) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    fclose(in);
    bytes[*len] = '\0';
    return(bytes);
}

// A number the machine printed, as a cell that is not negative.
static uint64_t ucell(long long x) {
#if CELL_BITS == 64
    return((uint64_t)x);
#else
    return((uint64_t)x & ((1ull << CELL_BITS) - 1));
#endif
}

// The display in two memories: row y is where vm_constants.h says it is.
static int displays(const char* one, const char* other) {
    size_t len, other_len;
    uint8_t* a = (uint8_t*)read_all(one, &len);
    uint8_t* b = (uint8_t*)read_all(other, &other_len);
    int across = DISP_W / CELL_BITS, total = 0, differ = 0, drawn = 0;
    for(int y=0; y < DISP_H; y++) {
        size_t row = (size_t)DISP_ROW(y) * CELL_BYTES;
        if (row + across * CELL_BYTES > len || row + across * CELL_BYTES > other_len) break;
        for(int idx=0; idx < across * CELL_BYTES; idx++) {
            if (a[row + idx] != b[row + idx]) differ++;
            if (a[row + idx]) drawn++;
        }
        total += across;
    }
    printf("the display: %d cells compared, %d bytes differ from the reference VM's\n", total, differ);
    if (!drawn) printf("NOT CHECKED: nothing was drawn\n");
    return(differ || !total || !drawn);
}

int main(int argc, char *argv[]) {
    if (argc == 3) return(displays(argv[1], argv[2]));
    if (argc != 5) {
        fprintf(stderr, "Usage: %s PICTURE.tiff own|half MEMORY CONSOLE\n       %s MEMORY MEMORY\n",
                argv[0], argv[0]);
        return(1);
    }
    tiff picture = read_tiff(argv[1]);
    bool half = !strcmp(argv[2], "half");
    int w, h, stride;
    int* gray = grays_of(picture.bytes, picture.w, picture.h, picture.samples, half, &w, &h);
    uint8_t* black = calloc((size_t)w * h + 1, 1);
    dither(gray, w, h, black);
    uint64_t* want = cells_of(black, w, 0, 0, w, h, CELL_BITS, DISP_LEFT_LOW, DISP_INK_BLACK, &stride);
    int total = stride * h;

    size_t len, said_len;
    uint8_t* memory = (uint8_t*)read_all(argv[3], &len);
    char* said = read_all(argv[4], &said_len);
    static const char asked[] = "b @ . b 1+ @ . ";
    char* at = strstr(said, asked);
    char* end;
    if (at == NULL) {
        printf("NOT CHECKED: the machine was not asked where its bitmap is\n");
        return(1);
    }
    uint64_t bitmap = ucell(strtoll(at + strlen(asked), &end, 10));
    uint64_t row = ucell(strtoll(end, NULL, 10));
    if (row != (uint64_t)stride || (bitmap + total) * CELL_BYTES > len) {
        printf("NOT CHECKED: a bitmap of %d cells a row at cell %llu, where %d cells a row were wanted\n",
               (int)row, (unsigned long long)bitmap, stride);
        return(1);
    }

    int differ = 0;
    for(int idx=0; idx < total; idx++) {
        uint64_t got = 0;
        memcpy(&got, memory + (bitmap + idx) * CELL_BYTES, CELL_BYTES);
        if (got != want[idx]) differ++;
    }
    printf("%d by %d: %d cells of bitmap compared, %d differ\n", w, h, total, differ);
    return(differ || !total);
}
