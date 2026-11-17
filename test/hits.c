//
// hits MEMORY.out forth/joins.fs
//
// How often each rule of the nucleus's peephole was used.  join.fs keeps a
// count for each rule, after the rules and in their order, and join-rules
// says where the rules and the counts are.  MEMORY.out is the memory the
// reference VM wrote when it stopped (`hexaforth IMAGE.hex DISK.img
// MEMORY.out`), so the counts are of all that the nucleus compiled while
// it ran.  joins.fs is the rules, a line each, for what to call them.
//
// Prints how the uses are spread, and then each rule that was used with
// its count, the most used first.  It is built for the machine the memory
// is of (CELL_BITS).
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../vm_opcodes.h"
#include "../util/rules.c"

typedef struct {
    long uses;
    int of_three;                       // a rule for three instructions, not two
    const char* said;
} rule;
static rule* RULES;                     // as many as joins.fs has (rules.c)
static long rules;

static uint64_t cell_at(const uint8_t* memory, uint64_t which) {
    uint64_t cell = 0;
    memcpy(&cell, memory + which * CELL_BYTES, CELL_BYTES);
    return(cell);
}

static int by_uses(const void* a, const void* b) {
    const rule* x = a;
    const rule* y = b;
    return(x->uses < y->uses ? 1 : x->uses > y->uses ? -1 : 0);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "usage: hits MEMORY.out forth/joins.fs\n");
        return(1);
    }
    uint8_t* memory;
    size_t len = file_bytes(argv[1], &memory);
    if (!len) { fprintf(stderr, "hits: %s is not the memory of a machine\n", argv[1]); return(1); }

    // join-rules is a word of the dictionary that pushes where its cells are
    uint64_t xt = 0;
    int found = 0;
    for(uint32_t head = newest_header(memory, len); head && !found; ) {
        char name[256];
        uint64_t this_xt;
        head = read_header(memory, len, head, &this_xt, name);
        if (!strcmp(name, "join-rules")) { xt = this_xt; found = 1; }
    }
    if (!found) { fprintf(stderr, "hits: join-rules is not in the dictionary of %s\n", argv[1]); return(1); }
    uint64_t cells = 0;
    for(const instruction* ins = (const instruction*)memory + xt; ins->lit.lit_f; ins++) {
        cells += (uint64_t)ins->lit.lit_v << (ins->lit.lit_shifts * LIT_BITS);
    }
    uint64_t how_many = cell_at(memory, cells + 1), counts = cell_at(memory, cells + 3);
    const uint16_t* count = (const uint16_t*)memory + counts;

    said_rule* said;
    rules = rules_of(argv[2], &said);
    RULES = room_for(rules, sizeof(rule));
    for(long idx=0; idx < rules; idx++) RULES[idx].said = said[idx].said, RULES[idx].of_three = said[idx].of_three;
    if (!how_many || (uint64_t)rules != how_many || sizeof(uint16_t) * (counts + how_many) > len) {
        fprintf(stderr, "hits: %s has %ld rules, and the memory says %llu, with their counts at %llu\n",
                argv[2], rules, (unsigned long long)how_many, (unsigned long long)counts);
        return(1);
    }

    long all = 0, used = 0, once = 0;
    for(int idx=0; idx < rules; idx++) {
        RULES[idx].uses = count[idx];
        all += RULES[idx].uses;
        used += RULES[idx].uses > 0;
        once += RULES[idx].uses == 1;
    }
    qsort(RULES, rules, sizeof(rule), by_uses);
    printf("%ld rules, used %ld times in all: %ld of them were used, %ld of those once, and %ld never\n",
           rules, all, used, once, rules - used);
    long so_far = 0;
    const int shares[] = { 50, 90, 99 };
    const int marks = sizeof(shares) / sizeof(shares[0]);
    for(int idx=0, share=0; idx < rules && share < marks; idx++) {
        so_far += RULES[idx].uses;
        while (share < marks && so_far * 100 >= all * shares[share]) {
            printf("the %d most used are %d%% of the uses\n", idx + 1, shares[share++]);
        }
    }
    printf("\n");
    for(int idx=0; idx < rules && RULES[idx].uses; idx++) {
        printf("%6ld  %s%s\n", RULES[idx].uses, RULES[idx].said, RULES[idx].of_three ? "   (three)" : "");
    }
    return(0);
}
