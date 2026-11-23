//
// room.h - growable storage, for any file that includes it.
//
// Code that reads an image, a file or a machine's output does not know in
// advance how much there will be, and nothing here has a fixed limit.
// Either the items are counted first and exactly enough memory is
// allocated, or a list grows as items are added.  If memory runs out, the
// program prints a message and exits.  It never continues with less than
// was asked for.
//

#ifndef HEXAFORTH_ROOM_H
#define HEXAFORTH_ROOM_H

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Allocates zeroed memory for `count` items of `size` bytes each.
static void* room_for(size_t count, size_t size) {
    void* room = calloc(count ? count : 1, size ? size : 1);
    if (!room) {
        fprintf(stderr, "there is no room for %zu of %zu bytes\n", count, size);
        exit(EXIT_FAILURE);
    }
    return(room);
}

// Makes sure a list has room for at least `count` items.  `*room` is its
// capacity, which is updated.  Returns the list, which may have moved.
// New space is zeroed.  The capacity at least doubles when it grows, so a
// list that grows one item at a time is not moved every time.
static void* room_for_as_many(void* list, long count, long* room, size_t size) {
    if (count <= *room) return(list);
    long more = 2 * *room > count ? 2 * *room : count;
    list = realloc(list, more * size);
    if (!list) {
        fprintf(stderr, "there is no room for %ld of %zu bytes\n", more, size);
        exit(EXIT_FAILURE);
    }
    memset((char*)list + *room * size, 0, (more - *room) * size);
    *room = more;
    return(list);
}

// Makes sure a list of `count` items has room for one more.
static void* room_for_one_more(void* list, long count, long* room, size_t size) {
    return(room_for_as_many(list, count + 1, room, size));
}

// The same as macros, which assign the result back to the list.  ONE_MORE
// makes room for COUNT + 1 items, and AS_MANY for COUNT items.
#define ONE_MORE(list, count, room) ((list) = room_for_one_more((list), (count), &(room), sizeof(*(list))))
#define AS_MANY(list, count, room)  ((list) = room_for_as_many((list), (count), &(room), sizeof(*(list))))

// A set of non-zero numbers, of any size: an open-addressed hash table.
// It is kept at most half full, so a lookup takes a probe or two.
typedef struct {
    uint64_t* at;
    int bits;                           // the table has 2^bits slots
    long n;
} number_set;

static uint64_t* number_at(const number_set* s, uint64_t x) {   // returns a number's slot, or the empty slot where it would go
    uint64_t at = (x * 0x9e3779b97f4a7c15ull) >> (8 * sizeof(uint64_t) - s->bits);
    while (s->at[at] && s->at[at] != x) at = (at + 1) & (((uint64_t)1 << s->bits) - 1);
    return(&s->at[at]);
}

// Returns 1 if a number is already in the set.  If it is not, adds it and
// returns 0.
static int already_in(number_set* s, uint64_t x) {
    if (!s->at || 2 * (s->n + 1) > (long)1 << s->bits) {
        number_set had = *s;
        s->bits = had.at ? had.bits + 1 : 1;
        s->at = room_for((size_t)1 << s->bits, sizeof(uint64_t));
        for(long idx=0; had.at && idx < (long)1 << had.bits; idx++) if (had.at[idx]) *number_at(s, had.at[idx]) = had.at[idx];
        free(had.at);
    }
    uint64_t* at = number_at(s, x);
    if (*at) return(1);
    *at = x;
    s->n++;
    return(0);
}

// Appends printf-formatted text to a string, growing the string to fit.
// `*text` is reallocated, and may be NULL to start a new string.
static void text_more(char** text, const char* format, ...) {
    va_list args, again;
    va_start(args, format);
    va_copy(again, args);
    size_t had = *text ? strlen(*text) : 0;
    int more = vsnprintf(NULL, 0, format, args);        // measure the text first, then write it
    va_end(args);
    *text = realloc(*text, had + more + 1);
    if (!*text) {
        fprintf(stderr, "there is no room for %zu characters\n", had + more + 1);
        exit(EXIT_FAILURE);
    }
    vsnprintf(*text + had, (size_t)more + 1, format, again);
    va_end(again);
}

// Reads a whole file.  Returns its length, and sets `*bytes` to an
// allocated buffer with its contents.  Returns 0 and sets `*bytes` to NULL
// if the file cannot be opened.
static size_t file_bytes(const char* path, unsigned char** bytes) {
    FILE* in = fopen(path, "rb");
    *bytes = NULL;
    if (!in) return(0);
    long room = 0;
    size_t have = 0;
    unsigned char* all = NULL;
    for(;;) {                           // read to the end, growing the buffer whenever it fills
        all = room_for_as_many(all, (long)have + BUFSIZ, &room, 1);
        size_t got = fread(all + have, 1, room - have, in);
        if (!got) break;
        have += got;
    }
    fclose(in);
    *bytes = all;
    return(have);
}

// Writes `len` bytes of text to a file, replacing it.  Returns 0 if the
// file could not be written.
static int write_whole(const char* path, const char* text, size_t len) {
    FILE* out = fopen(path, "wb");
    if (!out) return(0);
    int all = fwrite(text, 1, len, out) == len;
    return(!fclose(out) && all);
}

// Reads one line of a file, of any length, including its newline.  Returns
// the line, or NULL at the end of the file.  `*line` and `*room` are the
// buffer and its capacity, which are reused and grown from one call to the
// next.
static char* line_of(FILE* in, char** line, long* room) {
    long have = 0;
    int c;
    while ((c = fgetc(in)) != EOF) {
        ONE_MORE(*line, have + 1, *room);
        (*line)[have++] = (char)c;
        if (c == '\n') break;
    }
    if (!have) return(NULL);
    (*line)[have] = '\0';
    return(*line);
}

#endif // HEXAFORTH_ROOM_H
