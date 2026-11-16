#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tiff.h"

// A TIFF's numbers are in the order its first two bytes say.
static const uint8_t* file;
static int big;

static unsigned short2(long at) {
    return(big ? file[at] << 8 | file[at + 1] : file[at + 1] << 8 | file[at]);
}

static unsigned long4(long at) {
    return(big ? short2(at) << 16 | short2(at + 2) : short2(at + 2) << 16 | short2(at));
}

// The nth value of a directory entry, which is in the entry if it fits and
// otherwise where the entry says.
static unsigned value(long entry, unsigned n) {
    unsigned type = short2(entry + 2);
    unsigned size = type == 3 ? 2 : type == 4 ? 4 : 1;
    long at = size * long4(entry + 4) <= 4 ? entry + 8 : (long)long4(entry + 8);
    at += size * n;
    return(size == 2 ? short2(at) : size == 4 ? long4(at) : file[at]);
}

tiff read_tiff(const char* path) {
    FILE* in = fopen(path, "rb");
    if (in == NULL) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    fseek(in, 0, SEEK_END);
    size_t len = ftell(in);
    fseek(in, 0, SEEK_SET);
    uint8_t* all = malloc(len + 1);
    if (fread(all, 1, len, in) != len) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    fclose(in);

    file = all;
    big = file[0] == 'M';
    long dir = long4(4);
    tiff t = { 0, 0, 1, NULL };
    unsigned compression = 1, rows_a_strip = 0;
    long strips = 0;
    for(unsigned idx=0; idx < short2(dir); idx++) {
        long entry = dir + 2 + 12 * idx;
        switch (short2(entry)) {
            case 256: t.w = value(entry, 0); break;
            case 257: t.h = value(entry, 0); break;
            case 259: compression = value(entry, 0); break;
            case 273: strips = entry; break;
            case 277: t.samples = value(entry, 0); break;
            case 278: rows_a_strip = value(entry, 0); break;
        }
    }
    if (!t.w || !t.h || !strips || compression != 1 || (t.samples != 1 && t.samples != 3)) {
        fprintf(stderr, "%s: %d by %d, %d samples, compression %u: not a picture this reads\n",
                path, t.w, t.h, t.samples, compression);
        exit(EXIT_FAILURE);
    }
    if (!rows_a_strip || rows_a_strip > (unsigned)t.h) rows_a_strip = t.h;

    size_t row = (size_t)t.w * t.samples;
    t.bytes = malloc(row * t.h);
    for(int y=0; y < t.h; y++) {
        size_t from = value(strips, y / rows_a_strip) + (y % rows_a_strip) * row;
        if (from + row > len) {
            fprintf(stderr, "%s: row %d is past the end of the file\n", path, y);
            exit(EXIT_FAILURE);
        }
        memcpy(t.bytes + y * row, file + from, row);
    }
    free(all);
    return(t);
}

// A directory entry of one value: a SHORT (type 3) or a LONG (type 4).
static uint8_t* entry(uint8_t* at, unsigned tag, unsigned type, unsigned value) {
    unsigned fields[6] = { tag, type, 1, 0, value & 0xffff, type == 4 ? value >> 16 : 0 };
    for(int idx=0; idx < 6; idx++) {
        *at++ = fields[idx];
        *at++ = fields[idx] >> 8;
    }
    return(at);
}

#define HEAD 1024       // the grays start a block in, which is where a cell of any size starts

uint8_t* gray_tiff(const uint8_t* grays, int w, int h, size_t* len) {
    size_t pixels = (size_t)w * h;
    *len = HEAD + pixels;
    uint8_t* out = calloc(*len, 1);
    uint8_t* at = out;
    memcpy(at, "II*\0\10\0\0\0", 8);            // the low byte first; the directory is at byte 8
    at += 8;
    *at++ = 9;                                  // entries
    *at++ = 0;
    at = entry(at, 256, 3, w);                  // width
    at = entry(at, 257, 3, h);                  // height
    at = entry(at, 258, 3, 8);                  // bits in a sample
    at = entry(at, 259, 3, 1);                  // not compressed
    at = entry(at, 262, 3, 1);                  // black is nothing
    at = entry(at, 273, 4, HEAD);               // where the one strip is
    at = entry(at, 277, 3, 1);                  // samples to a pixel
    at = entry(at, 278, 3, h);                  // rows in the strip
    at = entry(at, 279, 4, pixels);             // and its bytes
    memcpy(out + HEAD, grays, pixels);
    return(out);
}
