# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

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
- `@@` (double indirection): `[T]->IN [IN] ->T` - chains two memory loads
- `swap>r`: Routes data through swap operation directly to return stack

#### Literal Encoding (64-bit Optimized)
The literal system is specifically designed for efficient 64-bit value encoding:

**Structure**: Each 16-bit literal instruction contains:
- 12-bit immediate value (0-4095)
- 2-bit shift selector (0, 12, 24, or 36 bit shifts)
- 1-bit add flag (push new value vs. add to existing)

**Key Features**:
- Can encode any 48-bit value in 1-4 instructions
- Zero-segment optimization (e.g., 32768 = `8<<12` in one instruction)
- Negative numbers encoded as positive + invert
- `lit_add` flag enables inline operations (e.g., `1+` as single instruction)

**Examples**:
- `20`: One instruction (direct 12-bit value)
- `-24923`: Two instructions (347 + 6<<12, then invert)
- `0x123456789ABC`: Four instructions (12 bits at a time)

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
- The VM implements both 8-bit character operations and full 64-bit operations
- Memory is word-addressed for branches/calls but byte-addressed for data access
- The "dumb VM" design eliminates all interpreter overhead for maximum performance
- The project explores how dataflow principles can optimize stack machine performance
- All Forth complexity lives in the toolchain, not the runtime

### Memory Access Operations

Hexaforth provides multiple memory access widths:

- **`@` and `!`**: 64-bit memory operations (8 bytes)
- **`w@` and `w!`**: 16-bit memory operations (2 bytes) - essential for dictionary access
- **`c@` and `c!`**: 8-bit memory operations (1 byte)

The dictionary structure inherited from J1/Swapforth uses 16-bit values, so proper `w@` and `w!` implementations are critical for correct operation. These preserve other bytes in the 64-bit word:
- `w@`: Reads 64 bits then masks to lower 16 bits using `lo16`
- `w!`: Reads existing 64-bit value, masks out lower 16 bits, ORs in new value
- `2w@`: Reads two consecutive 16-bit values (e.g., for source buffer)
- `2w!`: Writes two consecutive 16-bit values while preserving other bytes

### Aligned Access

The nucleus only ever fetches and stores whole cells at cell-aligned
addresses.  `c@`, `c!`, `uw@` and `w!` in `nuc.fs` are Forth words that fetch
the cell an address is in and shift the field out of it or into it
(`field@`, `field!`); `l@` and `l!` in `machine.fs` do the same for 32 bits.
A field must not cross a cell boundary.  `,` aligns `here` first, and code
is always fetched at its own word address.

Dictionary headers are whole cells: a link cell, an xt cell (top bit set for
an immediate word), then the name as a count byte and lower-case characters
padded with zeros to a cell boundary, so lookup compares a cell at a time.

To check, build the reference VM with `-DSTRICT_ALIGN` or the small kernel
with `make -C baremetal STRICT=1`; either stops at the first unaligned access.
The byte and 16-bit basewords in `vm_opcodes.h` (`c@`, `w@`, ...) still do
unaligned fetches and are kept for the instruction tests.

### Code Paging

The program counter is a flat word address and code is divided into pages of
4096 words (8KB). Page 0 is the kernel. The 13-bit target in a jump, call or
0branch means:

- bit 12 clear: an absolute address in the kernel, reachable from any page
- bit 12 set: an offset within the page the branch itself is in

Return addresses and execution tokens are full program counters, so `exit`
and `execute` cross pages with no extra state. Memory layout:

- `0x00000-0x01fff`: kernel code (cross-compiled)
- `0x04000-0x0ffff`: dictionary and data
- `0x10000-0x1ffff`: eight code pages for definitions made at run time

In `nuc.fs`, `compile,` emits a one-instruction call for a kernel or
same-page target and `literal xt` + `call execute` for another page.
`newpage?` starts a fresh page when under 1KB is left, and `code,` throws -8
if a definition runs off the end of its page. Execution tokens are stored in
16 bits in dictionary headers, which is what limits code to 64K words.

### Devices, Blocks and Graphics

`forth/system.fs` (included at the end of `nuc.fs`) is cross-compiled into
code page 1 (`0x2000`). The kernel never calls into it; its words are
reached by name or by execution token.

- `machine.fs`: exact-width device register access through IO ports
  `0xc0`-`0xc8`, `halloc` for memory above the dictionary, byte-order helpers
- `blocks.fs`: block device interface (`'blk-read`, `'blk-write`), `block`
  `update` `flush` `load`, and `disk-read` for byte ranges
- `display.fs`: 1-bit display interface (`'plot`, `fb-w`, `fb-h`)
- `virtio-blk.fs`, `ramfb.fs`: the QEMU `virt` drivers behind those interfaces
- `tiff.fs`, `atkinson.fs`, `mandrill.fs`: uncompressed TIFF reader, Atkinson
  dithering, and the demo

The desktop is cross-compiled into code pages 8 and 9 (`0x10000` up):

- `font.fs`, `keymap.fs`: generated by `util/mkfont.py` (8x16 Unifont
  glyphs; Linux key codes to ASCII)
- `virtio-input.fs`: keyboard and tablet events, delivered to `'event`
- `gfx.fs`, `wm.fs`: clipped rectangles and text; windows, pointer, event loop
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

In `baremetal/`, `make demo` boots QEMU into the desktop with a disk,
keyboard and tablet. `make demo-test` runs it headless, sends key and
pointer events through QMP, and checks the screen: text is read back by
matching font cells, and the image viewer is compared with a reference
dither computed on the host.

### Machine Sizes

The cell width and address width are properties of the machine definition.
`vm_constants.h` has `CELL_BITS` (64, 32 or 16: the width of a stack cell,
of the ALU, and of `@` and `!`) and `ADDR_BITS` (the address space in bytes,
as a power of two). `baseword_fs_gen` writes both into `basewords.fs` as
`cell-bits` and `addr-bits`, and `cross.fs` derives everything else from
them: the size `,` lays down, alignment, double literals, and where run-time
code pages go. `nuc.fs` reads them through words `cross.fs` provides:
`CELL`, `CELLSHIFT`, `TOPBIT`, `HALFBITS`, `CODE-BASE`, `CODE-SIZE`, and the
flags `W64?`, `W32?`, `W32+?` for `[if]`.

`util/build-machine.sh CELL_BITS [ADDR_BITS]` builds the generator, the
nucleus and the reference VM for a size, into `build/`. The default
(checked-in `basewords.fs`, plain `make`) is 64-bit cells and 128KB.

At 32 and 16 bits only the kernel is built: `system.fs` (drivers and the
desktop) is written for 64-bit cells. With a 16-bit cell the address space
is 64KB and run-time code goes at `0xc000`. A shift by the cell width or
more gives zero at every size, which `w!` and the dictionary search rely on.

### Hardware (`rtl/`)

`rtl/hexaforth.v` is the CPU in Verilog, with the same semantics as `vm.c`
and a cell width set by its `W` parameter. A plain instruction takes one
cycle; a shift or a memory read takes two and a multiply three. As in the
J1a, the memory has one read port: it normally fetches the next instruction
while the current one executes, and an instruction that needs data borrows
it for a cycle. `rtl/hf_mem.v` is the memory, one byte lane per byte of a
cell so that an unaligned cell access takes one cycle; with `ALIGNED=1` it
takes only aligned cell accesses, which is all the nucleus makes.
`rtl/soc.v` joins them for synthesis. The stacks below T, N and R are
memories read without a clock (LUT RAM on an FPGA); `STACKRAM=0` makes them
shift registers instead. `PLANES=1` builds the memory from one-bit-wide
block RAMs, which was tried for clock speed and gains little.
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
- `make -C rtl test-mem` checks the memory against a byte-array model
- `make -C rtl synth W=32 A=24` synthesises for an ECP5 FPGA with Yosys and
  nextpnr and reports size and clock speed; `A` is the address width

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