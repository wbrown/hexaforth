//
// region_test IMAGE.hex [SEED]
//
// Regions (forth/region.fs), against their pixels.
//
// A set of pixels has one form as a region: a count of the cells that
// follow, a band that ends at its top, a band for each run of rows whose
// crossings are the same, and a last band that ends at NEVER.  So a region
// the machine has made is right if every cell of it is that form of the
// pixels it should hold, and wrong if any is not.
//
// It is a contrastive test.  What is typed at the machine is read here
// too, a word at a time, and done to pixels: a region is a plane of them,
// a rectangle is each of its pixels set, and what is in both of two
// regions is each pixel that is in both.  Nothing is built from that, and
// nothing in it is a band or a crossing until the form is made of the
// pixels to be compared.
//
// The machine is the nucleus on vm.c, typed at (nucleus.h).  A check is a
// label and what is typed: rectangles made of the regions r0 to r7,
// combined, moved and copied.  When it has been typed every one of the
// eight regions is read out of the machine's memory and compared with the
// form of its pixels, and the region that was last made is asked whether
// it is empty (rgn-empty?) and for the rectangle around it (rgn-box).
//
// After the checks that are said there are steps by chance, the same every
// time for a seed, each typed at the machine as the step before left it:
// so regions are made of regions that were made of regions, as the windows
// make them.  A region that would not fit where the machine keeps one is
// made nothing instead: nothing in the machine checks that one fits.
//
// It says how many checks and steps it made, and how many regions and
// cells it compared.  It fails if that is none: then it found nothing to
// test.
//

#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "nucleus.h"

#define REGIONS 8               // r0 to r7
#define FROM    (-512)          // the plane the pixels are on: from here, across and down,
#define ACROSS  2048            // and this many
#define PLANE   ((size_t)ACROSS * ACROSS)
#define STEPS   400             // how many are taken by chance
#define UNDER   12345           // what is under what a check leaves

typedef struct {
    const char* label;
    const char* typed;
} check;

// Two rectangles, as r0 and r1; a frame, which is r2; four rectangles side
// by side, which are r2; and four down a diagonal, which are r2.
#define CORNER  "0 0 20 20 r0 rect-rgn  10 10 20 20 r1 rect-rgn\n"
#define APART   "0 0 20 20 r0 rect-rgn  50 60 20 20 r1 rect-rgn\n"
#define BESIDE  "0 0 20 20 r0 rect-rgn  50 0 20 20 r1 rect-rgn\n"
#define ACROSS_EDGE "0 0 20 20 r0 rect-rgn  20 0 20 20 r1 rect-rgn\n"
#define DOWN_EDGE   "0 0 20 20 r0 rect-rgn  0 20 20 20 r1 rect-rgn\n"
#define WITHIN  "0 0 60 60 r0 rect-rgn  20 20 20 20 r1 rect-rgn\n"
#define SAME    "5 6 30 40 r0 rect-rgn  5 6 30 40 r1 rect-rgn\n"
#define ONE     "5 6 30 40 r0 rect-rgn\n"
#define FRAME   WITHIN "r0 r1 r2 rgn-diff\n"
#define BAR     "10 25 40 10 r3 rect-rgn\n"
#define TEETH   "0 0 5 30 r0 rect-rgn  10 0 5 30 r1 rect-rgn  r0 r1 r2 rgn-or\n" \
                "20 0 5 30 r0 rect-rgn  r2 r0 r3 rgn-or  30 0 5 30 r0 rect-rgn  r3 r0 r2 rgn-or\n"
#define STAIRS  "0 0 10 10 r0 rect-rgn  5 5 10 10 r1 rect-rgn  r0 r1 r2 rgn-or\n" \
                "10 10 10 10 r0 rect-rgn  r2 r0 r3 rgn-or  15 15 10 10 r0 rect-rgn  r3 r0 r2 rgn-or\n"

static check SAID[] = {
    // A rectangle, and nothing.
    {"a rectangle", "10 20 30 40 r0 rect-rgn"},
    {"a rectangle of one pixel", "7 9 1 1 r0 rect-rgn"},
    {"a rectangle with no width is nothing", "10 20 0 40 r0 rect-rgn"},
    {"a rectangle with no height is nothing", "10 20 30 0 r0 rect-rgn"},
    {"a rectangle with less than no width is nothing", "10 20 -2 40 r0 rect-rgn"},
    {"a rectangle with less than no height is nothing", "10 20 30 -2 r0 rect-rgn"},
    {"a rectangle that starts left of nothing and above it", "-30 -20 50 40 r0 rect-rgn"},
    {"a rectangle that is all left of nothing and above it", "-300 -200 50 40 r0 rect-rgn"},
    {"a region made another rectangle", "0 0 9 9 r0 rect-rgn  30 30 5 5 r0 rect-rgn"},
    {"a region made nothing", "0 0 9 9 r0 rect-rgn  r0 rgn-none"},
    {"a copy of a rectangle", ONE "r0 r1 rgn-copy"},
    {"a copy of nothing, over a rectangle", ONE "r1 r0 rgn-copy"},
    {"a rectangle moved", ONE "10 20 r0 rgn-offset"},
    {"a rectangle moved left of nothing and above it", ONE "-100 -200 r0 rgn-offset"},
    {"nothing moved", "10 20 r0 rgn-offset"},

    // Two rectangles: most of what is asked of them looks at no band.
    {"what two rectangles share", CORNER "r0 r1 r2 rgn-and"},
    {"what is in either of two rectangles", CORNER "r0 r1 r2 rgn-or"},
    {"what is in one rectangle and not in another", CORNER "r0 r1 r2 rgn-diff"},
    {"and in the other and not in the one", CORNER "r1 r0 r2 rgn-diff"},
    {"two rectangles apart: what they share is nothing", APART "r0 r1 r2 rgn-and"},
    {"two rectangles apart, joined", APART "r0 r1 r2 rgn-or"},
    {"two rectangles apart, and the other way about", APART "r1 r0 r2 rgn-or"},
    {"two rectangles apart: one less the other is the one", APART "r0 r1 r2 rgn-diff"},
    {"two rectangles in the same rows: what they share is nothing", BESIDE "r0 r1 r2 rgn-and"},
    {"two rectangles in the same rows, joined", BESIDE "r0 r1 r2 rgn-or"},
    {"two rectangles in the same rows: one less the other", BESIDE "r0 r1 r2 rgn-diff"},
    {"two rectangles with an edge between them, joined, are a rectangle", ACROSS_EDGE "r0 r1 r2 rgn-or"},
    {"two rectangles with an edge between them share nothing", ACROSS_EDGE "r0 r1 r2 rgn-and"},
    {"two rectangles with an edge between them: one less the other", ACROSS_EDGE "r0 r1 r2 rgn-diff"},
    {"one rectangle on another, joined, is a rectangle", DOWN_EDGE "r0 r1 r2 rgn-or"},
    {"one rectangle on another shares nothing with it", DOWN_EDGE "r0 r1 r2 rgn-and"},
    {"one rectangle on another, less it", DOWN_EDGE "r0 r1 r2 rgn-diff"},
    {"a rectangle within another: what they share", WITHIN "r0 r1 r2 rgn-and"},
    {"a rectangle within another, joined", WITHIN "r0 r1 r2 rgn-or"},
    {"a rectangle less one within it is a frame", WITHIN "r0 r1 r2 rgn-diff"},
    {"a rectangle less one it is within is nothing", WITHIN "r1 r0 r2 rgn-diff"},
    {"a rectangle and the same rectangle: what they share", SAME "r0 r1 r2 rgn-and"},
    {"a rectangle and the same rectangle, joined", SAME "r0 r1 r2 rgn-or"},
    {"a rectangle less the same rectangle is nothing", SAME "r0 r1 r2 rgn-diff"},
    {"a rectangle and itself: what they share", ONE "r0 r0 r2 rgn-and"},
    {"a rectangle and itself, joined", ONE "r0 r0 r2 rgn-or"},
    {"a rectangle less itself is nothing", ONE "r0 r0 r2 rgn-diff"},

    // Nothing, and something.
    {"what a rectangle and nothing share", ONE "r0 r1 r2 rgn-and"},
    {"what nothing and a rectangle share", ONE "r1 r0 r2 rgn-and"},
    {"a rectangle and nothing, joined", ONE "r0 r1 r2 rgn-or"},
    {"nothing and a rectangle, joined", ONE "r1 r0 r2 rgn-or"},
    {"a rectangle less nothing", ONE "r0 r1 r2 rgn-diff"},
    {"nothing less a rectangle", ONE "r1 r0 r2 rgn-diff"},
    {"what nothing and nothing share, over a rectangle", ONE "r1 r2 r0 rgn-and"},
    {"nothing and nothing joined, over a rectangle", ONE "r1 r2 r0 rgn-or"},
    {"nothing less nothing, over a rectangle", ONE "r1 r2 r0 rgn-diff"},

    // What is not a rectangle: its bands are looked at.
    {"a frame and a bar across its hole, joined", FRAME BAR "r2 r3 r4 rgn-or"},
    {"a bar and a frame, joined", FRAME BAR "r3 r2 r4 rgn-or"},
    {"what a frame and a bar across its hole share", FRAME BAR "r2 r3 r4 rgn-and"},
    {"what a bar and a frame share", FRAME BAR "r3 r2 r4 rgn-and"},
    {"a frame less a bar across its hole", FRAME BAR "r2 r3 r4 rgn-diff"},
    {"a bar less a frame", FRAME BAR "r3 r2 r4 rgn-diff"},
    {"a frame and its hole, joined, are the rectangle again", FRAME "r2 r1 r4 rgn-or"},
    {"a frame shares nothing with its hole", FRAME "r2 r1 r4 rgn-and"},
    {"a frame less its hole is the frame", FRAME "r2 r1 r4 rgn-diff"},
    {"a frame less the rectangle it was made of is nothing", FRAME "r2 r0 r4 rgn-diff"},
    {"a frame and itself: what they share", FRAME "r2 r2 r4 rgn-and"},
    {"a frame and itself, joined", FRAME "r2 r2 r4 rgn-or"},
    {"a frame less itself is nothing", FRAME "r2 r2 r4 rgn-diff"},
    {"what a frame and nothing share", FRAME "r2 r5 r4 rgn-and"},
    {"nothing and a frame, joined", FRAME "r5 r2 r4 rgn-or"},
    {"a frame less nothing", FRAME "r2 r5 r4 rgn-diff"},
    {"a copy of a frame", FRAME "r2 r4 rgn-copy"},
    {"a copy of a rectangle, over a frame", FRAME "r1 r2 rgn-copy"},
    {"a frame made nothing", FRAME "r2 rgn-none"},
    {"a frame made a rectangle", FRAME "1 2 3 4 r2 rect-rgn"},
    {"a frame moved", FRAME "7 -9 r2 rgn-offset"},
    {"a frame moved by nothing", FRAME "0 0 r2 rgn-offset"},
    {"a frame moved left of nothing and above it", FRAME "-100 -100 r2 rgn-offset"},
    {"a frame and the frame moved, joined", FRAME "r2 r4 rgn-copy  5 5 r4 rgn-offset  r2 r4 r5 rgn-or"},
    {"what a frame and the frame moved share", FRAME "r2 r4 rgn-copy  5 5 r4 rgn-offset  r2 r4 r5 rgn-and"},
    {"a frame less the frame moved", FRAME "r2 r4 rgn-copy  5 5 r4 rgn-offset  r2 r4 r5 rgn-diff"},
    {"a frame less the frame moved across only", FRAME "r2 r4 rgn-copy  5 0 r4 rgn-offset  r2 r4 r5 rgn-diff"},
    {"a frame less the frame moved down only", FRAME "r2 r4 rgn-copy  0 5 r4 rgn-offset  r2 r4 r5 rgn-diff"},
    {"what is in one of two rectangles and not in both",
     CORNER "r0 r1 r2 rgn-diff  r1 r0 r3 rgn-diff  r2 r3 r4 rgn-or"},
    {"two rectangles joined, less what they share",
     CORNER "r0 r1 r2 rgn-or  r0 r1 r3 rgn-and  r2 r3 r4 rgn-diff"},

    // Many crossings to a band, and many bands.
    {"four rectangles side by side", TEETH "r2 r4 rgn-copy"},
    {"four rectangles side by side and a bar under them: a comb", TEETH "0 30 35 5 r0 rect-rgn  r2 r0 r4 rgn-or"},
    {"four rectangles side by side and a bar across them", TEETH "-5 10 50 5 r0 rect-rgn  r2 r0 r4 rgn-or"},
    {"what four rectangles side by side and a bar across them share",
     TEETH "-5 10 50 5 r0 rect-rgn  r2 r0 r4 rgn-and"},
    {"four rectangles side by side, less a bar across them", TEETH "-5 10 50 5 r0 rect-rgn  r2 r0 r4 rgn-diff"},
    {"a bar less four rectangles across it", TEETH "-5 10 50 5 r0 rect-rgn  r0 r2 r4 rgn-diff"},
    {"four side by side, and the gaps between them, are a rectangle",
     TEETH "0 0 35 30 r0 rect-rgn  r0 r2 r4 rgn-diff  r2 r4 r5 rgn-or"},
    {"four side by side and the same moved to where each ends",
     TEETH "r2 r4 rgn-copy  5 0 r4 rgn-offset  r2 r4 r5 rgn-or"},
    {"what four side by side and the same moved a pixel share",
     TEETH "r2 r4 rgn-copy  1 0 r4 rgn-offset  r2 r4 r5 rgn-and"},
    {"four rectangles down a diagonal: stairs", STAIRS "r2 r4 rgn-copy"},
    {"stairs and the stairs moved across, joined", STAIRS "r2 r4 rgn-copy  3 0 r4 rgn-offset  r2 r4 r5 rgn-or"},
    {"what stairs and the stairs moved down share", STAIRS "r2 r4 rgn-copy  0 3 r4 rgn-offset  r2 r4 r5 rgn-and"},
    {"stairs less the stairs moved", STAIRS "r2 r4 rgn-copy  -2 3 r4 rgn-offset  r2 r4 r5 rgn-diff"},
    {"stairs less a rectangle in the middle of them", STAIRS "8 8 9 9 r0 rect-rgn  r2 r0 r4 rgn-diff"},
    {"what stairs and four side by side share",
     STAIRS "r2 r6 rgn-copy\n" TEETH "r6 r2 r4 rgn-and"},
    {"stairs and four side by side, joined",
     STAIRS "r2 r6 rgn-copy\n" TEETH "r6 r2 r4 rgn-or"},
    {"stairs less four side by side",
     STAIRS "r2 r6 rgn-copy\n" TEETH "r6 r2 r4 rgn-diff"},
    {""}};

// ---- the pixels

static uint8_t* plane[REGIONS]; // a region's pixels: one for each of the plane, 1 if it is in the region
static uint8_t* moved;          // and a plane to move one into

// A pixel of a plane.  One that is not on the plane is this test's own
// mistake, and it stops.
static uint8_t* pixel(uint8_t* p, int x, int y) {
    if (x < FROM || x >= FROM + ACROSS || y < FROM || y >= FROM + ACROSS) {
        fprintf(stderr, "region_test: the pixel at %d, %d is not on the plane\n", x, y);
        exit(EXIT_FAILURE);
    }
    return(p + (size_t)(y - FROM) * ACROSS + (x - FROM));
}

static void rectangle(int x, int y, int w, int h, int r) {
    memset(plane[r], 0, PLANE);
    for(int yy=y; yy < y + h; yy++) {
        for(int xx=x; xx < x + w; xx++) *pixel(plane[r], xx, yy) = 1;
    }
}

// What is in both, in either, or in the one and not the other.
static void combined(int a, int b, int to, const char* how) {
    for(size_t idx=0; idx < PLANE; idx++) {
        bool in_a = plane[a][idx], in_b = plane[b][idx];
        plane[to][idx] = !strcmp(how, "rgn-and") ? in_a && in_b :
                         !strcmp(how, "rgn-or") ? in_a || in_b : in_a && !in_b;
    }
}

static void move(int dx, int dy, int r) {
    memset(moved, 0, PLANE);
    for(int y=FROM; y < FROM + ACROSS; y++) {
        for(int x=FROM; x < FROM + ACROSS; x++) {
            if (*pixel(plane[r], x, y)) *pixel(moved, x + dx, y + dy) = 1;
        }
    }
    uint8_t* was = plane[r];
    plane[r] = moved;
    moved = was;
}

// ---- the one form of a region's pixels

static int64_t AS_END;          // what a row is added to, to be the end of a band,
static int64_t NEVER;           // and where the last band ends: as the machine has them
static uint64_t room;           // the cells the machine gives a region

typedef struct {
    int64_t* cells;             // the count, and the bands: as many cells as the region is,
    int count, cells_room;      // how many that is, the count one of them, and how many there is room for
    int widest;                 // the most crossings a band of it has
    bool empty;                 // nothing is in it; or the rectangle around it is
    int x, y, w, h;
} form;

static form forms[REGIONS];     // of each region's pixels
static bool stale[REGIONS];     // the pixels have changed since it was made

// Room for a form to have so many cells: twice as many as it has room for,
// if that is more.
static void cells_for(form* f, int count) {
    if (count <= f->cells_room) return;
    f->cells_room = count > 2 * f->cells_room ? count : 2 * f->cells_room;
    f->cells = realloc(f->cells, f->cells_room * sizeof(int64_t));
    if (f->cells == NULL) {
        fprintf(stderr, "region_test: there is no room for a region of %d cells\n", count);
        exit(EXIT_FAILURE);
    }
}

static void cell(form* f, int64_t x) {
    cells_for(f, f->count + 1);
    f->cells[f->count++] = x;
}

static void form_of(int r) {
    static int cross[ACROSS + 1], last[ACROSS + 1];
    form* f = &forms[r];
    const uint8_t* p = plane[r];
    int top = INT_MAX, bottom = INT_MIN, left = INT_MAX, right = INT_MIN;
    for(int y=FROM; y < FROM + ACROSS; y++) {
        if (memchr(p + (size_t)(y - FROM) * ACROSS, 1, ACROSS)) {
            if (top == INT_MAX) top = y;
            bottom = y + 1;
        }
    }
    cells_for(f, 1);
    f->count = 1;               // the count is the first cell, and is known last
    f->widest = 0;
    f->empty = top == INT_MAX;
    if (!f->empty) {
        cell(f, 0);
        cell(f, top + AS_END);
        int last_n = -1;
        for(int y=top; y < bottom; y++) {
            // Where along the row it goes from outside to inside and back.
            const uint8_t* row = p + (size_t)(y - FROM) * ACROSS;
            int n = 0;
            bool was = false;
            for(int x=0; x < ACROSS; x++) {
                if ((bool)row[x] != was) cross[n++] = FROM + x;
                was = row[x];
            }
            if (was) cross[n++] = FROM + ACROSS;
            if (n == last_n && !memcmp(cross, last, n * sizeof(int))) {
                f->cells[f->count - 1] = y + 1 + AS_END;    // the band above goes on down
                continue;
            }
            cell(f, n);
            for(int idx=0; idx < n; idx++) cell(f, cross[idx]);
            cell(f, y + 1 + AS_END);
            memcpy(last, cross, n * sizeof(int));
            last_n = n;
            if (n > f->widest) f->widest = n;
            if (n && cross[0] < left) left = cross[0];
            if (n && cross[n - 1] > right) right = cross[n - 1];
        }
    }
    cell(f, 0);
    cell(f, NEVER);
    f->cells[0] = f->count - 1;
    f->x = left;
    f->y = top;
    f->w = right - left;
    f->h = bottom - top;
    stale[r] = false;
}

// ---- what is typed, done to the pixels

static int made;                // the region the last word was done to

// A word at a time: a number, a region, or one of region.fs's words, which
// takes what that word takes.
static void pixels_do(const char* typed) {
    int64_t stack[16];
    int sp = 0;
    for(const char* at = typed; ; ) {
        while (isspace((unsigned char)*at)) at++;
        if (!*at) break;
        char word[32];
        size_t len = strcspn(at, " \n");
        if (len >= sizeof(word)) len = sizeof(word) - 1;
        memcpy(word, at, len);
        word[len] = '\0';
        at += len;

        char* end;
        long n = strtol(word, &end, 10);
        int takes = !*end || (word[0] == 'r' && isdigit((unsigned char)word[1]) && !word[2]) ? 0 :
                    !strcmp(word, "rect-rgn") ? 5 :
                    !strcmp(word, "rgn-and") || !strcmp(word, "rgn-or") || !strcmp(word, "rgn-diff") ||
                    !strcmp(word, "rgn-offset") ? 3 :
                    !strcmp(word, "rgn-copy") ? 2 :
                    !strcmp(word, "rgn-none") ? 1 : -1;
        if (takes < 0 || sp < takes || (!takes && sp == 16)) {
            fprintf(stderr, "region_test: %s is typed where it cannot be done to pixels, in:\n%s\n", word, typed);
            exit(EXIT_FAILURE);
        }
        if (!takes) {
            stack[sp++] = *end ? word[1] - '0' : n;
            continue;
        }
        sp -= takes;
        const int64_t* s = stack + sp;
        made = (int)s[takes - 1];
        if (made < 0 || made >= REGIONS) {
            fprintf(stderr, "region_test: %s is done to what is no region, in:\n%s\n", word, typed);
            exit(EXIT_FAILURE);
        }
        if (takes == 5) rectangle((int)s[0], (int)s[1], (int)s[2], (int)s[3], made);
        else if (!strcmp(word, "rgn-offset")) move((int)s[0], (int)s[1], made);
        else if (takes == 3) combined((int)s[0], (int)s[1], made, word);
        else if (takes == 2) memcpy(plane[made], plane[s[0]], PLANE);
        else memset(plane[made], 0, PLANE);
        stale[made] = true;
    }
    for(int r=0; r < REGIONS; r++) {
        if (stale[r]) form_of(r);
    }
}

// ---- the machine

static uint64_t where[REGIONS]; // where the machine has each region
static int checks, steps, regions, compared, boxes, failed;

// The machine started, with eight regions of nothing, and their pixels.
static void regions_start(void) {
    start();
    for(int r=0; r < REGIONS; r++) line("new-rgn constant r%d", r);
    line("r0 r1 r2 r3 r4 r5 r6 r7");
    type();
    if (machine.SP != REGIONS) {
        printf("FAIL the machine did not make its regions.  It said:\n%s\n", said);
        exit(EXIT_FAILURE);
    }
    for(int r=0; r < REGIONS; r++) {
        where[r] = left(REGIONS - 1 - r);
        memset(plane[r], 0, PLANE);
        form_of(r);
    }
}

// What is typed, typed at the machine as it was left, which has been done
// to the pixels.  Every region is then compared with the form of its
// pixels, and the region last made is asked whether it is empty and for
// the rectangle around it.  Returns whether all of it was as it should be,
// and says what was not if it is told to.
static bool typed_leaves(const char* label, const char* typed, bool say) {
    const form* f = &forms[made];
    line("%s", typed);
    line("%d r%d rgn-empty?", UNDER, made);
    if (!f->empty) line("r%d rgn-box", made);
    type_more();

    bool right = true;
    for(int r=0; r < REGIONS; r++) {
        const form* want = &forms[r];
        int differ = 0, first = -1;
        if (!in_memory(where[r], want->count)) {
            differ = want->count;
        } else {
            for(int idx=0; idx < want->count; idx++) {
                if (fetch(where[r] + idx) != as_cell(want->cells[idx])) {
                    if (first < 0) first = idx;
                    differ++;
                }
            }
        }
        regions++;
        compared += want->count;
        if (!differ) continue;
        right = false;
        if (!say) continue;
        printf("FAIL %s: r%d is not the form of its pixels: %d cells of %d differ, the first cell %d\n",
               label, r, differ, want->count, first);
        printf("     it should be");
        for(int idx=0; idx < want->count && idx < 48; idx++) printf(" %lld", (long long)want->cells[idx]);
        printf("\n     and it is   ");
        for(int idx=0; idx < want->count && idx < 48; idx++) {
            printf(" %lld", (long long)(fetch(where[r] + idx) << (64 - CELL_BITS)) >> (64 - CELL_BITS));
        }
        printf("\n");
    }

    int64_t leaves[6] = { UNDER, f->empty ? -1 : 0, f->x, f->y, f->w, f->h };
    int count = f->empty ? 2 : 6;
    bool asked = machine.SP == count;
    for(int idx=0; asked && idx < count; idx++) {
        asked = left(count - 1 - idx) == as_cell(leaves[idx]);
    }
    boxes++;
    if (!asked) {
        right = false;
        if (say) {
            printf("FAIL %s: r%d, asked whether it is empty%s, should leave", label, made,
                   f->empty ? "" : " and for the rectangle around it");
            for(int idx=1; idx < count; idx++) printf(" %lld", (long long)leaves[idx]);
            printf(" and left %d cells.  The machine said:\n%s\n", machine.SP, said);
        }
    }
    return(right);
}

// ---- chance

static uint64_t seed;
static int below(int n) {       // a number that is not negative and is less than n
    seed ^= seed << 13;
    seed ^= seed >> 7;
    seed ^= seed << 17;
    return((int)((seed >> 32) % (uint64_t)n));
}
static int between(int low, int high) { return(low + below(high - low)); }
static int grid(int x) { return((x < 0 ? x - 49 : x) / 50 * 50); }      // the fifty it is in

// Would the region fit where the machine keeps one?  A band that is the
// same as the one above it is written before it is known to be, so while
// a region is made it can be a band longer than it is to be.
static bool fits(const form* f) {
    return((uint64_t)(f->count + f->widest + 2) <= room);
}

// A step: a rectangle made, two regions combined, a region moved or one
// copied.  Regions are joined more often than anything else is done, so
// that there is something in them to be shared and taken away.
static const char* step(int n, int* wrong) {
    static const char* how[] = { "rect-rgn", "rect-rgn", "rect-rgn", "rgn-or", "rgn-or", "rgn-or", "rgn-or",
                                 "rgn-or", "rgn-diff", "rgn-diff", "rgn-diff", "rgn-and", "rgn-and",
                                 "rgn-offset", "rgn-copy" };
    static char typed[80];      // what the step typed, which is what it returns
    int to = below(REGIONS), a, b;
    do a = below(REGIONS); while (a == to);
    do b = below(REGIONS); while (b == to);
    const char* does = how[below(sizeof(how) / sizeof(how[0]))];
    if (!strcmp(does, "rect-rgn")) {
        int x = between(-40, 800), y = between(-40, 600), w = between(-2, 300), h = between(-2, 200);
        if (below(10) < 3) {            // edges that others have, so that crossings meet
            x = grid(x), y = grid(y), w = grid(w), h = grid(h);
        }
        snprintf(typed, sizeof(typed), "%d %d %d %d r%d rect-rgn", x, y, w, h, to);
    } else if (!strcmp(does, "rgn-offset")) {
        int dx = between(-60, 60), dy = between(-60, 60);
        const form* f = &forms[to];
        if (!f->empty && (f->x + dx < FROM || f->x + f->w + dx > FROM + ACROSS ||
                          f->y + dy < FROM || f->y + f->h + dy > FROM + ACROSS)) {
            dx = dy = 0;                // it would leave the plane: it is moved by nothing
        }
        snprintf(typed, sizeof(typed), "%d %d r%d rgn-offset", dx, dy, to);
    } else if (!strcmp(does, "rgn-copy")) {
        snprintf(typed, sizeof(typed), "r%d r%d rgn-copy", a, to);
    } else {
        snprintf(typed, sizeof(typed), "r%d r%d r%d %s", a, b, to, does);
    }
    pixels_do(typed);
    if (!fits(&forms[made])) {
        snprintf(typed, sizeof(typed), "r%d rgn-none", to);
        pixels_do(typed);
    }
    char label[32];
    snprintf(label, sizeof(label), "step %d, %s", n, typed);
    steps++;
    if (!typed_leaves(label, typed, *wrong < 5)) (*wrong)++;
    return(typed);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s IMAGE.hex [SEED]\n", argv[0]);
        return(1);
    }
    int given = argc > 2 ? atoi(argv[2]) : 1;
    seed = 0x9e3779b97f4a7c15ull * (uint64_t)(given + 1);
    if (!seed) seed = 1;        // nothing comes of nothing
    alarm(900);                 // a machine that does not stop is stopped

    nucleus_is(argv[1]);
    for(int r=0; r < REGIONS; r++) plane[r] = malloc(PLANE);
    moved = malloc(PLANE);

    // The machine is asked what the end of a band is and how many cells it
    // gives a region.  A rectangle of one pixel at row nothing is 0 and
    // its top, which is AS-END by itself; 2 and its sides; its bottom; and
    // 0 and NEVER.  A second region is where the first one's cells end.
    start();
    line("new-rgn constant r0  0 0 1 1 r0 rect-rgn  r0 new-rgn");
    type();
    uint64_t r0 = machine.SP == 2 ? left(1) : 0;
    if (machine.SP != 2 || !in_memory(r0, 9) || fetch(r0) != 8 || fetch(r0 + 6) != fetch(r0 + 2) + 1) {
        printf("FAIL the machine did not make a rectangle.  It said:\n%s\n", said);
        return(1);
    }
    AS_END = (int64_t)fetch(r0 + 2);
    NEVER = (int64_t)fetch(r0 + 8);
    room = left(0) - r0;

    for(check* test = SAID; strlen(test->label); test++) {
        regions_start();
        pixels_do(test->typed);
        bool right = typed_leaves(test->label, test->typed, true);
        checks++;
        if (!right) failed++;
        printf("%s %s: r%d is %d cells\n", right ? "ok  " : "FAIL", test->label, made, forms[made].count);
    }

    // A rectangle is nine cells, the count one of them.  The steps are to
    // make regions that are more than that, or they tried little.
    int wrong = 0, most = 0, more = 0, started = 0;
    regions_start();
    for(int n=0; n < STEPS; n++) {
        const char* typed = step(n, &wrong);
        if (forms[made].count > most) most = forms[made].count;
        if (forms[made].count > 9) more++;
        if (strstr(typed, "rgn-none")) started++;
    }
    failed += wrong;
    printf("%s %d steps by chance, each from where the last left the regions: %d not as they should be; "
           "%d made a region that is more than a rectangle, the most cells one was is %d of the %d the "
           "machine gives one, and %d would not have fitted\n",
           wrong || !more ? "FAIL" : "ok  ", STEPS, wrong, more, most, (int)room, started);

    printf("regions, cells of %d bits, seed %d: %d checks and %d steps, %d failed; %d regions and %d cells "
           "compared with the form of their pixels, and %d asked for the rectangle around them\n",
           CELL_BITS, given, checks, steps, failed, regions, compared, boxes);
    return(failed || !compared || !boxes || !more);
}
