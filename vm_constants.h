//
// Created by Wes Brown on 12/31/20.
//

#ifndef HEXAFORTH_VM_CONSTANTS_H
#define HEXAFORTH_VM_CONSTANTS_H

#include <stdint.h>

// The machine's two sizes.  CELL_BITS is the width of a stack cell, of the
// ALU and of a memory fetch or store: 64, 32 or 16.  An address is the
// number of a cell, and memory is 2^ADDR_BITS cells; ADDR_BITS cannot exceed
// CELL_BITS, since an address is a cell.  Both reach the Forth toolchain
// through the generated basewords.fs.  See MODEL.md.
#ifndef CELL_BITS
#define CELL_BITS 64
#endif
#ifndef ADDR_BITS
#define ADDR_BITS (CELL_BITS == 64 ? 22 : CELL_BITS == 32 ? 17 : 16)
#endif
#define CELL_BYTES (CELL_BITS / 8)
#define MEM_BYTES ((uint64_t)CELL_BYTES << ADDR_BITS)

// Each of the two stacks is this many cells.  The stacks are not part of
// memory: they are reached by pushing, popping and the depth ports.  Going
// past either end is undefined; a strict build stops there.
#ifndef STACK_CELLS
#define STACK_CELLS 256
#endif

// The display: a bitmap in memory, one bit to a pixel.  See MODEL.md.
//   DISP_W, DISP_H    size in pixels; a width of 0 means no display
//   DISP_BASE         the cell where the bitmap starts
//   DISP_LEFT_LOW     1 if the leftmost pixel of a cell is its lowest bit
//   DISP_INK_BLACK    1 if a set bit is black
//   DISP_STRIDE       cells from one row to the next in the same bank
//   DISP_BANKS        how many banks the rows are dealt into, in turn
//   DISP_BANK_STEP    cells from the start of one bank to the next
// The machine with 64-bit cells has a display by default; smaller ones do
// not unless they are given one.
#ifndef DISP_W
#define DISP_W (CELL_BITS == 64 ? 768 : 0)
#endif
#ifndef DISP_H
#define DISP_H (CELL_BITS == 64 ? 576 : 0)
#endif
#ifndef DISP_BASE
#define DISP_BASE 0x4000
#endif
#ifndef DISP_LEFT_LOW
#define DISP_LEFT_LOW 1
#endif
#ifndef DISP_INK_BLACK
#define DISP_INK_BLACK 1
#endif
#ifndef DISP_STRIDE
#define DISP_STRIDE (DISP_W / CELL_BITS)
#endif
#ifndef DISP_BANKS
#define DISP_BANKS 1
#endif
#ifndef DISP_BANK_STEP
#define DISP_BANK_STEP 0
#endif

// A display has to suit the cell it is drawn with.  cross.fs makes the
// same checks, with the same wording, on the definition it is given.
#define DISP_ROWS_PER_BANK ((DISP_H + DISP_BANKS - 1) / DISP_BANKS)
#define DISP_ONE_LITERAL(a) ((a) != 0 && \
    ((((a) & 0xfff) == 0 && ((a) >> 12) < 4096) || \
     (((a) & 0xffffff) == 0 && ((a) >> 24) < 4096) || \
     (((a) & 0xfffffffffULL) == 0 && ((a) >> 36) < 4096)))
_Static_assert(DISP_W == 0 || DISP_W % CELL_BITS == 0,
               "display: the width is not a whole number of cells");
_Static_assert(DISP_W == 0 || DISP_STRIDE * CELL_BITS >= DISP_W,
               "display: a row is longer than the stride");
_Static_assert(DISP_W == 0 || DISP_BANKS >= 1,
               "display: there must be at least one bank");
_Static_assert(DISP_W == 0 || DISP_BANKS == 1 || DISP_BANK_STEP >= DISP_ROWS_PER_BANK * DISP_STRIDE,
               "display: the banks overlap");
_Static_assert(DISP_W == 0 || DISP_ONE_LITERAL((unsigned long long)DISP_BASE),
               "display: the base is not a single shifted literal");

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
