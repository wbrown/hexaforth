# hexaforth

A stack machine whose instructions are not encoded, a Forth operating system
with windows, a pointer and menus that runs on it, and the Verilog to build
it from.

The CPU with cells of 16 bits is fewer gates than a Z80.  At 1.2 MHz it runs
about a million instructions a second, and at that speed the desktop is
pleasant to use.

## What it is here to show

- **How much a small machine can do.**  What about 7,500 gates give, at the
  clock a machine of 1979 could have had
- **What could have been.**  The desktops of the 1980s that ran at one to
  five megahertz were admired and were slow.  This is one that is not, on a
  machine that could have been built before any of them
- **A Forth operating system with a desktop.**  Overlapping windows, regions,
  a menu, a pointer, proportional text, and programs: the Forth prompt in a
  window, a block editor, a picture viewer that reads a TIFF and dithers it,
  a calculator, a paint program, a decompiler
- **The instruction set.**  An instruction is a control word: where the ALU's
  input comes from, the operation, where the result goes, how each stack
  moves.  There are no opcodes
- **How little code it takes.**  The kernel is a 4KB ROM with room left.
  The paint program is a little over 2KB

It is meant to be built: as TTL, on an FPGA, or as a tile of silicon.

## The machine

Two stacks, a memory of cells, and ports for devices.  A cell is 16, 32 or 64
bits and an instruction is always 16.  `MODEL.md` says exactly what the
machine is.

An instruction of the ALU is seven fields, and every setting of them is an
instruction the machine carries out:

    15   14 13   12    11 10    9 8      7 6 5 4    3 2      1 0
     0    1  1   ret   input    output   operation  d-stack  r-stack

- **input**: the top of the stack, the cell under it, the cell of memory the
  top points at, or the top of the return stack
- **operation**: one of sixteen.  Pass the input through, two that also
  write the cell under the top, add, and, or, xor, multiply, complement,
  three comparisons, two shifts, fetch, and read a port
- **output**: the top of the stack, the top of the return stack, the cell of
  memory the top points at, or a port
- **the stacks**: each moves up one, down one or two, or stays
- **ret**: return as well

So fetching is a choice of input and storing is a choice of output, and an
operation can be done on memory where it lies.  `+!` is one instruction:
memory in, add, memory out.  So is OR-ing a mask into a cell of the display,
which is what drawing a glyph is made of.  There is no subtract and no
decrement: a complement and an add do it, and the value complemented can come
from any of the four inputs.

The other instructions are a jump, a conditional jump, a call, and a literal
of twelve bits that is pushed or added to the top of the stack, or that
fetches or stores one of 4,096 variables.

A word of Forth is a name for one of these instructions or for a run of
them.  The compiler joins a run into one instruction wherever one does the
same work, and it finds that out by asking the machine.

## What it comes to

These are what the tree's own targets print, and each is named so that it can
be asked again.  They are of the tree as it was when this was written.

| | | target |
|---|---|---|
| The CPU, cells of 16 bits | 7,458 gates | `gates` |
| The same without its multiplier | 6,449 gates | `gates` |
| A Z80 (tv80), counted the same way | 9,443 gates | `gates` |
| A 68000 (fx68k), counted the same way | 22,948 gates | `gates` |
| The CPU, cells of 32 and of 64 bits | 15,650 and 39,964 gates | `gates` |
| Clocks to an instruction, drawing | 1.06 to 1.36 | `bench-draw` |
| The kernel's code, in a ROM of 4,096 bytes | 3,332 bytes at 16 bits, 3,382 at 64 | `examine` |
| The whole system: kernel, desktop, programs | 844 words, 27,372 bytes of code | `examine` |
| The window manager | 2,350 bytes | `examine` |
| All drawing: fills, copying, text | 4,896 bytes | `examine` |
| The paint program | 2,334 bytes | `examine` |

A gate is a NAND, a NOT or a flip-flop as Yosys counts them, with memories
left out, and the Z80 and the 68000 are other people's Verilog put through
the same count.  The clocks are of the Verilog CPU with cells of 16 bits and
memory that answers in one cycle.  The system's sizes are of the image with
cells of 64 bits.

## What is in the kernel

The kernel is page 0: the part that would be in ROM, and all that is needed
to type Forth at the machine and have it compiled.

- **The stack and arithmetic words** of standard Forth, with `min`, `max`,
  `abs` and the comparisons
- **Double-cell arithmetic**: `d+ d- dnegate dabs d= d< du< d0= d0< d2* d2/
  s>d d>s 2@ 2!`, and `um* * um/mod u/mod`
- **Memory**: `@ ! +! or! and! xor! c@ c! fill cmove cmove>`, a cell at a
  time `cells-move cells-fill`, and fields of a cell `field@ field!`
- **The console**: `key key? emit cr space type . accept`
- **The interpreter**: `source parse parse-name >number refill evaluate
  quit`, in any `base`
- **The compiler**: `: ; :noname ] immediate create does> variable constant
  , c, s, compile, literal postpone recurse`, `if else then`, `begin while
  repeat until again`, `do ?do loop +loop leave`, and a hook for the peephole
- **The dictionary**: `sfind ' words name> here allot align`
- **Errors**: `catch throw`
- **Tasks**: `spawn pause`
- **Blocks**: `blk`, and the rest of the block words in the system

## Seeing it

It needs CMake and either Homebrew or apt.  Everything else is asked for
when CMake is configured.

    cmake -S . -B build/cmake
    cmake --build build/cmake --target demo      # the desktop, on QEMU
    cmake --build build/cmake --target check     # every test
    cmake --build build/cmake --target bench     # the benchmarks, on the Verilog CPU
    cmake --build build/cmake --target gates     # the gates

`cmake -DMIPS=1 build/cmake` before `demo` holds the machine to a million
instructions a second, which is the speed it is meant to be judged at.

## What there is

One machine, made three times, each tested against the first:

- `vm.c`, the reference: a virtual machine in C
- `baremetal/`, two kernels in AArch64 assembler that run on QEMU, where the
  desktop is shown
- `rtl/`, the CPU in Verilog, which runs the system in simulation and is
  synthesised for an FPGA

The system is Forth (`forth/`), built by a cross-compiler that is itself
Forth.  The tests, the tools and the benchmarks are C.

None of it has been built as hardware yet.

## Where it came from

It began in 2020 as an experiment: whether a virtual machine whose
instructions are unencoded, in the manner of the Novix NC4016 and the
[J1](https://excamera.com/files/j1.pdf), would be faster than one that
decodes bytecode.  The instruction came from the J1's, with its three fixed
flags for where a result goes replaced by two fields, one choosing the input
and one the output.  What those two fields made possible is what the project
turned into.

## Reading on

- `MODEL.md`: what the machine is, which all three implementations keep to
- `CLAUDE.md`: how to build, test and measure, where everything is, and the
  rules of working here
- `UNDERSTANDING.md`: an account of the design from 2025.  Its argument for
  the two fields holds; much of the rest is of the machine as it was
- the commits: each says what was changed, why, and what it cost or saved
