//
// finds IMAGE.hex [DISK.img [MEMORY.out]]
//
// The reference VM, recording every search of the dictionary.  It is
// main.c and vm.c unchanged.  vm.c reports each instruction it is about to
// run through the macro RUNS, and this file defines RUNS.
//
// sfind (nuc.fs) takes a name and follows the dictionary's links, from the
// header a search starts at, until it finds a header with that name or
// reaches the end.  Headers created on the machine come before the
// image's.  This program follows a search at four points in sfind:
//
//   - at its first instruction, the name is read from the machine's
//     memory
//   - each turn of its loop is one header examined
//   - where it calls same-cells?, it has a header whose name begins as
//     the name does.  The last such header before sfind returns is the
//     one it found
//   - its last instruction is the return it takes when it found no header
//
// This program does not know the layout of a header.  read_header and
// header_end (vm_opcodes.c) tell which header a cell belongs to.
//
// When the machine stops, the results are written to the file named by the
// environment variable FINDS, if it is set:
//
//   - the number of searches and the number of headers examined.  Of
//     those headers, the numbers examined for names found in the image's
//     own headers, for names found in headers created since, and for
//     names not found
//
//   - the number of headers the same searches would have examined if the
//     image's headers were ordered by how often each is found.  The
//     headers created since would still come first: a search that finds
//     one of the image's headers passes all of those, and then the
//     image's headers before the one it finds
//
//   - the number of headers the searches would have examined if the
//     dictionary were several chains, with a name's chain chosen from the
//     first cell of its packed name.  It is given for each way of choosing
//     a chain (chain_of) and each number of chains, beside the number it
//     would be if names were spread evenly over the chains
//
//   - one line for each name, ordered by the headers examined for it: the
//     number of times it was searched for and found, the headers examined
//     per search, the name's share of all the headers examined, and the
//     position of its header among the image's, counting the first header
//     examined as 1
//
// FINDS_NAMES names the file of names that goes with the image
// (nuc.tables), which has the words with no header.  Recording starts
// again each time the machine starts at instruction 0, so the results
// cover the time since the last start.
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
    long asked, found;          // the number of searches for the name, and the number that found it
    double looked;              // the total of headers examined in those searches
    double looked_missed;       // the part of that total from searches that did not find the name
    double looked_there;        // the part from searches that found the image's header for the name
    double looked_since;        // the part from searches that found a header created since the image
    long in_image;              // the position of the name's header among the image's: 1 is the first examined, and 0 means the image has none
    long found_there;           // the number of searches that found the image's header
    double made_since;          // the total of headers created since the image that those searches examined before they reached it
} asking;
static asking* ASKED;
static long nasked, asked_room;

typedef struct { uint32_t from, to; } header;   // the bytes one of the image's headers occupies; the list is in the order a search examines them
static header* HEADERS;
static long nheaders, headers_room;

static context* MACHINE;
static long FROM = -1, TO, HEAD = -1, SAME = -1;       // sfind's first address and the address after its last; the head of its loop; the address of same-cells?
static long NOW = -1;           // the index of the name being searched for, or -1 when no search is under way
static long turns;              // the number of turns of the loop in this search
static uint64_t candidate;      // the address on top of the stack at the last call of same-cells?, which is inside the header being compared

// Several chains.  Suppose the dictionary were several chains, with a
// name's chain chosen from the first cell of its packed name.  A search
// would then follow one chain.  Of the headers the real search examined,
// it would examine only those in the name's chain, and each chain would
// keep the order the single chain has.  sfind's loop compares a header's
// first name cell with the key's once for each header it examines (the
// instruction at COMPARED).  At that instruction the machine has both
// cells on its stack, so this program does not need to know which cell of
// a header holds the name.  A header is counted once for each way of
// choosing a chain, and each number of chains, that would put it in the
// name's chain.  The numbers of chains are 2, 4, and so on up to
// 1 << CHAINS_MOST.
//
// A packed name is a sequence of 5-bit codes, with the first code in the
// lowest bits of its first cell (nuc.fs).  The four ways of choosing a
// chain are:
//   - the low bits of that cell, which for up to 32 chains is the first
//     code alone
//   - the first two codes, xored together
//   - the first three codes, xored together
//   - the top bits of the cell after it is multiplied by a constant that
//     mixes all of its bits into them
#define CHAINS_MOST 6
static const char* const CHOSEN[] = { "its low bits", "two codes", "three codes", "multiplied" };
#define WAYS (int)(sizeof(CHOSEN) / sizeof(CHOSEN[0]))
static double WOULD[sizeof(CHOSEN) / sizeof(CHOSEN[0])][CHAINS_MOST + 1];
static double compared;         // the number of times the loop compared a header's first name cell with the key's
static long COMPARED = -1;      // the address of the instruction that compares them, or -1 if it was not found

// Returns the chain a name is in.  `first` is the first cell of its packed
// name, `way` is the way of choosing, and there are 1 << bits chains.
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

// Counts one compared header for every way of choosing and every number
// of chains that would put the header in the key's chain.
static void chained(uint64_t first, uint64_t key) {
    compared++;
    for(int way=0; way < WAYS; way++) {
        for(int bits=1; bits <= CHAINS_MOST; bits++) {
            if (chain_of(way, first, bits) == chain_of(way, key, bits)) WOULD[way][bits]++;
        }
    }
}

// Returns the index of the record for a name, adding one if the name is
// new.
static long asking_of(const char* name) {
    for(long n=0; n < nasked; n++) if (!strcmp(ASKED[n].name, name)) return(n);
    ONE_MORE(ASKED, nasked, asked_room);
    memset(&ASKED[nasked], 0, sizeof(asking));
    snprintf(ASKED[nasked].name, NAME_ROOM, "%s", name);
    return(nasked++);
}

// Starts recording.  It is called when the machine starts.  It finds
// sfind and reads the image's headers from the machine's memory, which
// holds the image unchanged at that time.
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
    // The head of sfind's loop is the target of its last backward jump.
    for(long at=FROM; at < TO; at++) {
        instruction ins = *(instruction*)&ctx->memory[at];
        if (ins.lit.lit_f || ins.jmp.op_type != OP_TYPE_JMP) continue;
        long to = (long)branch(at + 1, ins.jmp.target);
        if (to >= FROM && to < at) HEAD = to;
    }
    // The compare is the instruction in the loop that fetches the cell at
    // the address on top of the stack and tests whether it equals the cell
    // under that address.
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
        if (!ASKED[of].in_image) ASKED[of].in_image = nheaders;        // keep the first header with that name, which is the one a search finds
    }
}

// Returns the position, among the image's headers, of the header a cell
// belongs to, counting the first header examined as 1.  Returns 0 if the
// cell is in none of them.
static long header_holding(uint64_t cell) {
    uint64_t byte = cell * CELL_BYTES;
    for(long idx=0; idx < nheaders; idx++) if (byte >= HEADERS[idx].from && byte < HEADERS[idx].to) return(idx + 1);
    return(0);
}

// vm.c calls this before it runs each instruction.  `at` is the
// instruction's address, `sp` is the data stack's pointer and `t` is the
// top of the data stack.
static void ran(context* ctx, int64_t at, int sp, int64_t t) {
    if (at == 0 || !MACHINE) watch(ctx);
    if (FROM < 0 || HEAD < 0) return;
    if (at == FROM && sp >= 2) {                        // ( c-addr u ): the name, which is lowered because a name has one case
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
    if (at == COMPARED && sp >= 2) {                    // ( key name ): `name` is the address of the header's first name cell
        chained(ucell(cell_from(cell_at(ctx, t))), ucell(ctx->DSTACK[sp - 2]));
    }
    instruction ins = *(instruction*)&ctx->memory[at];
    if (!ins.lit.lit_f && ins.jmp.op_type == OP_TYPE_CALL && (long)branch(at + 1, ins.jmp.target) == SAME) candidate = ucell(t);
    if (ins.lit.lit_f || ins.alu.op_type != OP_TYPE_ALU || !ins.alu.r_eip) return;

    // sfind is returning.  A return from its last instruction means it
    // found no header, and any other return means it found one.
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

// Writes the results to the file FINDS names.
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

    // Compute what the searches would examine if the image's headers were
    // ordered by how often each is found, with the one found most often
    // first.  A search that finds one of them would still pass the headers
    // created since the image, as it did, and then the image's headers
    // that come before it in the new order.
    asking** order = room_for(nasked + 1, sizeof(asking*));
    long ordered = 0;
    for(long n=0; n < nasked; n++) if (ASKED[n].found_there) order[ordered++] = &ASKED[n];
    qsort(order, ordered, sizeof(asking*), by_found_there);
    double in_order = 0;
    for(long n=0; n < ordered; n++) in_order += (double)(n + 1) * order[n]->found_there;
    free(order);
    const double of_image = there - made_since;        // the image's own headers examined by the searches that found one of them
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

    // Several chains (chained(), above).  The counts are reported only if
    // the compare ran once for every header examined.  If the two numbers
    // differ, the instruction that was found is not that compare, and the
    // counts would describe something else.
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

    // The names, ordered by the headers examined for each.  A name with at
    // least one five-hundredth of all the headers examined has a line of
    // its own, and the rest are totalled on one line.
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
