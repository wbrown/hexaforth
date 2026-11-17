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
| `ADDR_BITS` | memory is 2^`ADDR_BITS` cells | 22 |
| `STACK_CELLS` | cells in each of the two stacks | 256 |
| `VAR_BASE` | the first of the 4,096 cells that one instruction can fetch or store | byte `0x4000` of the image, as a cell |
| `DISP_W`, `DISP_H` | the display's size in pixels; 0 for no display | 768, 576 |
| `DISP_BASE` | the cell where the bitmap starts | `0x4000` |
| `DISP_LEFT_LOW` | 1 if the leftmost pixel of a cell is its lowest bit, 0 if its highest | 1 |
| `DISP_INK_BLACK` | 1 if a set bit is black, 0 if it is white | 1 |
| `DISP_STRIDE` | cells from one row to the next in the same bank | 12 |
| `DISP_BANKS` | how many banks the rows are dealt into | 1 |
| `DISP_BANK_STEP` | cells from the start of one bank to the next | 0 |

The defaults are of the machine with cells of 64 bits, which this file
calls the hosted machine: it is the one `vm.c` is by default and the one the
AArch64 kernels are.  With cells of 32 or 16 bits `vm_constants.h` gives
memory of 2^17 or 2^16 cells and no display, unless one is said.
`util/machines` is the three machines the system is built for and tested
on:

| Cell | Memory | Display | The bitmap starts at cell |
|---|---|---|---|
| 64 bits | 2^22 cells | 768 by 576 | `0x4000` |
| 32 bits | 2^17 cells | 768 by 576 | `0x8000` |
| 16 bits | 2^16 cells | 512 by 342 | `0x8000` |

In these documents nothing is zero: a cell of nothing has every bit clear,
and a number below nothing is negative.

Where this file says a thing is undefined, implementations may differ, and a
program that does it is wrong. The strict builds stop at the first such
thing: `-DSTRICT` for `vm.c`, and for the small AArch64 kernel `STRICT` said
to CMake (`cmake -DSTRICT=ON build/cmake`).

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

1. Memory is an array of cells. An address is the number of a cell, from 0.
   `@` and `!` fetch and store one cell. There is no smaller unit: nothing
   in the machine counts bytes. Code is addressed by instruction, data by
   cell, and the stacks by cell.
2. Memory is exactly 2^`ADDR_BITS` cells. A fetch or store outside it is
   undefined.
3. Devices are not in memory. They are reached through the ports.

A machine with 16-bit cells can have 65,536 of them, which is 128KB.

Text is a convention of the Forth system. Characters
are packed into cells from the low end. Words that deal with whole strings
can work a cell at a time, as the dictionary search, the glyphs and the
image viewer do; words that deal with characters fetch a cell and shift.

`c@`, `c!`, `char+` and the rest of standard Forth's character words are
not part of the machine. They are ordinary Forth words, and how they
represent the address of a character is theirs. In the system the address
of a character is the address of its cell
times the bytes in a cell, plus its place in the cell. That reaches all of
memory when cells are 64 or 32 bits. When they are 16 bits it reaches the
first 32,768 cells, which is where the kernel keeps its text; a program
that wants text anywhere would use a segment instead: a variable, kept for
each task, holds the cell where character addresses start, and `c@` works
out the cell and the shift from there.

## Variables

A literal instruction is twelve bits and a shift of 0, 12 or 24, pushed or
added to the top of the stack.  The fourth value of its shift field makes
it something else: the twelve bits pick one of the 4,096 cells from
`VAR_BASE`, and the instruction pushes what is in that cell, or, with the
add flag set, stores the top of the stack there and drops it.  A
variable among those cells is fetched or stored in one instruction.

`VAR_BASE` is where the image's data starts, so that is where the Forth
system keeps its variables; dictionary headers are kept above them.  A
number wider than 36 bits is made with a shift.

## Code

1. An instruction is 16 bits, and code is addressed by instruction. Cells
   wider than 16 bits hold several instructions, the first in the low bits.
2. Code is in pages of 4,096 instructions, which is 8KB on every machine.
   Page 0 is the kernel.
3. The target of a jump, a call or a conditional branch is 13 bits. With bit
   12 clear, the other twelve are an address in the kernel, reachable from
   any page. With bit 12 set, they are an offset in the page the branch
   itself is in. A branch reaches 4,096 instructions either way.
4. A return address and an execution token are whole instruction addresses,
   so returning and `execute` cross pages. A call into another page is
   compiled as a literal execution token and a call to `execute`.
5. An execution token is kept in 16 bits, so code is the first 65,536
   instructions of memory. With 16-bit cells the top bit of that cell is
   the flag for an immediate word, so code is the first 32,768.
6. The first 2,048 instructions of page 0 (`KROM_BYTES`, which is 4,096
   bytes) are the kernel's code, and a machine may have them as a ROM. A
   store there is undefined.

A machine's layout is in whole pages, since code must start on a page
boundary; data and the display may lie between pages of code. With 16-bit
cells an instruction is one cell and a page is 4,096 cells.  Code is then
in the lower half of memory, by rule 5, and so is whatever is reached by
the address of a byte, which is twice a cell's address and must be a
number a cell holds.  The layout of the machine that is built and tested
with such cells, which has 65,536 of them, is:

| Page | Cells | Use |
|---|---|---|
| 0 | `0x0000`-`0x0FFF` | kernel code in the first 2,048, which are its ROM, and system code after it |
| 1 | `0x1000`-`0x1FFF` | system code |
| 2 | `0x2000`-`0x2FFF` | data: the 4,096 cells from `VAR_BASE` |
| 3 | `0x3000`-`0x3FFF` | dictionary headers |
| 4-7 | `0x4000`-`0x7FFF` | system code, and code compiled at run time |
| 8 up | `0x8000` up | the display: 512 by 342 ends at `0xAABF` |
| above it | to `0xFFFF` | data of the image's that is reached only a cell at a time, and then memory that is given out as it is asked for |

## Instructions

An instruction is 16 bits, and its fields are these (`vm_instruction.h`).
The names are the ones `vm_opcodes.h` gives them, by which an instruction
is said in source that `cross.fs` compiles.  What a literal or an
instruction of the ALU does is said once, in `vm_step.h`, and this is that
in words.

T is the top of the data stack, N the cell under it, and R the top of the
return stack.  A flag is a cell of all ones, or nothing.

**An instruction of nothing**, 16 bits that are all clear, halts the
machine.

**A literal** has bit 15 set.  Bits 0 to 11 are a number, bits 12 and 13 a
shift, and bit 14 says to add.

| Shift | Bit 14 clear | Bit 14 set (`imm+`) |
|---|---|---|
| 0, 1 (`imm<<12`), 2 (`imm<<24`) | the number, shifted left by 0, 12 or 24, is pushed | it is added to T, and the stack is as deep as it was |
| 3 | the cell at `VAR_BASE` plus the number is pushed | T is stored in that cell, and popped |

**A jump, a conditional jump and a call** have bit 15 clear, and bits 13
and 14 say which: 0 (`ubranch`), 1 (`0branch`) and 2 (`scall`).  Bits 0 to
12 are where it goes (Code, rule 3).  A conditional jump pops T, and goes there if T was nothing.  A call
pushes the address of the instruction after it on the return stack, and
goes there.

**An instruction of the ALU** has bit 15 clear and bits 13 and 14 set
(`alu`).  Each field is a number, and these are what each number is:

| Bits | Field | 0 | 1 | 2 | 3 |
|---|---|---|---|---|---|
| 0, 1 | the return stack moves | stays (`r+0`) | up one (`r+1`) | down two (`r-2`) | down one (`r-1`) |
| 2, 3 | the data stack moves | stays (`d+0`) | up one (`d+1`) | down two (`d-2`) | down one (`d-1`) |
| 8, 9 | the output | `->T` | `->R` | `->io[T]` | `->[T]` |
| 10, 11 | the input | `N->IN` | `T->IN` | `[T]->IN` | `R->IN` |

Bits 4 to 7 are the operation, 0 to 15, in the table below.  Bit 12 is
`RET`: return as well.  How far a stack moves is a number of two bits with
a sign.

In source, an instruction is its fields' names, the input first and `alu`
last, and a field that is left out is 0.  `!` is `N->IN ->[T] d-2 alu`:
input 0, output 3, the data stack down two, and so `0x6308`.

It is done in this order:

1. The input IN is N, T, the cell that T is the address of, or R.
2. The operation makes OUT of it:

   | | Operation | OUT |
   |---|---|---|
   | 0 | `IN->` | IN |
   | 1 | `T<>N,IN->` | IN as it was; and T and N are exchanged |
   | 2 | `T->N,IN->` | IN; and T is written into the cell under it |
   | 3 | `IN+N` | the sum |
   | 4, 5, 6 | `IN&N`, `IN\|N`, `IN^N` | and, or, exclusive or |
   | 7 | `IN*N` | the product, as much of it as a cell holds |
   | 8 | `~IN` | every bit of IN changed |
   | 9 | `IN==N` | a flag: they are the same |
   | 10 | `N<IN` | a flag: N is less than IN, as numbers with a sign |
   | 11 | `Nu<IN` | a flag: N is less than IN, as numbers without one |
   | 12, 13 | `IN>>T`, `IN<<T` | IN shifted right or left by T, with nothing brought in |
   | 14 | `[IN]` | the cell that IN is the address of |
   | 15 | `io[IN]` | what the port IN reads as |

   A shift is by the low six bits of T, whatever the cell: so a shift by
   the width of a cell of 16 or 32 bits gives nothing, and a shift by 64
   shifts by nothing.
3. With `RET`, the next instruction is the one R is the address of, R being
   what it was when the instruction began.
4. The stacks move.  A stack that moves up has what was its top in the cell
   under its new top.
5. OUT goes to T; or to R; or is stored in the cell that T is the address
   of; or is written to the port T.  In the last two, T is as the operation
   left it: the stack's moving has not yet changed it.
6. A stack that moved down has its top again from the cell that is now its
   top, unless that top is where OUT went.

`+` is `T->IN IN+N ->T d-1 alu`, `dup` is `T->IN IN-> ->T d+1 alu`, and
`exit` is `T->IN IN-> ->T RET r-1 alu`.

## Display

The display is a bitmap in memory, one bit to a pixel, of a size set in the
machine definition (which may be none). Drawing is fetching and storing
cells. How the bitmap is shown is each implementation's. On QEMU a bridge
copies it into the true-colour buffer that `ramfb` wants; the bridge is a
separate assembler file from the kernel, so that another display can take
its place.

The size, the order of the pixels in a cell, the meaning of a set bit and
where each row is are properties of the machine, because real displays
differ in all four.
The hosted machine's display is 768 by 576, which is 12 cells of 64 bits to
a row; the machine with cells of 16 bits has 512 by 342.  On the
hosted machine pixel x of a row is bit x of its cell, so the leftmost pixel
is the lowest bit, and a set bit is black, so memory that is all zero is a
blank page.  A machine built around an existing display takes that
display's order and polarity.

A row starts on a cell and is `DISP_STRIDE` cells long.  Rows are dealt in
turn into `DISP_BANKS` banks, so row y is in bank y mod `DISP_BANKS`, and is
row y / `DISP_BANKS` of that bank:

    row y starts at  DISP_BASE + (y mod DISP_BANKS) * DISP_BANK_STEP
                               + (y / DISP_BANKS) * DISP_STRIDE

With one bank, as on the hosted machine, the rows follow one another.
A Hercules card has four banks: with 16-bit cells its rows are 45 cells and
its banks are 4,096 cells apart.

The Forth system does not assume an order, a polarity or a row layout.  The
toolchain takes them from the machine definition: the font is laid down
with its pixels in the machine's order, and three words are defined to
suit, on which all drawing is built: the address of a row, the mask for a
pixel, and setting or clearing ink.  No drawing code steps from one row to
the next by adding to an address.

A display has to suit the cell it is drawn with, and a definition that does
not is refused when it is built, by `vm_constants.h` and again by
`cross.fs`: the width must be a whole number of cells, the stride must
cover a row, banks must not overlap, and the base must be a single shifted
literal.  720 by 348 in four banks, a Hercules card, is accepted for a
machine with 16-bit cells and refused for one with 64.

The bitmap starts at a cell address that is one shifted literal, a 12-bit
value shifted left by 12, so that the address is one instruction and adding
it to an offset is one instruction.  On the hosted machine it is cell
`0x4000`, which is 4 shifted by 12 and is the first cell after the 128KB
image.  It is 6,912 cells long, and the heap starts at `0x6000`.

The Forth system draws this way, in `display.fs`.  Of the implementations,
the AArch64 kernels show the bitmap on QEMU through the bridge in
`baremetal/ramfb.S`, which copies it when the machine asks for a key, at
most 32 times a second; a program that draws for a long time without
asking for a key is not seen until it does.  `vm.c` shows nothing while it
runs: it writes its memory when it stops, and `util/screen.c` takes the
display out of that.  The Verilog has the bitmap in its memory and no
video.  The bridge handles the hosted machine's order, polarity and row
layout only.

## The ports

All three implementations have these.  A port that a machine has not reads
zero, and writing it does nothing.

| Port | Read | Written |
|---|---|---|
| `0xd0`, `0xd1` | the depth of the data stack, of the return stack | sets it (Stacks, above) |
| `0xe0` | a character from the console, when there is one | |
| `0xe1` | true if a character is waiting | |
| `0xf1` | | a character to the console |
| `0xc0` | where the machine's cell 0 is, as a device is told an address | latches the address of a device's register |
| `0xc1`, `0xc2`, `0xc4`, `0xc8` | 8, 16, 32 or 64 bits at the latched address | the same, stored there |

The last two rows are how a driver written in Forth reaches a device whose
registers are a fixed width at an address of the host's.  A machine whose
cell cannot hold such an address reads `0xc0` as zero, and its latch is an
address in its own memory.  The reference VM says a character is always
waiting, and then waits for one.

## Other devices

The keyboard, the pointer and the disk of the machine the AArch64 kernels
run on are QEMU's virtio devices, reached through the last two rows above
by drivers written in Forth (`virtio.fs`, `virtio-blk.fs`,
`virtio-input.fs`). A built machine will have its own, and they are not yet
part of this model. What the system asks of a disk is two words, to read a
block of 1,024 bytes and to write one (`blocks.fs`); of a keyboard and a
pointer, a word that hands their events to `on-event` (`wm.fs`).

The reference VM (`vm.c`) can be given a file to be a disk, of blocks of
1,024 bytes, which is reached through three ports.  Port `0xb0` holds the
number of a block, and reads as how many blocks the disk has.  The address
of a cell written to port `0xb1` has that block read into the 1,024 bytes
from the cell, and written to port `0xb2` has it written from them.  It is
this VM's own: on a machine without it the ports read zero, as every port
that a machine has not does, and writing them does nothing.

Two more are this VM's own, for looking into it: port `0xf0` and port
`0xe0` when they are written (`8emit` and `.s` in `vm_opcodes.h`).  Neither
yet prints what it is for.  A cell written to the first sends its low byte
and then bytes that are not the cell's, and anything written to the second
prints one cell of each stack.
