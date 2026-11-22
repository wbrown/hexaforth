# The machine model

Hexaforth is one machine with three implementations: `vm.c`, the AArch64
kernels in `baremetal/`, and the Verilog in `rtl/`. This file defines the
machine. A program may rely on what is written here and nothing else, and
every implementation must behave the same way wherever this file defines
the behaviour.

Sizes are properties of a particular machine. They are set in
`vm_constants.h`, and the toolchain and the other implementations take them
from there.

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
| `DISP_BANKS` | how many banks the rows are spread across | 1 |
| `DISP_BANK_STEP` | cells from the start of one bank to the next | 0 |

The defaults describe the machine with 64-bit cells, which this file calls
the hosted machine: it is what `vm.c` builds by default and what the AArch64
kernels implement. For 32 or 16-bit cells, `vm_constants.h` gives 2^17 or
2^16 cells of memory and no display unless one is specified. `util/machines`
lists the three machines the system is built and tested on:

| Cell | Memory | Display | Bitmap starts at cell |
|---|---|---|---|
| 64 bits | 2^22 cells | 768 by 576 | `0x4000` |
| 32 bits | 2^17 cells | 768 by 576 | `0x8000` |
| 16 bits | 2^16 cells | 512 by 342 | `0x8000` |

Where this file says something is undefined, implementations may differ,
and a program that does it is wrong. The strict builds stop at the first
such thing: `-DSTRICT` for `vm.c`, and for the small AArch64 kernel, `STRICT`
given to CMake (`cmake -DSTRICT=ON build/cmake`).

## Stacks

1. There are two stacks, the data stack and the return stack. Each is an
   array of `STACK_CELLS` cells and is not part of memory: `@` and `!`
   cannot reach it. A program reaches a stack by pushing, by popping, and
   through its depth port.
2. Each stack has a depth. Port `0xd0` is the data stack's and `0xd1` the
   return stack's. Reading the port gives the depth as it was when the
   instruction started, so `port io@` counts the port number it has just
   pushed.
3. Writing the port sets the depth and changes nothing else. Cells hidden by
   lowering the depth keep their values, and reappear when the depth is
   raised again. An implementation may keep the top cells of a stack in
   registers, but it writes them back before the depth changes, so a program
   never sees the difference.
4. A cell that is popped is gone. Its value is undefined from then on, even
   if the depth is later raised over it.
5. Pushing onto a full stack, popping an empty one, and setting a depth
   outside 0 to `STACK_CELLS` are undefined.

Rule 3 is what lets several tasks share the stacks: each task owns a range
of depths, and switching tasks means saving two depths and setting two
others. It also gives access to any cell by its position, with no other
port needed: set the depth to that position, push or pop, and set the depth
back.

## Memory

1. Memory is an array of cells. An address is a cell number, starting from
   0. `@` and `!` fetch and store one cell. There is no smaller unit: nothing
   in the machine counts bytes. Code is addressed by instruction, data by
   cell, and the stacks by cell.
2. Memory is exactly 2^`ADDR_BITS` cells. A fetch or store outside it is
   undefined.
3. Devices are not in memory. They are reached through the ports.

A machine with 16-bit cells can have 65,536 of them, which is 128KB.

Text is a convention of the Forth system, not the machine. Characters are
packed into cells starting from the low end. Words that handle whole
strings can work a cell at a time, as the dictionary search, the glyphs and
the image viewer do; words that handle single characters fetch a cell and
shift.

`c@`, `c!`, `char+` and the rest of standard Forth's character words are not
part of the machine. They are ordinary Forth words, and how they represent
a character's address is up to them. In the system, a character's address
is its cell's address times the number of bytes in a cell, plus its
position in the cell. That reaches all of memory when cells are 64 or 32
bits. When they are 16 bits it reaches only the first 32,768 cells, which is
where the kernel keeps its text. A program that wanted text anywhere in
memory would use a segment instead: a variable, kept per task, holding the
cell where character addresses start, from which `c@` would work out the
cell and the shift.

## Variables

A literal instruction is twelve bits and a shift of 0, 12 or 24, which is
pushed or added to the top of the stack. The fourth value of the shift field
makes it something else: the twelve bits select one of the 4,096 cells
starting at `VAR_BASE`, and the instruction pushes that cell's value, or,
with the add flag set, stores the top of the stack there and drops it. So a
variable in those cells is fetched or stored in one instruction.

`VAR_BASE` is where the image's data starts, so that is where the Forth
system keeps its variables; the dictionary headers are above them. A number
wider than 36 bits is built with a shift.

## Code

1. An instruction is 16 bits, and code is addressed by instruction. Cells
   wider than 16 bits hold several instructions, the first in the low bits.
2. Code is in pages of 4,096 instructions, which is 8KB on every machine.
   Page 0 is the kernel.
3. The target of a jump, a call or a conditional branch is 13 bits. With bit
   12 clear, the other twelve are an address in the kernel, reachable from
   any page. With bit 12 set, they are an offset within the page the branch
   itself is in. Either way a branch reaches 4,096 instructions.
4. A return address and an execution token are full instruction addresses,
   so returning and `execute` can cross pages. A call into another page is
   compiled as a literal execution token followed by a call to `execute`.
5. An execution token is stored in 16 bits, so code must be in the first
   65,536 instructions of memory. With 16-bit cells the top bit of that
   cell flags an immediate word, so code must be in the first 32,768.
6. The first 2,048 instructions of page 0 (`KROM_BYTES`, which is 4,096
   bytes) are the kernel's code, and a machine may have them in ROM. A store
   there is undefined.

A machine's layout is in whole pages, since code must start on a page
boundary; data and the display may sit between pages of code. With 16-bit
cells an instruction is one cell, and a page is 4,096 cells. Code is then in
the lower half of memory, by rule 5, and so is anything reached by a byte
address, since a byte address is twice a cell address and must fit in a
cell. The layout of the 16-bit machine that is built and tested, with 65,536
cells, is:

| Page | Cells | Use |
|---|---|---|
| 0 | `0x0000`-`0x0FFF` | kernel code in the first 2,048 cells, which are its ROM, then system code |
| 1 | `0x1000`-`0x1FFF` | system code |
| 2 | `0x2000`-`0x2FFF` | data: the 4,096 cells from `VAR_BASE` |
| 3 | `0x3000`-`0x3FFF` | dictionary headers |
| 4-7 | `0x4000`-`0x7FFF` | system code, and code compiled at run time |
| 8 up | `0x8000` up | the display: 512 by 342 ends at `0xAABF` |
| above it | to `0xFFFF` | the image's data that is only read a cell at a time, then memory handed out on request |

## Instructions

An instruction is 16 bits, with the fields below (`vm_instruction.h`). The
names are the ones `vm_opcodes.h` gives them, which are how an instruction
is written in source that `cross.fs` compiles. What a literal or an ALU
instruction does is defined once, in `vm_step.h`; this section is that
definition in words.

T is the top of the data stack, N the cell under it, and R the top of the
return stack. A flag is a cell with all bits set (true) or all clear
(false).

**An instruction of all zero bits** halts the machine.

**A literal** has bit 15 set. Bits 0 to 11 are a number, bits 12 and 13 a
shift, and bit 14 says to add.

| Shift | Bit 14 clear | Bit 14 set (`imm+`) |
|---|---|---|
| 0, 1 (`imm<<12`), 2 (`imm<<24`) | the number, shifted left by 0, 12 or 24, is pushed | it is added to T, and the stack depth is unchanged |
| 3 | the cell at `VAR_BASE` plus the number is pushed | T is stored in that cell, and popped |

**A jump, a conditional jump and a call** have bit 15 clear, and bits 13 and
14 say which: 0 (`ubranch`), 1 (`0branch`) or 2 (`scall`). Bits 0 to 12 are
the target (Code, rule 3). A conditional jump pops T, and jumps if T was
zero. A call pushes the address of the next instruction onto the return
stack, then jumps.

**An ALU instruction** has bit 15 clear and bits 13 and 14 set (`alu`). Each
field is a number, with these meanings:

| Bits | Field | 0 | 1 | 2 | 3 |
|---|---|---|---|---|---|
| 0, 1 | return stack movement | none (`r+0`) | up one (`r+1`) | down two (`r-2`) | down one (`r-1`) |
| 2, 3 | data stack movement | none (`d+0`) | up one (`d+1`) | down two (`d-2`) | down one (`d-1`) |
| 8, 9 | output | `->T` | `->R` | `->io[T]` | `->[T]` |
| 10, 11 | input | `N->IN` | `T->IN` | `[T]->IN` | `R->IN` |

Bits 4 to 7 are the operation, 0 to 15, in the table below. Bit 12 is `RET`:
also return. A stack movement is a signed two-bit number.

In source, an instruction is written as its fields' names, input first and
`alu` last; a field left out is 0. `!` is `N->IN ->[T] d-2 alu`: input 0,
output 3, data stack down two, which is `0x6308`.

An ALU instruction is carried out in this order:

1. The input IN is N, T, the memory cell T points to, or R.
2. The operation produces OUT from it:

   | | Operation | OUT |
   |---|---|---|
   | 0 | `IN->` | IN |
   | 1 | `T<>N,IN->` | IN as it was; T and N are swapped |
   | 2 | `T->N,IN->` | IN; T is written into the cell under it |
   | 3 | `IN+N` | the sum |
   | 4, 5, 6 | `IN&N`, `IN\|N`, `IN^N` | and, or, exclusive or |
   | 7 | `IN*N` | the product, as much of it as fits in a cell |
   | 8 | `~IN` | IN with every bit inverted |
   | 9 | `IN==N` | a flag: they are equal |
   | 10 | `N<IN` | a flag: N is less than IN, as signed numbers |
   | 11 | `Nu<IN` | a flag: N is less than IN, as unsigned numbers |
   | 12, 13 | `IN>>T`, `IN<<T` | IN shifted right or left by T, bringing in zeros |
   | 14 | `[IN]` | the memory cell IN points to |
   | 15 | `io[IN]` | what port IN reads |

   A shift uses only the low six bits of T, whatever the cell size: so a
   shift by the full width of a 16 or 32-bit cell gives zero, and a shift by
   64 does not shift at all.
3. With `RET`, the next instruction is the one at R, using R's value from
   the start of the instruction.
4. The stacks move. A stack that moves up has its old top in the cell under
   its new top.
5. OUT goes to T; or to R; or is stored in the memory cell T points to; or is
   written to port T. In the last two cases, T is the value the operation
   saw: the stack movement has not yet changed it.
6. A stack that moved down takes its new top from the cell that is now on
   top, unless that is where OUT went.

`+` is `T->IN IN+N ->T d-1 alu`, `dup` is `T->IN IN-> ->T d+1 alu`, and
`exit` is `T->IN IN-> ->T RET r-1 alu`. `true` is `N->IN IN==N ->T d+1
alu`: N compared with itself, which pushes -1 whatever N is.

## Display

The display is a bitmap in memory, one bit per pixel, whose size is set in
the machine definition (and may be zero). Drawing is fetching and storing
cells. How the bitmap is shown is up to each implementation. On QEMU a
bridge copies it into the true-colour buffer that `ramfb` expects; the bridge
is a separate assembler file from the kernel, so that a different display
can replace it.

The size, the order of pixels in a cell, the meaning of a set bit and where
each row is are properties of the machine, because real displays differ in
all four. The hosted machine's display is 768 by 576, which is 12 64-bit
cells per row; the 16-bit machine's is 512 by 342. On the hosted machine
pixel x of a row is bit x of its cell, so the leftmost pixel is the lowest
bit, and a set bit is black, so memory that is all zero is a blank page. A
machine built around an existing display uses that display's pixel order
and polarity.

A row starts at a cell boundary and is `DISP_STRIDE` cells long. Rows are
spread in turn across `DISP_BANKS` banks, so row y is in bank
y mod `DISP_BANKS`, as row y / `DISP_BANKS` of that bank:

    row y starts at  DISP_BASE + (y mod DISP_BANKS) * DISP_BANK_STEP
                               + (y / DISP_BANKS) * DISP_STRIDE

With one bank, as on the hosted machine, rows simply follow one another. A
Hercules card has four banks: with 16-bit cells its rows are 45 cells long
and its banks are 4,096 cells apart.

The Forth system assumes no particular pixel order, polarity or row layout.
The toolchain takes them from the machine definition: fonts are laid down
with their pixels in the machine's order, and three words are defined to
match, on which all drawing is built: the address of a row, the mask for a
pixel, and setting or clearing ink. No drawing code moves from one row to
the next by adding to an address.

A display has to suit the cell size it is drawn with, and a definition that
doesn't is rejected at build time, by `vm_constants.h` and again by
`cross.fs`: the width must be a whole number of cells, the stride must cover
a row, banks must not overlap, and the base must be a single shifted
literal. 720 by 348 in four banks, a Hercules card, is accepted for a 16-bit
machine and rejected for a 64-bit one.

The bitmap starts at a cell address that is a single shifted literal, a
12-bit value shifted left by 12, so that loading the address is one
instruction and adding it to an offset is one instruction. On the hosted
machine it is cell `0x4000`, which is 4 shifted by 12 and is the first cell
after the 128KB image. It is 6,912 cells long, and what the system gives
out at run time (`halloc`) starts after it.

The Forth system draws this way, in `display.fs`. Of the implementations,
the AArch64 kernels show the bitmap on QEMU through the bridge in
`baremetal/ramfb.S`, which copies it whenever the machine asks for a key, at
most 32 times a second; a program that draws for a long time without asking
for a key isn't seen until it does. `vm.c` shows nothing while it runs: it
writes out its memory when it stops, and `util/screen.c` extracts the
display from that. The Verilog keeps the bitmap in its memory and has no
video output. The bridge handles only the hosted machine's pixel order,
polarity and row layout.

## The ports

All three implementations have these ports. A port a machine doesn't have
reads as zero, and writing it does nothing.

| Port | Read | Written |
|---|---|---|
| `0xd0`, `0xd1` | the depth of the data stack, of the return stack | sets it (Stacks, above) |
| `0xe0` | a character from the console, when there is one | |
| `0xe1` | true if a character is waiting | |
| `0xf1` | | a character to the console |
| `0xc0` | where the machine's cell 0 is, as an address a device would be given | latches the address of a device register |
| `0xc1`, `0xc2`, `0xc4`, `0xc8` | 8, 16, 32 or 64 bits at the latched address | the same, stored there |

The last two rows are how a driver written in Forth reaches a device whose
registers are a fixed width at a host address. A machine whose cell can't
hold such an address reads `0xc0` as zero, and its latch is an address in
its own memory. The reference VM reports that a character is always waiting,
and then waits for one.

## Other devices

A device is not part of the machine. It is reached through ports by a
driver written in Forth, and a machine of any kind (the reference VM, QEMU,
the Verilog, hardware) has the devices it has. The keyboard, pointer and
disk of the machine the AArch64 kernels run on are QEMU's virtio devices,
reached through the last two rows above (`virtio-disk.fs`, which is in
that machine's ROM, and `virtio.fs` and `virtio-input.fs`). What the system needs from a keyboard and a pointer is
a word that passes their events to `on-event` (`wm.fs`).

What the system needs from a disk is two words, to read and to write a
1,024-byte block, which are put in the kernel's vectors `'blk-read` and
`'blk-write` (`nuc.fs`); `blocks.fs` builds on them. A disk is no more
than 1,024 blocks, what an 8-inch floppy holds, and a machine may have
several: the one it starts from, and others, which the pictures are on. A
machine has the disks it has, and its ROM has the driver for the one it
starts from: a file of its own, which
the build of that machine's image names and the nucleus does not
(`port-disk.fs` for the controller below; `virt-disk.fs`, with
`virtio-disk.fs`, for QEMU's). When the machine starts it asks that driver
whether the disk is there, reads the image through it, and puts its words
in the vectors. Nothing in the nucleus or the system names a controller.

A machine may have several disks, and a block's number says which disk as
well: it is 16 bits, the top four the disk and the other twelve the block
on it (`vm_constants.h`). So a number with no disk in it is of disk 0,
there are at most sixteen disks, and a disk is at most 4,096 blocks. A
driver is given such a number, and reading a block of a disk that is not
there gives 1,024 bytes of nothing.

One such controller is three ports, which the reference VM (`vm.c`) has,
with a file it is given as each disk. Port `0xb0` holds a block's number,
disk and all, and reads as the number of blocks on the disk that number is
of. Writing a cell address to port `0xb1` reads that block into the 1,024
bytes starting at that cell; writing one to port `0xb2` writes the block
from there. A machine without the controller reads them as zero and
writing them does nothing, as with any port a machine doesn't have.

Two more ports are specific to this VM, for debugging: port `0xf0`, and port
`0xe0` when written (`8emit` and `.s` in `vm_opcodes.h`). Neither yet does
quite what it is meant to. A cell written to the first sends its low byte
followed by bytes that aren't from the cell, and anything written to the
second prints one cell of each stack.
