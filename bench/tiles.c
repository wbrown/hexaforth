//
// tiles PICTURE.tiff own|half [OUT-DIRECTORY]
//
// Measures a way of storing a picture as tiles from a codebook.  The
// codebook is a set of small black and white tiles.  The picture is stored
// as the number of a tile for each position, and it is drawn the way text
// is drawn from a font, with a tile in place of a glyph.  This program
// measures whether that is small enough and looks enough like the dither,
// so that a choice can be made before any of it is written for the
// machine.
//
// The picture is dithered as the machine dithers it (test/dither.c).  The
// tile shapes tried are 16 by 1; 4 by 4, which is also one 16-bit cell;
// 2 by 2; 16 by 4; 16 by 8; and 32 by 4.  The codebook sizes tried are
// 256, 1,024 and 4,096 tiles (4 and 8 for the 2 by 2 tile).  For each
// shape and size, a codebook is made from the dither's own tiles by
// clustering them on the number of pixels that differ.  Each tile is
// assigned to the nearest tile in the codebook, and then each tile in the
// codebook is replaced by what most of the tiles assigned to it have at
// each pixel.  That is repeated ten times.  A codebook with more than half
// as many tiles as the picture has shows nothing, and is skipped.  The
// codebook is made from this picture, so it is as good as a clustered
// codebook can be for it.  One made from other pictures would do worse.
//
// Each size of codebook is also made by design, the same for every
// picture.  It has as many grays as the square root of its size, spaced
// evenly from black to white.  For each gray it has the dither's own
// texture at that many positions along a row: a field of that gray is
// dithered, and the tile at each successive pixel position, starting well
// inside the field, is taken.  So it has every gray at every phase of the
// texture the dither makes of it, which clustering loses.  It has nothing
// from a picture in it, so the machine could generate it at startup
// instead of storing it.  Its bytes are counted in the totals all the
// same.
//
// The picture is then made from the codebook in two ways:
//   plain     each tile of the dither is replaced by the nearest tile in
//             the codebook
//   diffused  the picture's grays are processed a tile at a time, as the
//             dither processes them a pixel at a time.  The tile chosen is
//             the nearest to what the dither would make of the grays and
//             the error owed at that position.  The error of each of its
//             pixels is then passed on to the pixels after it, as the
//             dither passes it on
//
// For each it prints:
//   - the bytes the picture takes (the codebook and the tile numbers),
//     compared with the bitmap's
//   - the share of its pixels that differ from the dither's
//   - its distance from the picture's grays when both are blurred over 5
//     by 5 pixels, which is closer to what an eye sees than the pixels are
// The first line is the dither itself, measured the same way.
//
// With OUT-DIRECTORY, each is written there as a PBM file (util/png
// converts one to a PNG): dither.pbm, and SHAPE-SIZE-HOW-plain.pbm and
// SHAPE-SIZE-HOW-diffused.pbm, where HOW is cluster or design.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../test/dither.h"
#include "tiff.h"

#define WORDS 4                         // a tile has at most 256 pixels (32 by 8), which is four 64-bit words

typedef struct { uint64_t w[WORDS]; } tile;

static int pw, ph;                      // the picture's width and height in pixels
static const int* gray;                 // the picture's grays
static uint8_t* atk;                    // the dithered picture, one byte per pixel: 1 is black

// Returns zeroed memory for n items of `size` bytes each.  Exits if there
// is not enough memory.
static void* room(size_t n, size_t size) {
    void* p = calloc(n ? n : 1, size);
    if (p == NULL) {
        fprintf(stderr, "there is no room for %zu of %zu bytes\n", n, size);
        exit(EXIT_FAILURE);
    }
    return(p);
}

// Returns the number of pixels that differ between two tiles.
static int distance(const tile* a, const tile* b) {
    int d = 0;
    for(int k=0; k < WORDS; k++) d += __builtin_popcountll(a->w[k] ^ b->w[k]);
    return(d);
}

// Returns the number of black pixels in a tile.
static int black_in(const tile* t) {
    int n = 0;
    for(int k=0; k < WORDS; k++) n += __builtin_popcountll(t->w[k]);
    return(n);
}

// bit() returns true if pixel i of a tile is black, and set() makes pixel
// i black.
static bool bit(const tile* t, int i) { return(t->w[i >> 6] >> (i & 63) & 1); }
static void set(tile* t, int i) { t->w[i >> 6] |= 1ull << (i & 63); }

// Returns the tile at tile column tx and tile row ty of a bitmap.  The
// tile's pixels are numbered row by row.
static tile tile_of(const uint8_t* black, int tw, int th, int tx, int ty) {
    tile t = { 0 };
    for(int y=0; y < th; y++) {
        for(int x=0; x < tw; x++) {
            int px = tx * tw + x, py = ty * th + y;
            if (px < pw && py < ph && black[py * pw + px]) set(&t, y * tw + x);
        }
    }
    return(t);
}

// Returns the index of the tile, in a codebook of k tiles, that differs
// from `t` in the fewest pixels.
static int nearest(const tile* t, const tile* book, int k) {
    int best = 0, d = 1 << 30;
    for(int c=0; c < k; c++) {
        int e = distance(t, &book[c]);
        if (e < d) d = e, best = c;
    }
    return(best);
}

static const tile* sorting;
static int by_black(const void* a, const void* b) {
    return(black_in(&sorting[*(const int*)a]) - black_in(&sorting[*(const int*)b]));
}

// Returns a codebook of k tiles, clustered from n tiles that have `bits`
// pixels each.
static tile* codebook(const tile* tiles, int n, int k, int bits) {
    tile* book = room(k, sizeof(tile));
    int* order = room(n, sizeof(int));
    for(int i=0; i < n; i++) order[i] = i;
    sorting = tiles;
    qsort(order, n, sizeof(int), by_black);
    for(int c=0; c < k; c++) book[c] = tiles[order[(long)c * n / k]];   // start with tiles spaced evenly from the lightest to the darkest
    free(order);
    int* of = room(n, sizeof(int));
    int* votes = room((size_t)k * bits, sizeof(int));
    int* members = room(k, sizeof(int));
    for(int round=0; round < 10; round++) {
        memset(votes, 0, (size_t)k * bits * sizeof(int));
        memset(members, 0, k * sizeof(int));
        for(int i=0; i < n; i++) {
            int c = of[i] = nearest(&tiles[i], book, k);
            members[c]++;
            for(int b=0; b < bits; b++) votes[c * bits + b] += bit(&tiles[i], b);
        }
        for(int c=0; c < k; c++) {
            if (!members[c]) continue;
            tile t = { 0 };
            for(int b=0; b < bits; b++) {
                int v = 2 * votes[c * bits + b];
                if (v > members[c] || (v == members[c] && bit(&book[c], b))) set(&t, b);
            }
            book[c] = t;
        }
    }
    free(of), free(votes), free(members);
    return(book);
}

// Returns a designed codebook of k tiles: `levels` grays, each at
// k / levels positions of the dither's texture.
static tile* designed(int k, int tw, int th) {
    int levels = 1;
    while (levels * levels < k) levels++;
    int phases = k / levels, fw = 64 + phases + tw, fh = 64 + th;
    tile* book = room(k, sizeof(tile));
    int* field = room((size_t)fw * fh, sizeof(int));
    uint8_t* made = room((size_t)fw * fh, 1);
    for(int level=0; level < levels; level++) {
        int g = ((2 * level + 1) * 255 + levels) / (2 * levels);
        for(int i=0; i < fw * fh; i++) field[i] = g;
        dither(field, fw, fh, made);
        for(int p=0; p < phases; p++) {
            tile t = { 0 };
            for(int y=0; y < th; y++) {
                for(int x=0; x < tw; x++) if (made[(64 + y) * fw + 64 + p + x]) set(&t, y * tw + x);
            }
            book[level * phases + p] = t;
        }
    }
    free(field), free(made);
    return(book);
}

// Copies a tile into a bitmap at tile column tx and tile row ty.
static void put(uint8_t* black, const tile* t, int tw, int th, int tx, int ty) {
    for(int y=0; y < th; y++) {
        for(int x=0; x < tw; x++) {
            int px = tx * tw + x, py = ty * th + y;
            if (px < pw && py < ph) black[py * pw + px] = bit(t, y * tw + x);
        }
    }
}

// The six pixels the dither passes an eighth of a pixel's error to, as
// offsets across and down.
static const int AHEAD[6][2] = { {1, 0}, {2, 0}, {-1, 1}, {0, 1}, {1, 1}, {0, 2} };

// Makes the picture from the codebook by processing its grays a tile at a
// time and passing the error on.
static void diffused(const tile* book, int k, int tw, int th, uint8_t* black) {
    int* owed = room((size_t)pw * ph, sizeof(int));
    int* here = room((size_t)tw * th, sizeof(int));
    for(int ty=0; ty * th < ph; ty++) {
        for(int tx=0; tx * tw < pw; tx++) {
            // Dither this tile on its own, from its grays and the error owed to it.
            tile want = { 0 };
            for(int y=0; y < th; y++) {
                for(int x=0; x < tw; x++) {
                    int px = tx * tw + x, py = ty * th + y;
                    here[y * tw + x] = px < pw && py < ph ? gray[py * pw + px] + owed[py * pw + px] : 255;
                }
            }
            for(int y=0; y < th; y++) {
                for(int x=0; x < tw; x++) {
                    int old = here[y * tw + x], new = old > 127 ? 255 : 0, e = (old - new) / 8;
                    if (!new) set(&want, y * tw + x);
                    for(int a=0; a < 6; a++) {
                        int nx = x + AHEAD[a][0], ny = y + AHEAD[a][1];
                        if (nx >= 0 && nx < tw && ny < th) here[ny * tw + nx] += e;
                    }
                }
            }
            const tile* got = &book[nearest(&want, book, k)];
            put(black, got, tw, th, tx, ty);
            // Pass on the error of each pixel of the chosen tile.  A share
            // that would go to a pixel of a tile that is already chosen
            // (one to the left in this band of tiles) goes instead to the
            // pixel in the same column just below the band, which is not
            // chosen yet, so that the share is not lost.
            for(int y=0; y < th; y++) {
                for(int x=0; x < tw; x++) {
                    int px = tx * tw + x, py = ty * th + y;
                    if (px >= pw || py >= ph) continue;
                    int old = gray[py * pw + px] + owed[py * pw + px];
                    int e = (old - (bit(got, y * tw + x) ? 0 : 255)) / 8;
                    for(int a=0; a < 6; a++) {
                        int nx = px + AHEAD[a][0], ny = py + AHEAD[a][1];
                        if (ny < (ty + 1) * th && nx < tx * tw) ny = (ty + 1) * th;
                        if (nx >= 0 && nx < pw && ny < ph) owed[ny * pw + nx] += e;
                    }
                }
            }
        }
    }
    free(owed), free(here);
}

// Returns the mean difference between a bitmap and the picture's grays
// when both are blurred over 5 by 5 pixels, out of 255.
static double blurred(const uint8_t* black) {
    double sum = 0;
    for(int y=0; y < ph; y++) {
        for(int x=0; x < pw; x++) {
            double g = 0, b = 0;
            int n = 0;
            for(int dy=-2; dy <= 2; dy++) {
                for(int dx=-2; dx <= 2; dx++) {
                    int px = x + dx, py = y + dy;
                    if (px < 0 || px >= pw || py < 0 || py >= ph) continue;
                    g += gray[py * pw + px], b += black[py * pw + px] ? 0 : 255, n++;
                }
            }
            sum += fabs(g - b) / n;
        }
    }
    return(sum / ((double)pw * ph));
}

// Returns the percentage of a bitmap's pixels that differ from the
// dither's.
static double differ(const uint8_t* black) {
    long n = 0;
    for(long i=0; i < (long)pw * ph; i++) n += black[i] != atk[i];
    return(100.0 * n / ((double)pw * ph));
}

// Writes a bitmap to DIR/NAME.pbm.  Does nothing if `dir` is NULL.
static void pbm(const char* dir, const char* name, const uint8_t* black) {
    if (!dir) return;
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s.pbm", dir, name);
    FILE* out = fopen(path, "wb");
    if (!out) { perror(path); exit(EXIT_FAILURE); }
    fprintf(out, "P4\n%d %d\n", pw, ph);
    for(int y=0; y < ph; y++) {
        for(int x=0; x < pw; x += 8) {
            int byte = 0;
            for(int b=0; b < 8; b++) byte |= (x + b < pw && black[y * pw + x + b]) << (7 - b);
            fputc(byte, out);
        }
    }
    fclose(out);
}

// Makes the picture from a codebook both ways, plain and diffused.  It
// prints one line of the table and writes each result as a picture.
static void try_book(const char* dir, const char* how, const tile* book, int k, const tile* tiles, int n,
                     int tw, int th, long bitmap, uint8_t* made) {
    int bits = 0, across = (pw + tw - 1) / tw;
    while ((1 << bits) < k) bits++;
    long book_bytes = (long)k * tw * th / 8, index_bytes = ((long)n * bits + 7) / 8;
    long total = book_bytes + index_bytes;
    char name[64];
    for(int i=0; i < n; i++) put(made, &book[nearest(&tiles[i], book, k)], tw, th, i % across, i / across);
    double plain_px = differ(made), plain_bl = blurred(made);
    snprintf(name, sizeof(name), "%dx%d-%d-%s-plain", tw, th, k, how);
    pbm(dir, name, made);
    diffused(book, k, tw, th, made);
    double diff_px = differ(made), diff_bl = blurred(made);
    snprintf(name, sizeof(name), "%dx%d-%d-%s-diffused", tw, th, k, how);
    pbm(dir, name, made);
    char shape[16];
    snprintf(shape, sizeof(shape), "%dx%d", tw, th);
    printf("%-7s %-7s %5d %6d %9ld %8ld %7ld %6.1fx  %8.1f%% %9.2f  %8.1f%% %9.2f\n", shape, how, k, n, book_bytes,
           index_bytes, total, (double)bitmap / total, plain_px, plain_bl, diff_px, diff_bl);
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "usage: tiles PICTURE.tiff own|half [OUT-DIRECTORY]\n");
        return(1);
    }
    const char* dir = argc > 3 ? argv[3] : NULL;
    tiff picture = read_tiff(argv[1]);
    gray = grays_of(picture.bytes, picture.w, picture.h, picture.samples, !strcmp(argv[2], "half"), &pw, &ph);
    atk = room((size_t)pw * ph, 1);
    dither(gray, pw, ph, atk);
    long bitmap = (long)pw * ph / 8;
    printf("%s at %s size: %d by %d, a bitmap of %ld bytes\n\n", argv[1], strcmp(argv[2], "half") ? "its own" : "half",
           pw, ph, bitmap);
    printf("%-7s %-7s %5s %6s %9s %8s %7s %7s  %9s %9s  %9s %9s\n", "tile", "book", "size", "tiles", "book B",
           "index B", "total B", "smaller", "plain px", "plain bl", "diff px", "diff bl");
    printf("%-7s %-7s %5s %6s %9s %8s %7ld %7s  %9s %9.2f\n", "dither", "-", "-", "-", "-", "-", bitmap, "1.0x",
           "0.0%", blurred(atk));
    pbm(dir, "dither", atk);

    // Each tile shape, and the codebook sizes it is tried with.  A 2 by 2
    // tile has four pixels, so there are only sixteen different tiles, and
    // a codebook saves space only if it has fewer.
    static const struct { int tw, th, books[3]; } SHAPES[] = {
        {16, 1, {256, 1024, 4096}}, {4, 4, {256, 1024, 4096}}, {2, 2, {4, 8, 0}},
        {16, 4, {256, 1024, 4096}}, {16, 8, {256, 1024, 4096}}, {32, 4, {256, 1024, 4096}} };
    uint8_t* made = room((size_t)pw * ph, 1);
    for(int s=0; s < (int)(sizeof(SHAPES) / sizeof(SHAPES[0])); s++) {
        int tw = SHAPES[s].tw, th = SHAPES[s].th, across = (pw + tw - 1) / tw, down = (ph + th - 1) / th;
        int n = across * down;
        tile* tiles = room(n, sizeof(tile));
        for(int ty=0; ty < down; ty++) for(int tx=0; tx < across; tx++) tiles[ty * across + tx] = tile_of(atk, tw, th, tx, ty);
        for(int b=0; b < 3; b++) {
            int k = SHAPES[s].books[b];
            if (!k || 2 * k > n) continue;
            tile* book = codebook(tiles, n, k, tw * th);
            try_book(dir, "cluster", book, k, tiles, n, tw, th, bitmap, made);
            free(book);
            book = designed(k, tw, th);
            try_book(dir, "design", book, k, tiles, n, tw, th, bitmap, made);
            free(book);
        }
        free(tiles);
    }
    printf("\npx: of the pixels, how many are not the dither's.  bl: how far from the grays, blurred, of 255.\n");
    return(0);
}
