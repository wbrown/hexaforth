//
// png SCREEN.pbm PICTURE.png
//
// A screen that the desktop's test left, as a PNG: what a page that shows
// the picture wants.  The screen is a PBM of bits, a row at a time, the
// leftmost pixel the highest bit of a byte and a set bit black
// (test/deskvm.c writes it so).  The PNG is of one bit a pixel too, in
// which a set bit is white.
//
// Nothing in it is compressed.  The rows go into it as they are, each
// after the byte that says it is given whole, in the blocks that zlib
// calls stored: so that nothing but this is needed to make one.
//
// It fails if the file is not such a PBM, or has fewer rows than it says.
//

#include <ctype.h>
#include "room.c"

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

// A number of a PBM's header, past the spaces and the comments before it.
static long header_number(const unsigned char* bytes, size_t len, size_t* at) {
    for(;;) {
        while (*at < len && isspace(bytes[*at])) (*at)++;
        if (*at >= len || bytes[*at] != '#') break;
        while (*at < len && bytes[*at] != '\n') (*at)++;
    }
    if (*at >= len || !isdigit(bytes[*at])) return(-1);
    long n = 0;
    while (*at < len && isdigit(bytes[*at])) n = n * 10 + (bytes[(*at)++] - '0');
    return(n);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "usage: png SCREEN.pbm PICTURE.png\n");
        return(1);
    }
    unsigned char* pbm;
    size_t len = file_bytes(argv[1], &pbm), at = 2;
    long across = -1, down = -1;
    if (len > 2 && pbm[0] == 'P' && pbm[1] == '4') {
        across = header_number(pbm, len, &at);
        down = header_number(pbm, len, &at);
    }
    size_t row = across > 0 ? (size_t)(across + 7) / 8 : 0;
    at++;                                               // the one space after the header
    if (across <= 0 || down <= 0 || at + row * (size_t)down > len) {
        fprintf(stderr, "%s is not a PBM of bits with all its rows\n", argv[1]);
        return(1);
    }

    // The rows, each after a byte that says nothing was done to it, and
    // its bits the other way up; the pixels a row has past its width are
    // left white.
    size_t raw_len = (row + 1) * (size_t)down;
    unsigned char* raw = room_for(raw_len, 1);
    for(long y=0; y < down; y++) {
        unsigned char* to = raw + (size_t)y * (row + 1) + 1;
        for(size_t x=0; x < row; x++) to[x] = (unsigned char)~pbm[at + (size_t)y * row + x];
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

    FILE* out = fopen(argv[2], "wb");
    if (!out) { perror(argv[2]); return(1); }
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, out);
    unsigned char header[13] = {0};
    be32(header, (uint32_t)across);
    be32(header + 4, (uint32_t)down);
    header[8] = 1;                                      // a bit a pixel, of gray
    chunk(out, "IHDR", header, sizeof(header));
    chunk(out, "IDAT", data, n);
    chunk(out, "IEND", NULL, 0);
    if (fclose(out)) { perror(argv[2]); return(1); }
    printf("%s: %ld by %ld\n", argv[2], across, down);
    return(0);
}
