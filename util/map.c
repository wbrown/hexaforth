//
// memory-map IMAGE.hex IMAGE.tables [MEMORY.out]
//
// Prints a map of a machine's memory: every part of it in address order,
// what each part holds, and the free space between the parts up to the
// end of memory.  With two arguments the map is of the image as cross.fs
// built it.  With MEMORY.out it is of a machine that started from that
// image and ran.  MEMORY.out is the memory file the reference VM writes
// when it stops (the file named after the disk on `hexaforth`'s command
// line, or MEMORY for the desktop's test).
//
// The parts:
//
//   The kernel's ROM, and each page of code: the code of the words that
//   start on the page (layout.h).  Also the code compiled
//   at run time, from where the image left the code pointer (cp) to where
//   the pointer is now.
//
//   The data, from VAR_BASE to the data pointer (dp).
//
//   The display, at the address vm_constants.h gives.
//
//   The dictionary's headers, from the oldest to the newest, and the
//   header pointer (hdp).
//
//   The heap: the memory allocated so far, from where the image left himem
//   to where himem is now, and within it the block that has been freed for
//   reuse (hole).  Memory does not record who allocated a block, and this
//   program does not guess.  The reference VM that watches the heap
//   reports that for a machine it ran (bench/heap.c).
//
//   Everything else that is not zero.  Where the image has data there,
//   cross.fs placed that data outside the variables (bulk, high).  Where
//   the image has none, it is memory that was allocated and then freed, or
//   a store to an address where nothing is kept.
//
// An address is printed as the byte a part starts at and as that byte's
// cell number, both in hex.  Sizes are in bytes.  The last line gives
// where the last part ends.  The machine is the one the image was built
// for.  It fails if it finds no words in the image, because then it
// examined nothing.
//
// memory-map --table WHOLE.hex WHOLE.tables BOOT.hex BOOT.tables MEMORY.out
//
// Prints the same information as a table that can be read at a glance:
// one line for each part that has a limit, with the bytes used and the
// bytes left, for two machines.  The first is WHOLE.hex, the system as
// cross.fs built it with the applications in it.  The second is the
// machine that started from BOOT.hex, the system without the
// applications, and compiled them from its disk.  MEMORY.out is that
// machine's memory afterwards (the targets quick and check print the
// table).  The space a part has left comes from the nucleus's own
// pointers, not from which bytes of memory are zero:
//
//   the kernel's ROM, and    used up to the end of the last word in each
//   the rest of its page
//   a page of code           used up to where its code has got to (pages,
//                            nuc.fs), and free from there to its end.  A
//                            page that ends in a table has no space left,
//                            and its line says so
//   the scratch              the end of the last page of code
//                            (placement.fs)
//   variables                used from VAR_BASE to the data pointer, and
//                            free from there to dpl
//   bulk                     used up to its pointer, and free from there
//                            to its limit
//   the dictionary           used up to the header pointer, and free from
//                            there to hdl
//   the heap                 used from where the image left himem to where
//                            himem is now, and free from there to the end
//                            of the memory that can be allocated
//
// Memory is cells, and nothing in the machine distinguishes code from
// data: a page is the range one call instruction reaches, and which cells
// hold code depends on how cross.fs laid out the image.  So every size is
// in bytes, which add up, and an address is the byte a part starts at.
// Where a part's limit has a unit of its own, the space left is also given
// in that unit: instructions for a page of code, cells for data.
//

#include <stdarg.h>
#include "layout.h"

#define MEMORY ((size_t)CELL_BYTES << ADDR_BITS)       // the machine's memory, in bytes
#define CLOSE  256                                      // non-zero stretches separated by at most this many bytes are reported as one

static layout IMAGE;
static uint8_t* memory;         // the memory being mapped: the machine's memory file, or the image
static size_t memory_len;

typedef struct {
    size_t from, to;            // its first byte, and the byte after its last
    int inner;                  // 1 if it lies inside another part, and is printed indented under that one
    char* what;                 // its description
} part;
static part* parts;
static long nparts, parts_room;

// Returns a formatted string in allocated memory.
static char* said(const char* format, va_list args) {
    va_list again;
    va_copy(again, args);
    int len = vsnprintf(NULL, 0, format, args);
    char* text = room_for((size_t)len + 1, 1);
    vsnprintf(text, (size_t)len + 1, format, again);
    va_end(again);
    return(text);
}

// Adds a part to the list.  Its description is given as a printf format
// and arguments.
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

// Returns the value of a cell of the memory being mapped, or 0 if the
// memory does not reach that cell.
static uint64_t in_memory(uint64_t cell) {
    return((cell + 1) * CELL_BYTES <= memory_len ? cell_of(memory, cell) : 0);
}

// Returns the address of one of the nucleus's variables.  The address is
// a data label in the .tables file, or the number pushed by a word of that
// name (a word made with `create` pushes its address).  Returns 0 if the
// image has neither.
static uint64_t variable(const char* tables, const char* name) {
    uint64_t at = data_called(tables, name);
    if (at) return(at);
    long place = place_of(IMAGE.words, IMAGE.n, name);
    return(place >= 0 ? pushed_by(IMAGE.bytes, place) : 0);
}

// Returns 1 if a byte is inside one of the parts listed so far.
static int in_a_part(size_t byte) {
    for(long idx=0; idx < nparts; idx++) {
        if (!parts[idx].inner && byte >= parts[idx].from && byte < parts[idx].to) return(1);
    }
    return(0);
}

static void place_said(size_t byte) {
    printf("  $%-8zx $%-8zx", byte, byte / CELL_BYTES);
}

// ---- the table

enum { NO_UNIT, INSTRUCTIONS, CELLS };
enum { BUILT, BOOTED, STATES };

typedef struct {
    long used, left;            // in bytes; `left` is negative for a part that has no limit
    int is;                     // 1 if this machine has the part
} amount;

typedef struct {
    char what[40];
    size_t from;                // the byte it starts at
    int unit;                   // the unit the space left is also given in
    amount of[STATES];
} row;
static row* ROWS;
static long nrows, rows_room;

// Records one machine's figures for a part, in the row with that name and
// address, which is created if it does not exist.
static void row_is(int state, const char* what, size_t from, long used, long left, int unit) {
    row* r = NULL;
    for(long idx=0; idx < nrows && !r; idx++) if (ROWS[idx].from == from && !strcmp(ROWS[idx].what, what)) r = &ROWS[idx];
    if (!r) {
        ONE_MORE(ROWS, nrows, rows_room);
        r = &ROWS[nrows++];
        memset(r, 0, sizeof(row));
        snprintf(r->what, sizeof(r->what), "%s", what);
        r->from = from;
        r->unit = unit;
    }
    r->of[state].used = used;
    r->of[state].left = left;
    r->of[state].is = 1;
}

static int by_from(const void* a, const void* b) {
    const row* x = a;
    const row* y = b;
    if (x->from != y->from) return(x->from < y->from ? -1 : 1);
    return(strcmp(x->what, y->what));
}

// Returns the address of a variable of an image's nucleus, found the same
// way as variable().
static uint64_t cell_named(const layout* m, const char* tables, const char* name) {
    uint64_t at = data_called(tables, name);
    if (at) return(at);
    long place = place_of(m->words, m->n, name);
    return(place >= 0 ? pushed_by(m->bytes, place) : 0);
}

// Returns the number that a word of an image pushes, or 0 if the image has
// no such word.  That is how an image makes a constant of cross.fs
// available (names.fs).
static long pushed_named(const layout* m, const char* name) {
    long place = place_of(m->words, m->n, name);
    return(place >= 0 ? (long)pushed_by(m->bytes, place) : 0);
}

// Adds the rows for one machine.  `m` is its image, and `mem` is the
// memory of the machine that started from that image, which is the image
// itself if the machine never ran.
static void rows_of(int state, const layout* m, const char* tables, const uint8_t* mem, size_t mem_len) {
#define NOW(cell) ((cell) && ((uint64_t)(cell) + 1) * CELL_BYTES <= mem_len ? cell_of(mem, (cell)) : 0)
    const long two = sizeof(instruction);

    long rest_end = KROM_BYTES / two;
    for(int idx=0; idx < m->n; idx++) {
        const word* w = &m->words[idx];
        if (w->size && w->at >= KROM_BYTES / two && w->at < PAGE && w->at + w->size > rest_end) rest_end = w->at + w->size;
    }
    row_is(state, "the kernel's ROM", 0, m->kernel * two, (long)KROM_BYTES - m->kernel * two, INSTRUCTIONS);
    row_is(state, "page 0 past the ROM", KROM_BYTES, rest_end * two - (long)KROM_BYTES, (PAGE - rest_end) * two, INSTRUCTIONS);

    // The pages of code: page 1, and the pages after it that cross.fs
    // reserves for code.  If the image does not record which pages those
    // are, they are the pages its words are on.  The last page of code
    // ends where the scratch starts.
    uint64_t pages = cell_named(m, tables, "pages"), scratch = cell_named(m, tables, "scratch");
    long cp = (long)NOW(cell_named(m, tables, "cp"));
    long code_base = pushed_named(m, "code-base"), code_end = code_base + pushed_named(m, "code-size");
    long scratch_from = scratch ? (long)cell_of(m->bytes, scratch) : 0, scratch_now = scratch ? (long)NOW(scratch) : 0;
    long last = code_base ? code_end / PAGE : m->npages;
    for(long page=1; page < last; page++) {
        int kept_for_code = code_base ? page >= code_base / PAGE : page < m->npages && m->pages[page].words;
        if (page != 1 && !kept_for_code) continue;
        long start = page * PAGE, end = start + PAGE, got_to = pages ? (long)NOW(pages + page) : 0;
        if (!got_to && page < m->npages && m->pages[page].words) got_to = start + m->pages[page].end;
        if (cp / PAGE == page && cp > got_to) got_to = cp;      // code is being compiled onto this page, past the recorded position
        if (got_to < start) got_to = start;
        if (scratch_from / PAGE == page) end = scratch_from;
        char what[40];
        snprintf(what, sizeof(what), "page %ld", page);
        row_is(state, what, start * two, (got_to - start) * two, (end - got_to) * two, INSTRUCTIONS);
    }
    if (scratch_from) {
        row_is(state, "the scratch", scratch_from * two, (scratch_now - scratch_from) * two, (code_end - scratch_now) * two,
               INSTRUCTIONS);
    }

    // The data and the dictionary, from the nucleus's pointers and limits.
    long dp = (long)NOW(cell_named(m, tables, "dp")), dpl = (long)NOW(cell_named(m, tables, "dpl"));
    long bdp = (long)NOW(cell_named(m, tables, "bdp")), bdl = (long)NOW(cell_named(m, tables, "bdl"));
    long hdp = (long)NOW(cell_named(m, tables, "hdp")), hdl = (long)NOW(cell_named(m, tables, "hdl"));
    const long data_from = (long)VAR_BASE * CELL_BYTES;
    if (dp) row_is(state, "variables", data_from, dp - data_from, dpl - dp, CELLS);
    if (bdp) row_is(state, "bulk", dpl, bdp - dpl, bdl - bdp, CELLS);
    size_t headers_from, headers_to;
    headers_of(mem, mem_len, &headers_from, &headers_to);
    if (hdp) row_is(state, "the dictionary", headers_from, hdp * CELL_BYTES - (long)headers_from, (hdl - hdp) * CELL_BYTES, CELLS);

    // The display, the data the image placed above it, and the heap.
    uint64_t himem = cell_named(m, tables, "himem");
    long heap_from = himem ? (long)cell_of(m->bytes, himem) : 0, heap_now = (long)NOW(himem);
    long given_to = pushed_named(m, "mem-end");
    if (!given_to) given_to = (long)(MEMORY / CELL_BYTES);
    if (DISP_W) {
        long display_end = DISP_BASE + (long)(DISP_BANKS - 1) * DISP_BANK_STEP + (long)DISP_ROWS_PER_BANK * DISP_STRIDE;
        row_is(state, "the display", (size_t)DISP_BASE * CELL_BYTES, (display_end - DISP_BASE) * CELL_BYTES, -1, NO_UNIT);
        if (heap_from > display_end) {
            row_is(state, "data above the display", (size_t)display_end * CELL_BYTES, (heap_from - display_end) * CELL_BYTES,
                   -1, NO_UNIT);
        }
    }
    if (himem) {
        row_is(state, "the heap", (size_t)heap_from * CELL_BYTES, (heap_now - heap_from) * CELL_BYTES,
               (given_to - heap_now) * CELL_BYTES, CELLS);
    }
#undef NOW
}

static void amount_said(const amount* a) {
    if (!a->is) printf(" %9s %8s", "", "");
    else if (a->left < 0) printf(" %9ld %8s", a->used, "");
    else printf(" %9ld %8ld", a->used, a->left);
}

static int table(char *argv[]) {
    static layout WHOLE, BOOT;
    lay_out(&WHOLE, argv[0], argv[1]);
    lay_out(&BOOT, argv[2], argv[3]);
    uint8_t* left_as;
    size_t len = file_bytes(argv[4], &left_as);
    if (len != MEMORY) {
        fprintf(stderr, "memory-map: %s is %zu bytes, and this machine's memory is %zu\n", argv[4], len, MEMORY);
        return(1);
    }
    rows_of(BUILT, &WHOLE, argv[1], WHOLE.bytes, WHOLE.len);
    rows_of(BOOTED, &BOOT, argv[3], left_as, len);
    qsort(ROWS, nrows, sizeof(row), by_from);

    printf("a machine of %d-bit cells: a cell is %d bytes and an instruction %d; %zu bytes of memory\n",
           CELL_BITS, CELL_BYTES, (int)sizeof(instruction), MEMORY);
    printf("bytes used and left: built, as cross.fs laid the whole system; booted, as a machine left them\n"
           "that started without the applications and compiled them from its disk.  left (units) is what\n"
           "the booted machine has left, in instructions of a page of code or cells of data\n\n");
    printf("%-24s %9s %9s %8s %9s %8s   %s\n", "", "from", "built", "left", "booted", "left", "left (units)");
    for(long idx=0; idx < nrows; idx++) {
        const row* r = &ROWS[idx];
        char from[24];
        snprintf(from, sizeof(from), "$%zx", r->from);
        printf("%-24s %9s", r->what, from);
        amount_said(&r->of[BUILT]);
        amount_said(&r->of[BOOTED]);
        const amount* said_of = r->of[BOOTED].is ? &r->of[BOOTED] : &r->of[BUILT];
        long in_units = said_of->left / (r->unit == CELLS ? CELL_BYTES : (long)sizeof(instruction));
        if (r->unit != NO_UNIT && said_of->left >= 0) {
            printf("   %ld %s%s", in_units, r->unit == CELLS ? "cell" : "instruction", in_units == 1 ? "" : "s");
        }
        printf("\n");
    }
    printf("\n");
    return(!WHOLE.n || !BOOT.n);
}

int main(int argc, char *argv[]) {
    if (argc == 7 && !strcmp(argv[1], "--table")) return(table(argv + 2));
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "usage: memory-map IMAGE.hex IMAGE.tables [MEMORY.out]\n"
                        "       memory-map --table WHOLE.hex WHOLE.tables BOOT.hex BOOT.tables MEMORY.out\n");
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

    // The nucleus's pointers: their values in the image, and their values
    // now.
    uint64_t dp = variable(tables, "dp"), hdp = variable(tables, "hdp"), cp = variable(tables, "cp");
    uint64_t himem = variable(tables, "himem"), hole = variable(tables, "hole");
    uint64_t data_then = cell_of(IMAGE.bytes, dp), data_now = in_memory(dp);
    // The header pointer holds a cell address, which is converted to a
    // byte address here.
    uint64_t headers_then = cell_of(IMAGE.bytes, hdp) * CELL_BYTES, headers_now = in_memory(hdp) * CELL_BYTES;
    uint64_t code_then = cell_of(IMAGE.bytes, cp), code_now = in_memory(cp);
    uint64_t heap_then = cell_of(IMAGE.bytes, himem), heap_now = in_memory(himem);
    if (!dp || !hdp || !cp || !himem) {
        fprintf(stderr, "memory-map: the image does not say where its data, headers, code or heap are\n");
        return(1);
    }

    // The kernel's ROM and the pages of code.
    const long two = sizeof(instruction);                   // the size of an instruction in bytes
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
                "page %ld: %ld words, %ld bytes of code, %ld left",
                page, has->words, has->code * two, (PAGE - has->end) * two);
    }
    // Code compiled at run time starts in the space page 1 has left, and
    // continues on the first page the image left empty (cpn, nuc.fs).  So
    // it can be in two places.  Its part of page 1 ends at that page's
    // last non-zero instruction.
    uint64_t cpn = variable(tables, "cpn"), go_on = cpn ? cell_of(IMAGE.bytes, cpn) : code_then;
    if (code_now != code_then && code_now / PAGE != code_then / PAGE && go_on != code_then) {
        const uint16_t* code = (const uint16_t*)memory;
        uint64_t end = (code_then / PAGE + 1) * PAGE;
        while (end > code_then && !code[end - 1]) end--;
        if (end > code_then) {
            part_is(code_then * two, end * two, 1, "compiled at run time, in what the page had left: %llu bytes",
                    (unsigned long long)((end - code_then) * two));
        }
        code_then = go_on;
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
    if (DISP_W) {
        uint64_t display_end = DISP_BASE + (uint64_t)(DISP_BANKS - 1) * DISP_BANK_STEP
                             + (uint64_t)DISP_ROWS_PER_BANK * DISP_STRIDE;
        part_is((size_t)DISP_BASE * CELL_BYTES, display_end * CELL_BYTES, 0, "the display: %d by %d", DISP_W, DISP_H);
    }
    size_t headers_from, headers_to;
    int headers = headers_of(memory, memory_len, &headers_from, &headers_to);
    if (headers_now > headers_to) headers_to = headers_now;
    part_is(headers_from, headers_to, 0, "the dictionary: %d headers, %llu bytes of them made at run time",
            headers, (unsigned long long)(headers_now > headers_then ? headers_now - headers_then : 0));

    // The heap, and the block in it that has been freed for reuse.
    if (heap_now > heap_then) {
        part_is(heap_then * CELL_BYTES, heap_now * CELL_BYTES, 0, "the heap: given out");
    } else {
        part_is(heap_then * CELL_BYTES, heap_then * CELL_BYTES, 0, "the heap starts here: nothing given out");
    }
    uint64_t hole_at = hole ? in_memory(hole) : 0, hole_cells = hole ? in_memory(hole + 1) : 0;
    if (hole_cells) {
        part_is(hole_at * CELL_BYTES, (hole_at + hole_cells) * CELL_BYTES, 1, "given back, to be had again (hole)");
    }

    // Everything non-zero that is in none of those parts.  A stretch is one
    // of two kinds: the image has data there, or it does not.  Stretches
    // of the same kind at most CLOSE bytes apart are merged.
    static const char* KINDS[] = {
        "not nothing, and not the image's: given out and given back, or stored where nothing is kept",
        "laid down by the image in a place of its own (bulk, high)"};
    size_t from = 0, last = 0;
    int in = -1;                                        // the kind of the current stretch, or -1 if there is none
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

    // Print the parts in address order, with the free space between them.
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
