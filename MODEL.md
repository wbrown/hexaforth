# The machine model

Hexaforth is one machine with three implementations: `vm.c`, the AArch64 kernels in `baremetal/`, and the Verilog in `rtl/`. This file defines the machine. A program may rely only on what is written here, and every implementation must behave the same way wherever this file defines the behaviour.

Where this file says something is undefined, implementations may differ, and a program that does it is wrong. The strict builds stop at the first such thing:

| Implementation | Strict build |
|---|---|
| `vm.c` | compiled with `-DSTRICT` |
| the small AArch64 kernel | `STRICT` given to CMake: `cmake -DSTRICT=ON build/cmake` |

## Sizes

Sizes are properties of a particular machine. They are set in `vm_constants.h`, and the toolchain and the other implementations take them from there.

| Name | Meaning | Default |
|---|---|---|
| `CELL_BITS` | width of a stack cell, the ALU, and a fetch or store: 64, 32 or 16 | 64 |
| `ADDR_BITS` | memory is 2^`ADDR_BITS` cells | 22 |
| `STACK_CELLS` | cells in each of the two stacks | 256 |
| `VAR_BASE` | the first of the 4,096 cells that one instruction can fetch or store | byte `0x4000` of the image, as a cell |
| `DISP_W`, `DISP_H` | the display's size in pixels; 0 where the machine lacks a display | 768, 576 |
| `DISP_BASE` | the cell where the bitmap starts | `0x4000` |
| `DISP_LEFT_LOW` | 1 if the leftmost pixel of a cell is its lowest bit, 0 if its highest | 1 |
| `DISP_INK_BLACK` | 1 if a set bit is black, 0 if it is white | 1 |
| `DISP_STRIDE` | cells from one row to the next in the same bank | 12 |
| `DISP_BANKS` | how many banks the rows are spread across | 1 |
| `DISP_BANK_STEP` | cells from the start of one bank to the next | 0 |

The defaults describe the machine with 64-bit cells, which this file calls the hosted machine. It is what `vm.c` builds by default and what the AArch64 kernels implement. For 32- or 16-bit cells, `vm_constants.h` gives 2^17 or 2^16 cells of memory, and a display only where one is specified.

`util/machines` lists the three machines the system is built and tested on:

| Cell | Memory | Display | Bitmap starts at cell |
|---|---|---|---|
| 64 bits | 2^22 cells | 768 by 576 | `0x4000` |
| 32 bits | 2^17 cells | 768 by 576 | `0x8000` |
| 16 bits | 2^16 cells | 512 by 342 | `0x8000` |

## Stacks

1. There are two stacks, the data stack and the return stack. Each is an array of `STACK_CELLS` cells, separate from memory and out of the reach of `@` and `!`. A program reaches a stack only by pushing, by popping, and through its depth port.
2. Each stack has a depth. Port `0xd0` is the data stack's and `0xd1` the return stack's. Reading the port gives the depth as it was when the instruction started, so `port io@` counts the port number it has just pushed.
3. Writing the port sets the depth, and that is all it changes. Cells hidden by lowering the depth keep their values, and reappear when the depth is raised again. An implementation may keep the top cells of a stack in registers, but it writes them back before the depth changes, so a program sees the same stack either way.
4. A cell that is popped is gone. Its value is undefined from then on, even if the depth is later raised over it.
5. Pushing onto a full stack, popping an empty one, and setting a depth outside 0 to `STACK_CELLS` are undefined.

Rule 3 lets the Forth system's tasks share the two stacks, each task owning a range of depths, so that switching tasks means saving two depths and setting two others. It also lets a program reach any cell of a stack by its position through the depth port alone, by setting the depth to that position, pushing or popping, and setting the depth back.

## Memory

1. Memory is an array of cells. An address is a cell number, starting from 0. `@` and `!` fetch and store one cell. A cell is the smallest unit of data, and the machine counts only in cells and instructions: code is addressed by instruction, data by cell, and the stacks by cell.
2. Memory is exactly 2^`ADDR_BITS` cells. A fetch or store outside it is undefined.
3. Devices are outside memory, and are reached through the ports.

A machine with 16-bit cells can have 65,536 of them, which is 128KB.

Text is a convention of the Forth system. Characters are packed into cells starting from the low end. Words that handle whole strings can work a cell at a time, as the dictionary search, the glyphs and the image viewer do, and words that handle single characters fetch a cell and shift.

`c@`, `c!`, `char+` and the rest of standard Forth's character words are ordinary Forth words, outside the machine, so how they represent a character's address is up to them. In the Forth system a character's address is its cell's address times the number of bytes in a cell, plus its position in the cell. Such an address reaches all of memory when cells are 64 or 32 bits and only the first 32,768 cells when they are 16 bits, so the kernel keeps its text in those cells. A program that wanted text anywhere in memory would use segments instead, with a variable per task holding the cell where character addresses start and `c@` working out the cell and the shift from it.

## Variables

A literal with the fourth value of its shift field fetches or stores one of the 4,096 cells starting at `VAR_BASE` (Instructions, below), so a variable in those cells is fetched or stored in one instruction. `VAR_BASE` is where the data in the Forth system's image starts, so the system keeps its variables there and its dictionary headers above them.

## Code

1. An instruction is 16 bits, and code is addressed by instruction. Cells wider than 16 bits hold several instructions, the first in the low bits.
2. Code is in pages of 4,096 instructions, which is 8KB on every machine.
3. The target of a jump, a call or a conditional branch is 13 bits. Bit 12 says what the other twelve are:

   | Bit 12 | The other twelve bits |
   |---|---|
   | clear | an address in page 0, reachable from any page |
   | set | an offset within the page the branch itself is in |

   Either way a branch reaches 4,096 instructions. The Forth system has its kernel in page 0, so code in any page calls the kernel with one instruction.
4. A return address and an execution token are full instruction addresses, so returning and `execute` can cross pages. A call instruction reaches only page 0 and its own page, so the Forth system's compilers compile a call into any other page as a literal execution token followed by a call to `execute`.
5. The program counter is a cell, so a machine can execute anywhere in its memory.
6. The first 2,048 instructions of page 0 (`KROM_BYTES`, which is 4,096 bytes) are the code of the Forth system's kernel, and a machine may have them in ROM. A store there is undefined.

A machine's layout is in whole pages, because code must start on a page boundary, and data and the display may sit between pages of code. With 16-bit cells an instruction is one cell, so a page is 4,096 cells.

A header in the Forth system's dictionary holds a word's execution token in one cell and uses the top bit of that cell to flag an immediate word. With 16-bit cells a token then has fifteen bits, so the Forth system executes only in the first 32,768 instructions, although the machine can execute anywhere in its memory (rule 5).

The Forth system's character words take a byte address. With 16-bit cells a byte address is twice the cell address and must fit in a cell, so those words reach only the first 32,768 cells, although the machine itself addresses cells and fetches or stores any of them (Memory, above).

The Forth system's layout of the 16-bit machine that is built and tested, which has 65,536 cells, therefore has its code and everything it reaches by a byte address in the lower half of memory:

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

An instruction is 16 bits, with the fields below (`vm_instruction.h`), named here as `vm_opcodes.h` names them and as they are written in source that `cross.fs` compiles. This section defines what a literal or an ALU instruction does. `vm.c` and `util/fuse.h` share one implementation of it, `vm_step.h`.

T is the top of the data stack, N the cell under it, and R the top of the return stack. A flag is a cell with all bits set (true) or all clear (false).

The 16 bits, from bit 15 down to bit 0:

    1 a ss nnnnnnnnnnnn       literal    a: add   ss: shift   n: number
    0 00 ttttttttttttt        ubranch    t: target
    0 01 ttttttttttttt        0branch
    0 10 ttttttttttttt        scall
    0 11 e ii oo ffff dd rr   alu        e: RET   ii: input   oo: output   ffff: operation
                                         dd, rr: movement of the data and return stacks

An instruction of all zero bits halts the machine.

### Literals

| Shift | Bit 14 clear | Bit 14 set (`imm+`) |
|---|---|---|
| 0 | the number is pushed | it is added to T, and the stack depth is unchanged |
| 1 (`imm<<12`) | the number, shifted left by 12, is pushed | it is added to T, and the stack depth is unchanged |
| 2 (`imm<<24`) | the number, shifted left by 24, is pushed | it is added to T, and the stack depth is unchanged |
| 3 | the cell at `VAR_BASE` plus the number is pushed | T is stored in that cell, and popped |

Literals reach 36 bits, so the Forth system's compilers build a wider number with a shift.

### Jumps and calls

| Instruction | What it does |
|---|---|
| `ubranch` | jumps |
| `0branch` | pops T, and jumps if T was zero |
| `scall` | pushes the address of the next instruction onto the return stack, then jumps |

Bits 0 to 12 are the target (Code, rule 3).

### ALU instructions

Each field is a number, with these meanings:

| Field | Bits | 0 | 1 | 2 | 3 |
|---|---|---|---|---|---|
| input | 10, 11 | `N->IN` | `T->IN` | `[T]->IN` | `R->IN` |
| output | 8, 9 | `->T` | `->R` | `->io[T]` | `->[T]` |
| data stack movement | 2, 3 | none (`d+0`) | up one (`d+1`) | down two (`d-2`) | down one (`d-1`) |
| return stack movement | 0, 1 | none (`r+0`) | up one (`r+1`) | down two (`r-2`) | down one (`r-1`) |

A stack movement is a signed two-bit number. Bit 12, `RET`, makes the instruction also return. Bits 4 to 7 are the operation:

| | Operation | OUT |
|---|---|---|
| 0 | `IN->` | IN |
| 1 | `T<>N,IN->` | IN as it was; T and N are swapped |
| 2 | `T->N,IN->` | IN; T is written into the cell under it |
| 3 | `IN+N` | the sum |
| 4 | `IN&N` | and |
| 5 | `IN\|N` | or |
| 6 | `IN^N` | exclusive or |
| 7 | `IN*N` | the product, as much of it as fits in a cell |
| 8 | `~IN` | IN with every bit inverted |
| 9 | `IN==N` | a flag: they are equal |
| 10 | `N<IN` | a flag: N is less than IN, as signed numbers |
| 11 | `Nu<IN` | a flag: N is less than IN, as unsigned numbers |
| 12 | `IN>>T` | IN shifted right by T, bringing in zeros |
| 13 | `IN<<T` | IN shifted left by T, bringing in zeros |
| 14 | `[IN]` | the memory cell IN points to |
| 15 | `io[IN]` | what port IN reads |

A shift uses only the low six bits of T, whatever the cell size, so a shift by the full width of a 16- or 32-bit cell gives zero and a shift by 64 leaves the value as it is.

An ALU instruction is carried out in this order:

1. The input IN is N, T, the memory cell T points to, or R.
2. The operation produces OUT from it.
3. With `RET`, the next instruction is the one at R, using R's value from the start of the instruction.
4. The stacks move. A stack that moves up has its old top in the cell under its new top.
5. OUT goes to T; or to R; or is stored in the memory cell T points to; or is written to port T. In the last two cases, T is the value the operation saw: the stack movement has not yet changed it.
6. A stack that moved down takes its new top from the cell that is now on top, unless that is where OUT went.

### In source

An instruction is written as its fields' names, input first and `alu` last. A field left out is 0.

| Word | Fields |
|---|---|
| `!` | `N->IN ->[T] d-2 alu` |
| `+` | `T->IN IN+N ->T d-1 alu` |
| `dup` | `T->IN IN-> ->T d+1 alu` |
| `exit` | `T->IN IN-> ->T RET r-1 alu` |
| `true` | `N->IN IN==N ->T d+1 alu` |

`!` is input 0, output 3, data stack down two, which is `0x6308`. `true` is N compared with itself, which pushes -1 whatever N is.

## Display

The display is a bitmap in memory, one bit per pixel, whose size is set in the machine definition and may be zero. Drawing is fetching and storing cells. How the bitmap is shown is up to each implementation.

### Geometry

The size, the order of pixels in a cell, the meaning of a set bit and where each row is are properties of the machine, because real displays differ in all four. A machine built around an existing display uses that display's pixel order and polarity.

The hosted machine's display is 768 by 576, which is 12 64-bit cells per row, and the 16-bit machine's is 512 by 342. On the hosted machine pixel x of a row is bit x of its cell, so the leftmost pixel is the lowest bit, and a set bit is black, so memory that is all zero is a blank page.

A row starts at a cell boundary and is `DISP_STRIDE` cells long. Rows are spread in turn across `DISP_BANKS` banks, so row y is in bank y mod `DISP_BANKS`, as row y / `DISP_BANKS` of that bank:

    row y starts at  DISP_BASE + (y mod DISP_BANKS) * DISP_BANK_STEP
                               + (y / DISP_BANKS) * DISP_STRIDE

With one bank, as on the hosted machine, rows simply follow one another. A Hercules card has four banks: with 16-bit cells its rows are 45 cells long and its banks are 4,096 cells apart.

### Constraints

A display has to suit the cell size it is drawn with, so the build rejects a definition that breaks any of these rules, in `vm_constants.h` and again in `cross.fs`:

- The width must be a whole number of cells.
- The stride must cover a row.
- Banks must be disjoint.
- The base must be a 12-bit value shifted left by 12, so that the Forth system's drawing code can load it with one literal instruction and add it to an offset with one more.

720 by 348 in four banks, a Hercules card, is accepted for a 16-bit machine and rejected for a 64-bit one.

On the hosted machine `cross.fs` lays the Forth system's image out in the first 65,536 instructions of memory, which is 128KB, and the bitmap starts at the next cell, `0x4000`, which is 4 shifted by 12. The bitmap is 6,912 cells long, and the memory the Forth system allocates at run time (`halloc`) starts after it.

### In the Forth system

The Forth system takes the pixel order, the polarity and the row layout from the machine definition, through the toolchain:

- Fonts are compiled with their pixels in the machine's order.
- All drawing is built on three words that are defined to match: the address of a row, the mask for a pixel, and setting or clearing ink (`display.fs`).
- Drawing code moves from one row to the next only through the first of those words.

### How each implementation shows it

| Implementation | How the bitmap is shown | Limits |
|---|---|---|
| the AArch64 kernels, on QEMU | a bridge, `baremetal/ramfb.S`, copies it into the true-colour buffer that `ramfb` expects whenever the machine asks for a key, at most 32 times a second | a program that draws for a long time is seen only when it next asks for a key, and the bridge handles only the hosted machine's pixel order, polarity and row layout |
| `vm.c` | it writes out its memory when it stops, and `util/screen.c` extracts the display from that | |
| the Verilog | it only keeps the bitmap in its memory | |

The bridge is a separate assembler file from the kernel, so that a different display can replace it.

## The ports

A port that a machine lacks reads as zero and ignores what is written to it. The fourth column says which implementations have each port: all three, or only the reference VM, `vm.c`.

| Port | Read | Written | Has it | Notes |
|---|---|---|---|---|
| `0xb0` | the number of blocks on the disk that the block's number selects | a block's number, including the disk | VM | the disk controller: Other devices, below |
| `0xb1` | | a cell address: that block is read into the 1,024 bytes starting at that cell | VM | |
| `0xb2` | | a cell address: the block is written from there | VM | |
| `0xc0` | | latches the number of a device cell | all | |
| `0xc1` | the latched device cell | a cell, stored to the latched device cell | all | |
| `0xc2` | | a cell address: that cell's address, in the form the device takes, is stored starting at the latched device cell | all | where that form is wider than a cell, it continues into the cells that follow |
| `0xc3` | | the address of a memory cell that holds a cell address: the address it holds is replaced by the form the device takes | all | the same |
| `0xd0`, `0xd1` | the depth of the data stack, of the return stack | sets it | all | Stacks, above |
| `0xe0` | a character from the console, when there is one | anything written prints one cell of each stack | all for a read, VM for a write | the write is `.s` in `vm_opcodes.h`, for debugging, and is still faulty |
| `0xe1` | true if a character is waiting | | all | the reference VM reports that a character is always waiting, and then waits for one |
| `0xf0` | | a cell written sends its low byte followed by stray bytes | VM | `8emit` in `vm_opcodes.h`, for debugging; it is still faulty |
| `0xf1` | | a character to the console | all | |

`0xc0` to `0xc3` are how a driver written in Forth operates a device at every cell size. A device appears to the machine as device cells: cells of the machine's own size, numbered from the machine's first device, which a driver reads and writes one at a time. A 16-bit machine reads and writes 16 bits to a 16-bit device cell, a 32-bit machine 32 bits to a 32-bit one, and a 64-bit machine 64 bits to a 64-bit one. What each device cell holds is the driver's knowledge. Where the device and the machine's memory are on the platform is the implementation's knowledge: a driver names memory by cell address, and the implementation makes the address the device takes. On a machine that lacks devices, every device cell reads zero and writes to these ports are ignored.

## Other devices

A device is outside the machine: a driver written in Forth reaches it through ports, and each kind of machine (the reference VM, QEMU, the Verilog, hardware) has its own devices.

| Device | The system needs | Where it is kept |
|---|---|---|
| keyboard and pointer | a word that passes their events to `on-event` (`wm.fs`) | |
| disk | two words: one to read a 1,024-byte block and one to write it | the kernel's vectors `'blk-read` and `'blk-write` (`nuc.fs`), which `blocks.fs` builds on |

The machine the AArch64 kernels run on has QEMU's virtio keyboard, pointer and disk, reached through `0xc0` to `0xc3` (`virtio-disk.fs`, which is in that machine's ROM, and `virtio.fs` and `virtio-input.fs`). Its device cells are QEMU's 32 virtio-mmio slots in order, 64 device cells each, and the address a device takes there is the cell's 64-bit physical address.

### Disk

The Forth system allows a disk at most 1,024 blocks, the capacity of an 8-inch floppy. A machine can have several disks: the one it starts from and others that hold the pictures.

The block number that a driver is given is 16 bits and selects the disk as well as the block (`vm_constants.h`):

| Bits | Field |
|---|---|
| the top four | the disk |
| the other twelve | the block on that disk |

A plain block number therefore means disk 0, there are at most sixteen disks, and a number reaches at most 4,096 blocks of a disk. Reading a block of an absent disk gives 1,024 zero bytes.

The driver for the disk a machine starts from is in that machine's ROM, and is a file of its own that the build of the machine's image names, so that the nucleus and the system stay independent of the controller:

| Disk | Driver |
|---|---|
| the controller below | `port-disk.fs` |
| QEMU's | `virt-disk.fs`, with `virtio-disk.fs` |

When the machine starts, the Forth system's kernel asks that driver whether the disk is present, reads the image through it, and stores the driver's words in the vectors.

### The reference VM's disk controller

The reference VM (`vm.c`) has a disk controller of three ports, `0xb0` to `0xb2` (The ports, above), with a file for each disk. A machine that lacks the controller reads these ports as zero and ignores writes to them, as with any port a machine lacks.
