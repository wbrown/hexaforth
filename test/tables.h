//
// Reads the system's tables from its Forth source.
//
// A font, a pattern and the positions of the windows are tables in the
// Forth, written as readable source (forth/mono.fs and sans.fs,
// display.fs, paint.fs, desk.fs).  A test that needs one reads it from
// there, so no test has its own copy of a glyph, a pattern or a position.
//

#ifndef HEXAFORTH_TABLES_H
#define HEXAFORTH_TABLES_H

#include <stdbool.h>
#include <stdint.h>

// Sets the directory the Forth source is in.  Call it once, before
// anything else.
void tables_are(const char* dir);

// Reads the numbers of a table: the ones compiled after `create NAME` in a
// source file.  Each is a number in Forth notation (decimal, optionally
// with a # prefix, or hex with a $ prefix) followed by the word that
// compiles it, which ends in a comma.  The table ends at the first word
// that is neither.  Returns how many numbers there are.  If `count` is not
// 0, the table must have exactly that many.  The program exits if the
// table is missing or has the wrong count.  The numbers are stored in an
// allocated array at `*numbers`, which the caller frees.
int table(const char* file, const char* name, int64_t** numbers, int count);

// A font in the layout mono.fs describes, read from the file named after
// it (mono.fs, sans.fs, spin.fs).  The table is:
//
//   the height in rows, the first character, the number of characters,
//   the number of numbers in a row of the strike, and a cell the system
//   uses
//   the column where each glyph starts, and then the column where the last
//   glyph ends
//   the strike, row by row, 64 pixels to a number with the leftmost pixel
//   in the highest bit.  A strike can instead be drawn as a picture of #
//   and . (spin.fs), which is read as the same numbers.
typedef struct {
    int rows, first, count, across;
    const int64_t* starts;
    const int64_t* strike;
} font;

font font_named(const char* name);

// Returns the column in the strike where a character's glyph starts, and
// stores the glyph's width in `*width`.  A character the font lacks gets
// the glyph of the font's first character, which is a space.
int glyph_of(const font* f, int c, int* width);

// Returns true if a pixel of the strike is black.
bool strike_pixel(const font* f, int x, int row);

// A pattern is eight rows of eight pixels, with the leftmost pixel in the
// highest bit.  pattern_pixel() returns true if the pattern is black at a
// screen position.
typedef struct { int64_t rows[8]; } pattern;

pattern pattern_named(const char* file, const char* name);
bool pattern_pixel(const pattern* p, int x, int y);

#endif //HEXAFORTH_TABLES_H
