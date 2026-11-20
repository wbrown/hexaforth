//
// png SCREEN.pbm PICTURE.png
//
// Converts a screen saved by the desktop's test to a PNG, for a page that
// shows it.  The screen is a binary PBM (test/deskvm.c writes it), and
// png_file.h writes the PNG.
//
// It fails if the file is not a binary PBM, or has fewer rows than it says.
//

#include <ctype.h>
#include "png_file.h"

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
    if (!png_bits(argv[2], pbm + at, across, down)) return(1);
    printf("%s: %ld by %ld\n", argv[2], across, down);
    return(0);
}
