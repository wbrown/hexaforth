# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## The machine model

`MODEL.md` says what the machine is: the stacks, memory and display that
`vm.c`, the AArch64 kernels and the Verilog must all provide in the same
way. Read it before changing any of the three, and keep them to it.

## The instruction set is the whole encoding

An instruction is a control word: where the ALU's input comes from, the
operation, where the result goes, how each stack moves, and whether to
return. Every combination is an instruction that `vm.c`, the kernels and
the Verilog already execute: 8,192 encodings, 7,712 distinct behaviours.
There are no opcodes, and a baseword is only a name for one of them.

- Do not judge an encoding, or a sequence that one encoding equals, by
  whether it looks useful. Measure: `insn_map.py --image` says where it
  occurs, and `make demo-profile` how often it runs. A site that is rare in
  the source may be in the hottest loop
- A name costs a line in `vm_opcodes.h` and nothing in the machine. An
  encoding with no name is one the programmer cannot say and the nucleus's
  compiler cannot emit
- Any control word can be said in source by its fields, and the peephole
  joins it to its neighbours as it does a named one:
  `meta N->IN [IN] ->[T] d-1 alu target`
- Most instructions are said by a phrase of standard words that the
  peephole joins. `python3 util/phrases.py` is the phrase book: for each
  instruction that a run of up to three of the machine's words equals (four
  with `LONGEST=4`, five minutes), the shortest run, and whether `cross.fs`
  makes one instruction of it; the report is `build/phrases.txt`. With
  four words 1,289 of the 3,856 behaviours (not counting those that also
  return) have a phrase and 1,136 compile to one instruction. It also runs
  every word here that gforth has in gforth, to see that the name means
  the same. A baseword's name must mean what the standard says: the shift
  right by one is `u2/`, and `2/`, which keeps the sign, is a word of the
  nucleus
- Do not propose a new ALU operation or any other change to the machine
  for speed. The operation field is full, the ALU has two inputs, and the
  design is that the fields compose. First show what the existing fields
  give, and whether the work is needed at all
- Think of a word as data moving: from where, through what, to where, a
  cell at a time. Bytes, opcodes and per-element loops are habits from
  other machines
- A loop goes up, not down. Adding one is one instruction; taking one away
  is three, since a literal can only add what is not negative. A loop over
  memory keeps no count: `bounds begin 2dupxor while ... 1+ repeat 2drop`,
  and a second place is the first plus how far apart they are
  (`cells-move`). A loop that only counts starts below nothing and goes up
  to it: `negate begin dup while ... 1+ repeat drop`. What is the same
  every turn is kept on the return stack, where it is an input of the ALU
  (`cells-fill`). Both words are in the kernel: `cells-move ( from to n )`
  and `cells-fill ( x a n )`, six and four instructions a turn; use them
  before writing another loop over cells

## Shell code is a last resort

Change a file with the Edit tool, and make a new one with Write. Do not
write a shell or Python script to edit source: a change made that way
cannot be read as a diff or approved, the harness does not know the file
changed, and a wrong replacement is easy to miss. Read and search with
Read, Grep and Glob, not `cat`, `sed` and `grep`.

The shell is for what has no tool: building, running the suites,
measuring, and git. When a suite is run, keep all of its output in a file
and print a summary from that; do not cut or head the results.

## IMPORTANT: Read UNDERSTANDING.md First

Before working with this codebase, read `UNDERSTANDING.md` which provides deep insights into:
- How hexaforth transforms the J1 CPU design for software
- The dataflow multiplexer architecture and why it matters
- The metacircular build system
- Performance implications of the design

This document provides practical information for development.

## Build Commands

```bash
# Standard build
make

# Clean build
make clean

# Build and run tests
make tests

# Build specific targets
make hexaforth          # Main release executable
make hexaforth-debug    # Debug version with tracing
make hexaforth_test     # Test runner
make basewords-fs-gen   # Basewords generator utility
```

## Running and Testing

```bash
# Run hexaforth with a hex file
./hexaforth <hexfile>

# Run debug version (includes instruction tracing)
./hexaforth-debug <hexfile>

# Run all tests
./hexaforth_test
# or
make tests

# Test output is redirected to hexaforth_test.debug
```

## Architecture Overview

Hexaforth is a 64-bit stack machine VM designed for maximum performance through three key innovations:

1. **"Dumb VM" design philosophy**: The VM executes only raw 16-bit instructions with zero interpreter overhead. All Forth knowledge is moved to compile time, eliminating dictionary lookups, threading overhead, and runtime complexity.

2. **Dataflow multiplexer architecture**: Unlike traditional stack machines where all operations must route through the stack, hexaforth allows data to flow directly between memory, ALU, and I/O components. This eliminates unnecessary stack traffic and enables complex operations in single instructions.

3. **64-bit optimized literal encoding**: A sophisticated shift-and-add system that can efficiently encode 48-bit values while enabling inline operations through the `lit_add` flag. This works synergistically with the dataflow architecture to minimize instruction count.

The combination creates a system where Forth compiles to efficient machine code with near-native performance potential.

### Core Components

- **VM Core** (`vm.c/h`): Main virtual machine implementation with 64K 16-bit memory, separate data and return stacks (128 elements each)
- **Opcodes** (`vm_opcodes.c/h`): Instruction definitions and decoding logic
- **Stack** (`util/stack.c/h`): Stack operations implementation
- **Debug** (`vm_debug.c/h`): Debugging support including disassembly and tracing

### Instruction Architecture

The VM uses a dataflow-inspired microinstruction architecture that allows data to flow directly between components without mandatory stack operations. Key features:

#### Dataflow Multiplexer Architecture
Each ALU instruction contains:
- **Input mux** (2 bits): Selects data source (N, T, [T], or R)
- **ALU operation** (4 bits): Performs computation
- **Output mux** (2 bits): Routes result (to T, R, IO[T], or [T])

This enables complex single-instruction operations like:
- `+!` (add to memory): `[T]->IN IN+N ->[T]` - memory→ALU→memory in one cycle
- the rest of that family, one instruction each: `or!` `and!` `xor!` `*!`
  ( x addr -- ) and `not!` ( addr -- ) put the result back in the cell;
  `@+` `@and` `@or` `@xor` `@*` `@=` `@<` `@u<` ( x addr -- y ) and `@not`
  ( addr -- y ) leave it on the stack. A path the machine has costs nothing
  to name: a line in `vm_opcodes.h`, and no change to `vm.c`, the kernels
  or the Verilog
- `@@` (double indirection): `[T]->IN [IN] ->T` - chains two memory loads
- `swap>r`: Routes data through swap operation directly to return stack

#### Literal Encoding (64-bit Optimized)
The literal system is specifically designed for efficient 64-bit value encoding:

**Structure**: Each 16-bit literal instruction contains:
- 12-bit immediate value (0-4095)
- 2-bit shift selector (0, 12 or 24 bit shifts)
- 1-bit add flag (push new value vs. add to existing)

The fourth value of the shift selector is not a literal: the instruction
fetches the cell at `VAR_BASE` plus its twelve bits, or with the add flag
set stores the top of the stack there. `VAR_BASE` is where the image's data
starts, and `cross.fs` keeps variables in the 4,096 cells from it, so
`var @` and `var !` are one instruction each (`fold-var`). See `MODEL.md`.

**Key Features**:
- Can encode any 36-bit value in 1-3 instructions; a wider one is its high
  part, a shift of 36, and the rest added
- Zero-segment optimization (e.g., 32768 = `8<<12` in one instruction)
- Negative numbers encoded as positive + invert
- `lit_add` flag enables inline operations (e.g., `1+` as single instruction)

**Examples**:
- `20`: One instruction (direct 12-bit value)
- `-24923`: Two instructions (347 + 6<<12, then invert)
- `0x123456789ABC`: Six instructions (the high nine bits, 36, `lshift`, then three adds)

This complements the dataflow architecture by allowing efficient constant generation that can flow directly to any destination without stack overhead.

#### Instruction Types
- ALU operations with flexible input/output routing
- Memory operations (load/store with 8-bit and 64-bit variants)
- Stack operations (can adjust both stacks by ±1 or ±2 in one instruction)
- IO operations for console input/output

### Toolchain and Bootstrap

The system uses a sophisticated toolchain that keeps the VM simple while providing powerful development tools:

**Instruction Definitions**: `vm_opcodes.h` contains Forth words defined using micro-operation syntax:
```c
{"+", "T->IN   IN+N   ->T   d-1   alu"}  // Add operation
```

**Build Process**:
1. `basewords-fs-gen` reads C definitions and generates `forth/basewords.fs`
2. `forth/cross.fs` cross-compiles Forth source to hex files
3. VM loads and executes hex files directly

**Testing**: The test framework includes a mini Forth compiler that uses the same instruction definitions to compile test cases.

This design provides a single source of truth for instruction encodings while keeping the runtime VM completely free of Forth-specific knowledge.

### Testing Framework

Tests are defined in `test/main.c` as an array of `hexaforth_test` structures. Each test specifies:
- Input Forth code
- Expected data stack state
- Expected return stack state
- Optional IO expectations

## Development Notes

- Debug builds define the `DEBUG` macro and include verbose tracing
- Memory is an array of cells: `@` and `!` take the number of a cell, and a
  branch, call or execution token the number of an instruction
- The "dumb VM" design eliminates all interpreter overhead for maximum performance
- The project explores how dataflow principles can optimize stack machine performance
- All Forth complexity lives in the toolchain, not the runtime

### Addresses

An address is the number of a cell, and `@` and `!` fetch and store one
cell: the machine has nothing smaller, and nothing in it counts bytes.
`cell+` is `1+` and `cells` does nothing.

Text, and structures a device lays out in bytes, are reached by the address
of a byte: the cell's address times the bytes in a cell, plus the byte's
place in it. `>c` turns a cell's address into the address of its first byte
and `c>` gives the cell a byte is in. `c@`, `c!`, `uw@`, `w!` (and `l@`,
`l!` and the big-endian words in `machine.fs`) take the address of a byte,
fetch the cell it is in and shift the field out of it or into it
(`field@`, `field!`); a field must not cross a cell boundary. The data
pointer `dp` counts bytes, so that `c,` can lay down one; `here` is the
cell it is in, and `,` aligns first. `create` gives the address of a cell,
so a buffer of text is used as `buf >c`. In source, `CELL` and `CELL2` are
the steps to the next cell and the one after, and `c# n` to the nth.

Code is addressed by instruction: `cp`, a branch origin and an execution
token are instruction numbers, and `i@` and `i!` fetch and store one.

Dictionary headers are whole cells: a link cell, an xt cell (top bit set for
an immediate word), then the name as a count byte and lower-case characters
padded with zeros to a cell boundary, so lookup compares a cell at a time.

To check, build the reference VM with `-DSTRICT` or the small kernel with
`make -C baremetal STRICT=1`; either stops at a fetch or store outside
memory, and at a stack leaving its `STACK_CELLS` cells.

### Tasks

The nucleus has cooperative tasks. The two stacks are shared by depth, as
`MODEL.md` allows: a task owns a range of depths on each, and `pause` saves
the two depths and sets the next task's. A task is a record of thirteen
cells: the next task round the ring, the two depths it stopped at, the word
it runs, and then the variables each task has its own of: `sbase` (where
its data stack starts), the `catch` handler, `base`, `'emit`, `sourceid`,
`>in`, the source, and the `DO` loop offset. Those are words that find the
cell through `up`, the task that is running, so changing task copies
nothing but the loop offset. `state`, `dp`, `cp` and the dictionary are
shared: only one task should compile at a time.

`xt dbase rbase record spawn` starts a task that runs `xt` again and again
with a `pause` between; if it throws, the task reports it and from then on
only takes its turn. `key` pauses while no key is waiting. Nothing stops a
task that runs past its part of a stack. The kernel's code is 3,978 bytes
with 64-bit cells, 3,998 with 32 and 3,942 with 16, against the 4,096 the
kernel ROM holds.

On the desktop the Forth window's interpreter is a task with the upper half
of each stack (`repl` in `term.fs`), and the event loop pauses each time
round. The desktop takes the keys and hands over a line at a time, so it
goes on running while a line does, as long as the line pauses.

The image viewer reads and dithers its picture in a third task (`view-row`
in `view.fs`), a row of the image at each of its turns, so the desktop is up
at once and the picture comes down its window. It has cells 96 to 127 of
each stack: the desktop's own task was measured to reach cell 35 and no
further, and this one uses 10 cells of the data stack and 16 of the return
stack (7 and 14 before the regions kept what they work on on the stacks,
and a window put out of date only what can be seen of it: putting rows
out of date is where it goes deepest). `make demo-profile`
says how far each task got. Nothing stops a task from outside. This one takes itself out of the
ring when the last row is made (`retire`): the task before it is made to
point past it, and its next `pause` does not come back. The rows are put
out of date eight at a time, since every window that lies over what is out
of date is painted again: a row at a time made the picture half as long
again in coming. At 1 MIPS the desktop is ready in 2 seconds and the
picture is finished 35 seconds after, where it was 34 seconds before
anything was drawn; a turn of the event loop is 69 ms while it loads.

### Code Paging

The program counter is a flat word address and code is divided into pages of
4096 words (8KB). Page 0 is the kernel. The 13-bit target in a jump, call or
0branch means:

- bit 12 clear: an absolute address in the kernel, reachable from any page
- bit 12 set: an offset within the page the branch itself is in

Return addresses and execution tokens are full program counters, so `exit`
and `execute` cross pages with no extra state. Memory layout:

- instructions `0x0000-0x0fff`: kernel code (cross-compiled)
- bytes `0x04000-0x09fff` of the image: data. `cross.fs` lays variables
  and tables down here, and data made at run time (`here`, `,`, `allot`,
  `variable`) carries on after it
- bytes `0x0a000-0x0ffff`: dictionary headers, and after them the headers
  made at run time. The nucleus keeps two pointers, `dp` for
  data and `hdp` for headers, as `cross.fs` does, so that variables stay
  in the cells that one instruction fetches and stores; making a header
  throws -8 if the data has reached the headers
- instructions `0x8000-0xffff`: eight code pages for definitions made at
  run time

On a 64-bit machine these are cells `0-0x3ff`, `0x800-0x13ff`,
`0x1400-0x1fff` and `0x2000-0x3fff`; the display is at cell `0x4000` and `halloc` gives out
memory from cell `0x6000`.

In `nuc.fs`, `compile,` emits a one-instruction call for a kernel or
same-page target and `literal xt` + `call execute` for another page.
`newpage?` starts a fresh page when under 1KB is left, and `code,` throws -8
if a definition runs off the end of its page. Execution tokens are stored in
16 bits in dictionary headers, which is what limits code to 64K words.

### Devices, Blocks and Graphics

`forth/system.fs` (included at the end of `nuc.fs`) is cross-compiled into
code page 1 (`0x2000`) and code pages 8, 9 and 10 (`0x10000` up; page 10
is the nucleus's peephole, `join.fs`). The kernel
never calls into it; its words are reached by name or by execution token.
A page is 4,096 instructions, 8,192 bytes of code as `build/nuc.sizes`
gives it, and the image may use all of each.

`insn_map.py --compare` checks the image against a build with the peephole
off (`PEEPHOLE=off gforth cross.fs basewords.fs nuc.fs`), which is about a
third bigger. That build is never run, and nothing about the image is
arranged for it: it takes no notice of where `system.fs` starts a page
(`code-page`), and starts one of its own before any definition that has
less than a quarter of a page left to go in (`?own-page` in `cross.fs`).
The code is in the same order in both, and the comparison takes a call or
a jump across pages (an execution token and `execute`) to be the same as
one within a page. If the two are ever reported to have the same number of
instructions, the bigger build did not finish, and the image left in
`build/` was compared with itself.

Page 1 is what the rest is built on:

- `machine.fs`: exact-width device register access through IO ports
  `0xc0`-`0xc8`, `halloc` for memory above the dictionary, byte-order helpers
- `blocks.fs`: block device interface (`'blk-read`, `'blk-write`), `block`
  `update` `flush` `load`, and `disk-read` for byte ranges. There is one
  block buffer. `disk-read` does not use it for a whole block that is
  wanted where a cell starts: the driver is handed that place, so nothing
  is copied, and the block being edited is not turned out of the buffer or
  written back by something else reading the disk. It keeps nothing in a
  variable: what is read goes up through the device from the offset to
  where it ends, and where it goes in memory is that plus how far apart
  the two are, on the return stack
- `display.fs`: the display, a bitmap in memory of the size, place, bit
  order, polarity and row layout that `vm_constants.h` gives (see
  `MODEL.md`), and all drawing on it; nothing outside this file knows the
  layout. A cell is as many pixels across as it has bits (`PXCELL`,
  `PXSHIFT`, `PXMASK`). Drawing is modelled on QuickDraw:
  - the *port* is the state drawing depends on: origin (`ox`, `oy`), clip
    rectangle, clip region, `pattern`, `'mode`, `font`, and `inverse`
    `bold` `italic`. Everything that draws takes coordinates in the port.
    `port!` changes port; a port not in use is a record of eighteen cells.
    A clip region that is a rectangle, as most are, needs no mask: it and
    the clip rectangle asked for are one smaller rectangle (`reclip`), and
    only a region that is not one is looked at a row at a time. Even then,
    where the region is one run of pixels across the row (a window covered
    at a corner), what is drawn is trimmed to the run and nothing is masked
    (`mone`); a line of text whose rows are all in such a band has the clip
    rectangle trimmed for as long as it is drawn. A mask is used only where
    a row has more than one run
  - a *slab* is a run of pixels in the pattern and mode, and every fill is
    made of slabs (`pat-hline`, `pat-rect`). Modes are `copy-mode`
    `or-mode` `xor-mode` `bic-mode`; a pattern is eight rows of eight
    pixels (`p-white` `p-black` `p-gray` `p-ltgray` `p-dkgray`)
  - `copy-bits` copies a rectangle of a bitmap to the screen at any
    alignment
  - a font is a *strike* (all glyphs side by side, and a table of where
    each starts), so it may be proportional: `mono` is Unifont, `sans` is
    DejaVu Sans Bold. `px,` in `cross.fs` lays a strike down in the
    display's order. A font is opened out on first use into a cell for each
    row of each glyph. `draw-text` works a cell of the screen at a time, for
    all the rows of the font at once: each glyph is set into a band of
    cells, one for each row (`or!`, memory through the ALU to memory), and
    the band is stored when it is full. With `text-or` set in the port, text
    that is not inverse only adds its black to what is there; then plain
    text with no region to mask goes straight to the screen with `or!`, and
    no band is gathered. The window manager sets it for what a window
    draws, having just made the window white. A font opened out says, for
    each glyph, which rows have any black in them, and text that only adds
    its black draws only those
  - a fill that replaces what is there, with nothing masked, merges the
    cell at each end under its mask and stores the row of the pattern into
    every cell between (`fast-slab`); a copy of the same kind from cells
    lined up with those they go to, which every scroll up or down is,
    moves the cells between straight across (`move-row`: where a cell
    comes from is on the return stack, where it is what the store reads
    from, so a cell is two instructions and a turn six)
  - a rectangle is clipped once and not a row at a time (`pat-rect`): the
    rows the clip rectangle has, and the pixels of a row, which are the
    same for every row unless a region has to be looked at. A rectangle of
    plain white or plain black in copy mode, which most are, is then
    filled a column at a time, a column being one cell of every row
    (`plain-rect`, `column`): where it starts in a cell and which pixels
    of its two ends are its own is worked out once (`spans`), the pixels
    that change are taken out of each cell of the column with `and!` and
    those to be set put in with `or!`, and a cell that is all in the
    rectangle is stored. That is an instruction a cell and five to go
    round, where a row at a time was about 75 before a cell was touched.
    A display whose rows are dealt into banks goes a row at a time
  - under a region that is not a rectangle, a rectangle is a rectangle for
    each run of each band of the region that it meets (`bands-fill`,
    `runs`): the band's rows that it has, and what the run and it both
    have across, each filled as any rectangle is (`rows-fill`). No mask is
    made for a fill and no row of one is looked at by itself; the clip
    rectangle is trimmed to the rectangle's left and right for as long as
    it takes, so that a run is clipped to it as anything is. Lines, text
    and copies under such a region still go by the mask of the row. In
    the profile the display's work went from 13.6 million instructions to
    7.0 with the three

Pages 8 and 9 are the drivers and the desktop:

- `region.fs`: regions, as bands of where a shape goes in and out, with
  `rect-rgn`, `rgn-and`, `rgn-or`, `rgn-diff`, `rgn-offset`, `rgn-box`,
  `rgn-none`. `rgn-clip!` clips a port to one: drawing makes a mask of the
  region for the row it is on. `scroll-rect` moves what is in a rectangle:
  a pixel moves only if where it comes from and where it goes can both be
  seen, and the places left with nothing are made white and returned as the
  region `scroll-update`, to be drawn again (`inval-rgn`). See Regions,
  below
- `virtio.fs`, `virtio-blk.fs`: the QEMU `virt` block driver
- `tiff.fs`, `atkinson.fs`, `mandrill.fs`: uncompressed TIFF reader, Atkinson
  dithering, and the demo. Neither touches memory a pixel at a time. In
  the dither a pixel is a lane of a cell, sixteen bits, and all it is
  thresholded on is added into its lane before it is looked at: its gray,
  what the rows above gave it, and what the two pixels before it gave it.
  The last is the cell's own doing: the cell is on the stack with the
  pixel being done its lowest lane, and when the pixel is done the cell is
  shifted down a lane and what the pixel gives is added to the lowest two
  by one multiplication (`CELL-DONE`, the lanes of a cell written out in
  line), so what is left after a cell's last pixel is what the next
  cell's first two are owed. What a pixel gives is its error and, above it
  at bit 12, whether it is black, so a row that is done is both the errors
  for the rows below and the pixels: the pixels of a cell are brought
  together by one multiplication (`anib`), and the errors are added to the
  two rows below a cell at a time. The dither keeps three rows of lanes,
  the bitmap and how far down it is, between one row and the next, and
  nothing else: it had seventeen variables and a pixel cost 73
  instructions, and it has six and a pixel costs 38. A cell of 16 bits is
  one lane, and there what a pixel gives the two after it is two numbers
  on the stacks and not lanes of the cell; the rest is the same words.
  `take` shifts a pixel out of a
  cell fetched once, for an image that has to be asked for a pixel at a
  time. A row of an image of three samples that lies on cells
  (its width a multiple of eight) is not asked for a pixel at a time:
  eight pixels are three cells, two pixels' grays are made together (the
  reds masked where they are and multiplied by 77, the greens and blues
  shifted to the same two places and multiplied by theirs, and the three
  added), and four pixels' grays are a cell of lanes, which is added to
  the lanes of what those pixels are owed (`gray-lanes`, through
  `'grays`). Any other image gives a pixel at a time (`'pixel`).
  `atkinson ( bitmap -- )` makes the bitmap; `copy-bits`
  shows it. Each can be done a row at a time, by something that has other
  things to do between the rows: `tiff-start` and `tiff-row` read the
  image, noting where the strips are first so that the directory is not
  gone back to, and `dither-start` and `dither-row` dither it.
  `tiff-decode` and `atkinson` are the whole of each
- `font.fs`, `keymap.fs`: generated by `util/mkfont.py` (the two fonts;
  Linux key codes to ASCII); `font.fs` is on page 1
- `virtio-input.fs`: keyboard and tablet events, delivered to `'event`
- `gfx.fs`, `wm.fs`: rectangles in plain white and black; windows,
  pointer, event loop. Nothing draws when something changes: it is added
  to the region that is out of date (`inval-rect`, `win-inval`), and the
  event loop paints that (`refresh`), each window only where it can be
  seen, in its own port. What a window says has changed (`win-inval`) is
  out of date only where the window can be seen: where another lies over
  it there is nothing of it to paint, and the one on top is not painted
  again for it
- `term.fs`, `edit.fs`, `view.fs`, `calc.fs`: the Forth window, block
  editor, image viewer and RPN calculator
- `see.fs`: a decompiler (`see name`), and the Words window, which lists
  the dictionary and shows a definition with each word in it as a link.
  Words in bold are one instruction between them, and are underlined as
  one, the space between them too. With the pointer on those, on a number
  in brackets (an instruction with no words) or on a plain word that is an
  instruction by itself (`rshift`), a small box by it says what the
  instruction is to the machine: its number and its fields
  (`687C [T]->IN IN*N ->T d-1`), which is how it is written in source. The
  window manager runs `'moved` when the pointer has moved; the box is part
  of what the window draws, and the field names are in `fused.fs`
- `fused.fs`: generated by `python3 util/insn_map.py --fused build/nuc.hex
  forth/fused.fs` from a built image: each instruction as words. What is
  shown is meant to be read, so it is the plain words a programmer would
  write: the standard word that is the instruction (`dup`, `+`), or two
  standard words (`2dup xor`, `over +`, `dup @`), which read better than a
  name made of them (`2dupxor`, `over+`, `dup@`); then the machine's own
  name if it is in the dictionary (`or!`); then three standard words, two
  or three of any in the dictionary, or four (a five-minute search, kept
  in `build/fused4.cache`). A word is standard if gforth has it. A word
  of the dictionary that is not in `vm_opcodes.h` is never used, though it
  be one instruction (`ink`, which is `or!`), and the decompiler does not
  search the dictionary for one. More than one word is shown in bold. The
  entries the image has most of are first. Only what the image uses needs
  a label, and that is known after a build: about 140 instructions, not
  the whole encoding. An entry is the instruction, how many words, and the
  address of each word's header (`hdr, name` in `cross.fs`), not text: the
  name is in one place and nothing is looked up to show it. Regenerate and
  rebuild after changing code
- `desk.fs`: `desktop`, and `cold`, which the kernel runs at boot
- when all of that is compiled, `name-tables` in `cross.fs` makes two
  tables of the names for the decompiler, the dictionary's words and the
  labels: by execution token, and those that are a number (a word that
  only pushes one, a label for data) by the number. An entry is a cell,
  the key in the upper half and in the lower where the name is (a
  header's address, or for a label how far it is into the labels, with a
  bit to say so), and they are in order, so `tfind` in `see.fs` finds one
  by halving. The tables and the labels are not in the image: they are
  the file `build/nuc.tables` (22KB), which `mkdisk.py` puts on the disk
  from block 1024.
  `names-load` reads them into memory from `halloc` and `names-unload`
  gives the memory back, if nothing has taken more since. The Words
  window has them in while it is the window in front (`'front` in `wm.fs`
  is run when the front window changes; windows do not close), and `see`
  for as long as it runs. A stamp in the file must be the image's.
  Without them, or with no disk (the reference VM has none), the
  decompiler searches the dictionary a word at a time and has no labels

On a 64-bit machine `cross.fs` also keeps the name of every definition that
has no header: a label, laid out as a header is. A label for code holds
its execution token, and one for `create`d data holds the data's address
with the top bit set. The labels are for the decompiler and the tools, so
they are not in the image: they are in the file of names beside it
(`build/nuc.tables`, after the two tables), which goes on the disk. While
the decompiler has that file in memory, `labels` heads their chain, a call
to a headerless word and a headerless variable are shown by name, and
`see` accepts a label's name; without it they are shown as numbers. The
tools that want an image's labels (`insn_map.py`, `deskprof.py`,
`loops.py`, `shorter.py`) read the file with the image's name
(`labels_of`), so a copy of an image needs a copy of its `.tables`, and
`insn_map.py --compare` needs each of the two images'. The file's stamp
must be the image's, which is the cell the dictionary word `tstamp`
points at: a tool given names that are another build's stops and says so.

A word that another file uses has a header, so that it can be typed at the
prompt and used in new definitions: the drawing, window, block and device
words are all in the dictionary. A word used only inside its own file has
no header and gets a label. `header name create name` is allowed in
`cross.fs`: it lays down code that pushes the data's address, for the
header to point at.

The window manager puts a window's port in use before calling its draw
routine, with the origin at the top left of its content and the clip set to
what is to be painted of it. Each build writes `build/<name>.sizes`:
for every file included, the bytes of code it compiled and of data it laid
down, how much of that data is dictionary headers, a number that was the
labels (which are no longer in the image, so nothing), and the
instruction its code starts at. A file's figures include those of the
files it includes; `deskprof.py` gives an instruction to the smallest
file that has it.

`cross.fs` compiles a call to another code page as `literal xt` +
`call execute`, so code past the kernel needs only an `org` per page and
each page's code to fit in 8KB. Run-time definitions start on the page after
the last cross-compiled one. Output goes through the `'emit` vector, which
is how the Forth window captures it.

`baremetal/` has two native kernels with identical behaviour.
`hexaforth.S` is the small one and decodes each field with compares and
branches. `hexaforth-fast.S` decodes with two table lookups (input mux and
ALU, then output mux and stack deltas) and executes about half as many
native instructions; select it with `make KERNEL=hexaforth-fast <target>`.

QEMU has no 1-bit display, so `baremetal/ramfb.S` is a bridge: it sets up
`ramfb` through the firmware configuration device and copies the bitmap
into 32-bit pixels, a changed cell at a time, at most 32 times a second. It
is a separate file from the kernel, linked in front of it for the
`-display` builds that `make demo` and `make demo-test` use; a kernel built
with `-DBRIDGE` calls `bridge_init` at boot and `bridge_poll` whenever the
machine asks for a key or whether one is waiting. `vm.c` and the Verilog do
not show the bitmap yet.

`make demo SCALE=2` (or 3, or more) has the bridge draw each pixel of the
display that many across and that many down, for a screen whose own pixels
are small. The picture is a whole number of times the size and so still
exact, which QEMU's stretching of a window is not: a dither stretched by
anything but a whole number comes out in bands. Nothing in the machine or
the kernel changes, only what `ramfb` is given. `make demo-test SCALE=2`
runs the same checks on one pixel of each block, and checks as well that
every block is all one colour. The profile build is always the plain
bridge.

`make demo MIPS=5` (and `make run MIPS=5`) builds the kernel to execute
that many million instructions a second and no more, to see what a machine
of that speed is like to use. After every 256 instructions it waits for the
time at which such a machine would have finished them, so it is slow
evenly: QEMU's own speed setting runs the CPU flat out for 10 ms and stops
it for the rest of each tenth of a second, which makes the pointer jump.
The speed is the same on either kernel and on any host that can reach it,
and it may have a fraction (`MIPS=0.5`). If the time has passed already
(the host is slower, or the machine has been waiting for a key) it carries
on from where it is and does not hurry to catch up. The count and the time
are in two registers and nothing is kept in memory; a kernel built without
`MIPS` is the same to the byte as before. The tests and the profile are
always at full speed.

`test/prompt_test.py VM IMAGE.hex [CELL_BITS]` types Forth at the nucleus's
prompt on the reference VM and checks what comes back: definitions, wide
numbers, that the nucleus's compiler uses the variable instructions and
does not join across a loop's start, and tasks. Run it at each cell width
after changing `nuc.fs`.

`test/display_test.py VM IMAGE.hex [SEED [COUNT]]` checks the drawing
words: it draws a random sequence of fills, text, copies and scrolls, in
ports with random origins, clip rectangles and clip regions, on the
reference VM and in a model, and compares every pixel. The display is in
the VM's memory, so nothing has to show it. Run it after changing
`display.fs` or `region.fs`.

In `baremetal/`, `make demo` boots QEMU into the desktop with a disk,
keyboard and tablet. `make demo-test` runs it headless, sends key and
pointer events through QMP, and checks the screen: text is read back by
matching font cells, and the image viewer is compared with a reference
dither computed on the host.

`make demo-profile` says where the desktop's instructions go. It builds the
small kernel with `-DPROFILE`, which counts how often the instruction at
each address is executed, and `baremetal/deskprof.py` drives the desktop
through typing, scrolling, raising and dragging windows, the calculator,
the Words window and the editor, reading the counts out of the machine
after each. What a turn of the idle event loop costs is taken off, and the
rest is given by file, by word and by kind of instruction. At the end it
says how far up its part of each stack every task got.

`python3 util/survey.py build/nuc.hex build/desktop.counts` lays the whole
encoding out against that profile, which `demo-profile` leaves in
`build/desktop.counts`: for every source, operation and destination, how
many behaviours there are, how many have a name, how many the peephole can
produce, how many the image uses and how often they run; every behaviour
used, by how often; and every run of up to six instructions that one or two
would do the work of, by what that would save. The report is
`build/survey.txt`. Run it before deciding that anything about the
instruction set does or does not matter.

`python3 util/loops.py build/nuc.hex build/desktop.counts` lists the loops
that ran, by the cycles spent in them: turns, instructions and cycles a
turn, and each loop's instructions with how often each ran
(`build/loops.txt`). Most of the work is not in a loop's own instructions
but in the words it calls.

`python3 util/paths.py build/nuc.hex build/desktop.counts` charges each
word with the work of all it calls (`build/paths.txt`): a call that ran c
times is charged c times what the word called costs each time it is
entered. So a word's figure is what would be saved if it were never
called; then, for the words that cost most, where the cost is, by the word
it calls; and the words reached by `execute` of a vector, whose callers
cannot be seen. Read it before saying what a change will save, and before
choosing what to change. A count of how often a thing is done, times what
one is thought to cost, leaves out the loop that does them and whatever
is rare and dear (a fill under a region was a sixth dearer for the 3% of
its rows that needed a mask); and the profile by word shows a way of doing
something as a dozen words that each look like little, which is how fills
were half the display's work for as long as nobody added them up.

`python3 util/shorter.py build/nuc.hex build/desktop.counts` looks at one
turn of each loop that calls nothing, as a whole, for the fewest
instructions that do the same: with the loop's values where they are, and
with them kept in any other of the top cells of the two stacks
(`build/shorter.txt`). It searches every ALU instruction, and checks what
it finds with the model. A turn of more than about eight instructions is
too much for it to search whole; it says when it gave up, and then
shortens the turn a part at a time. It also tries each number the turn
pushes kept on the return stack instead, where it is an input of the ALU
(`0 >r begin r@ over ! ...`).

`python3 util/vars.py build/nuc.hex build/desktop.counts [file.fs ...]`
says what variables cost the code of the files named: how much of the
work is fetches and stores; each variable, with how often it is fetched
and stored and by how many words; and each word, with the variables it
uses (`build/vars.txt`). A variable is one instruction to fetch, as a
cell of a stack is, so moving a value to a stack does not by itself save
instructions, and if it takes juggling it costs them. What it saves is
the store, the cycle a fetch takes to read memory, and an instruction
wherever the value can be the ALU's input where it is (second on the
stack, or on the return stack). Putting the fill's place and length on
the stacks (`slab`, `fast-slab`) left the instructions of the drawing
benchmark as they were and took 1.6% off its cycles. The port (`clip`,
`ox`, `oy`, `pattern`, `'mode`) is state that many words read, and is
rightly variables; what one word stores for the next to fetch is not.

What matters more than the cost is that a word which keeps what it is
working on in a variable can be in use only once: two tasks, or something
that draws while something else is drawing, would each write over the
other's. `vars.py` counts the words that do. In `display.fs`:

- a word has the top of the data stack, the cell under it and the top of
  the return stack to hand, and what it works on is there. Where it needs
  one more it is made of smaller words that each need no more: a slab has
  the row of the pattern and the cell both on the return stack, the cell
  on top while the ends are done and the row while the cells between are
  stored (`fast-slab`, `row-out`, `move-row`); and a second place is the
  first plus how far apart they are
- where it needs a great deal more (`copy-bits`, `draw-text`: thirty
  values and the cells they gather), it is variables, an instruction each
  to reach, all in one run of memory from `copy-state` to `copy-end` and
  `text-state` to `text-end`. The word says it is busy while it runs
  (`copy-busy`, `text-busy`), and if it is run while it is busy it first
  sets that run of memory aside and afterwards puts it back, so it costs
  nothing unless it happens. `test/prompt_test.py` tells each it is
  running, and checks that it draws and leaves the variables as they were
- the memory to set it aside in is allotted at `here` (`borrow`), and
  given back by putting `dp` where it was: a word's locals. Memory above
  `here` that has not been allotted is nobody's to keep anything in. A record
  reached through a pointer, in place of the variables, cost the drawing
  benchmark 22% of its cycles, since the loops of text fetch from it at
  every row
- what is left is the port's own: the mask of the clip region for the row
  in hand (`rmask`, thirteen words), which goes with where a task's port
  is to live; and the opening of a font, which is done once

### Regions

A region's data says what code for a machine with registers would keep, so
`region.fs` keeps nothing: none of its words has a variable, and any
number of them can be in hand at once. (It had 45: limits, flags for
inside and outside, cursors, the operation, and arguments stored to be
fetched back.)

- a region is a count of the cells that follow, and then bands. A band is
  `n x0 .. x(n-1) end`: `n` crossings, and where the band ends, which is
  the row it goes down to plus `AS-END`. The end is further along than any
  crossing (anything from `END-MIN` up is an end), so it is also what ends
  the crossings, and nothing counts them to merge them; and the nearer of
  two bands' ends is the end of the band they make. `display.fs` makes
  the three numbers from the width of a cell, `END-MIN` half `AS-END` and
  `NEVER` twice it: 2^31, 2^32 and 2^33 with 64 bits, each a number one
  instruction pushes; 2^28, 2^29, 2^30 with 32; 2^12, 2^13, 2^14 with 16
- the first band has no crossings and ends at the region's top, and the
  last has none and ends at `NEVER`: every row is in a band, and a
  top or a bottom is nothing special. Nothing at all is that last band
  (`rgn-none`; never store a count of 0). A rectangle is eight cells, and
  nothing else is
- a band is an even number of cells and a region starts on an even cell
  (`new-rgn`), so a crossing in is at an even address and a crossing out
  at an odd one. A place among the crossings is inside the region if its
  address is odd; that is so of the two regions read and of the one
  written, and nothing keeps a flag. A region anywhere else would read as
  everything it is not
- a merge has two places to read on the data stack and the place to write
  on the return stack. The nearer crossing is written where the answer's
  next would go; each place that was at it moves on; and it stays written
  if the answer is not what it was. Whether the answer is inside is one
  instruction on the two places to read: `and` for what is in both, `or`
  for either, `invert and` for the first and not the second. The three
  merges are that instruction between two words of the cross-compiler
  (`MERGE[` `]MERGE`), compiled in line
- the end of a band is a crossing like any other: the place that was at
  the nearer end moves on to its region's next band, and the other is left
  at its end. So the sweep (`rgn-op`) has the band of each region on the
  data stack and the last band written on the return stack, and takes the
  next band of each from where the merge left its place (`on?`). It starts
  with a band of nothing; a band with the crossings of the last one is not
  written, and the last one is made to end where it would have
- only `display.fs` (`mfind`, `rmask`, `reclip`) reads the form besides
  `region.fs`. `scroll-rect` works in three regions next to each other,
  taken from `halloc` and given back, with one place on the return stack
  for the three; `scroll-update`, which whoever scrolled reads, is the one
  variable left
- coordinates are less than `END-MIN` from nothing either way: 2^31 with
  64 bits, 4,096 with 16. `test/sizes.sh` runs the region test and the
  drawing under regions with cells of 64, 32 and 16 bits

In the profile `region.fs` ran 1,617,696 instructions and 2,142,456 cycles
as it was, 29% of them fetches and stores of variables, and runs 1,166,113
and 1,280,433 now. The merges are half what `merge` and `inside` were; the
sweep is about what it was. It did not make the picture come sooner:
putting a row out of date cost about 27,000 instructions, and most of
that was painting. `scroll-rect` is about 470 instructions a scroll dearer,
0.3% of one, for asking its regions for the rectangle round them. The
stacks are no deeper in the desktop's task (18 cells and 30) and three
cells of the data stack deeper in the picture's (10 of its 32).

`test/region_test.py VM IMAGE.hex [SEED [COUNT]]` checks it: a set of
pixels has one form as a region, so random rectangles are made, combined,
moved, copied and boxed at the prompt of the reference VM, and every cell
of every region is compared with the one form of what it should hold. Run
with no machine it checks the idea alone: the merge and the sweep in
Python, with memory an array and inside taken from the address. Run it
after changing `region.fs`, on a VM built with `-DSTRICT`.

A loop that tests at the top goes round with a jump. Where the test is one
instruction that says whether two things are the same (`2dupxor while`,
which every loop over memory has), `repeat` in `cross.fs` compiles the
test the other way round and a conditional branch back after the body, so
a turn is one instruction shorter and the loop one longer (`turned`).

### The Peephole

`cross.fs` emits one instruction at a time and offers it to those just
before it. `forth/peephole.fs` holds the tables, generated by
`python3 util/insn_map.py --peephole forth/peephole.fs --verify` from a
symbolic model of `vm.c`; `--verify` runs every entry on the VM as well.

- pairs and triples that one instruction equals. A pair has a plain
  baseword's instruction on at least one side, so the result of a fusion
  fuses again with the instruction before it. Pairs of two fused
  instructions are left out: there are about 140,000
- threes that two instructions equal, where the three are plain basewords'
  instructions (the last may have a return folded in). With a fused
  instruction allowed as the first there are 450,000, so those are left out
- `exit` folds into the instruction before it, `+` into a literal, and an
  add of a constant into a literal or another add of the same shift
- `@` or `!` after the address of a variable becomes the one instruction
  that fetches or stores it. The nucleus's own compiler does this too
  (`var,` in `nuc.fs`), and never across a place a branch lands on

`forth/litrules.fs` is the peephole's rules that have a literal in them,
generated by `python3 util/litrules.py forth/litrules.fs --verify` from the
same model. There the code is tokens: an ALU instruction, a push of a
number (the push, the adds after it, and the invert that makes it negative),
an add of a number, and the fetch of a variable. A rule is a shape of up to
four tokens and the one being compiled, and what does the same, with its literal's value made from
those of the shape; the shapes are tried with every instruction the
peephole can leave in each place. `cross.fs` uses a rule when what it gives
is shorter, which depends on the value (`fuse-lit`), and first puts a push
in its shortest form: `16 -` is a push of 15, an invert and an add. A rule
may fetch the same cells in another order (`var @ 0 >` becomes `0 var @ <`):
a fetch changes nothing, so the fetches a sequence makes are compared as a
set, by the generator and by `insn_map.py --compare`. A shape may end at a
conditional branch, which asks only whether the top of the stack is zero
(`is_zero` in `insn_map.py`), so what is before it may be anything that
leaves the same answer: `<> if` is `xor if`. Regenerate it after
`peephole.fs`.

The peephole sees only instructions that are next to each other, and a
call hides what a word does. So `cross.fs` tries a small word in place of a
call to it: a word of up to four instructions, with no call or branch, that
returns only at its end and does not look at its return address. Its
instructions are compiled where the call would be, each offered to the
peephole. If the peephole joins any of them to what is before, or joins the
next instruction to them, they stay; if it does nothing with them they are
taken out and the word is called (`pend`, `settle`). A push of a number
that follows is part of what comes next, so the word waits past it. A word of one
instruction or none always stays, and in the kernel a word stays only if
what is left is no longer than the call. The build writes which calls were
compiled in place to `build/<name>.inline`, and a `PEEPHOLE=off` build
reads it and does the same, so build with the peephole on first.

The nucleus has a peephole of its own (`forth/join.fs`), so that what is
typed at the prompt is plain words too: `2dup xor`, `over +` and `dup @`
are each compiled to the one instruction. An instruction about to be
compiled is offered to the two before it and then to the one before, and
if there is a rule they become one, which is offered again; nothing is
joined across a place a branch lands on. A word of up to three
instructions with no call or branch, that leaves the return stack alone,
is compiled in place so that its instructions can be joined (`2dup` is
two), and `+` after a push makes it an add. The rules are
`forth/joins.fs`, generated with `fused.fs` by `python3 util/insn_map.py
--fused build/nuc.hex forth/fused.fs forth/joins.fs`: they are the
decompiler's table the other way round, so what `see` shows compiles, when
typed, to the instruction it was shown for, and every instruction a rule
gives is one the decompiler can show. Mostly that is one the image has.
But a rule joins two instructions or three, so an instruction shown as
four words is reached a step at a time, and where a step is not an
instruction of the image there is a rule for it all the same and the
table has it too: `r> @`, on the way to the `r> @ over !` that `move-row`
has, which was written by its fields. The generator says which entries
are steps, and which phrases, if any, are still not joined when typed;
`test/prompt_test.py` types that one. A rule is four
instructions laid down with `i,` after the peephole's code (nothing or the
instruction two before, the one before, the one being compiled, and what
they are together), in order, and found by halving. The kernel's ROM has
only a vector for this, `compile,` running `'compile`, which the build
sets to `pcompile,`; and `join-rules`, two cells that say where the rules
are and how many. It is in every size of machine: on code page 1 where
only the kernel is built, and with 64-bit cells on a page of its own,
page 10, after the desktop's, since page 1 is the display's. `insn_map.py`
takes the rules out of the code it reads, finding them through
`join-rules`. The rules are all on one page, since they are found by how
far along they are: `joins.fs` says how many instructions they are first
(`table-room` in `cross.fs`), and a page is started if this one has not
room, which the build with the peephole off needs. Build until `fused.fs`
and `joins.fs` stop changing.

After changing any of this, build with and without `PEEPHOLE=off` and run
`python3 util/insn_map.py --compare plain.hex optimised.hex`: every
straight-line run of the optimised image must behave as the plain one does.

### Machine Sizes

The cell width and address width are properties of the machine definition.
`vm_constants.h` has `CELL_BITS` (64, 32 or 16: the width of a stack cell,
of the ALU, and of `@` and `!`) and `ADDR_BITS` (memory is that power of two
cells: 22 by default with 64-bit cells, 17 with 32 and 16 with 16). `baseword_fs_gen` writes both into `basewords.fs` as
`cell-bits` and `addr-bits`, and `cross.fs` derives everything else from
them: the size `,` lays down, alignment, double literals, and where run-time
code pages go. `nuc.fs` reads them through words `cross.fs` provides:
`CELL`, `CELLB`, `BYTESHIFT`, `TOPBIT`, `HALFBITS`, `CODE-BASE`, `CODE-SIZE`, and the
flags `W64?`, `W32?`, `W32+?` for `[if]`.

`util/build-machine.sh CELL_BITS [ADDR_BITS]` builds the generator, the
nucleus and the reference VM for a size, into `build/`. The default
(checked-in `basewords.fs`, plain `make`) is 64-bit cells and 32MB.

`test/sizes.sh` builds the machine with cells of 64 bits, of 32 and of 16,
each with a display and each `STRICT`, and runs on each the tests that
type at the reference VM's prompt: the nucleus, the drawing words against
their model, regions a cell at a time, and the dither against a dither a
pixel at a time (`test/dither_test.py`, which makes images in memory with
`image-is`). Run it after changing anything that draws: nothing is cell-
size safe that it has not run. The first time the drawing was built at 32
and 16 bits it found text wrong at both, since a font said how long a row
of its strike is in cells of 64 bits; fonts now say it in runs of 64
pixels (`px-cells,`).

A machine with cells of 32 or 16 bits that has a display (`-DDISP_W=...`
and the rest, through `EXTRA` to `build-machine.sh`) has `graphics.fs`:
the display, regions, rectangles, the TIFF reader and the dither, which
are written for any cell, and no drivers or desktop. With 16-bit cells its
code is below instruction `0x8000`, where a header's 15 bits reach: page
1, and the first of the two pages that run-time definitions have. The
desktop and its drivers (`system.fs`) are built only with 64-bit cells,
and so are the AArch64 kernels, which is why nothing of the desktop is
tested at the other sizes yet. With a 16-bit cell memory is 65,536
cells, run-time code goes at instruction `0x6000`, and an execution token is
15 bits, since the top bit of its cell in a header is the immediate flag. A shift count is taken modulo 64, so a shift by
16 to 63 gives zero where cells are 16 or 32 bits, which `w!` and the
dictionary search rely on, but a shift by 64 shifts by nothing.

### Hardware (`rtl/`)

`rtl/hexaforth.v` is the CPU in Verilog, with the same semantics as `vm.c`
and a cell width set by its `W` parameter. A plain instruction takes one
cycle; a shift or a memory read takes two and a multiply three. As in the
J1a, the memory has one read port: it normally fetches the next instruction
while the current one executes, and an instruction that needs data borrows
it for a cycle. `rtl/hf_mem.v` is the memory: an array of cells, addressed
by cell, with one write enable. A cell wider than 16 bits holds several
instructions, and the read port is told which one is wanted. Ports `0xc1`
to `0xc8` reach devices and this machine has none, so they read zero.
`rtl/soc.v` joins CPU and memory for synthesis. The stacks below T, N and R
are memories read without a clock (LUT RAM on an FPGA).
`LAT=2` registers the memory's output: about 29% more cycles (a cycle lost
on every jump, call, return and taken branch, and on every memory read) for
a clock 55-70% faster on an ECP5 (61, 58 and 52 MHz at 16, 32 and 64 bits).
`KROM=1` (with `LAT=2`) also builds the first 4KB of the kernel as a ROM
in logic, read without a clock, so a jump, call or return into it costs no
extra cycle: about 15% fewer cycles for a clock about 5% slower and 2,400
more LUTs. The kernel's code fits in 4KB and nothing stores there: every
variable is above the kernel. Build the reference VM with `-DROM_BYTES=4096`
to check, or the small kernel with `STRICT=1`.

- `make -C rtl test W=64` (or 32, or 16) replays differential cases from
  `baremetal/difftest_dump.c`, built for that cell width, on the Verilog
  CPU under Verilator, and boots a nucleus built for that width on the CPU
  and on the whole system, comparing output with the reference VM
- `make -C rtl test-mem` checks the memory against an array of cells
- `make -C rtl bench` (and `LAT=2`) runs the drawing in `rtl/bench.txt` on
  the 64-bit CPU and says how many cycles it took. That is runtime: counts
  of instructions are not, since a memory read, a shift, a multiply and
  (with `LAT=2`) a jump each take more than a cycle. Measure with this
  before and after a change to the compiler or the peephole
- `make -C rtl synth W=32 AB=16` synthesises for an ECP5 FPGA with Yosys and
  nextpnr and reports size and clock speed; `AB` is the log2 of the memory's
  size in cells

## Cross-Compiling Programs

To create a program for hexaforth using the cross-compiler (cross.fs), follow this structure:

### Basic Template

```forth
\ Your program description

meta
    4 org        \ Set origin after basewords
target

header word1 : word1
    \ your code here
;

header word2 : word2
    \ your code here
;

\ REQUIRED: Define main and quit
header main : main
    \ your initialization/main code
    \ main should eventually call quit or halt
;

header quit : quit
    halt  \ or your main loop
;
```

### Key Requirements

1. **Vocabulary Switching**: Use `meta`/`target` to switch between host and target compilation
2. **Headers**: Each word needs `header name` before its definition for dictionary linking
3. **Entry Points**: Must define both `main` and `quit` - cross.fs expects these at addresses 0 and 1
4. **Number Literals**: Use prefixes in target vocabulary:
   - `d# 123` for decimal numbers
   - `h# FF` for hex numbers
   - Plain numbers like `5` won't work in target vocabulary

### Compilation

```bash
cd forth
gforth ./cross.fs ./basewords.fs ./yourprogram.fs
```

This creates:
- `../build/yourprogram.hex` - The compiled hex file
- `../build/yourprogram.lst` - Symbol listing (addresses include 0x4000 offset)

### Example

```forth
\ Simple test program

meta
    4 org
target

header double : double d# 2 * ;
header test : test d# 21 double ;
header main : main test drop halt ;
header quit : quit halt ;
```

### Debugging

Use the disassembler to inspect compiled output:
```bash
./disasm_hex build/yourprogram.hex
```

The first few addresses (0x0000-0x0002) contain the bootloader that calls main and quit.

## Key Architectural Insight

The core innovation is that by adding just two 2-bit multiplexer fields to J1's instruction format, hexaforth transforms a traditional stack machine into a dataflow machine. This enables:

1. **Direct memory-to-memory operations** without stack intermediaries
2. **Single-cycle complex operations** that would take 3-5 cycles traditionally
3. **Flexible data routing** between any components (memory, ALU, I/O, stacks)

When extending the system, think in terms of dataflow paths rather than stack operations. The stack is just another node in the dataflow graph, not the center of computation.

## Adding New Instructions

To add a new ALU operation:

1. Add the operation to `vm_opcodes.h` in the `INS_FIELDS` array:
   ```c
   {"N%T", FIELD, {{.alu.alu_op = YOUR_NEW_OP}}},
   ```

2. Implement the operation in `vm.c` in the ALU switch statement

3. Rebuild to regenerate `basewords.fs`:
   ```bash
   make clean && make
   ```

The build system will automatically:
- Generate the Forth assembler word
- Update the cross-compiler
- Ensure the VM can execute it

No manual synchronization needed - the single source of truth in `vm_opcodes.h` propagates everywhere.