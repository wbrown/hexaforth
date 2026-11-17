//
// The system's tables, read from its source.
//
// A font, a pattern and where the windows go are tables in the Forth,
// written as source to be read (forth/mono.fs and sans.fs, display.fs,
// paint.fs, desk.fs), and a test that needs one reads it from there: it has no
// glyph, no pattern and no place of its own.
//

#ifndef HEXAFORTH_TABLES_H
#define HEXAFORTH_TABLES_H

#include <stdbool.h>
#include <stdint.h>

// Where the Forth is.  This is said once, first.
void tables_are(const char* dir);

// The numbers a table is: those laid down after `create NAME` in a file of
// the source.  Each is a number as Forth has it, with # before it or $ if
// it is hex or neither, and after each is the word that lays it down; the
// table ends at the first word that is neither.  Returns how many there
// are, which is to be `count` if that is not 0: the test stops if a table
// is not there or is not as many.  The numbers are made here, as many as
// there are, and are whoever asked's to free.
int table(const char* file, const char* name, int64_t** numbers, int count);

// A font, as mono.fs says one is, read from the file named for it (mono.fs,
// sans.fs): how high, the first character, how many,
// the numbers in a row of the strike, a cell the system uses; then where
// each glyph starts and where the last ends; then the strike a row at a
// time, 64 pixels to a number with the leftmost highest.
typedef struct {
    int rows, first, count, across;
    const int64_t* starts;
    const int64_t* strike;
} font;

font font_named(const char* name);

// The glyph a font has for a character, which is a space's if it has not
// the character: where it starts in the strike, and how wide it is.
int glyph_of(const font* f, int c, int* width);

// A pixel of the strike: true if it is black.
bool strike_pixel(const font* f, int x, int row);

// A pattern: eight rows of eight pixels, the leftmost the highest bit.
// One pixel of it, where the screen has it: true if it is black.
typedef struct { int64_t rows[8]; } pattern;

pattern pattern_named(const char* file, const char* name);
bool pattern_pixel(const pattern* p, int x, int y);

#endif //HEXAFORTH_TABLES_H
