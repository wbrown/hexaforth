//
// room.h - room for as many of a thing as there are, for whatever has this
// in it.
//
// Nothing that reads an image, a file or what a machine printed knows
// beforehand how much of it there will be, and nothing here has room for
// only so much: what there is is counted and room made for it, or a list
// is given more room as it grows.  If there is no more room to be had it
// stops and says so; it does not go on with less than it was asked for.
//

#ifndef HEXAFORTH_ROOM_H
#define HEXAFORTH_ROOM_H

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Room for so many of something, all of it nothing.
static void* room_for(size_t count, size_t size) {
    void* room = calloc(count ? count : 1, size ? size : 1);
    if (!room) {
        fprintf(stderr, "there is no room for %zu of %zu bytes\n", count, size);
        exit(EXIT_FAILURE);
    }
    return(room);
}

// A list that has room for `*room` things, with room for at least `count`:
// the same list, or the same things somewhere bigger.  What is new is
// nothing.  It is given more than was asked, so that a list that grows a
// thing at a time is not moved every time.
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

// The same for a list that has `count` things in it and is to have one
// more.
static void* room_for_one_more(void* list, long count, long* room, size_t size) {
    return(room_for_as_many(list, count + 1, room, size));
}

// The same for a list by name: LIST has room for ROOM things, and is to
// have room for one more than COUNT, or for as many as COUNT.
#define ONE_MORE(list, count, room) ((list) = room_for_one_more((list), (count), &(room), sizeof(*(list))))
#define AS_MANY(list, count, room)  ((list) = room_for_as_many((list), (count), &(room), sizeof(*(list))))

// Numbers that have been seen, however many: none of them is nothing.
// There is room for at least twice as many as there are, so that one is
// found, or found not to be there, in a step or two.
typedef struct {
    uint64_t* at;
    int bits;                           // there is room for two to this many
    long n;
} number_set;

static uint64_t* number_at(const number_set* s, uint64_t x) {   // where a number is, or would be
    uint64_t at = (x * 0x9e3779b97f4a7c15ull) >> (8 * sizeof(uint64_t) - s->bits);
    while (s->at[at] && s->at[at] != x) at = (at + 1) & (((uint64_t)1 << s->bits) - 1);
    return(&s->at[at]);
}

// Has a number been seen?  It has now.
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

// Text made as printf makes it, put after the text there is: all of it,
// however long it comes to.  The text is made here, and with none to begin
// with it is begun.
static void text_more(char** text, const char* format, ...) {
    va_list args, again;
    va_start(args, format);
    va_copy(again, args);
    size_t had = *text ? strlen(*text) : 0;
    int more = vsnprintf(NULL, 0, format, args);        // how long it is, and then it is written
    va_end(args);
    *text = realloc(*text, had + more + 1);
    if (!*text) {
        fprintf(stderr, "there is no room for %zu characters\n", had + more + 1);
        exit(EXIT_FAILURE);
    }
    vsnprintf(*text + had, (size_t)more + 1, format, again);
    va_end(again);
}

// A whole file, as bytes: how many, and the bytes, which are made here.
// Nothing, if there is no such file.
static size_t file_bytes(const char* path, unsigned char** bytes) {
    FILE* in = fopen(path, "rb");
    *bytes = NULL;
    if (!in) return(0);
    long room = 0;
    size_t have = 0;
    unsigned char* all = NULL;
    for(;;) {                           // to its end, with more room whenever what there is is full
        all = room_for_as_many(all, (long)have + BUFSIZ, &room, 1);
        size_t got = fread(all + have, 1, room - have, in);
        if (!got) break;
        have += got;
    }
    fclose(in);
    *bytes = all;
    return(have);
}

// Text that is to be a file's, written there whole.  False if it could not
// be written.
static int write_whole(const char* path, const char* text, size_t len) {
    FILE* out = fopen(path, "wb");
    if (!out) return(0);
    int all = fwrite(text, 1, len, out) == len;
    return(!fclose(out) && all);
}

// A line of a file, however long: the text, which is made here and is the
// same room the next time, or nothing at the end of the file.
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
