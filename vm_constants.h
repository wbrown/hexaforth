//
// Created by Wes Brown on 12/31/20.
//

#ifndef HEXAFORTH_VM_CONSTANTS_H
#define HEXAFORTH_VM_CONSTANTS_H

#include <stdint.h>

// The machine's two sizes.  CELL_BITS is the width of a stack cell, of the
// ALU and of a memory fetch or store: 64, 32 or 16.  ADDR_BITS is the size
// of the address space in bytes, as a power of two; it cannot exceed
// CELL_BITS, since an address is a cell.  Both reach the Forth toolchain
// through the generated basewords.fs.
#ifndef CELL_BITS
#define CELL_BITS 64
#endif
#ifndef ADDR_BITS
#define ADDR_BITS (CELL_BITS < 17 ? CELL_BITS : 17)
#endif
#define CELL_BYTES (CELL_BITS / 8)

// Each of the two stacks is this many cells.  The stacks are not part of
// memory: they are reached by pushing, popping and the depth ports.  Going
// past either end is undefined; a strict build stops there.
#ifndef STACK_CELLS
#define STACK_CELLS 256
#endif

#define HX      "$"
#define BYTE    uint8_t
#define SBYTE   int8_t
#define CELL    int64_t
#define WORD    uint16_t
#define VM_ADDR uint64_t
#define TRUE    0xffffffffffffffff
#define FALSE   0x0000000000000000

static const CELL CELL_SZ = CELL_BITS;
static const WORD UART_D = 0x1000;
static const WORD UART_STATUS = 0x2000;
static const CELL LIT_BITS = 12;
static const CELL LIT_UBITS = CELL_SZ - LIT_BITS;
static const CELL LIT_MASK = 0xffffffffffffffff << LIT_BITS;
static const CELL LIT_UMASK = 0xffffffffffffffff >> (CELL_SZ - LIT_BITS);

#endif //HEXAFORTH_VM_CONSTANTS_H
