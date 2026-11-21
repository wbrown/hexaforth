//
// Writes a bitmap as a PNG.  The bitmap is laid out as a PBM's pixels are:
// rows of whole bytes, the leftmost pixel in the highest bit, a set bit
// black.  The PNG is one bit per pixel, where a set bit is white, so each
// byte is inverted.
//
// Nothing is compressed: the rows go into zlib's "stored" blocks as they
// are, so no library is needed.
//
// util/png.c uses this to convert a PBM; a tool with a bitmap of its own
// includes this file.
//

#ifndef HEXAFORTH_PNG_FILE_H
#define HEXAFORTH_PNG_FILE_H

#include <stdbool.h>
#include "room.h"

static void be32(unsigned char* to, uint32_t x) {
    to[0] = x >> 24, to[1] = x >> 16, to[2] = x >> 8, to[3] = x;
}

// The CRC a chunk of a PNG ends with, of some bytes after what came before.
static uint32_t crc_of(uint32_t crc, const unsigned char* bytes, size_t n) {
    crc = ~crc;
    for(size_t idx=0; idx < n; idx++) {
        crc ^= bytes[idx];
        for(int bit=0; bit < 8; bit++) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
    }
    return(~crc);
}

// A chunk: how long, what it is, the bytes, and the CRC of the last two.
static void chunk(FILE* out, const char* kind, const unsigned char* bytes, size_t n) {
    unsigned char four[4];
    be32(four, (uint32_t)n);
    fwrite(four, 1, 4, out);
    fwrite(kind, 1, 4, out);
    fwrite(bytes, 1, n, out);
    be32(four, crc_of(crc_of(0, (const unsigned char*)kind, 4), bytes, n));
    fwrite(four, 1, 4, out);
}

// Write `down` rows of `across` pixels to `path` as a PNG.  Returns false,
// after printing why, if the file could not be written.
static bool png_bits(const char* path, const unsigned char* rows, long across, long down) {
    size_t row = (size_t)(across + 7) / 8;

    // The rows, each after a byte that says nothing was done to it, and
    // its bits the other way up; the pixels a row has past its width are
    // left white.
    size_t raw_len = (row + 1) * (size_t)down;
    unsigned char* raw = room_for(raw_len, 1);
    for(long y=0; y < down; y++) {
        unsigned char* to = raw + (size_t)y * (row + 1) + 1;
        for(size_t x=0; x < row; x++) to[x] = (unsigned char)~rows[(size_t)y * row + x];
    }

    // Those as zlib has them: its two bytes, the rows in stored blocks of
    // as many bytes as a block's length can say, and the sum it ends with.
    const size_t most = 65535;
    size_t blocks = (raw_len + most - 1) / most;
    unsigned char* data = room_for(2 + raw_len + 5 * blocks + 4, 1);
    size_t n = 0;
    data[n++] = 0x78, data[n++] = 0x01;
    for(size_t from=0; from < raw_len; from += most) {
        size_t this = raw_len - from < most ? raw_len - from : most;
        data[n++] = from + this == raw_len;             // the last block says so
        data[n++] = this & 0xff, data[n++] = this >> 8;
        data[n++] = ~this & 0xff, data[n++] = (~this >> 8) & 0xff;
        memcpy(data + n, raw + from, this);
        n += this;
    }
    uint32_t a = 1, b = 0;                              // Adler-32 of the rows
    for(size_t idx=0; idx < raw_len; idx++) a = (a + raw[idx]) % 65521, b = (b + a) % 65521;
    be32(data + n, (b << 16) | a);
    n += 4;

    FILE* out = fopen(path, "wb");
    if (!out) { perror(path); free(raw), free(data); return(false); }
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, out);
    unsigned char header[13] = {0};
    be32(header, (uint32_t)across);
    be32(header + 4, (uint32_t)down);
    header[8] = 1;                                      // a bit a pixel, of gray
    chunk(out, "IHDR", header, sizeof(header));
    chunk(out, "IDAT", data, n);
    chunk(out, "IEND", NULL, 0);
    free(raw), free(data);
    if (fclose(out)) { perror(path); return(false); }
    return(true);
}

#endif // HEXAFORTH_PNG_FILE_H
