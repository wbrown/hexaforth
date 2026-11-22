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
    uint16_t   memory[MEM_MOST / 2];        // room for the largest machine: MEM_BYTES of it is this one's
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
    FILE       *DISKS[DISK_DRIVES];     // the file that is each disk, if there is one
    int64_t    DISKBLK;                 // the number the disk's ports are at: a disk, and a block of it
    int64_t    DISKBLOCKS[DISK_DRIVES]; // how many blocks each disk is
    } context;

static inline uint8_t clz(uint64_t N);
static inline int64_t io_write_handler(context *ctx, uint64_t io_addr, int64_t io_write);
static inline int64_t io_read_handler(context *ctx, uint64_t io_addr);
int vm(context *ctx);

#endif //HEXAFORTH_VM_H
