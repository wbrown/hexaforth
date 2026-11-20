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

// Where variables are.  A literal instruction whose shift field is 3 is not
// a literal: it fetches or stores the cell at VAR_BASE plus its twelve
// bits, so the 4,096 cells from VAR_BASE can each be fetched or stored by
// one instruction.  VAR_BASE is where the image's data starts.
#define VAR_BASE (0x4000 / CELL_BYTES)

// The kernel's ROM, on a machine built with one: this many bytes from the
// start of the image, which the kernel's code is to fit in and nothing
// stores to.  The Verilog has it from here (krom.vh, which util/memimage.c
// writes).
#define KROM_BYTES 4096

// A disk is blocks of this many bytes, and this is what the system keeps on
// the one it is given, by block.  The Forth has these from here, through
// basewords.fs, and so has what makes a disk (util/mkdisk.c).
#define DISK_BLOCK          1024
#define DISK_LINE           64      // a block of source is lines of this many characters
//
// The source is first, and all of it is: whoever turns the pages of a disk
// in the editor sees source from block 1 on, and nothing else until it has
// ended.  Each thing after it starts where the one before it ends, so only
// how many blocks each is kept is a number here.  (The picture was once at
// block 16, in among the source of a disk that had more than a few blocks
// of it.)
//
// A disk is 1,024 blocks, what an 8-inch floppy holds (DISK_FLOPPY), and
// what is here is all on the first: the pictures are on a second, found
// by their labels (doc/boot.md, Decided 12; doc/disk.md).  What makes a
// disk holds it to that (util/mkdisk.c).
#define DISK_FLOPPY         1024    // the blocks a disk is, at the most
#define DISK_SOURCE_BLOCK   1       // Forth source, which the editor opens at,
#define DISK_SOURCE_BLOCKS  255     // and how many blocks are kept for it
#define DISK_NAMES_BLOCK    (DISK_SOURCE_BLOCK + DISK_SOURCE_BLOCKS)    // the names that go with the image,
#define DISK_NAMES_BLOCKS   128     // and how many blocks are kept for them
#define DISK_CANVAS_BLOCK   (DISK_NAMES_BLOCK + DISK_NAMES_BLOCKS)      // HexaPaint's canvas,
#define DISK_CANVAS_BLOCKS  16      // and how many blocks are kept for it
#define DISK_MASKS_BLOCK    (DISK_CANVAS_BLOCK + DISK_CANVAS_BLOCKS)
                                    // threshold masks, as planes of bits (util/bluenoise.h says how):
                                    // blue noise of 64 by 64 from this block,
#define DISK_MASK32_BLOCK   (DISK_MASKS_BLOCK + 2)                      // blue noise of 32 by 32 from this,
#define DISK_BAYER_BLOCK    (DISK_MASKS_BLOCK + 3)                      // and Bayer's of 8 by 8 from this,
#define DISK_MASKS_BLOCKS   4       // which is all the blocks they are
#define DISK_MASK_WIDTH     64      // pixels across a row of a mask's plane, a narrower mask repeated across it
#define DISK_MASK_PLANES    4       // planes to a mask: a threshold is one of sixteen levels
#define DISK_VIEW_BLOCK     (DISK_MASKS_BLOCK + DISK_MASKS_BLOCKS)
                                    // the viewer's dithered bitmap, which is made here a block at a time,
#define DISK_VIEW_BLOCKS    32      // and how many blocks are kept for it
#define DISK_GRAY_BLOCK     (DISK_VIEW_BLOCK + DISK_VIEW_BLOCKS)
                                    // the viewer's picture kept as gray, in its four planes, made here too,
#define DISK_GRAY_BLOCKS    128     // and how many blocks are kept for it: 512 by 512 samples

// A machine may have several disks, and a block's number says which as
// well (doc/disk.md, Several disks): the number is 16 bits, of which the
// low DISK_BLOCK_BITS are the block and those above them the disk.  So a
// number with no disk in it is of disk 0, a disk is at most 4,096 blocks,
// and there are at most DISK_DRIVES disks.
#define DISK_BLOCK_BITS     12
#define DISK_DRIVES         (1 << (16 - DISK_BLOCK_BITS))
#define DISK_DRIVE_OF(n)    (((n) >> DISK_BLOCK_BITS) & (DISK_DRIVES - 1))  // which disk a number is of,
#define DISK_BLOCK_OF(n)    ((n) & ((1 << DISK_BLOCK_BITS) - 1))            // and which block of it

// The disk's index (doc/disk.md): block 0 is entries of DISK_ENTRY bytes,
// each a run of blocks with a label and a type.  A number in an entry is
// two bytes, or four for its length, the lowest byte first.  These are the
// bytes of an entry each starts at, and the types.
#define DISK_ENTRY          32      // bytes of an entry
#define DISK_LABEL          16      // its label, first: a count and the characters, zeros after
#define DISK_ENTRY_TYPE     16      // what its blocks are, one of the types below
#define DISK_ENTRY_FIRST    18      // its first block
#define DISK_ENTRY_BLOCKS   20      // how many blocks it is
#define DISK_ENTRY_WHERE    22      // binary: where in memory it goes, counted in blocks; entry 0: the bits of a cell
#define DISK_ENTRY_LENGTH   24      // binary and data: bytes; source: lines; entry 0: the disk's blocks
#define DISK_FREE           0       // the types: a slot that is free,
#define DISK_BINARY         1       // memory, read into place when a machine starts,
#define DISK_SOURCE         2       // Forth source, as load reads it,
#define DISK_DATA           3       // an application's,
#define DISK_INDEX          4       // and more of the index; entry 0 of block 0 is the disk's own

// Each of the two stacks is this many cells.  The stacks are not part of
// memory: they are reached by pushing, popping and the depth ports.  Going
// past either end is undefined; a strict build stops there.
#ifndef STACK_CELLS
#define STACK_CELLS 256
#endif

// Stack depth ports.  Reading gives the depth of the data or return stack
// as the instruction starts (so `port io@` counts the port number itself).
// Writing sets the depth and changes nothing else: every cell keeps its
// value, whether it is covered or uncovered by the change, so lowering a
// depth and raising it again gives back the same stack.  T and R are only
// copies of the top cells, and are written back before a depth moves.
#define IO_SP  0xd0
#define IO_RSP 0xd1

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
// The cell where row y of the bitmap starts.
#define DISP_ROW(y) (DISP_BASE + ((y) % DISP_BANKS) * DISP_BANK_STEP + ((y) / DISP_BANKS) * DISP_STRIDE)

// A display has to suit the cell it is drawn with.  cross.fs makes the
// same checks, with the same wording, on the definition it is given.
#define DISP_ROWS_PER_BANK ((DISP_H + DISP_BANKS - 1) / DISP_BANKS)
#define DISP_ONE_LITERAL(a) ((a) != 0 && \
    ((((a) & 0xfff) == 0 && ((a) >> 12) < 4096) || \
     (((a) & 0xffffff) == 0 && ((a) >> 24) < 4096)))
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
