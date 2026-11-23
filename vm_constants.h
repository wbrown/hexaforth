//
// Created by Wes Brown on 12/31/20.
//

#ifndef HEXAFORTH_VM_CONSTANTS_H
#define HEXAFORTH_VM_CONSTANTS_H

#include <stdint.h>

// The machine's two sizes.  CELL_BITS is the width of a stack cell, of the
// ALU, and of a memory fetch or store: 64, 32 or 16.  An address is a cell
// number, and memory is 2^ADDR_BITS cells.  ADDR_BITS cannot exceed
// CELL_BITS, because an address has to fit in a cell.  Both reach the
// Forth toolchain through the generated basewords.fs.  See MODEL.md.
//
// These are values read at run time, not compile-time constants.  A
// program keeps the machine it is running for in THIS_MACHINE (below), and
// each of these names reads it from there, so one program serves a machine
// of any size.  THIS_MACHINE starts as the sizes given to the compiler
// (-DCELL_BITS and the rest), or as the 64-bit machine if none were given.
// No cell is wider than CELL_MOST bits, so a buffer sized for CELL_MOST
// holds any cell.
#define CELL_MOST 64
//
// A file that compiles in a machine of its own, with its own memory size
// (util/fuse.h), redefines ADDR_BITS, DISP_W and MEM_MOST as constants
// after including this.  Its cell size still comes from THIS_MACHINE.
//
// A size that is not given takes the default for the cell size
// (ADDR_BITS_OF, DISP_W_OF, DISP_H_OF).  That applies to sizes given to
// the compiler, and to sizes given to a program when it runs
// (machine_said, util/image.h).
#define ADDR_BITS_OF(cell)  ((cell) == 64 ? 22 : (cell) == 32 ? 17 : 16)
#define DISP_W_OF(cell)     ((cell) == 64 ? 768 : 0)
#define DISP_H_OF(cell)     ((cell) == 64 ? 576 : 0)
#ifndef CELL_BITS
#define CELL_BITS 64
#endif
#ifndef ADDR_BITS
#define ADDR_BITS ADDR_BITS_OF(CELL_BITS)
#endif
#define CELL_BYTES (CELL_BITS / 8)
#define BYTE_SHIFT (CELL_BITS == 64 ? 3 : CELL_BITS == 32 ? 2 : 1)     // shift a cell address left by this for its byte address
#define MEM_BYTES ((uint64_t)CELL_BYTES << ADDR_BITS)

// Where variables are.  A literal instruction whose shift field is 3 is not
// a literal: it fetches or stores the cell at VAR_BASE plus its twelve
// bits, so the 4,096 cells from VAR_BASE can each be fetched or stored by
// one instruction.  VAR_BASE is where the image's data starts.
#define VAR_BASE (0x4000 / CELL_BYTES)

// The size of the kernel's ROM, on a machine built with one.  The ROM is
// the first KROM_BYTES bytes of the image.  The kernel's code must fit in
// it, and nothing may store to it.  The Verilog gets this value from
// krom.vh, which util/memimage.c writes.
#define KROM_BYTES 4096

// A disk is blocks of DISK_BLOCK bytes.  The constants below say which
// blocks of its disk the system uses for what.  The Forth gets them
// through basewords.fs, and util/mkdisk.c, which makes a disk, uses them
// too.
#define DISK_BLOCK          1024
#define DISK_LINE           64      // a block of source is lines of this many characters
//
// All the source comes first.  Someone paging through a disk in the editor
// sees source from block 1 on, and nothing else until the source ends.
// Each area after it starts where the one before ends, so only the number
// of blocks reserved for each is written here.
//
// A disk is at most 1,024 blocks, the capacity of an 8-inch floppy
// (DISK_FLOPPY).  Everything below is on the first disk.  The pictures are
// on a second disk and are found by their labels (doc/boot.md, Decided 12;
// doc/disk.md).  util/mkdisk.c refuses to make a larger disk.
#define DISK_FLOPPY         1024    // the most blocks a disk has
#define DISK_SOURCE_BLOCK   1       // the first block of Forth source, where the editor opens
#define DISK_SOURCE_BLOCKS  511     // blocks reserved for source; the system's own source takes most of them
#define DISK_NAMES_BLOCK    (DISK_SOURCE_BLOCK + DISK_SOURCE_BLOCKS)    // the first block of the names that go with the image
#define DISK_NAMES_BLOCKS   128     // blocks reserved for the names
#define DISK_CANVAS_BLOCK   (DISK_NAMES_BLOCK + DISK_NAMES_BLOCKS)      // the first block of HexaPaint's canvas
#define DISK_CANVAS_BLOCKS  16      // blocks reserved for the canvas
#define DISK_MASKS_BLOCK    (DISK_CANVAS_BLOCK + DISK_CANVAS_BLOCKS)
                                    // the threshold masks, stored as bit planes (util/bluenoise.h describes
                                    // the layout); the 64 by 64 blue-noise mask starts at this block
#define DISK_MASK32_BLOCK   (DISK_MASKS_BLOCK + 2)                      // the first block of the 32 by 32 blue-noise mask
#define DISK_BAYER_BLOCK    (DISK_MASKS_BLOCK + 3)                      // the first block of the 8 by 8 Bayer mask
#define DISK_MASKS_BLOCKS   4       // blocks for all the masks
#define DISK_MASK_WIDTH     64      // pixels in a row of a mask's plane; a narrower mask is repeated across it
#define DISK_MASK_PLANES    4       // planes per mask: a threshold is one of sixteen levels
#define DISK_VIEW_BLOCK     (DISK_MASKS_BLOCK + DISK_MASKS_BLOCKS)
                                    // the viewer's dithered bitmap, which is written here a block at a time
#define DISK_VIEW_BLOCKS    32      // blocks reserved for the bitmap
#define DISK_GRAY_BLOCK     (DISK_VIEW_BLOCK + DISK_VIEW_BLOCKS)
                                    // the viewer's picture kept as gray, in four planes, also written here
#define DISK_GRAY_BLOCKS    128     // blocks reserved for the gray: 512 by 512 samples
// A disk that a machine boots its desktop from reserves DISK_OWN_BLOCKS
// blocks for the names of the words the boot compiles.  Those names are
// private to their file, and are removed from the dictionary once the file
// has been read (forth/files.fs).  The disk's index says where the blocks
// are (its entry own-names, util/mkdisk.c).
#define DISK_OWN_BLOCKS     32

// A machine can have several disks, and a block number also selects the
// disk (doc/disk.md, Several disks).  The number is 16 bits: the low
// DISK_BLOCK_BITS are the block, and the bits above them are the disk.  So
// a plain block number means disk 0, a disk has at most 4,096 blocks, and
// there are at most DISK_DRIVES disks.
#define DISK_BLOCK_BITS     12
#define DISK_DRIVES         (1 << (16 - DISK_BLOCK_BITS))
#define DISK_DRIVE_OF(n)    (((n) >> DISK_BLOCK_BITS) & (DISK_DRIVES - 1))  // the disk a block number selects
#define DISK_BLOCK_OF(n)    ((n) & ((1 << DISK_BLOCK_BITS) - 1))            // the block on that disk

// The disk's index (doc/disk.md).  Block 0 is entries of DISK_ENTRY bytes,
// each describing a run of blocks with a label and a type.  A number in an
// entry is two bytes, or four for its length, lowest byte first.  The
// constants below are the byte offset of each field in an entry, and the
// types.
#define DISK_ENTRY          32      // the size of an entry, in bytes
#define DISK_LABEL          16      // the size of the label, which comes first: a count, the characters, then zeros
#define DISK_ENTRY_TYPE     16      // the type of its blocks, one of the types below
#define DISK_ENTRY_FIRST    18      // its first block
#define DISK_ENTRY_BLOCKS   20      // its number of blocks
#define DISK_ENTRY_WHERE    22      // binary: where it goes in memory, counted in blocks; entry 0: the cell size in bits
#define DISK_ENTRY_LENGTH   24      // binary and data: its length in bytes; source: in lines; entry 0: the disk's size in blocks
#define DISK_FREE           0       // type: a free slot
#define DISK_BINARY         1       // type: memory, read into place when a machine starts
#define DISK_SOURCE         2       // type: Forth source, as `load` reads it
#define DISK_DATA           3       // type: an application's data
#define DISK_INDEX          4       // type: more of the index; entry 0 of block 0 describes the disk itself

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
#define DISP_W DISP_W_OF(CELL_BITS)
#endif
#ifndef DISP_H
#define DISP_H DISP_H_OF(CELL_BITS)
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
_Static_assert(CELL_BITS == 64 || CELL_BITS == 32 || CELL_BITS == 16,
               "a cell is 64, 32 or 16 bits: the machine's loop is compiled for each (vm_loop.h)");
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

// The machine a program is running for: the sizes above, as values.  Every
// file of a program defines THIS_MACHINE as a weak symbol, so the linker
// merges them into one, and every file initializes it to the same machine.
// From here on, each size name reads it.  A program has room for the
// largest machine: MEM_MOST bytes of memory, and a display of DISP_W_MOST
// by DISP_H_MOST pixels.
#define MEM_MOST ((uint64_t)8 << 22)
#define DISP_W_MOST 1024
#define DISP_H_MOST 1024
typedef struct {
    int       cell_bits, addr_bits;
    int       disp_w, disp_h;
    long long disp_base;
    int       disp_left_low, disp_ink_black, disp_stride, disp_banks, disp_bank_step;
} machine_sizes;
__attribute__((weak)) machine_sizes THIS_MACHINE = {
    CELL_BITS, ADDR_BITS, DISP_W, DISP_H, DISP_BASE,
    DISP_LEFT_LOW, DISP_INK_BLACK, DISP_STRIDE, DISP_BANKS, DISP_BANK_STEP
};
#undef CELL_BITS
#undef ADDR_BITS
#undef DISP_W
#undef DISP_H
#undef DISP_BASE
#undef DISP_LEFT_LOW
#undef DISP_INK_BLACK
#undef DISP_STRIDE
#undef DISP_BANKS
#undef DISP_BANK_STEP
#define CELL_BITS       (THIS_MACHINE.cell_bits)
#define ADDR_BITS       (THIS_MACHINE.addr_bits)
#define DISP_W          (THIS_MACHINE.disp_w)
#define DISP_H          (THIS_MACHINE.disp_h)
#define DISP_BASE       (THIS_MACHINE.disp_base)
#define DISP_LEFT_LOW   (THIS_MACHINE.disp_left_low)
#define DISP_INK_BLACK  (THIS_MACHINE.disp_ink_black)
#define DISP_STRIDE     (THIS_MACHINE.disp_stride)
#define DISP_BANKS      (THIS_MACHINE.disp_banks)
#define DISP_BANK_STEP  (THIS_MACHINE.disp_bank_step)

#define HX      "$"
#define BYTE    uint8_t
#define SBYTE   int8_t
#define CELL    int64_t
#define WORD    uint16_t
#define VM_ADDR uint64_t
#define TRUE    0xffffffffffffffff
#define FALSE   0x0000000000000000

#define CELL_SZ ((CELL)CELL_BITS)
static const WORD UART_D = 0x1000;
static const WORD UART_STATUS = 0x2000;
static const CELL LIT_BITS = 12;
#define LIT_UBITS (CELL_SZ - LIT_BITS)
static const CELL LIT_MASK = 0xffffffffffffffff << LIT_BITS;
#define LIT_UMASK ((CELL)(0xffffffffffffffff >> (CELL_SZ - LIT_BITS)))

#endif //HEXAFORTH_VM_CONSTANTS_H
