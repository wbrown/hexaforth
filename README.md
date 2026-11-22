# hexaforth

A stack machine whose instructions are fields that drive the hardware
directly; a Forth operating system for it, with windows, a pointer and
menus; and the Verilog to build it.

The CPU with 16-bit cells is 7,458 gates, fewer than a Z80 counted the same
way (with the memory for its two stacks left out). When drawing, it takes
1.06 to 1.36 clocks per instruction, so at 1.2 MHz it runs 0.88 to 1.13
million instructions a second.

1.2 MHz is the lowest clock it is held to, to show how little it needs, not
the most it can do. In a single instruction it takes its input from a
stack, the return stack or memory, does an operation, sends the result to a
stack or to memory, and moves both stacks. That is one clock, plus one for a
memory read or a shift. A 68000 instruction takes at least four clocks, and
often twelve or more.

The same step of the same dither shows the difference. A pixel's value is
compared with a threshold, and an eighth of the error is kept to pass on. On
a 68000, as `bench/m68k/atk.s` does it after MacPaint, that is four
instructions and a branch, about 30 clocks:

    cmp.w   d7,d2           ;  4   is it more than 127?
    bgt.s   ItsWhite        ; 10
    sub.w   d4,d2           ;  4   the error: how far short of white it is
    asr.w   #3,d2           ; 12   an eighth of it

With 16-bit cells hexaforth does it with no branch, keeping whether the
pixel is black inside the number it passes on (`forth/atkinson.fs`): nine
instructions and 10 clocks.

    dup 896 <  h# 80ff and + 1+  3 rshift

So dithering the Mandrill in colour takes 21.5 million clocks with 64-bit
cells, reading the picture from disk, where a 68000 takes 63.2 million with
the picture already in memory. At the Mac 128K's 8 MHz that is 2.7 seconds
against 7.9. The design is meant to run at 10 to 14 MHz built from TTL; on
an FPGA the 64-bit system places and routes at 30 MHz, where the same
picture takes 0.7 seconds.

![The desktop: the Forth window, the editor, the Words window, the calculator and the picture, with HexaPaint in front](doc/desktop.png)

## What it is meant to show

- How much a 7,458-gate machine can do at 1.2 MHz: fewer gates than the
  1976 Z80, at a clock speed microcomputers of 1979 ran at
- How much more it does per clock than the processors of its time, so that
  at their clock speeds it is the faster machine
- What could have been: a desktop on a machine of that size and speed
- A Forth operating system with a desktop: overlapping windows, regions, a
  menu, a pointer, proportional text, and programs. There is a Forth prompt
  in a window, a block editor, a viewer that reads a TIFF and dithers it, a
  calculator, a paint program and a decompiler
- The instruction set. An instruction is a control word: where the ALU's
  input comes from, the operation, where the result goes, and how each
  stack moves
- How little code it takes. The kernel is 3,564 bytes, in a 4,096-byte ROM.
  The paint program is 2,250 bytes. The whole machine, written in AArch64
  assembler, is 880 bytes

It is meant to be built from TTL, on an FPGA, or through TinyTapeout.

## The machine

Two stacks, a memory of cells, and ports for devices. A cell is 16, 32 or 64
bits, and an instruction is always 16 bits. `MODEL.md` describes the machine
exactly, and `UNDERSTANDING.md` explains why it is designed this way.

An ALU instruction is three bits that mark it as one, followed by six
fields. Every combination of field values is an instruction the machine
will carry out:

    15   14 13   12    11 10    9 8      7 6 5 4     3 2      1 0
     0    1  1   ret   input    output   operation   d-stack  r-stack

- **input**: the top of the stack, the cell under it, the memory cell the
  top points to, or the top of the return stack
- **operation**: one of sixteen. Pass the input through, two that also write
  the cell under the top, add, and, or, xor, multiply, complement, three
  comparisons, two shifts, fetch, and read a port
- **output**: the top of the stack, the top of the return stack, the memory
  cell the top points to, or a port
- **the stacks**: each moves up one, down one or two, or stays where it is
- **ret**: also return

Fetching is a choice of input and storing is a choice of output, so an
operation can be done on memory in place. `+!` is one instruction: read
memory, add, write memory. So is OR-ing a mask into a cell of the display,
which is what drawing a glyph is made of. Subtraction is a complement, an
add of one and an add, three instructions, and the value being complemented
can come from any of the four inputs.

The other instructions are a jump, a conditional jump, a call, and a 12-bit
literal that is either pushed or added to the top of the stack, or that
fetches or stores one of 4,096 variables.

A Forth word is a name for one of these instructions or for a sequence of
them. Leaving out the ones that read or write a port, the fields give 5,344
instructions that behave differently; many combinations do the same thing.
Fifty-six of those have names, and the system uses 395. The rest of the
ones it uses come from the compiler, which replaces a sequence with a single
instruction wherever one does the same work, and finds out which by asking
the machine. (The target `profile` counts them, for the 64-bit image.)
`MODEL.md` describes every field.

## The numbers

Each figure below is printed by one of the tree's own targets, named so it
can be run again. They were current when this was written.

**The CPU** (`gates`). A gate is a NAND, a NOT or a flip-flop as Yosys counts
them, with memories left out. For hexaforth that leaves out its two stacks,
256 cells each; the Z80 has no memory to leave out, so its registers are
counted. The Z80 and 68000 figures come from other people's Verilog put
through the same count.

| | gates |
|---|---|
| hexaforth, 16-bit cells | 7,458 |
| the same without its multiplier | 6,449 |
| a Z80 (tv80) | 9,443 |
| a 68000 (fx68k) | 22,948 |
| hexaforth, 32-bit cells | 15,650 |
| hexaforth, 64-bit cells | 39,964 |

**Speed** (`bench-draw`). On the 16-bit Verilog CPU, with memory that
answers in one cycle, an instruction takes 1.36 clocks on average drawing
text, 1.16 scrolling and 1.06 filling. An instruction is one clock, plus one
for a memory read or a shift, and two more for a multiply.

**Against a 68000** (`bench-dither`, `bench-m68k`). The Mandrill, 512 by
512, read from disk a block at a time as the system reads it, and dithered
on the Verilog CPU with memory that answers in one cycle; and the same dither
on a 68000, in code written for it, with the picture already in memory.
Every bitmap is checked against a reference dither, done a pixel at a time,
before its figure is printed. In millions of clocks:

| | 68000 | 64-bit cells | 32-bit | 16-bit |
|---|---|---|---|---|
| colour, full size | 63.2 | 21.5 | 31.5 | 30.5 * |
| colour, half size | 40.3 | 11.7 | 17.4 | 17.4 |
| grayscale, full size | 31.7 | 14.8 | 21.2 | 21.5 * |
| grayscale, half size | 13.3 | 4.4 | 6.7 | 7.6 |

\* Run on a smaller system, `forth/picture.fs`: the windowing system, the
TIFF reader and the dither, without the other applications. The 16-bit
machine has 128KB, and with the full desktop loaded the 32KB bitmap does
not fit in what is left. (The desktop's viewer does not need it to: it
makes its bitmap a block at a time and keeps it on the disk.)

**On an FPGA** (`synth`). The 64-bit system, placed and routed for an 85k
ECP5: 14,989 LUTs and 986 flip-flops, at 30.21 MHz.

**The system** (`examine-16`), on the 16-bit machine: 65,536 cells, which is
128KB, and a 512 by 342 display.

| | words | bytes of code |
|---|---|---|
| The kernel, in a 4,096-byte ROM | 270 | 3,564 |
| All drawing: fills, copying, text | 156 | 5,276 |
| Regions | 37 | 1,418 |
| The window manager | 116 | 3,548 |
| The TIFF reader and the dither | 121 | 3,946 |
| Text kept as lines, which two windows scroll | 44 | 1,158 |
| The Forth window | 22 | 696 |
| The block editor | 9 | 394 |
| The picture viewer, and its picture kept as gray | 158 | 4,302 |
| The calculator | 17 | 638 |
| The paint program | 51 | 2,250 |
| The decompiler and the Words window | 133 | 4,848 |
| What the disks have, by label, and the boot's console | 47 | 1,574 |
| The compiler's peephole, for what is typed | 51 | 1,770 |
| Everything | 1,561 | 40,440 |

The dictionary holds 641 names in 5,994 bytes, at five bits per character.

**The machine on another processor** (`size`). `baremetal/hexaforth.S` is
the whole machine in AArch64 assembler, with a console. It is 880 bytes:

| | bytes |
|---|---|
| Starting up | 60 |
| Fetching an instruction and telling what kind it is | 24 |
| Jumps, conditional jumps and calls | 64 |
| Literals and variables | 80 |
| The ALU: choosing its input | 52 |
| The sixteen operations, two instructions each | 128 |
| Moving the stacks, and sending the result where it goes | 116 |
| The ports: the console, the stack depths, the host's memory | 344 |
| Halting | 12 |

The code that carries out every instruction is 464 bytes, or 116 AArch64
instructions.

## What is in the kernel

The kernel is the first 4KB of page 0. It is the part that sits in ROM, and
it is everything needed to type Forth at the machine and have it compiled.
It has ~270 words, and these have names you can type:

- **The stacks**: `dup drop swap over nip tuck rot -rot ?dup 2dup 2drop 2swap
  2over 3dup depth adepth`, and `>r r> r@ rdrop i j unloop`
- **Arithmetic**: `+ - * 1+ 1- 2* 2/ negate abs min max um* um/mod u/mod`
- **Bits**: `and or xor invert lshift rshift`
- **Comparison**: `= <> < > u< u> 0= 0<> 0< 0> true false`
- **Double-cell numbers**: `d+ dnegate s>d d2* 2@ 2!`
- **Memory**: `@ ! +! or! and! xor! *! not! c@ c! uw@ w! field@ field! >c
  c> hi32 lo32 hi16 lo16 count cell+ cells aligned align cmove cells-move
  cells-fill /string bounds i@ i!`
- **The console and the ports**: `key key? emit uart-emit 'emit 'console cr
  space bl type . .x .x2 hex1 accept io@ io!`
- **The interpreter**: `source source-id sourceid >in parse parse-name
  isspace? isnotspace? isnotdelim >number base decimal hex state interpret1
  numbered refill evaluate quit execute \ (`
- **The compiler**: `: :noname ] [ immediate create does> variable constant
  , c, w, s, compile, literal 2literal postpone recurse exit ' inline:
  join-rules`, and `if else then ahead begin again until while repeat do ?do
  loop +loop leave`. `;` is the peephole's, which is laid straight after
  the kernel, and so is `[']` on a machine with a display
- **The dictionary**: `forth sfind >key samename? name> words here allot dp cp`
- **Errors**: `catch throw -throw`
- **Tasks**: `up sbase spawn pause`
- **Blocks**: `blk 'blk-read 'blk-write`

Twelve more words of the nucleus are in the other half of the kernel's
page and not in the ROM, since nothing needs them before the system is in
memory: `2rot`, `fill`, `cmove>`, and the double-cell `dabs d>s d= d< du<
d- d0< d0= d2/`. That left room in the ROM for the driver of the disk a
machine starts from, which is a file of its own for each kind of machine.

## Running it

You need CMake and either Homebrew or apt. Configuring installs the rest: it
runs `brew install`, or `apt-get install` as root, for whichever of the
twenty programs and libraries it needs are missing, QEMU, Verilator, Yosys
and LLVM among them. With `-DINSTALL_NEEDED=OFF` it only checks and reports
what is missing. `cmake/needs.cmake` lists them all.

    cmake -S . -B build/cmake
    cmake --build build/cmake --target demo      # the desktop, on QEMU
    cmake --build build/cmake --target demo-boot # the same, from its ROM and a disk: it compiles its desktop
    cmake --build build/cmake --target quick     # the tests on the reference VM alone; with no target, this
    cmake --build build/cmake --target check     # every test
    cmake --build build/cmake --target bench     # the benchmarks, on the Verilog CPU
    cmake --build build/cmake --target gates     # the gate counts

`demo` runs the machine the project is meant to show: a million
instructions a second, a disk as fast as an 8-inch floppy, and each pixel
drawn at twice its size. These settings, which also apply to `run`,
`demo-test` and `demo-time`, are given to CMake when it is configured, and
stay until they are changed:

| | | default |
|---|---|---|
| `MIPS` | millions of instructions a second; empty for as fast as the host can go | 1 |
| `DISK` | thousands of bytes a second the disk reads and writes; empty for no limit | 40 |
| `SCALE` | how many times larger each pixel is drawn | 2 |
| `KERNEL` | `hexaforth`, the small one, or `hexaforth-fast` | `hexaforth` |
| `STRICT` | the kernel halts on a fetch or a store outside memory | `OFF` |

    cmake -DMIPS= -DDISK= -DSCALE=1 build/cmake    # as fast as the host, at actual size
    cmake -USCALE -UMIPS -UDISK build/cmake        # back to the defaults
    cmake --build build/cmake --target demo

They are not arguments to `cmake --build`: a setting written after
`--target demo` is ignored. The tests that `check` runs set their own.

`demo` needs a QEMU built with a display. Without one:

    cmake --build build/cmake --target hello        # a window typed at the reference VM, and a picture of it
    cmake --build build/cmake --target screenshot   # remakes the picture above

`DESKTOP.md` explains how to use the desktop and how to write a program for
it.

## What is in the tree

The machine is implemented three times, and the second and third are tested
against the first:

- `vm.c`, the reference: a virtual machine in C
- `baremetal/`, the machine twice in AArch64 assembler for QEMU, where the
  desktop is shown: the small one above, and a faster one that decodes by
  table. The tree also calls these kernels (`KERNEL`, `kernel_size`,
  `build/kernel/`), since they are what QEMU boots. Everywhere else, "the
  kernel" means the first 4KB of the Forth
- `rtl/`, the CPU in Verilog, which runs the system in simulation and which
  the target `synth` places and routes for an FPGA

The system is written in Forth (`forth/`) and built by a cross-compiler that
is also Forth. The image it builds is called the nucleus, after `nuc.fs`:
the kernel, which is the part in ROM, plus the system that file includes.
The tests, tools and benchmarks are in C. All of it, at 64, 32 and 16-bit
cells, is built by CMake.

**The cross-compiler** (`forth/cross.fs`) runs on gforth and lays down the
image. Code is split into pages: a branch is relative to its page, and a
call to another page goes through an execution token. Its peephole
optimizer replaces a sequence of instructions with a single one wherever
one does the same work, and it has no table of which sequences those are:
it asks the machine. `util/fuse.h`, which gforth builds and calls, finds
each answer by stepping the machine on symbols instead of numbers
(`vm_step.h`, the same step `vm.c` runs on real values), and each answer is
then run on `vm.c` before it is used. A word without a dictionary header
keeps its name as a label, for the tools and the decompiler, and a small
word is compiled inline where that is no longer than a call to it.

**The nucleus** is a Forth system at its prompt: an interpreter and a
compiler, `create` and `does>`, `catch` and `throw`, comments, blocks loaded
as Forth source, and names stored at five bits per character. It has
cooperative tasks, each with its own part of the two stacks, and the Forth
window and the picture's dither run as tasks. Code typed at the prompt is
compiled through its own peephole optimizer (`forth/join.fs`), which works
out constant arithmetic by running it, and keeps only the rules that proved
worth their space when measured.

**The desktop** is drawn in the style of QuickDraw, on a one-bit-per-pixel
display in the machine's memory: transfer modes and patterns, `copy-bits`,
a port for each window, fonts stored as strikes with proportional text, and
regions. Each window is repainted only where it is visible, and only where
it is out of date. Words keep their working values on the stacks rather
than in variables. The two that need too many values for that, `copy-bits`
and `draw-text`, save their state if they are called again while running,
so drawing can be restarted partway through. The programs on the desktop
are the Forth window, a block editor, the Words window and a decompiler
that shows each instruction as the words that make it up, a calculator, a
viewer that reads a TIFF from disk and dithers it a row at a time, and
HexaPaint, whose canvas is stored on disk.

**The CPU** follows the design of the J1a: one memory read port, the top
cells of each stack in registers and the rest in two small memories, and the
expensive operations taking extra cycles rather than slowing the clock. Its
memory can answer in one cycle or two, and the kernel can be in a ROM, from
which a jump costs no extra cycle.

## How it is tested

    cmake --build build/cmake --target check

runs every test and prints a line for each:

- each instruction test compiled by a C compiler and by `cross.fs`, the two
  compared instruction for instruction, then run
- the image built with the peephole optimizer against the image built
  without it: every straight-line sequence must leave the same result, and
  every call, jump and branch must go to the same place
- on the 64, 32 and 16-bit machines: the nucleus at its prompt, comments and
  blocks read as source, regions against their pixels, every drawing word
  against a model that draws a pixel at a time, the dither and the TIFF
  reader against a reference dither, and the desktop driven with keys and a
  pointer and its screen checked
- each AArch64 kernel against `vm.c` on eight thousand cases; the desktop on
  QEMU with the small kernel, its screens checked; and with each kernel
  again with nothing in the machine's memory but its ROM, which reads the
  system from the disk and compiles the desktop from source
- the Verilog against `vm.c` at every cell size, with one-cycle and two-cycle
  memory, and with the kernel in ROM: on the same cases, and on a session
  typed at the nucleus
- that the kernel contains nothing that can never be reached

`vm.c` is the definition of the machine, and everything else is held to it.
The reports show where the work goes and what a change costs: `profile`,
`peephole`, `phrases`, `subtracts`, `examine` and `hits`. `CLAUDE.md`
describes each one.

## Status

None of it has been built as hardware yet. The starting point for that is
`rtl/soc.v`: the CPU and its memory, with a clock, a reset, a console that
sends and receives bytes with a handshake, and a signal that says the
machine has halted. The cell size, the memory size and the memory's latency
are parameters, and with `HF_KROM` the kernel is a ROM. The target `synth`
places and routes it for an ECP5 and reports the clock it would reach.

A physical machine still needs:

- video output for the display: the Verilog keeps the bitmap in memory but
  has nothing that shows it
- a keyboard, a pointer and a disk, with a driver for each: the system's
  drivers are for QEMU's devices. `DESKTOP.md` says what the system needs
  from each
- a board

## History

It began in 2020 as an experiment: would a virtual machine with unencoded
instructions, in the style of the Novix NC4016 and the
[J1](https://excamera.com/files/j1.pdf), be faster than one that decodes
bytecode? The instruction format came from the J1, as swapforth's J1b has
it, with its single field of fixed routes for where a result goes replaced
by two fields: one choosing the input and one the output.

## Further reading

- `MODEL.md`: the definition of the machine, which all three implementations
  follow
- `UNDERSTANDING.md`: why it is designed the way it is
- `DESKTOP.md`: how to use the desktop, and how to write a program for it
- `doc/superoptimizations.md`: the ways the machine's work has been made
  shorter and faster, by hand and by search
- `CLAUDE.md`: how to build, test and measure, where everything is, and the
  rules for working on it
- the commit history: each commit says what changed, why, and what it cost
  or saved
