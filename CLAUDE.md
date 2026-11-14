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
- Do not propose a new ALU operation or any other change to the machine
  for speed. The operation field is full, the ALU has two inputs, and the
  design is that the fields compose. First show what the existing fields
  give, and whether the work is needed at all
- Think of a word as data moving: from where, through what, to where, a
  cell at a time. Bytes, opcodes and per-element loops are habits from
  other machines

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
task that runs past its part of a stack. The kernel's code is 3,982 bytes
with 64-bit cells, 3,986 with 32 and 3,950 with 16, against the 4,096 the
kernel ROM holds.

On the desktop the Forth window's interpreter is a task with the upper half
of each stack (`repl` in `term.fs`), and the event loop pauses each time
round. The desktop takes the keys and hands over a line at a time, so it
goes on running while a line does, as long as the line pauses.

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
- bytes `0x0a000-0x0ffff`: dictionary headers and labels, and after them
  the headers made at run time. The nucleus keeps two pointers, `dp` for
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
code page 1 (`0x2000`) and code pages 8 and 9 (`0x10000` up). The kernel
never calls into it; its words are reached by name or by execution token.
Each page has room for a build without the peephole, which is about 8%
bigger: `PEEPHOLE=off gforth cross.fs basewords.fs nuc.fs` must still fit,
since `insn_map.py --compare` checks the optimised image against it.

Page 1 is what the rest is built on:

- `machine.fs`: exact-width device register access through IO ports
  `0xc0`-`0xc8`, `halloc` for memory above the dictionary, byte-order helpers
- `blocks.fs`: block device interface (`'blk-read`, `'blk-write`), `block`
  `update` `flush` `load`, and `disk-read` for byte ranges
- `display.fs`: the display, a bitmap in memory of the size, place, bit
  order, polarity and row layout that `vm_constants.h` gives (see
  `MODEL.md`), and all drawing on it; nothing outside this file knows the
  layout. A cell is as many pixels across as it has bits (`PXCELL`,
  `PXSHIFT`, `PXMASK`). Drawing is modelled on QuickDraw:
  - the *port* is the state drawing depends on: origin (`ox`, `oy`), clip
    rectangle, clip region, `pattern`, `'mode`, `font`, and `inverse`
    `bold` `italic`. Everything that draws takes coordinates in the port.
    `port!` changes port; a port not in use is a record of thirteen cells
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
    the band is stored when it is full
- `region.fs`: regions, as rows of where a shape goes in and out, with
  `rect-rgn`, `rgn-and`, `rgn-or`, `rgn-diff`, `rgn-offset`, `rgn-box`.
  `rgn-clip!` clips a port to one: drawing makes a mask of the region for
  the row it is on. `scroll-rect` moves what is in a rectangle: a pixel
  moves only if where it comes from and where it goes can both be seen, and
  the places left with nothing are made white and returned as the region
  `scroll-update`, to be drawn again (`inval-rgn`)

Pages 8 and 9 are the drivers and the desktop:

- `virtio.fs`, `virtio-blk.fs`: the QEMU `virt` block driver
- `tiff.fs`, `atkinson.fs`, `mandrill.fs`: uncompressed TIFF reader, Atkinson
  dithering, and the demo. Neither touches memory a pixel at a time: `take`
  shifts a pixel out of a cell fetched once, the errors for the rows below
  are kept in 16-bit lanes (as many to a cell as fit) and added a cell at a
  time, and the pixels are gathered into a cell of a bitmap in the
  display's form. `atkinson ( bitmap -- )` makes the bitmap; `copy-bits`
  shows it
- `font.fs`, `keymap.fs`: generated by `util/mkfont.py` (the two fonts;
  Linux key codes to ASCII); `font.fs` is on page 1
- `virtio-input.fs`: keyboard and tablet events, delivered to `'event`
- `gfx.fs`, `wm.fs`: rectangles in plain white and black; windows,
  pointer, event loop. Nothing draws when something changes: it is added
  to the region that is out of date (`inval-rect`, `win-inval`), and the
  event loop paints that (`refresh`), each window only where it can be
  seen, in its own port
- `term.fs`, `edit.fs`, `view.fs`, `calc.fs`: the Forth window, block
  editor, image viewer and RPN calculator
- `see.fs`: a decompiler (`see name`), and the Words window, which lists
  the dictionary and shows a definition with each word in it as a link
- `fused.fs`: generated by `python3 util/insn_map.py --fused build/nuc.hex
  forth/fused.fs` from a built image: each fused instruction in it as the
  words it stands for, which the decompiler shows in bold. Regenerate and
  rebuild after changing code; a missing entry is shown as a number in
  brackets
- `desk.fs`: `desktop`, and `cold`, which the kernel runs at boot

On a 64-bit machine `cross.fs` also keeps the name of every definition that
has no header, in a second chain headed by `labels` and laid out as the
dictionary is. The prompt does not search it; the decompiler does, so a
call to a headerless word and a headerless variable are shown by name. A
label for code holds its execution token, and one for `create`d data holds
the data's address with the top bit set. `see` accepts a label's name too.

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
down, and how much of that data is dictionary headers and labels.

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
rest is given by file, by word and by kind of instruction.

`python3 util/survey.py build/nuc.hex build/desktop.counts` lays the whole
encoding out against that profile, which `demo-profile` leaves in
`build/desktop.counts`: for every source, operation and destination, how
many behaviours there are, how many have a name, how many the peephole can
produce, how many the image uses and how often they run; every behaviour
used, by how often; and every run of up to six instructions that one or two
would do the work of, by what that would save. The report is
`build/survey.txt`. Run it before deciding that anything about the
instruction set does or does not matter.

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

At 32 and 16 bits only the kernel is built: `system.fs` (drivers and the
desktop) is written for 64-bit cells. With a 16-bit cell memory is 65,536
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