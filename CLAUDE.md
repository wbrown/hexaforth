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
- Memory is word-addressed (not byte-addressed)
- The "dumb VM" design eliminates all interpreter overhead for maximum performance
- The project explores how dataflow principles can optimize stack machine performance
- All Forth complexity lives in the toolchain, not the runtime

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