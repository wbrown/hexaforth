#include <stdlib.h>
#include "dither.h"

int gray_of(const uint8_t* pixel, int samples) {
    if (samples == 3) return((77 * pixel[0] + 150 * pixel[1] + 29 * pixel[2]) >> 8);
    return(pixel[0]);
}

int* grays_of(const uint8_t* bytes, int w, int h, int samples, bool half, int* gw, int* gh) {
    *gw = half ? w / 2 : w;
    *gh = half ? h / 2 : h;
    int* gray = calloc((size_t)*gw * *gh + 1, sizeof(int));
    for(int y=0; y < *gh; y++) {
        for(int x=0; x < *gw; x++) {
            if (half) {
                // The average of four pixels' grays, rounded to the nearest.
                const uint8_t* over = bytes + ((size_t)2 * y * w + 2 * x) * samples;
                const uint8_t* under = over + (size_t)w * samples;
                gray[y * *gw + x] = (gray_of(over, samples) + gray_of(over + samples, samples) +
                                     gray_of(under, samples) + gray_of(under + samples, samples) + 2) >> 2;
            } else {
                gray[y * *gw + x] = gray_of(bytes + ((size_t)y * w + x) * samples, samples);
            }
        }
    }
    return(gray);
}

// A pixel's error is the difference between its value and the black or
// white it became.  An eighth of the error is passed to each of six
// neighbours: the two pixels after it, the three under it, and the one two
// rows under it.  The other two eighths are discarded.
void dither(const int* gray, int w, int h, uint8_t* black) {
    int across = w + 4;         // a row of errors has two extra entries at each end
    int* owed = calloc((size_t)across * (h + 2), sizeof(int));
    for(int y=0; y < h; y++) {
        for(int x=0; x < w; x++) {
            int* here = owed + y * across + x + 2;
            int old = gray[y * w + x] + *here;
            int new = old > 127 ? 255 : 0;
            int eighth = ((old - new + 1024) >> 3) - 128;   // rounds down, for a negative error too
            here[1] += eighth;
            here[2] += eighth;
            here[across - 1] += eighth;
            here[across] += eighth;
            here[across + 1] += eighth;
            here[2 * across] += eighth;
            black[y * w + x] = !new;
        }
    }
    free(owed);
}

uint64_t* cells_of(const uint8_t* black, int w, int px, int py, int pw, int ph,
                   int cell_bits, bool left_low, bool ink_black, int* stride) {
    uint64_t all = cell_bits == 64 ? ~0ull : (1ull << cell_bits) - 1;
    *stride = (pw + cell_bits - 1) / cell_bits;
    uint64_t* cells = calloc((size_t)*stride * ph + 1, sizeof(uint64_t));
    for(int y=0; y < ph; y++) {
        for(int c=0; c < *stride; c++) {
            uint64_t cell = 0;
            for(int bit=0; bit < cell_bits; bit++) {
                int x = px + c * cell_bits + bit;
                if (x < w && black[(py + y) * w + x]) {
                    cell |= 1ull << (left_low ? bit : cell_bits - 1 - bit);
                }
            }
            cells[y * *stride + c] = (ink_black ? cell : ~cell) & all;
        }
    }
    return(cells);
}
