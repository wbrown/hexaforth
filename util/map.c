//
// memory-map IMAGE.hex IMAGE.tables [MEMORY.out]
//
// Where a machine's memory goes: each part of it, in the order it is in
// memory, what is in it, and what is free between the parts up to the end
// of memory.  Of the image as cross.fs built it; or, given the memory the
// reference VM writes when it stops (the file after the disk that
// `hexaforth` is given, or MEMORY for the desktop's test), of the machine
// as it was then, started from that image.
//
// The parts:
//
//   the kernel's ROM, and each page of code: the code and the tables of
//   the words that start on it (layout.h); and the code compiled at run
//   time, from where the image left the code pointer (cp) to where it is
//
//   the data, from VAR_BASE to the data pointer (dp)
//
//   the display, where vm_constants.h says it is
//
//   the dictionary's headers, from the oldest to the newest, and the
//   header pointer (hdp)
//
//   the heap: what has been given out, from where the image left himem to
//   where it is, and of that the run that has been given back (hole).  Who
//   it was given to is not in memory, and is not guessed here: the
//   reference VM that watches the heap says, of a machine it ran
//   (bench/heap.c)
//
//   and what is not nothing and in none of those: what the image has there,
//   which is data that cross.fs laid down in a place of its own (bulk,
//   high); or what it has not, which is memory that was given out and has
//   been given back, or was stored to where nothing is kept
//
// A place is the byte it starts at and the cell that is, in hex, and a size
// is in bytes.  At the end it says where the last part ends.  It is built
// for the machine whose memory it is (vm_constants.h), and fails if it
// finds no words in the image: then it looked at nothing.
//

#include <stdarg.h>
#include "layout.h"

#define MEMORY ((size_t)CELL_BYTES << ADDR_BITS)       // the machine's memory, in bytes
#define CLOSE  256                                      // the bytes of nothing that two stretches of something may have between them and be one

static layout IMAGE;
static uint8_t* memory;         // the memory the map is of: what the machine left, or the image
static size_t memory_len;

typedef struct {
    size_t from, to;            // the bytes it is
    int inner;                  // a part of the part it is in, said under it
    char* what;
} part;
static part* parts;
static long nparts, parts_room;

static char* said(const char* format, va_list args) {
    va_list again;
    va_copy(again, args);
    int len = vsnprintf(NULL, 0, format, args);
    char* text = room_for((size_t)len + 1, 1);
    vsnprintf(text, (size_t)len + 1, format, again);
    va_end(again);
    return(text);
}

static void part_is(size_t from, size_t to, int inner, const char* format, ...) {
    ONE_MORE(parts, nparts, parts_room);
    va_list args;
    va_start(args, format);
    parts[nparts].what = said(format, args);
    va_end(args);
    parts[nparts].from = from;
    parts[nparts].to = to;
    parts[nparts++].inner = inner;
}

static int by_where(const void* a, const void* b) {
    const part* x = a;
    const part* y = b;
    if (x->from != y->from) return(x->from < y->from ? -1 : 1);
    if (x->inner != y->inner) return(x->inner - y->inner);
    return(x->to < y->to ? -1 : x->to > y->to);
}

// A cell of the memory the map is of, or nothing if it has no such cell.
static uint64_t in_memory(uint64_t cell) {
    return((cell + 1) * CELL_BYTES <= memory_len ? cell_of(memory, cell) : 0);
}

// The cell a variable of the nucleus is: a label of data in the file of
// names, or a word that pushes it, as one made with create does.  Nothing,
// if the image has neither.
static uint64_t variable(const char* tables, const char* name) {
    uint64_t at = data_called(tables, name);
    if (at) return(at);
    long place = place_of(IMAGE.words, IMAGE.n, name);
    return(place >= 0 ? pushed_by(IMAGE.bytes, place) : 0);
}

// Is a byte in one of the parts said so far?
static int in_a_part(size_t byte) {
    for(long idx=0; idx < nparts; idx++) {
        if (!parts[idx].inner && byte >= parts[idx].from && byte < parts[idx].to) return(1);
    }
    return(0);
}

static void place_said(size_t byte) {
    printf("  $%-8zx $%-8zx", byte, byte / CELL_BYTES);
}

int main(int argc, char *argv[]) {
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "usage: memory-map IMAGE.hex IMAGE.tables [MEMORY.out]\n");
        return(1);
    }
    const char* tables = argv[2];
    lay_out(&IMAGE, argv[1], tables);
    if (argc == 4) {
        memory_len = file_bytes(argv[3], &memory);
        if (memory_len != MEMORY) {
            fprintf(stderr, "memory-map: %s is %zu bytes, and this machine's memory is %zu\n", argv[3], memory_len, MEMORY);
            return(1);
        }
    } else {
        memory = IMAGE.bytes;
        memory_len = IMAGE.len;
    }

    // The nucleus's pointers, where the image left them and where they are.
    uint64_t dp = variable(tables, "dp"), hdp = variable(tables, "hdp"), cp = variable(tables, "cp");
    uint64_t himem = variable(tables, "himem"), hole = variable(tables, "hole");
    uint64_t data_then = cell_of(IMAGE.bytes, dp), data_now = in_memory(dp);
    uint64_t headers_then = cell_of(IMAGE.bytes, hdp), headers_now = in_memory(hdp);
    uint64_t code_then = cell_of(IMAGE.bytes, cp), code_now = in_memory(cp);
    uint64_t heap_then = cell_of(IMAGE.bytes, himem), heap_now = in_memory(himem);
    if (!dp || !hdp || !cp || !himem) {
        fprintf(stderr, "memory-map: the image does not say where its data, headers, code or heap are\n");
        return(1);
    }

    // The kernel's ROM and the pages of code.
    const long two = sizeof(instruction);                   // the bytes of an instruction
    part_is(0, KROM_BYTES, 0, "the kernel's ROM: %ld bytes of code, %ld left",
            IMAGE.kernel * two, (long)KROM_BYTES - IMAGE.kernel * two);
    long rest_words = 0, rest_code = 0, rest_end = 0;
    for(int idx=0; idx < IMAGE.n; idx++) {
        const word* w = &IMAGE.words[idx];
        if (!w->size || w->at < KROM_BYTES / two || w->at >= PAGE) continue;
        rest_words++;
        rest_code += w->size;
        if (w->at + w->size > rest_end) rest_end = w->at + w->size;
    }
    if (rest_words) {
        part_is(KROM_BYTES, PAGE * two, 0, "page 0 after the ROM: %ld words, %ld bytes of code, %ld left",
                rest_words, rest_code * two, (PAGE - rest_end) * two);
    }
    for(long page=1; page < IMAGE.npages; page++) {
        const code_page* has = &IMAGE.pages[page];
        if (!has->words) continue;
        part_is(page * PAGE * two, (page + 1) * PAGE * two, 0,
                "page %ld: %ld words, %ld bytes of code and %ld of tables, %ld left",
                page, has->words, has->code * two, has->tables * two, (PAGE - has->end) * two);
    }
    if (code_now > code_then) {
        long first = (long)(code_then / PAGE), last = (long)((code_now - 1) / PAGE);
        if (first >= IMAGE.npages || !IMAGE.pages[first].words) {
            if (last > first) {
                part_is(first * PAGE * two, (last + 1) * PAGE * two, 0, "pages %ld to %ld: code compiled at run time", first, last);
            } else {
                part_is(first * PAGE * two, (first + 1) * PAGE * two, 0, "page %ld: code compiled at run time", first);
            }
        }
        part_is(code_then * two, code_now * two, 1, "compiled at run time: %llu bytes",
                (unsigned long long)((code_now - code_then) * two));
    }

    // The data, the display and the dictionary.
    part_is((size_t)VAR_BASE * CELL_BYTES, data_now, 0, "the data, to the data pointer: %llu bytes, %llu of them made at run time",
            (unsigned long long)(data_now - VAR_BASE * CELL_BYTES),
            (unsigned long long)(data_now > data_then ? data_now - data_then : 0));
#if DISP_W
    uint64_t display_end = DISP_BASE + (uint64_t)(DISP_BANKS - 1) * DISP_BANK_STEP + (uint64_t)DISP_ROWS_PER_BANK * DISP_STRIDE;
    part_is((size_t)DISP_BASE * CELL_BYTES, display_end * CELL_BYTES, 0, "the display: %d by %d", DISP_W, DISP_H);
#endif
    size_t headers_from, headers_to;
    int headers = headers_of(memory, memory_len, &headers_from, &headers_to);
    if (headers_now > headers_to) headers_to = headers_now;
    part_is(headers_from, headers_to, 0, "the dictionary: %d headers, %llu bytes of them made at run time",
            headers, (unsigned long long)(headers_now > headers_then ? headers_now - headers_then : 0));

    // The heap, what of it has been given back, and what holds a place in it.
    if (heap_now > heap_then) {
        part_is(heap_then * CELL_BYTES, heap_now * CELL_BYTES, 0, "the heap: given out");
    } else {
        part_is(heap_then * CELL_BYTES, heap_then * CELL_BYTES, 0, "the heap starts here: nothing given out");
    }
    uint64_t hole_at = hole ? in_memory(hole) : 0, hole_cells = hole ? in_memory(hole + 1) : 0;
    if (hole_cells) {
        part_is(hole_at * CELL_BYTES, (hole_at + hole_cells) * CELL_BYTES, 1, "given back, to be had again (hole)");
    }

    // What is not nothing and in none of those parts: what the image has
    // there, or what it has not.  Stretches of one or the other less than
    // CLOSE bytes apart are one.
    static const char* KINDS[] = {
        "not nothing, and not the image's: given out and given back, or stored where nothing is kept",
        "laid down by the image in a place of its own (bulk, high)"};
    size_t from = 0, last = 0;
    int in = -1;                                        // which of the two the stretch is, if there is one
    for(size_t byte=0; byte + CELL_BYTES <= memory_len; byte += CELL_BYTES) {
        if (!cell_of(memory, byte / CELL_BYTES) || in_a_part(byte)) continue;
        int kind = byte + CELL_BYTES <= IMAGE.len && cell_of(IMAGE.bytes, byte / CELL_BYTES);
        if (in >= 0 && (byte - last > CLOSE || kind != in)) {
            part_is(from, last, 0, "%s: %zu bytes", KINDS[in], last - from);
            in = -1;
        }
        if (in < 0) from = byte, in = kind;
        last = byte + CELL_BYTES;
    }
    if (in >= 0) part_is(from, last, 0, "%s: %zu bytes", KINDS[in], last - from);

    // The parts in order, and what is free between them.
    qsort(parts, nparts, sizeof(part), by_where);
    if (argc == 4) printf("%s, as the machine left it, started from %s\n", argv[3], argv[1]);
    else printf("%s, as cross.fs built it\n", argv[1]);
    printf("a machine of %d-bit cells, with %zu bytes of memory\n\n", CELL_BITS, MEMORY);
    printf("  byte      cell          bytes  what\n");
    size_t reached = 0, used = 0;
    for(long idx=0; idx < nparts; idx++) {
        const part* p = &parts[idx];
        if (!p->inner && p->from > reached) {
            place_said(reached);
            printf(" %10zu  free\n", p->from - reached);
        }
        place_said(p->from);
        printf(" %10zu  %s%s\n", p->to - p->from, p->inner ? "  " : "", p->what);
        if (!p->inner && p->to > reached) reached = p->to;
        if (!p->inner && p->to > used) used = p->to;
    }
    if (reached < MEMORY) {
        place_said(reached);
        printf(" %10zu  free, to the end of memory\n", MEMORY - reached);
    }
    printf("\nthe last part ends at byte $%zx, cell $%zx: %zu bytes from the start of memory\n",
           used, used / CELL_BYTES, used);
    return(!IMAGE.n);
}
