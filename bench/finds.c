//
// finds IMAGE.hex [DISK.img [MEMORY.out]]
//
// The reference VM, watching what the dictionary is asked for.  It is
// main.c and vm.c as they are, with what vm.c tells of an instruction about
// to be run (RUNS) taken up here.
//
// sfind (nuc.fs) is given a name and goes along the dictionary, from the
// header a search starts at, until it finds the name's or comes to the end:
// a header made on the machine is before the image's.  Where it is
// entered, the name is read as it is given; each time round its loop is a
// header looked at; where it calls same-cells? it has a header whose name
// starts as the name does, and the last of those before it returns is the
// one it found; and its last instruction is where it returns when it has
// found none.  Nothing here knows the form of a header: which header a cell
// is in is asked of read_header and header_end (vm_opcodes.c).
//
// When the machine stops, what was seen is written to the file that the
// environment's FINDS names, if it names one:
//
//   how often the dictionary was asked, and how many headers were looked
//   at; of those, how many for names found in the image's own headers, for
//   names found in headers made since, and for names not found
//
//   what the same asking would have looked at were the image's headers in
//   the order of how often each is found, with the headers made since still
//   before them: a header of the image's is found past the headers made
//   since, and then past those of the image's that are before it
//
//   what it would have looked at were the dictionary several chains, a
//   name's chosen from the first cell of its packed name: for each way of
//   choosing one (chain_of) and each number of chains, beside what it
//   would be if names fell evenly among them
//
//   a line a name, by the headers looked at for it: how often it was asked
//   for and found, the headers looked at each time, the share of all that
//   is, and where its header is among the image's, the first looked at
//   being 1
//
// FINDS_NAMES names the file of names that goes with the image
// (nuc.tables), for the words that have no header.  The machine is watched
// from when it last started at instruction 0.
//

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../vm.h"

static void ran(context* ctx, int64_t at, int sp, int64_t t);

#define RUNS(at)    ran(ctx, at, SP, T)
#include "../vm.c"
#include "../util/words.h"

typedef struct {
    char name[NAME_ROOM];
    long asked, found;          // how often it was asked for, and how often found
    double looked;              // the headers looked at for it, in all;
    double looked_missed;       // of those, when it was not found,
    double looked_there;        // when the image's header of it was found,
    double looked_since;        // and when one made since was
    long in_image;              // where the image's header of it is among the image's: the first looked at is 1, and none is 0
    long found_there;           // how often the one found was that header,
    double made_since;          // and the headers made since the image that were looked at before it, in all
} asking;
static asking* ASKED;
static long nasked, asked_room;

typedef struct { uint32_t from, to; } header;   // the bytes one of the image's is: they are kept in the order they are looked at
static header* HEADERS;
static long nheaders, headers_room;

static context* MACHINE;
static long FROM = -1, TO, HEAD = -1, SAME = -1;       // sfind's instructions, where its loop starts, and same-cells?
static long NOW = -1;           // the name being looked for, or less than nothing
static long turns;              // how many times round the loop for it
static uint64_t candidate;      // the cell same-cells? was last given

// Several chains.  Were the dictionary several chains, a name's chosen from
// the first cell of its packed name, a search would go along one of them:
// of the headers it looked at, it would look at those of the name's chain,
// each chain being in the order the one chain is in.  The loop has a
// header's first name cell against the key's once for each header it looks
// at (COMPARED), and there the two cells are the machine's to give: which
// cell of a header that is, nothing here knows.  A header is counted for
// each way of choosing a chain, and each number of chains, that would have
// it in the name's: two chains, four, and so on to two to the CHAINS_MOST.
//
// A packed name is codes of five bits, the first in the lowest bits of its
// first cell (nuc.fs).  A chain is chosen by that cell's low bits, which of
// up to 32 chains is by the first code alone; by its first two codes, one
// xor the other; by its first three; or by the top bits of the cell
// multiplied by a number that mixes all of it into them.
#define CHAINS_MOST 6
static const char* const CHOSEN[] = { "its low bits", "two codes", "three codes", "multiplied" };
#define WAYS (int)(sizeof(CHOSEN) / sizeof(CHOSEN[0]))
static double WOULD[sizeof(CHOSEN) / sizeof(CHOSEN[0])][CHAINS_MOST + 1];
static double compared;         // how often the loop had a header's first name cell against the key's
static long COMPARED = -1;      // the instruction that does

static uint64_t chain_of(int way, uint64_t first, int bits) {
    const uint64_t most = ((uint64_t)1 << bits) - 1;
    switch (way) {
        case 0: return(first & most);
        case 1: return((first ^ first >> 5) & most);
        case 2: return((first ^ first >> 5 ^ first >> 10) & most);
        default: {
            const uint64_t by = CELL_BITS == 64 ? 0x9e3779b97f4a7c15ull : CELL_BITS == 32 ? 0x9e3779b9ull : 0x9e37ull;
            return(ucell((int64_t)(first * by)) >> (CELL_BITS - bits));
        }
    }
}

static void chained(uint64_t first, uint64_t key) {
    compared++;
    for(int way=0; way < WAYS; way++) {
        for(int bits=1; bits <= CHAINS_MOST; bits++) {
            if (chain_of(way, first, bits) == chain_of(way, key, bits)) WOULD[way][bits]++;
        }
    }
}

static long asking_of(const char* name) {
    for(long n=0; n < nasked; n++) if (!strcmp(ASKED[n].name, name)) return(n);
    ONE_MORE(ASKED, nasked, asked_room);
    memset(&ASKED[nasked], 0, sizeof(asking));
    snprintf(ASKED[nasked].name, NAME_ROOM, "%s", name);
    return(nasked++);
}

// The machine starts: so does the watch.  The words and the headers are the
// image's, which is what is in memory then.
static void watch(context* ctx) {
    MACHINE = ctx;
    const uint8_t* image = (const uint8_t*)ctx->memory;
    const size_t len = MEM_BYTES;
    word* words;
    int n = words_of(image, len, getenv("FINDS_NAMES"), &words);
    FROM = place_of(words, n, "sfind");
    SAME = place_of(words, n, "same-cells?");
    nasked = nheaders = 0;
    NOW = HEAD = COMPARED = -1;
    compared = 0;
    memset(WOULD, 0, sizeof(WOULD));
    for(int idx=0; idx < n; idx++) if (words[idx].at == FROM && words[idx].size) TO = FROM + words[idx].size;
    free(words);
    if (FROM < 0) return;
    // its loop starts where its last jump back goes
    for(long at=FROM; at < TO; at++) {
        instruction ins = *(instruction*)&ctx->memory[at];
        if (ins.lit.lit_f || ins.jmp.op_type != OP_TYPE_JMP) continue;
        long to = (long)branch(at + 1, ins.jmp.target);
        if (to >= FROM && to < at) HEAD = to;
    }
    // and the compare is the instruction of it that fetches the cell at the
    // address on the stack and says whether it is the cell under that address
    for(long at=HEAD; HEAD >= 0 && at < TO && COMPARED < 0; at++) {
        instruction ins = *(instruction*)&ctx->memory[at];
        if (ins.lit.lit_f || ins.alu.op_type != OP_TYPE_ALU) continue;
        if (ins.alu.in_mux == INPUT_LOAD_T && ins.alu.alu_op == ALU_EQ) COMPARED = at;
    }
    for(uint32_t head = first_header(image, len); head; ) {
        uint64_t xt;
        char name[NAME_ROOM];
        ONE_MORE(HEADERS, nheaders, headers_room);
        HEADERS[nheaders].from = head;
        HEADERS[nheaders++].to = header_end(image, len, head);
        head = read_header(image, len, head, &xt, name);
        long of = asking_of(name);
        if (!ASKED[of].in_image) ASKED[of].in_image = nheaders;        // the first of that name, which is the one found
    }
}

// Where the header of the image's that a cell is in is among them, the
// first looked at being 1; or nothing, if it is in none of them.
static long header_holding(uint64_t cell) {
    uint64_t byte = cell * CELL_BYTES;
    for(long idx=0; idx < nheaders; idx++) if (byte >= HEADERS[idx].from && byte < HEADERS[idx].to) return(idx + 1);
    return(0);
}

static void ran(context* ctx, int64_t at, int sp, int64_t t) {
    if (at == 0 || !MACHINE) watch(ctx);
    if (FROM < 0 || HEAD < 0) return;
    if (at == FROM && sp >= 2) {                        // ( c-addr u ): the name, which has one case
        uint64_t from = ucell(ctx->DSTACK[sp - 2]), count = ucell(t);
        char name[NAME_ROOM];
        size_t len = 0;
        const uint8_t* bytes = (const uint8_t*)ctx->memory;
        for(uint64_t idx=0; idx < count && len + 1 < NAME_ROOM && from + idx < MEM_BYTES; idx++) {
            name[len++] = (char)tolower(bytes[from + idx]);
        }
        name[len] = '\0';
        NOW = asking_of(name);
        turns = 0;
        candidate = 0;
        return;
    }
    if (NOW < 0 || at < FROM || at >= TO) return;
    if (at == HEAD) turns++;
    if (at == COMPARED && sp >= 2) {                    // ( key name ): the header's first name cell is at name
        chained(ucell(cell_from(cell_at(ctx, t))), ucell(ctx->DSTACK[sp - 2]));
    }
    instruction ins = *(instruction*)&ctx->memory[at];
    if (!ins.lit.lit_f && ins.jmp.op_type == OP_TYPE_CALL && (long)branch(at + 1, ins.jmp.target) == SAME) candidate = ucell(t);
    if (ins.lit.lit_f || ins.alu.op_type != OP_TYPE_ALU || !ins.alu.r_eip) return;

    // It returns: with the name's header, or at its end with none.
    asking* a = &ASKED[NOW];
    int found = at != TO - 1;
    long looked = found ? turns : turns - 1, which = found ? header_holding(candidate) : 0;
    a->asked++;
    a->looked += looked;
    if (!found) {
        a->looked_missed += looked;
    } else if (which) {
        a->found++, a->found_there++;
        a->looked_there += looked;
        a->made_since += looked - which;
    } else {
        a->found++;
        a->looked_since += looked;
    }
    NOW = -1;
}

static int by_looked(const void* a, const void* b) {
    const asking* x = a;
    const asking* y = b;
    if (x->looked != y->looked) return(x->looked > y->looked ? -1 : 1);
    return(strcmp(x->name, y->name));
}

static int by_found_there(const void* a, const void* b) {
    const asking* x = *(const asking* const*)a;
    const asking* y = *(const asking* const*)b;
    if (x->found_there != y->found_there) return(x->found_there > y->found_there ? -1 : 1);
    return(x->in_image < y->in_image ? -1 : x->in_image > y->in_image);
}

void finds_said(void) {
    const char* path = getenv("FINDS");
    if (!path) return;
    FILE* out = fopen(path, "w");
    if (!out) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    if (FROM < 0 || HEAD < 0) {
        fprintf(out, "the dictionary: the image has no sfind, or it has no loop, and nothing was watched\n");
        fclose(out);
        return;
    }
    double asked = 0, found = 0, looked = 0, there = 0, since = 0, missed = 0, found_there = 0, made_since = 0;
    long names = 0;
    for(long n=0; n < nasked; n++) {
        const asking* a = &ASKED[n];
        names += a->asked != 0;
        asked += a->asked, found += a->found, looked += a->looked;
        there += a->looked_there, since += a->looked_since, missed += a->looked_missed;
        found_there += a->found_there, made_since += a->made_since;
    }

    // Were the image's headers in the order of how often each is found: the
    // one found most is 1.  A name found there is then found past the
    // headers made since, as it was, and past those before it in that order.
    asking** order = room_for(nasked + 1, sizeof(asking*));
    long ordered = 0;
    for(long n=0; n < nasked; n++) if (ASKED[n].found_there) order[ordered++] = &ASKED[n];
    qsort(order, ordered, sizeof(asking*), by_found_there);
    double in_order = 0;
    for(long n=0; n < ordered; n++) in_order += (double)(n + 1) * order[n]->found_there;
    free(order);
    const double of_image = there - made_since;        // of the image's own headers, looked at when one of them was found
    const double all = looked ? looked : 1;

    fprintf(out, "the dictionary, as it was asked for while the machine ran (sfind): %.0f times, and %.0f found\n", asked, found);
    fprintf(out, "  the image has %ld headers; %ld names were asked for\n", nheaders, names);
    fprintf(out, "  headers looked at: %.0f, which is %.0f each time\n", looked, asked ? looked / asked : 0);
    fprintf(out, "    when a header of the image's was found (%.0f times)  %12.0f  %5.1f%%\n", found_there, there, 100 * there / all);
    fprintf(out, "      of those, headers made since the image             %12.0f  %5.1f%%\n", made_since, 100 * made_since / all);
    fprintf(out, "      and the image's own                                %12.0f  %5.1f%%\n", of_image, 100 * of_image / all);
    fprintf(out, "    when one made since was found (%.0f times)           %12.0f  %5.1f%%\n", found - found_there, since, 100 * since / all);
    fprintf(out, "    when none was (%.0f times)                           %12.0f  %5.1f%%\n", asked - found, missed, 100 * missed / all);
    fprintf(out, "  were the image's headers in the order of how often each is found, the image's own looked at\n"
                 "  would be %.0f where they are %.0f, and all of them %.0f: %.1f%% of what they are\n\n",
            in_order, of_image, looked - of_image + in_order, 100 * (looked - of_image + in_order) / all);

    // Several chains (chained, above).  What was counted there is of every
    // header looked at, or it is of something else and is not said.
    if (COMPARED < 0 || compared != looked) {
        fprintf(out, "  several chains: the loop had a header's first name cell against the key's %.0f times, where %.0f\n"
                     "  headers were looked at, so nothing is said of them\n\n", compared, looked);
    } else {
        fprintf(out, "  were the dictionary several chains, a name's chosen from the first cell of its packed name, and\n"
                     "  each in the order the one chain is in, the headers looked at would be these.  A chain is chosen by\n"
                     "  that cell's low bits; by its first two codes, one xor the other; by its first three; or by the\n"
                     "  top bits of the cell multiplied.  Even is what they would be if names fell evenly\n");
        fprintf(out, "  chains        even");
        for(int way=0; way < WAYS; way++) fprintf(out, "  %19s", CHOSEN[way]);
        fprintf(out, "\n");
        for(int bits=1; bits <= CHAINS_MOST; bits++) {
            fprintf(out, "  %6d  %10.0f", 1 << bits, looked / (1 << bits));
            for(int way=0; way < WAYS; way++) fprintf(out, "  %11.0f  %5.1f%%", WOULD[way][bits], 100 * WOULD[way][bits] / all);
            fprintf(out, "\n");
        }
        fprintf(out, "\n");
    }

    // The names, by what was looked at for each: those that are a five
    // hundredth of all of it or more a line each, and the rest together.
    qsort(ASKED, nasked, sizeof(asking), by_looked);
    fprintf(out, "name                             asked    found   looked at     each  of all  in the image\n");
    double rest_asked = 0, rest_found = 0, rest_looked = 0;
    long rest = 0;
    for(long n=0; n < nasked; n++) {
        const asking* a = &ASKED[n];
        if (!a->asked) continue;
        if (a->looked * 500 < all) {
            rest++, rest_asked += a->asked, rest_found += a->found, rest_looked += a->looked;
            continue;
        }
        fprintf(out, "%-30s %7ld  %7ld  %10.0f  %7.1f  %5.1f%%", a->name, a->asked, a->found, a->looked,
                a->looked / a->asked, 100 * a->looked / all);
        if (a->in_image) fprintf(out, "  %6ld", a->in_image);
        fprintf(out, "\n");
    }
    if (rest) {
        char others[40];
        snprintf(others, sizeof(others), "(%ld more names)", rest);
        fprintf(out, "%-30s %7.0f  %7.0f  %10.0f  %7.1f  %5.1f%%\n", others, rest_asked, rest_found, rest_looked,
                rest_looked / rest_asked, 100 * rest_looked / all);
    }
    if (fclose(out)) {
        perror(path);
        exit(EXIT_FAILURE);
    }
}

#define main run_image
#include "../main.c"
#undef main

int main(int argc, char *argv[]) {
    run_image(argc, argv);
    finds_said();
    return(0);
}
