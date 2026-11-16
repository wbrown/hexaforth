//
// display_test IMAGE.hex FORTH [SEED]
//
// The drawing words of forth/display.fs, against the same drawing done a
// pixel at a time.
//
// It is a contrastive test.  What is typed at the machine is read here
// too, a word at a time, and drawn on a screen that is a byte to a pixel:
// a pixel is drawn if the clip rectangle and the clip region have it, a
// fill is the pattern's bit for that pixel put through the mode, text is
// each glyph's pixels with its style, and a copy and a scroll take each
// pixel from where it was.  The machine draws a cell at a time, with masks
// and shifts.  Nothing is built from what is here.
//
// FORTH is where the system's source is.  The fonts and the patterns are
// tables there (font.fs, display.fs), written as source to be read, and
// they are read from there: the test has no glyph and no pattern of its
// own.
//
// The machine is the nucleus on vm.c, typed at (nucleus.h).  A check is a
// label and what is typed, from a display just started.  When it has been
// typed every pixel of the display is read out of the machine's memory and
// compared, and the machine is to have left nothing on its stack.
//
// After the checks that are said there are steps by chance, the same every
// time for a seed, each typed at the machine as the step before left it:
// fills, text, copies and scrolls, between changes of the origin, the clip
// rectangle and region, the pattern, the mode, the font and the style.
// The display is compared after every step.
//
// It says how many checks and steps it made and how many pixels it
// compared, and of the steps that draw how many changed what can be seen.
// It fails if for any of the four drawing words that is none: then it
// found nothing to test.
//

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "nucleus.h"

#define W       DISP_W
#define H       DISP_H
#define SCREEN  ((size_t)W * H)
#define STEPS   300             // how many are taken by chance
#define UNDER   12345           // what is under what a check leaves
#define GLYPH_ROWS 16           // the rows of a glyph

typedef struct {
    const char* label;
    const char* typed;
} check;

// Something to draw over, and the two regions most of the checks clip to:
// rc is a frame, and re is two rectangles that share a corner, less a bar.
#define GROUND  "p-gray pattern !  0 0 400 200 pat-rect  p-black pattern !\n"
#define FRAME   "40 30 200 100 ra rect-rgn  90 60 80 30 rb rect-rgn  ra rb rc rgn-diff\n"
#define CORNERS "20 20 120 80 ra rect-rgn  100 60 150 90 rb rect-rgn  ra rb rc rgn-or\n" \
                "60 50 200 20 rd rect-rgn  rc rd re rgn-diff\n"

static check SAID[] = {
    // Fills.
    {"a rectangle of black", "10 20 100 50 pat-rect"},
    {"a rectangle of one pixel", "77 33 1 1 pat-rect"},
    {"a rectangle a pixel high, across the whole screen", "0 100 9999 1 pat-rect"},
    {"a rectangle a pixel wide, down the whole screen", "130 0 1 9999 pat-rect"},
    {"a rectangle that starts and ends in one cell", "65 10 30 5 pat-rect"},
    {"a rectangle from the first pixel of a cell to the last of the next", "64 10 128 5 pat-rect"},
    {"a rectangle off the left and the top", "-40 -40 100 60 pat-rect"},
    {"a rectangle off the right and the bottom", "-60 fb-w @ + -30 fb-h @ + 100 60 pat-rect"},
    {"a rectangle that is not on the screen", "-200 -200 100 60 pat-rect"},
    {"a rectangle bigger than the screen", "-10 -10 9999 9999 pat-rect"},
    {"a rectangle of gray", "p-gray pattern !  3 5 150 40 pat-rect"},
    {"a rectangle of light gray", "p-ltgray pattern !  3 5 150 40 pat-rect"},
    {"a rectangle of dark gray", "p-dkgray pattern !  3 5 150 40 pat-rect"},
    {"a rectangle of white, over black", "0 0 200 100 pat-rect  p-white pattern !  30 20 90 40 pat-rect"},
    {"gray over gray: the pattern is where the screen has it", GROUND "p-gray pattern !  33 17 90 40 pat-rect"},
    {"black, or-mode, over gray", GROUND "or-mode  30 20 90 40 pat-rect"},
    {"dark gray, or-mode, over gray", GROUND "or-mode  p-dkgray pattern !  30 20 90 40 pat-rect"},
    {"black, xor-mode, over gray", GROUND "xor-mode  30 20 90 40 pat-rect"},
    {"light gray, xor-mode, over gray", GROUND "xor-mode  p-ltgray pattern !  30 20 90 40 pat-rect"},
    {"black, bic-mode, over gray", GROUND "bic-mode  30 20 90 40 pat-rect"},
    {"dark gray, bic-mode, over gray", GROUND "bic-mode  p-dkgray pattern !  30 20 90 40 pat-rect"},
    {"copy-mode again, after another", GROUND "xor-mode copy-mode  p-ltgray pattern !  30 20 90 40 pat-rect"},

    // The origin and the clip rectangle.
    {"a rectangle in a port that starts elsewhere", "100 50 origin!  10 20 100 50 pat-rect"},
    {"a rectangle in a port that starts off the screen", "-30 -20 origin!  10 10 100 50 pat-rect"},
    {"a rectangle cut by the clip rectangle", "50 40 100 60 clip!  10 20 300 200 pat-rect"},
    {"a rectangle outside the clip rectangle", "50 40 100 60 clip!  200 20 30 20 pat-rect"},
    {"a clip rectangle off the screen's edges", "-20 -20 9999 9999 clip!  -40 -40 100 60 pat-rect"},
    {"the clip rectangle is in the port", "100 50 origin!  10 10 60 40 clip!  0 0 300 200 pat-rect"},
    {"a clip rectangle made smaller", "50 40 100 60 clip!  70 20 200 50 clip-to  0 0 9999 9999 pat-rect"},
    {"a clip rectangle made nothing", "50 40 100 60 clip!  300 20 20 20 clip-to  0 0 9999 9999 pat-rect"},
    {"no clip rectangle again", "50 40 100 60 clip!  noclip  10 20 300 200 pat-rect"},

    // The clip region.
    {"a rectangle clipped to a frame", FRAME "rc rgn-clip!  0 0 9999 9999 pat-rect"},
    {"gray clipped to a frame", FRAME "rc rgn-clip!  p-gray pattern !  10 40 300 60 pat-rect"},
    {"xor-mode clipped to a frame, over gray", GROUND FRAME "rc rgn-clip!  xor-mode  10 40 300 60 pat-rect"},
    {"a region and a clip rectangle", FRAME "rc rgn-clip!  60 20 100 80 clip!  0 0 9999 9999 pat-rect"},
    {"a region, in a port that starts elsewhere: a region is on the screen",
     FRAME "rc rgn-clip!  30 30 origin!  0 0 150 60 pat-rect"},
    {"a rectangle clipped to what is not a frame", CORNERS "re rgn-clip!  0 0 9999 9999 pat-rect"},
    {"a region that is a rectangle", "40 30 200 100 ra rect-rgn  ra rgn-clip!  0 0 9999 9999 pat-rect"},
    {"a region of nothing", "rb rgn-clip!  0 0 9999 9999 pat-rect"},
    {"a region partly off the screen", "-50 -50 200 100 ra rect-rgn  ra rgn-clip!  0 0 9999 9999 pat-rect"},
    {"no region again", FRAME "rc rgn-clip!  0 rgn-clip!  10 20 300 200 pat-rect"},
    {"no clip is no region either", FRAME "rc rgn-clip!  noclip  10 20 300 200 pat-rect"},

    // Text.
    {"text", "t\" Hexaforth\" 20 30 draw-text"},
    {"text in sans", "sans font !  t\" Hexaforth, and more\" 20 30 draw-text"},
    {"every character, in mono",
     "t\"  !\" 8 8 draw-text  34 0 ch tb 1 100 8 draw-text\n"
     "t\" #$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNO\" 8 30 draw-text\n"
     "t\" PQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\" 8 50 draw-text"},
    {"every character, in sans",
     "sans font !  t\"  !\" 8 8 draw-text  34 0 ch tb 1 100 8 draw-text\n"
     "t\" #$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNO\" 8 30 draw-text\n"
     "t\" PQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\" 8 50 draw-text"},
    {"a character that is not in the font is drawn as a space", "7 0 ch 200 1 ch tb 2 20 30 draw-text"},
    {"text of no characters", "tb 0 20 30 draw-text"},
    {"text that starts in the middle of a cell", "t\" Hexaforth\" 61 30 draw-text"},
    {"bold text", "-1 bold !  t\" Hexaforth\" 20 30 draw-text"},
    {"italic text", "-1 italic !  t\" Hexaforth\" 20 30 draw-text"},
    {"inverse text", "-1 inverse !  t\" Hexaforth\" 20 30 draw-text"},
    {"bold italic inverse text, in sans",
     "sans font !  -1 bold !  -1 italic !  -1 inverse !  t\" Hexaforth\" 20 30 draw-text"},
    {"plain text again", "-1 bold !  -1 italic !  0 bold !  0 italic !  t\" Hexaforth\" 20 30 draw-text"},
    {"text over gray: its white is drawn too", GROUND "t\" Hexaforth\" 20 30 draw-text"},
    {"text that only adds its black, over gray", GROUND "-1 text-or !  t\" Hexaforth\" 20 30 draw-text"},
    {"inverse text that only adds its black is drawn whole", GROUND
     "-1 text-or !  -1 inverse !  t\" Hexaforth\" 20 30 draw-text"},
    {"text is not drawn through the mode", GROUND "xor-mode  t\" Hexaforth\" 20 30 draw-text"},
    {"text off the top", "t\" Hexaforth\" 20 -7 draw-text"},
    {"text off the bottom", "t\" Hexaforth\" 20 -9 fb-h @ + draw-text"},
    {"text off the right: the character that is cut is drawn", "t\" Hexaforth\" -43 fb-w @ + 30 draw-text"},
    {"text off the left: the character that is cut is not drawn", "t\" Hexaforth\" -13 30 draw-text"},
    {"text cut by the clip rectangle", "30 34 50 6 clip!  t\" Hexaforth\" 20 30 draw-text"},
    {"text in a port that starts elsewhere", "100 50 origin!  t\" Hexaforth\" 20 30 draw-text"},
    {"text clipped to a frame", FRAME "rc rgn-clip!  sans font !  t\" Hexaforth, and more of it\" 30 55 draw-text"},
    {"bold text clipped to what is not a frame", CORNERS "re rgn-clip!  -1 bold !  t\" Hexaforth and more\" 30 44 draw-text"},

    // Copies.
#define PICTURE GROUND "sans font !  t\" Hexaforth\" 20 30 draw-text  60 70 40 9 pat-rect\n"
    {"a copy to elsewhere", PICTURE "screen 10 20 420 30 100 70 copy-bits"},
    {"a copy of one pixel", PICTURE "screen 62 71 500 300 1 1 copy-bits"},
    {"a copy to the right, over itself", PICTURE "screen 10 20 17 20 100 70 copy-bits"},
    {"a copy to the left, over itself", PICTURE "screen 10 20 3 20 100 70 copy-bits"},
    {"a copy down, over itself", PICTURE "screen 10 20 10 25 100 70 copy-bits"},
    {"a copy up, over itself", PICTURE "screen 10 20 10 15 100 70 copy-bits"},
    {"a copy down and to the right, over itself", PICTURE "screen 10 20 13 22 100 70 copy-bits"},
    {"a copy up and to the left by a whole cell", PICTURE "screen 74 20 10 12 100 70 copy-bits"},
    {"a copy onto itself", PICTURE "screen 10 20 10 20 100 70 copy-bits"},
    {"a copy to off the left and the top", PICTURE "screen 10 20 -30 -20 100 70 copy-bits"},
    {"a copy to off the right and the bottom",
     PICTURE "screen 10 20 -50 fb-w @ + -40 fb-h @ + 100 70 copy-bits"},
    {"a copy, or-mode", PICTURE "or-mode  screen 10 20 60 40 100 70 copy-bits"},
    {"a copy, xor-mode", PICTURE "xor-mode  screen 10 20 60 40 100 70 copy-bits"},
    {"a copy, bic-mode", PICTURE "bic-mode  screen 10 20 60 40 100 70 copy-bits"},
    {"a copy cut by the clip rectangle", PICTURE "430 40 60 30 clip!  screen 10 20 420 30 100 70 copy-bits"},
    {"a copy in a port that starts elsewhere", PICTURE "5 7 origin!  screen 5 13 415 23 100 70 copy-bits"},
    {"a copy clipped to a frame", PICTURE FRAME "rc rgn-clip!  screen 10 20 60 40 100 70 copy-bits"},

    // Scrolls.
    {"a scroll up by a line", PICTURE "0 0 300 150 0 -16 scroll-rect"},
    {"a scroll down by a line", PICTURE "0 0 300 150 0 16 scroll-rect"},
    {"a scroll to the right", PICTURE "0 0 300 150 8 0 scroll-rect"},
    {"a scroll to the left", PICTURE "0 0 300 150 -3 0 scroll-rect"},
    {"a scroll up and to the right", PICTURE "0 0 300 150 5 -7 scroll-rect"},
    {"a scroll of part of the picture", PICTURE "30 25 100 60 0 -16 scroll-rect"},
    {"a scroll by more than the rectangle is", PICTURE "30 25 100 60 0 -80 scroll-rect"},
    {"a scroll by nothing", PICTURE "30 25 100 60 0 0 scroll-rect"},
    {"a scroll cut by the clip rectangle", PICTURE "40 30 80 40 clip!  0 0 300 150 0 -16 scroll-rect"},
    {"a scroll in a port that starts elsewhere", PICTURE "5 7 origin!  0 0 300 150 0 -16 scroll-rect"},
    {"a scroll clipped to a frame", PICTURE FRAME "rc rgn-clip!  0 0 300 150 0 -16 scroll-rect"},
    {"a scroll clipped to what is not a frame", PICTURE CORNERS "re rgn-clip!  0 0 300 150 5 -7 scroll-rect"},
    {"a scroll leaves the pattern white and the mode a copy",
     PICTURE "xor-mode  0 0 300 150 0 -16 scroll-rect  200 100 150 80 pat-rect"},
    {""}};

// ---- the system's tables, read from its source

// The numbers a table is, in a source file: those laid down after
// `header NAME create NAME`.  Each is # and decimal digits or $ and hex,
// and after each is the word that lays it down; the table ends at the
// first word that is neither.  Returns how many there are.
static int numbers_of(const char* source, const char* path, const char* name, uint64_t* out, int most) {
    const char* at = NULL;
    for(const char* row = source; row && !at; row = strchr(row, '\n') ? strchr(row, '\n') + 1 : NULL) {
        char is[64], made[64];
        int used = 0;
        if (sscanf(row, "header %63s create %63s%n", is, made, &used) == 2 && !strcmp(is, name) && !strcmp(made, name)) {
            at = row + used;
        }
    }
    if (!at) {
        fprintf(stderr, "display_test: %s has no %s\n", path, name);
        exit(EXIT_FAILURE);
    }
    int count = 0;
    for(;;) {
        char word[64], lays[64];
        int used = 0;
        if (sscanf(at, "%63s%n", word, &used) != 1) break;
        at += used;
        if (!strcmp(word, "font[")) continue;       // what a font's numbers are between
        if (*word != '#' && *word != '$') break;
        if (sscanf(at, "%63s%n", lays, &used) != 1) break;
        at += used;
        if (count == most) {
            fprintf(stderr, "display_test: %s has more numbers than %d\n", name, most);
            exit(EXIT_FAILURE);
        }
        out[count++] = strtoull(word + 1, NULL, *word == '#' ? 10 : 16);
    }
    return(count);
}

static char* source_of(const char* dir, const char* file, char* path, size_t path_len) {
    snprintf(path, path_len, "%s/%s", dir, file);
    FILE* in = fopen(path, "rb");
    if (in == NULL) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    fseek(in, 0, SEEK_END);
    size_t len = ftell(in);
    fseek(in, 0, SEEK_SET);
    char* text = calloc(len + 1, 1);
    if (fread(text, 1, len, in) != len) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    fclose(in);
    return(text);
}

// A pattern is eight rows of eight pixels, the leftmost the highest bit.
static const char* PATTERNS[] = { "p-white", "p-black", "p-gray", "p-ltgray", "p-dkgray", NULL };
static uint64_t pattern_rows[5][8];

// A font, as font.fs says one is: how high, the first character, how many,
// the numbers in a row of the strike, a cell the system uses; then where
// each glyph starts and where the last ends; then the strike a row at a
// time, 64 pixels to a number with the leftmost highest.
static const char* FONTS[] = { "mono", "sans", NULL };
typedef struct {
    int rows, first, count, across;
    const uint64_t* starts;
    const uint64_t* strike;
} font;
static font fonts[2];

static bool strike_pixel(const font* f, int x, int row) {
    return(f->strike[row * f->across + x / 64] >> (63 - x % 64) & 1);
}

static void tables(const char* dir) {
    char path[512];
    char* source = source_of(dir, "display.fs", path, sizeof(path));
    for(int n=0; PATTERNS[n]; n++) {
        if (numbers_of(source, path, PATTERNS[n], pattern_rows[n], 8) != 8) {
            fprintf(stderr, "display_test: %s is not eight rows\n", PATTERNS[n]);
            exit(EXIT_FAILURE);
        }
    }
    free(source);
    source = source_of(dir, "font.fs", path, sizeof(path));
    for(int n=0; FONTS[n]; n++) {
        int most = 1 << 12;
        uint64_t* numbers = malloc(most * sizeof(uint64_t));
        int count = numbers_of(source, path, FONTS[n], numbers, most);
        font* f = &fonts[n];
        f->rows = (int)numbers[0], f->first = (int)numbers[1], f->count = (int)numbers[2], f->across = (int)numbers[3];
        f->starts = numbers + 5;
        f->strike = f->starts + f->count + 1;
        if (count < 5 || count != 5 + f->count + 1 + f->rows * f->across || f->rows != GLYPH_ROWS) {
            fprintf(stderr, "display_test: %s is not a font as %s says one is\n", FONTS[n], path);
            exit(EXIT_FAILURE);
        }
    }
    free(source);
}

// ---- the screen a pixel at a time

#define REGIONS 5               // ra to re
static uint8_t* px;             // the screen: a byte a pixel, 1 if it is black
static uint8_t* before;         // the screen as it was, for what takes pixels from it
static uint8_t* rgn[REGIONS];   // a region's pixels, of those on the screen: 1 if it is in the region
static int clip_x0, clip_y0, clip_x1, clip_y1;  // the clip rectangle, on the screen
static int clip_rgn;            // the clip region, or -1
static int ox, oy;              // where the port's coordinates start
static int pattern, mode, type_face;
static bool bold, italic, inverse, text_or;
static char text[64];           // the text buffer, tb

enum { COPY, OR, XOR, BIC };

static void screen_start(void) {
    memset(px, 0, SCREEN);
    for(int r=0; r < REGIONS; r++) memset(rgn[r], 0, SCREEN);
    clip_x0 = clip_y0 = 0, clip_x1 = W, clip_y1 = H;
    clip_rgn = -1;
    ox = oy = 0;
    pattern = 1, mode = COPY, type_face = 0;        // black, copying, plain text in mono
    bold = italic = inverse = text_or = false;
    memset(text, 0, sizeof(text));
}

static bool on_screen(int x, int y) { return(x >= 0 && x < W && y >= 0 && y < H); }

// Can the pixel be drawn?
static bool visible(int x, int y) {
    return(x >= clip_x0 && x < clip_x1 && y >= clip_y0 && y < clip_y1 &&
           (clip_rgn < 0 || rgn[clip_rgn][(size_t)y * W + x]));
}

static int through(int how, int was, int is) {
    return(how == COPY ? is : how == OR ? was | is : how == XOR ? was ^ is : was & !is);
}

static int low(int a, int b) { return(a < b ? a : b); }
static int high(int a, int b) { return(a > b ? a : b); }

static void pat_rect(int x, int y, int w, int h) {
    x += ox, y += oy;
    for(int yy=high(y, 0); yy < low(y + h, H); yy++) {
        for(int xx=high(x, 0); xx < low(x + w, W); xx++) {
            if (!visible(xx, yy)) continue;
            int is = pattern_rows[pattern][yy & 7] >> (7 - (xx & 7)) & 1;
            uint8_t* p = px + (size_t)yy * W + xx;
            *p = through(mode, *p, is);
        }
    }
}

// Text: each character's glyph, or a space's if the font has not the
// character.  A character is not drawn if it is all left of the clip
// rectangle, if it starts left of the screen, or if it starts right of the
// clip rectangle.  In italic the top half of a glyph is a pixel to the
// right; bold is each pixel and the one to its left; inverse is the
// glyph's black for white.  Text is copied, or with text-or only its black
// is added, unless it is inverse.
static void draw_text(const char* chars, int count, int x, int y) {
    const font* f = &fonts[type_face];
    int pen = x + ox;
    y += oy;
    for(int n=0; n < count; n++) {
        int code = (unsigned char)chars[n] - f->first;
        if (code < 0 || code >= f->count) code = 0;
        int start = (int)f->starts[code], width = (int)f->starts[code + 1] - start;
        if (!(pen + width <= clip_x0 || pen < 0 || pen >= clip_x1)) {
            for(int row=0; row < f->rows; row++) {
                for(int k=0; k < width; k++) {
                    int from = italic && row < f->rows / 2 ? k - 1 : k;     // the pixel of the glyph that is here
                    bool is = from >= 0 && strike_pixel(f, start + from, row);
                    if (bold && from >= 1 && strike_pixel(f, start + from - 1, row)) is = true;
                    if (inverse) is = !is;
                    if (!visible(pen + k, y + row)) continue;
                    uint8_t* p = px + (size_t)(y + row) * W + pen + k;
                    *p = text_or && !inverse ? *p | is : is;
                }
            }
        }
        pen += width;
    }
}

static void copy_bits(int sx, int sy, int dx, int dy, int w, int h) {
    sx += ox, sy += oy, dx += ox, dy += oy;
    if (w > 0 && h > 0 && (!on_screen(sx, sy) || !on_screen(sx + w - 1, sy + h - 1))) {
        fprintf(stderr, "display_test: a copy is from what is not on the screen\n");
        exit(EXIT_FAILURE);
    }
    memcpy(before, px, SCREEN);
    for(int j=0; j < h; j++) {
        for(int i=0; i < w; i++) {
            if (!visible(dx + i, dy + j)) continue;
            uint8_t* p = px + (size_t)(dy + j) * W + dx + i;
            *p = through(mode, *p, before[(size_t)(sy + j) * W + sx + i]);
        }
    }
}

// A pixel moves to a place if the place and where it comes from can both
// be drawn in and are both in the rectangle; a place that can be drawn in
// and has nothing to come to it is made white.  The pattern is left white
// and the mode a copy.
static int sc_x, sc_y, sc_w, sc_h;
static bool moves(int x, int y) {
    return(x >= sc_x && x < sc_x + sc_w && y >= sc_y && y < sc_y + sc_h && on_screen(x, y) && visible(x, y));
}
static void scroll_rect(int x, int y, int w, int h, int dx, int dy) {
    sc_x = x + ox, sc_y = y + oy, sc_w = w, sc_h = h;
    memcpy(before, px, SCREEN);
    for(int j=high(sc_y, 0); j < low(sc_y + h, H); j++) {
        for(int i=high(sc_x, 0); i < low(sc_x + w, W); i++) {
            if (!moves(i, j)) continue;
            px[(size_t)j * W + i] = moves(i - dx, j - dy) ? before[(size_t)(j - dy) * W + i - dx] : 0;
        }
    }
    mode = COPY;
    pattern = 0;
}

// ---- what is typed, drawn here

// What a word leaves for the next: a number, or what a name is.
typedef struct {
    char is;                    // n a number; p a pattern; f a font; r a region; v a variable;
    long n;                     // t the text buffer; q text from the line; s the screen
    const char* chars;
} value;

static const char* VARIABLES[] = { "pattern", "font", "bold", "italic", "inverse", "text-or", NULL };
static const char* MODES[] = { "copy-mode", "or-mode", "xor-mode", "bic-mode", NULL };

static int among(const char* names[], const char* word) {
    for(int n=0; names[n]; n++) {
        if (!strcmp(names[n], word)) return(n);
    }
    return(-1);
}

static const char* reading;     // what is typed, for when it cannot be read
static void cannot(const char* word, const char* why) {
    fprintf(stderr, "display_test: %s %s, in:\n%s\n", word, why, reading);
    exit(EXIT_FAILURE);
}

// A word at a time.  A word is a number, a name, or a word of the
// display's, which takes what that word takes; t" is the test's, and is
// the text up to the next " of the line.
static void pixels_do(const char* typed) {
    value stack[16];
    int sp = 0;
    reading = typed;
    for(const char* at = typed; ; ) {
        while (isspace((unsigned char)*at)) at++;
        if (!*at) break;
        char word[32];
        size_t len = strcspn(at, " \n");
        if (len >= sizeof(word)) len = sizeof(word) - 1;
        memcpy(word, at, len);
        word[len] = '\0';
        at += len;

        // What leaves something.
        char* end;
        long n = strtol(word, &end, 10);
        value v = { 0 };
        int which;
        if (!*end) v = (value){ 'n', n };
        else if ((which = among(PATTERNS, word)) >= 0) v = (value){ 'p', which };
        else if ((which = among(FONTS, word)) >= 0) v = (value){ 'f', which };
        else if ((which = among(VARIABLES, word)) >= 0) v = (value){ 'v', which };
        else if (word[0] == 'r' && word[1] >= 'a' && word[1] < 'a' + REGIONS && !word[2]) v = (value){ 'r', word[1] - 'a' };
        else if (!strcmp(word, "tb")) v = (value){ 't' };
        else if (!strcmp(word, "screen")) v = (value){ 's' };
        else if (!strcmp(word, "fb-w") || !strcmp(word, "fb-h")) v = (value){ 'n', word[3] == 'w' ? W : H };
        if (v.is) {
            if (sp == 16) cannot(word, "is one more than the stack holds");
            stack[sp++] = v;
            continue;
        }
        if (!strcmp(word, "t\"")) {
            if (*at == ' ') at++;                   // the space that ends the word is not text
            const char* close = strchr(at, '"');
            if (!close || sp > 14) cannot(word, "has no text after it");
            stack[sp++] = (value){ 'q', 0, at };
            stack[sp++] = (value){ 'n', close - at };
            at = close + 1;
            continue;
        }

        // What takes something: every word here takes numbers, but where it says.
        int takes = !strcmp(word, "pat-rect") || !strcmp(word, "clip!") || !strcmp(word, "clip-to") ||
                    !strcmp(word, "draw-text") ? 4 :
                    !strcmp(word, "copy-bits") ? 7 :
                    !strcmp(word, "scroll-rect") ? 6 :
                    !strcmp(word, "rect-rgn") ? 5 :
                    !strncmp(word, "rgn-", 4) && strcmp(word, "rgn-clip!") ? 3 :
                    !strcmp(word, "origin!") || !strcmp(word, "!") || !strcmp(word, "ch") ||
                    !strcmp(word, "+") ? 2 :
                    !strcmp(word, "rgn-clip!") || !strcmp(word, "@") ? 1 :
                    !strcmp(word, "noclip") || among(MODES, word) >= 0 ? 0 : -1;
        if (takes < 0) cannot(word, "is not a word that is drawn here");
        if (sp < takes) cannot(word, "has not what it takes");
        sp -= takes;
        const value* s = stack + sp;
        long a[7];
        for(int idx=0; idx < takes; idx++) a[idx] = s[idx].n;

        if (!strcmp(word, "@")) {
            stack[sp++] = s[0];                     // fb-w and fb-h are said as what is in them
        } else if (!strcmp(word, "+")) {
            stack[sp++] = (value){ 'n', a[0] + a[1] };
        } else if (!strcmp(word, "pat-rect")) {
            pat_rect((int)a[0], (int)a[1], (int)a[2], (int)a[3]);
        } else if (!strcmp(word, "draw-text")) {
            if (s[0].is != 't' && s[0].is != 'q') cannot(word, "is not given text");
            draw_text(s[0].is == 't' ? text : s[0].chars, (int)a[1], (int)a[2], (int)a[3]);
        } else if (!strcmp(word, "ch")) {
            if (a[1] < 0 || a[1] >= (long)sizeof(text)) cannot(word, "is past the end of the text");
            text[a[1]] = (char)a[0];
        } else if (!strcmp(word, "copy-bits")) {
            if (s[0].is != 's') cannot(word, "is not from the screen");
            copy_bits((int)a[1], (int)a[2], (int)a[3], (int)a[4], (int)a[5], (int)a[6]);
        } else if (!strcmp(word, "scroll-rect")) {
            scroll_rect((int)a[0], (int)a[1], (int)a[2], (int)a[3], (int)a[4], (int)a[5]);
        } else if (!strcmp(word, "clip!")) {
            int x = (int)a[0] + ox, y = (int)a[1] + oy;
            clip_x0 = high(x, 0), clip_y0 = high(y, 0);
            clip_x1 = low(x + (int)a[2], W), clip_y1 = low(y + (int)a[3], H);
        } else if (!strcmp(word, "clip-to")) {
            int x = (int)a[0] + ox, y = (int)a[1] + oy;
            clip_x0 = high(x, clip_x0), clip_y0 = high(y, clip_y0);
            clip_x1 = low(x + (int)a[2], clip_x1), clip_y1 = low(y + (int)a[3], clip_y1);
        } else if (!strcmp(word, "noclip")) {
            clip_x0 = clip_y0 = 0, clip_x1 = W, clip_y1 = H;
            clip_rgn = -1;
        } else if (!strcmp(word, "origin!")) {
            ox = (int)a[0], oy = (int)a[1];
        } else if (!strcmp(word, "rgn-clip!")) {
            if (s[0].is != 'r' && !(s[0].is == 'n' && !a[0])) cannot(word, "is given no region");
            clip_rgn = s[0].is == 'r' ? (int)a[0] : -1;
        } else if (!strcmp(word, "rect-rgn")) {
            if (s[4].is != 'r' || a[4] == clip_rgn) cannot(word, "is not to a region that can be made");
            for(int y=0; y < H; y++) {
                for(int x=0; x < W; x++) {
                    rgn[a[4]][(size_t)y * W + x] = x >= a[0] && x < a[0] + a[2] && y >= a[1] && y < a[1] + a[3];
                }
            }
        } else if (takes == 3) {
            int how = !strcmp(word, "rgn-and") ? 0 : !strcmp(word, "rgn-or") ? 1 : !strcmp(word, "rgn-diff") ? 2 : -1;
            if (how < 0 || s[0].is != 'r' || s[1].is != 'r' || s[2].is != 'r' || a[2] == clip_rgn) {
                cannot(word, "is not of regions, to a region that can be made");
            }
            for(size_t idx=0; idx < SCREEN; idx++) {
                bool in_a = rgn[a[0]][idx], in_b = rgn[a[1]][idx];
                rgn[a[2]][idx] = how == 0 ? in_a && in_b : how == 1 ? in_a || in_b : in_a && !in_b;
            }
        } else if (!strcmp(word, "!")) {
            if (s[1].is != 'v') cannot(word, "is not to a variable drawn here");
            const char* name = VARIABLES[a[1]];
            if (!strcmp(name, "pattern") && s[0].is == 'p') pattern = (int)a[0];
            else if (!strcmp(name, "font") && s[0].is == 'f') type_face = (int)a[0];
            else if (!strcmp(name, "bold") && s[0].is == 'n') bold = a[0];
            else if (!strcmp(name, "italic") && s[0].is == 'n') italic = a[0];
            else if (!strcmp(name, "inverse") && s[0].is == 'n') inverse = a[0];
            else if (!strcmp(name, "text-or") && s[0].is == 'n') text_or = a[0];
            else cannot(name, "is given what it does not hold");
        } else {
            mode = among(MODES, word);
        }
    }
    if (sp) cannot("what is typed", "leaves something");
}

// ---- the machine

static int checks, steps, compared, drew, failed;
static uint8_t* last;           // the screen as it was when it was last compared

// The machine started, with its display, and what the test types besides
// the display's words: a buffer for text and what puts a character in it,
// text from the line, and five regions.
static void display_start(void) {
    start();
    line("display-init");
    line("create tb0 64 allot  : tb tb0 >c ;  : ch tb + c! ;  : t\" 34 parse ;");
    for(int r=0; r < REGIONS; r++) line("new-rgn constant r%c", 'a' + r);
    line("%d", UNDER);
    type();
    if (machine.SP != 1 || left(0) != UNDER) {
        printf("FAIL the machine did not start its display.  It said:\n%s\n", said);
        exit(EXIT_FAILURE);
    }
    screen_start();
    memset(last, 0, SCREEN);
}

// A pixel of the display, as the machine has it: 1 if it is black.
static int machine_pixel(int x, int y) {
    uint64_t cell = fetch(DISP_ROW(y) + x / CELL_BITS);
    int bit = x % CELL_BITS;
    int set = cell >> (DISP_LEFT_LOW ? bit : CELL_BITS - 1 - bit) & 1;
    return(DISP_INK_BLACK ? set : !set);
}

// What is typed, typed at the machine as it was left, which has been
// drawn here.  Every pixel is then compared, and the machine is to have
// left nothing.  Returns whether all of it was as it should be, and says
// what was not if it is told to.
static bool typed_leaves(const char* label, const char* typed, bool say) {
    for(const char* row = typed; row; row = strchr(row, '\n') ? strchr(row, '\n') + 1 : NULL) {
        if (strcspn(row, "\n") > 120) {             // the machine takes a line of no more
            fprintf(stderr, "display_test: %s: a line is too long to type:\n%s\n", label, row);
            exit(EXIT_FAILURE);
        }
    }
    line("%d", UNDER);
    line("%s", typed);
    type_more();

    bool kept = machine.SP == 1 && left(0) == UNDER;
    if (!kept && say) {
        printf("FAIL %s: the machine left %d cells, and is to leave what was there.  It said:\n%s\n",
               label, machine.SP, said);
    }
    int differ = 0, shown = 0;
    for(int y=0; y < H; y++) {
        for(int x=0; x < W; x++) {
            int is = machine_pixel(x, y), should = px[(size_t)y * W + x];
            if (is == should) continue;
            differ++;
            if (say && shown++ < 5) {
                if (shown == 1) printf("FAIL %s:\n", label);
                printf("     the pixel at %d, %d is %s and should be %s\n", x, y,
                       is ? "black" : "white", should ? "black" : "white");
            }
        }
    }
    if (differ && say) printf("     %d pixels differ\n", differ);
    compared += W * H;
    if (memcmp(last, px, SCREEN)) drew++;
    memcpy(last, px, SCREEN);
    return(kept && !differ);
}

// ---- chance

static uint64_t seed;
static int below(int n) {       // a number that is not negative and is less than n
    seed ^= seed << 13;
    seed ^= seed >> 7;
    seed ^= seed << 17;
    return((int)((seed >> 32) % (uint64_t)n));
}
static int between(int low, int past) { return(low + below(past - low)); }      // from low up to past
#define ONE_OF(...) ((int[]){ __VA_ARGS__ })[below(sizeof((int[]){ __VA_ARGS__ }) / sizeof(int))]

static char typed[2048];        // what a step types
static void more(const char* format, ...) {
    va_list args;
    va_start(args, format);
    size_t len = strlen(typed);
    vsnprintf(typed + len, sizeof(typed) - len, format, args);
    va_end(args);
}

// A rectangle for a region to be: on the screen or partly off it.
static void region_rect(char name) {
    more("%d %d %d %d r%c rect-rgn\n", between(-30, W - 19), between(-30, H - 19),
         between(1, 501), between(1, 401), name);
}

// A pixel that can be drawn in, on the screen, if chance finds one: it is
// in the clip rectangle, and the region may have it.
static bool can_be_seen(int* x, int* y) {
    if (clip_x1 <= clip_x0 || clip_y1 <= clip_y0) return(false);
    for(int n=0; n < 64; n++) {
        *x = between(clip_x0, clip_x1), *y = between(clip_y0, clip_y1);
        if (visible(*x, *y)) return(true);
    }
    return(false);
}

// Somewhere to draw, on the screen: anywhere on it or a little off it; or,
// more often, a little up and left of a pixel that can be drawn in, so
// that what is drawn is not mostly what the clip rectangle and the region
// hide.
static void somewhere(int* x, int* y) {
    if (below(4) && can_be_seen(x, y)) {
        *x -= below(30), *y -= below(12);
    } else {
        *x = between(-40, W + 1), *y = between(-40, H + 1);
    }
}

// A step: something is drawn, or how things are drawn is changed.  Fills
// and text are the most of it.
static const char* step(void) {
    static const char* does[] = { "rect", "rect", "rect", "rect", "text", "text", "text", "text", "copy",
                                  "scroll", "clip", "noclip", "mode", "origin", "font", "region", "region",
                                  "noregion", "pattern", "style", "text-or" };
    const char* what = does[below(sizeof(does) / sizeof(does[0]))];
    typed[0] = '\0';
    int x, y;
    somewhere(&x, &y);
    if (!strcmp(what, "rect")) {
        if (below(2)) more("%s pattern !  ", PATTERNS[below(5)]);       // or with the pattern there is
        more("%d %d %d %d pat-rect", x - ox, y - oy, ONE_OF(1, 7, 64, 130, 300), ONE_OF(1, 3, 16, 60));
    } else if (!strcmp(what, "text")) {
        int count = ONE_OF(1, 2, 9, 20);
        for(int n=0; n < count; n++) more("%d %d ch\n", between(33, 127), n);
        more("tb %d %d %d draw-text", count, x - ox, y - oy);
    } else if (!strcmp(what, "copy")) {
        // From anywhere on the screen, to somewhere or to near where it is from.
        int w = between(1, 201), h = between(1, 81);
        int sx = between(0, W - w + 1), sy = between(0, H - h + 1);
        if (below(2)) x = sx + between(-9, 10), y = sy + between(-3, 4);
        more("screen %d %d %d %d %d %d copy-bits", sx - ox, sy - oy, x - ox, y - oy, w, h);
    } else if (!strcmp(what, "scroll")) {
        // A rectangle on the screen: anywhere, or more often round a pixel that can be drawn in.
        int w = between(10, 301), h = between(10, 121);
        if (below(4) && can_be_seen(&x, &y)) {
            x = low(high(x - below(w), 0), W - w), y = low(high(y - below(h), 0), H - h);
        } else {
            x = between(0, W - w + 1), y = between(0, H - h + 1);
        }
        int way = below(5);
        more("%d %d %d %d %d %d scroll-rect", x - ox, y - oy, w, h,
             ((int[]){ 0, 0, 8, -3, 5 })[way], ((int[]){ -16, 16, 0, 0, -7 })[way]);
    } else if (!strcmp(what, "clip")) {
        more("%d %d %d %d clip!", between(-20, W - 49), between(-20, H - 49), between(20, 501), between(20, 401));
    } else if (!strcmp(what, "noclip")) {
        more("noclip");
    } else if (!strcmp(what, "mode")) {
        more("%s", MODES[below(4)]);
    } else if (!strcmp(what, "origin")) {
        if (below(2)) more("0 0 origin!"); else more("%d %d origin!", between(-30, 301), between(-30, 301));
    } else if (!strcmp(what, "font")) {
        more("%s font !", FONTS[below(2)]);
    } else if (!strcmp(what, "region")) {
        // Two or three rectangles, combined.  The region that is clipped to
        // is not to be made while it is: so nothing is clipped to first.
        static const char* how[] = { "and", "or", "diff", "diff" };
        more("0 rgn-clip!\n");
        region_rect('a');
        region_rect('b');
        more("ra rb rc rgn-%s\n", how[below(4)]);
        if (below(10) < 6) {
            region_rect('d');
            more("rc rd re rgn-%s\nre rgn-clip!", how[below(3)]);
        } else {
            more("rc rgn-clip!");
        }
    } else if (!strcmp(what, "noregion")) {
        more("0 rgn-clip!");
    } else if (!strcmp(what, "pattern")) {
        more("%s pattern !", PATTERNS[below(5)]);
    } else if (!strcmp(what, "style")) {
        int which = below(3);
        bool is = which == 0 ? inverse : which == 1 ? bold : italic;
        more("%d %s !", is ? 0 : -1, which == 0 ? "inverse" : which == 1 ? "bold" : "italic");
    } else {
        more("%d text-or !", below(2) ? -1 : 0);
    }
    return(typed);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s IMAGE.hex FORTH [SEED]\n", argv[0]);
        return(1);
    }
    if (!W) {
        fprintf(stderr, "this machine has no display\n");
        return(1);
    }
    int given = argc > 3 ? atoi(argv[3]) : 1;
    seed = 0x9e3779b97f4a7c15ull * (uint64_t)(given + 1);
    if (!seed) seed = 1;        // nothing comes of nothing
    alarm(1200);                // a machine that does not stop is stopped

    nucleus_is(argv[1]);
    tables(argv[2]);
    px = malloc(SCREEN);
    before = malloc(SCREEN);
    last = malloc(SCREEN);
    for(int r=0; r < REGIONS; r++) rgn[r] = malloc(SCREEN);

    for(check* test = SAID; strlen(test->label); test++) {
        display_start();
        pixels_do(test->typed);
        bool right = typed_leaves(test->label, test->typed, true);
        int black = 0;
        for(size_t idx=0; idx < SCREEN; idx++) black += px[idx];
        checks++;
        if (!right) failed++;
        printf("%s %s: %d pixels are black\n", right ? "ok  " : "FAIL", test->label, black);
    }

    // The steps, and of those that draw how many drew what can be seen.
    static const char* DRAWS[] = { "pat-rect", "draw-text", "copy-bits", "scroll-rect", NULL };
    // They start from a screen with something all over it, so that what a
    // step copies, scrolls or fills is not mostly white: gray, and rows of
    // text across it.
    int wrong = 0, tried[4] = { 0 }, seen[4] = { 0 };
    display_start();
    for(int y=-24; y < H; y += 24) {
        if (y < 0) {
            snprintf(typed, sizeof(typed), "p-ltgray pattern !  0 0 %d %d pat-rect  p-black pattern !", W, H);
        } else {
            snprintf(typed, sizeof(typed), "%s font !  %d bold !", FONTS[y / 24 % 2], y / 48 % 2 ? -1 : 0);
            for(int x=-(y % 7); x < W; x += 260) more("\nt\" Rows of text, 0123456789, to move.\" %d %d draw-text", x, y);
            more("\nmono font !  0 bold !");
        }
        pixels_do(typed);
        if (!typed_leaves("the screen the steps start from", typed, wrong < 5)) wrong++;
    }
    for(int n=0; n < STEPS; n++) {
        const char* does = step();
        char label[48];
        snprintf(label, sizeof(label), "step %d, %.30s", n, strrchr(does, '\n') ? strrchr(does, '\n') + 1 : does);
        pixels_do(does);
        steps++;
        int was = drew;
        if (!typed_leaves(label, does, wrong < 5)) wrong++;
        int which = among(DRAWS, strrchr(does, ' ') ? strrchr(does, ' ') + 1 : does);
        if (which >= 0) tried[which]++, seen[which] += drew - was;
    }
    failed += wrong;
    bool all_seen = seen[0] && seen[1] && seen[2] && seen[3];
    printf("%s %d steps by chance, each from where the last left the display: %d not as they should be; "
           "what could be seen changed in %d of %d fills, %d of %d texts, %d of %d copies and %d of %d scrolls\n",
           wrong || !all_seen ? "FAIL" : "ok  ", STEPS, wrong,
           seen[0], tried[0], seen[1], tried[1], seen[2], tried[2], seen[3], tried[3]);

    printf("the display, cells of %d bits, %d by %d, seed %d: %d checks and %d steps, %d failed; "
           "%d pixels compared with the same drawn a pixel at a time\n",
           CELL_BITS, W, H, given, checks, steps, failed, compared);
    return(failed || !compared || !all_seen);
}
