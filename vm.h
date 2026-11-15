//
// Created by Wes Brown on 12/31/20.
//

#ifndef HEXAFORTH_VM_H
#define HEXAFORTH_VM_H

#include <inttypes.h>
#include <stdio.h>
#include "vm_opcodes.h"

typedef struct { int EIP;
    int        HERE;
    int        SP;
    int        RSP;
    int        DBGP;
    uint16_t   memory[MEM_BYTES / 2];
    int64_t    DSTACK_park;
    int64_t    DSTACK[STACK_CELLS];
    int64_t    RSTACK_park;
    int64_t    RSTACK[STACK_CELLS];
    int64_t    DBGSTACK_park;
    int64_t    DBGSTACK[32];
    FILE       *OUT;
    FILE       *IN;
    char*      meta[65536];
    word_node* words;
    uint64_t   CYCLES;
    int64_t    IOADDR;
    FILE       *DISK;           // the file that is the disk, if there is one
    int64_t    DISKBLK;         // the block the disk's ports are at
    int64_t    DISKBLOCKS; } context;

static inline uint8_t clz(uint64_t N);
static inline int64_t io_write_handler(context *ctx, uint64_t io_addr, int64_t io_write);
static inline int64_t io_read_handler(context *ctx, uint64_t io_addr);
int vm(context *ctx);

// The name in a dictionary header, for the tools that read an image: it is
// not something the machine knows.  A name is five bits a character, as
// many to a cell as a cell holds, the first the lowest, and the top bit of
// its last cell is set.  The codes are those of forth/cross.fs: nothing,
// the 26 letters, three more, and two that say the next code is one of the
// rest, of which the first is 1.  What is returned is how many bytes the
// name's cells are.
static inline int header_name(const uint8_t *cells, char *out) {
    static const char direct[] = "-!?";
    static const char rest1[33] = " 0123456789><@'(),.+*/=\"#:;[]$%&";
    static const char rest2[33] = " \\^`{|}~_";
    const char *rest = NULL;
    int n = 0, bytes = 0;
    for (;;) {
        uint64_t cell = 0;
        for (int i = 0; i < CELL_BYTES; i++) cell |= (uint64_t)cells[bytes + i] << (8 * i);
        bytes += CELL_BYTES;
        for (int i = 0; i < CELL_BITS / 5; i++) {
            unsigned c = (unsigned)(cell >> (5 * i)) & 31;
            if (rest) {
                out[n++] = c && rest[c] ? rest[c] : '?';
                rest = NULL;
            } else if (c == 30) {
                rest = rest1;
            } else if (c == 31) {
                rest = rest2;
            } else if (c) {
                out[n++] = c < 27 ? (char)('a' + c - 1) : direct[c - 27];
            }
        }
        if (cell >> (CELL_BITS - 1) || n > 120) break;
    }
    out[n] = '\0';
    return bytes;
}

#endif //HEXAFORTH_VM_H
