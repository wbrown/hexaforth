# The machine model

Hexaforth is one machine with three implementations: `vm.c`, the AArch64
kernels in `baremetal/`, and the Verilog in `rtl/`. This file says what the
machine is. A program may rely on what is written here and on nothing else,
and each implementation must behave the same way wherever this file defines
the behaviour.

Sizes are properties of a machine and are set in `vm_constants.h`, from
which they reach the toolchain and the other implementations.

| Name | Meaning | Default |
|---|---|---|
| `CELL_BITS` | width of a stack cell, the ALU, and a fetch or store: 64, 32 or 16 | 64 |
| `ADDR_BITS` | memory is 2^`ADDR_BITS` cells | to be set |
| `STACK_CELLS` | cells in each of the two stacks | 256 |
| `DISP_W`, `DISP_H` | the display's size in pixels; 0 for no display | 768, 576 |
| `DISP_BASE` | the cell where the bitmap starts | `0x4000` |
| `DISP_LEFT` | which bit of a cell is the leftmost pixel: `low` or `high` | `low` |
| `DISP_INK` | whether a set bit is `black` or `white` | `black` |
| `DISP_STRIDE` | cells from one row to the next in the same bank | 12 |
| `DISP_BANKS` | how many banks the rows are dealt into | 1 |
| `DISP_BANK_STEP` | cells from the start of one bank to the next | 0 |

Where this file says a thing is undefined, implementations may differ, and a
program that does it is wrong. The strict builds stop at the first such
thing: `-DSTRICT` for `vm.c`, `make STRICT=1` for the small AArch64 kernel.

## Stacks

1. There are two stacks, the data stack and the return stack. Each is an
   array of `STACK_CELLS` cells and is not part of memory: `@` and `!`
   cannot reach it. A program reaches a stack by pushing, by popping, and
   through its depth port.
2. Each stack has a depth. Port `0xd0` is the data stack's and `0xd1` the
   return stack's. Reading a port gives the depth as the instruction starts,
   so `port io@` counts the port number it has just pushed.
3. Writing a port sets the depth and changes nothing else. The cells that a
   lowered depth hides keep their values, and are there again when the depth
   is raised. An implementation may keep the top cells of a stack in
   registers; it writes them back before the depth moves, so a program never
   sees the difference.
4. A cell that is popped is gone. Its value is undefined from then on, even
   if the depth is later raised over it.
5. Pushing onto a full stack, popping an empty one, and setting a depth
   outside 0 to `STACK_CELLS` are undefined.

Rule 3 is what lets several tasks share the stacks: each task owns a range
of depths, and changing task is saving two depths and setting two others.
It also gives access to a cell by its position without any further port:
set the depth to that position, push or pop, and set the depth back.

## Memory

Decided; not yet true of the implementations, which still count bytes.

1. Memory is an array of cells. An address is the number of a cell, from 0.
   `@` and `!` fetch and store one cell. There is no smaller unit: nothing
   in the machine counts bytes. Code is addressed by instruction, data by
   cell, and the stacks by cell.
2. Memory is exactly 2^`ADDR_BITS` cells. A fetch or store outside it is
   undefined.
3. Devices are not in memory. They are reached through the ports.

So a machine with 16-bit cells can have 65,536 of them, which is 128KB.

Text is a convention of the Forth system, not of the machine. A string is
the address of a cell and a count of characters, packed into cells from the
low end. Words that deal with whole strings work a cell at a time; words
that deal with characters fetch a cell and shift. Dictionary names, glyphs
and the image viewer already work this way.

`c@`, `c!`, `char+` and the rest of standard Forth's character words are
not part of the machine and are not needed by the system. They can be
provided as a library of ordinary Forth words for programs that want them.
How that library represents the address of a character is its own affair
and puts no constraint on the model. One way is a segment: a variable,
kept for each task, holds the cell where character addresses start, and
`c@` works out the cell and the shift from there. A program's text can
then be anywhere in memory.

## Code

1. An instruction is 16 bits, and code is addressed by instruction. Cells
   wider than 16 bits hold several instructions, the first in the low bits.
2. Code is in pages of 4,096 instructions, which is 8KB on every machine.
   Page 0 is the kernel.
3. The target of a jump, a call or a conditional branch is 13 bits. With bit
   12 clear, the other twelve are an address in the kernel, reachable from
   any page. With bit 12 set, they are an offset in the page the branch
   itself is in. So a branch reaches 4,096 instructions either way.
4. A return address and an execution token are whole instruction addresses,
   so returning and `execute` cross pages. A call into another page is
   compiled as a literal execution token and a call to `execute`.
5. An execution token is kept in 16 bits, so code is the first 65,536
   instructions of memory.

A machine's layout is in whole pages, since code must start on a page
boundary; data and the display may lie between pages of code. With 16-bit
cells an instruction is one cell and a page is 4,096 cells, so a layout for
such a machine is:

| Page | Cells | Use |
|---|---|---|
| 0 | `0x0000`-`0x0FFF` | kernel code |
| 1-3 | `0x1000`-`0x3FFF` | dictionary and data |
| 4 up | `0x4000` up | the display: 384 by 288 ends at `0x5AFF`, 768 by 576 at `0xABFF` |
| above it | to `0xFFFF` | run-time code pages and heap |

How many of the pages above the display are for code is set in the machine
definition.

## Display

Decided in outline; the details are open.

The display is a bitmap in memory, one bit to a pixel, of a size set in the
machine definition (which may be none). Drawing is fetching and storing
cells. Each implementation shows the bitmap however it can: `vm.c` may write
it out, the Verilog scans it out as video, and on QEMU a bridge copies it
into the true-colour buffer that `ramfb` wants. The bridge is a separate
assembler file from the kernel, so that another display can take its place.

The size, the order of the pixels in a cell, the meaning of a set bit and
where each row is are properties of the machine, because real displays
differ in all four.
The hosted machine's display is 768 by 576, which is 12 cells of 64 bits to
a row; a smaller machine takes a size to suit, such as 384 by 288.  On the
hosted machine pixel x of a row is bit x of its cell, so the leftmost pixel
is the lowest bit, and a set bit is black, so memory that is all zero is a
blank page.  A machine built around an existing display takes that
display's order and polarity instead of converting on the way out.

A row starts on a cell and is `DISP_STRIDE` cells long.  Rows are dealt in
turn into `DISP_BANKS` banks, so row y is in bank y mod `DISP_BANKS`, and is
row y / `DISP_BANKS` of that bank:

    row y starts at  DISP_BASE + (y mod DISP_BANKS) * DISP_BANK_STEP
                               + (y / DISP_BANKS) * DISP_STRIDE

With one bank, as on the hosted machine, the rows simply follow one another.
A Hercules card has four banks: with 16-bit cells its rows are 45 cells and
its banks are 4,096 cells apart.

The Forth system does not assume an order, a polarity or a row layout.  The
toolchain takes them from the machine definition: the font is generated
with its pixels in the machine's order, and three words are defined to
suit, on which all drawing is built: the address of a row, the mask for a
pixel, and setting or clearing ink.  No drawing code steps from one row to
the next by adding to an address.

The bitmap starts at a cell address that is one shifted literal, a 12-bit
value shifted left by 12, so that the address is one instruction and adding
it to an offset is one instruction.  On the hosted machine it is cell
`0x4000`, which is 4 shifted by 12 and is the first cell after the 128KB
image.  It is 6,912 cells long, and the heap starts at `0x6000`.

Not yet true of the implementations. The Forth system draws through
`display.fs` into a driver, `ramfb.fs`, that keeps four bytes for each pixel
in memory above the image.

## Other devices

The keyboard, the pointer and the disk are reached through QEMU's virtio
devices, by drivers written in Forth. They are not yet part of this model.
