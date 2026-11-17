//
// tiles PICTURE.tiff own|half [OUT-DIRECTORY]
//
// What a picture would be stored as a vocabulary of tiles: a codebook of
// small black and white tiles, and the picture as a tile's number in it for
// each place, which is drawn as a font is, a tile for a glyph.  Whether
// that is small enough, and looks enough like the dither, is what this
// measures, before anything of it is written for the machine.
//
// The picture is dithered as the machine dithers it (test/dither.c).  For
// each shape of tile (16 by 1; 4 by 4, which is a cell of 16 bits too;
// 2 by 2; 16 by 4, 16 by 8 and 32 by 4), and for each size of codebook,
// 256, 1,024 or 4,096 tiles (4 or 8 for 2 by 2), a codebook
// is made from the dither's own tiles by clustering them by how many pixels
// differ: each tile goes to the nearest of the codebook's, and each of the
// codebook's is made again of what most of its tiles have, ten times over.
// A codebook that is not fewer than half the tiles there are says nothing,
// and is left out.  It is made of this picture, and so is as good as a
// codebook made that way can be for it: one made of other pictures would
// do worse.
//
// Each size of codebook is also made by design, the same for every picture:
// of as many grays as the square root of its size, evenly from black to
// white, and of each the dither's own texture, at as many places along a
// row: a field of that gray is dithered, and the tile at each place across
// from a place well into it is taken.  So it has every gray at every phase
// of the texture the dither makes of it, which clustering loses.  Nothing
// of a picture is in it, and the machine could make it when it starts and
// not keep it; its bytes are counted all the same.
//
// The picture is then made of the codebook two ways:
//   plain     each tile of the dither becomes the codebook's nearest to it
//   diffused  the picture's grays are gone through a tile at a time, as the
//             dither goes through them a pixel at a time: the tile chosen is
//             the nearest to what the dither would make of what is owed
//             there, and what each of its pixels is out by is passed on to
//             the pixels after it as the dither passes it on
//
// For each it prints how many bytes the picture is (the codebook and the
// tiles' numbers, against the bitmap's), how many of its pixels are not the
// dither's, and how far it is from the picture's grays when both are
// blurred over 5 by 5 pixels, which is nearer what an eye sees than the
// pixels are.  The dither itself is the first line, measured the same way.
//
// With OUT-DIRECTORY, each is written there as a PBM (util/png makes a PNG
// of one): dither.pbm, and SHAPE-SIZE-HOW-plain.pbm and -diffused.pbm, HOW
// being cluster or design.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../test/dither.h"
#include "tiff.h"

#define WORDS 4                         // a tile is at most 256 pixels: 32 by 8

typedef struct { uint64_t w[WORDS]; } tile;

static int pw, ph;                      // the picture's size
static const int* gray;                 // its grays
static uint8_t* atk;                    // the dither: 1 is black

static void* room(size_t n, size_t size) {
    void* p = calloc(n ? n : 1, size);
    if (p == NULL) {
        fprintf(stderr, "there is no room for %zu of %zu bytes\n", n, size);
        exit(EXIT_FAILURE);
    }
    return(p);
}

static int distance(const tile* a, const tile* b) {
    int d = 0;
    for(int k=0; k < WORDS; k++) d += __builtin_popcountll(a->w[k] ^ b->w[k]);
    return(d);
}

static int black_in(const tile* t) {
    int n = 0;
    for(int k=0; k < WORDS; k++) n += __builtin_popcountll(t->w[k]);
    return(n);
}

static bool bit(const tile* t, int i) { return(t->w[i >> 6] >> (i & 63) & 1); }
static void set(tile* t, int i) { t->w[i >> 6] |= 1ull << (i & 63); }

// The tile at tile column tx, row ty of a bitmap, its pixels a row at a time.
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

// A codebook of k tiles, made of n.
static tile* codebook(const tile* tiles, int n, int k, int bits) {
    tile* book = room(k, sizeof(tile));
    int* order = room(n, sizeof(int));
    for(int i=0; i < n; i++) order[i] = i;
    sorting = tiles;
    qsort(order, n, sizeof(int), by_black);
    for(int c=0; c < k; c++) book[c] = tiles[order[(long)c * n / k]];   // from the lightest to the darkest
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

// A codebook of k tiles by design: levels grays, each at k / levels places.
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

static void put(uint8_t* black, const tile* t, int tw, int th, int tx, int ty) {
    for(int y=0; y < th; y++) {
        for(int x=0; x < tw; x++) {
            int px = tx * tw + x, py = ty * th + y;
            if (px < pw && py < ph) black[py * pw + px] = bit(t, y * tw + x);
        }
    }
}

// The dither's six to pass an eighth to, from a pixel: across and down.
static const int AHEAD[6][2] = { {1, 0}, {2, 0}, {-1, 1}, {0, 1}, {1, 1}, {0, 2} };

// The picture made of the codebook, its grays gone through a tile at a time
// with what is owed passed on.
static void diffused(const tile* book, int k, int tw, int th, uint8_t* black) {
    int* owed = room((size_t)pw * ph, sizeof(int));
    int* here = room((size_t)tw * th, sizeof(int));
    for(int ty=0; ty * th < ph; ty++) {
        for(int tx=0; tx * tw < pw; tx++) {
            // What the dither would make of this tile, on its own, from what is owed it.
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
            // What each pixel of the tile chosen is out by, passed on.  A
            // share for a pixel of a tile that is already chosen, on the
            // left in this band, goes to the pixel under the band in its
            // column, which is not, rather than being lost.
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

// How far a bitmap is from the grays, both blurred over 5 by 5: the mean, of
// 255.
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

static double differ(const uint8_t* black) {
    long n = 0;
    for(long i=0; i < (long)pw * ph; i++) n += black[i] != atk[i];
    return(100.0 * n / ((double)pw * ph));
}

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

// The picture made of a codebook, plain and diffused: a line of the table,
// and each written as a picture.
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

    // A shape, and the sizes of codebook it is tried with: a 2 by 2 tile is
    // four pixels, which sixteen tiles are all of, so it is smaller only
    // with fewer.
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
