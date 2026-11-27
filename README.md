# hexaforth

A stack machine whose instructions are fields that drive the hardware directly; a Forth operating system for it, with windows, a pointer and menus; and the Verilog to build it.

The CPU with 16-bit cells is 7,256 gates, fewer than a Z80 counted the same way (with the memory for its two stacks left out). When drawing, it takes 1.06 to 1.36 clocks per instruction, so at 1.2 MHz it runs 0.88 to 1.13 million instructions a second.

The 1.2 MHz clock is chosen to show how little the machine needs, and the machine can be clocked faster. In a single instruction the machine takes its input from a stack, the return stack or memory, does an operation, sends the result to a stack or to memory, and moves both stacks, all in one clock, plus one for a memory read or a shift. A 68000 instruction takes at least four clocks, and often twelve or more.

One step of the same dither shows the difference: a pixel's value is compared with a threshold, and an eighth of the error is kept to pass on. On a 68000, as `bench/m68k/atk.s` does it after MacPaint, the step is four instructions, one of them a branch, and about 30 clocks:

    cmp.w   d7,d2           ;  4   is it more than 127?
    bgt.s   ItsWhite        ; 10
    sub.w   d4,d2           ;  4   the error: how far short of white it is
    asr.w   #3,d2           ; 12   an eighth of it

With 16-bit cells hexaforth does the step in nine branch-free instructions and 10 clocks, by keeping whether the pixel is black inside the number it passes on (`forth/atkinson.fs`):

    dup 896 <  h# 80ff and + 1+  3 rshift

As a result, dithering the Mandrill in colour takes 21.5 million clocks with 64-bit cells, reading the picture from disk, where a 68000 takes 63.2 million with the picture already in memory. At the Mac 128K's 8 MHz that is 2.7 seconds against 7.9. The design is meant to run at 10 to 14 MHz built from TTL, and on an FPGA the 64-bit system places and routes at 27 MHz, where the same picture takes 0.8 seconds.

![The desktop: the Forth window, the editor, the Words window, the calculator and the picture, with HexaPaint in front](doc/desktop.png)

## What it is meant to show

- How much a 7,256-gate machine can do at 1.2 MHz, which is fewer gates than the 1976 Z80 at a clock speed that microcomputers of 1979 ran at.
- How much more it does per clock than the processors of its time, so that at their clock speeds it is the faster machine.
- What could have been: a desktop on a machine of that size and speed.
- A Forth operating system with a desktop: overlapping windows, regions, a menu, a pointer, proportional text, and programs. There is a Forth prompt in a window, a block editor, a viewer that reads a TIFF and dithers it, a calculator, a paint program and a decompiler.
- The instruction set, in which an instruction is a control word: where the ALU's input comes from, the operation, where the result goes, and how each stack moves.
- How little code it takes. The kernel is 3,522 bytes, in a 4,096-byte ROM, the paint program is 2,232 bytes, and the whole machine, written in AArch64 assembler, is 800 bytes.

It is meant to be built from TTL, on an FPGA, or through TinyTapeout.

## The machine

The machine is two stacks, a memory of cells, and ports for devices. A cell is 16, 32 or 64 bits, and an instruction is always 16 bits. `MODEL.md` defines the machine, and `UNDERSTANDING.md` explains why it is designed this way.

An ALU instruction is three bits that mark it as one, followed by six fields. Every combination of field values is an instruction the machine will carry out:

    15   14 13   12    11 10    9 8      7 6 5 4     3 2      1 0
     0    1  1   ret   input    output   operation   d-stack  r-stack

| Field | What it chooses |
|---|---|
| input | the top of the stack, the cell under it, the memory cell the top points to, or the top of the return stack |
| operation | one of sixteen: pass the input through, two that also write the cell under the top, add, and, or, xor, multiply, complement, three comparisons, two shifts, fetch, and read a port |
| output | the top of the stack, the top of the return stack, the memory cell the top points to, or a port |
| d-stack, r-stack | whether each stack moves up one, down one or two, or stays where it is |
| ret | whether the instruction also returns |

Fetching is a choice of input and storing is a choice of output, so an operation can be done on memory in place. `+!` is one instruction (read memory, add, write memory), and so is OR-ing a mask into a cell of the display, which is what drawing a glyph is made of. Subtraction is three instructions, a complement, an add of one and an add, and the value being complemented can come from any of the four inputs.

The other instructions are a jump, a conditional jump, a call, and a 12-bit literal that is either pushed or added to the top of the stack, or that fetches or stores one of 4,096 variables.

A Forth word is a name for one of these instructions or for a sequence of them. Leaving out the ones that read or write a port, the fields give 4,960 instructions that behave differently, because many combinations do the same thing. Fifty-eight of those have names, and the system uses 415 (the target `profile` counts them, for the 64-bit image). The unnamed ones it uses come from the compiler, which replaces a sequence with a single instruction wherever one does the same work, and finds out which by asking the machine. `MODEL.md` describes every field.

## The numbers

Each figure below is printed by one of the tree's own targets, named so it can be run again. They were current when this was written.

**The CPU** (`gates`). A gate is a NAND, a NOT or a flip-flop as Yosys counts them, with memories left out. For hexaforth that leaves out its two stacks, 256 cells each; the Z80's storage is all registers, which are counted. The Z80 and 68000 figures come from other people's Verilog put through the same count.

| | gates |
|---|---|
| hexaforth, 16-bit cells | 7,256 |
| the same without its multiplier | 6,270 |
| a Z80 (tv80) | 9,868 |
| a 68000 (fx68k) | 23,423 |
| hexaforth, 32-bit cells | 16,169 |
| hexaforth, 64-bit cells | 41,729 |

**Speed** (`bench-draw`). On the 16-bit Verilog CPU, with one-cycle memory, an instruction takes 1.36 clocks on average drawing text, 1.16 scrolling and 1.06 filling. An instruction is one clock, plus one for a memory read or a shift, and two more for a multiply.

**Against a 68000** (`bench-dither`, `bench-m68k`). The Mandrill, 512 by 512, is read from disk a block at a time, as the system reads it, and dithered on the Verilog CPU with one-cycle memory. The 68000 runs the same dither, in code written for it, with the picture already in memory. Every bitmap is checked against a reference dither, done a pixel at a time, before its figure is printed. The figures are millions of clocks:

| | 68000 | 64-bit cells | 32-bit | 16-bit |
|---|---|---|---|---|
| colour, full size | 63.2 | 21.5 | 31.5 | 30.5 * |
| colour, half size | 40.3 | 11.7 | 17.4 | 17.4 |
| grayscale, full size | 31.7 | 14.8 | 21.2 | 21.4 * |
| grayscale, half size | 13.3 | 4.4 | 6.7 | 7.6 |

\* Run on a smaller system, `forth/picture.fs`, which is the windowing system, the TIFF reader and the dither, with the other applications left out. The 16-bit machine has 128KB, and with the full desktop loaded the 32KB bitmap is larger than what is left.

**The viewer's picture** (`bench-view`). The same picture is made as the desktop's viewer makes it, on the whole system at every cell size. The viewer reads a block of the picture, dithers its rows, and writes each block of the bitmap to the disk, so one block of the bitmap is in memory at a time. The bitmap is read back from the disk and checked against the same reference dither. The figures are millions of clocks:

| | 64-bit cells | 32-bit | 16-bit |
|---|---|---|---|
| colour, full size | 22.0 | 32.0 | 30.9 |
| colour, half size | 11.9 | 17.7 | 17.7 |

**On an FPGA** (`synth`). The 64-bit system, placed and routed for an 85k ECP5, is 15,836 LUTs and 1,039 flip-flops, at 27.49 MHz.

**The system** (`examine-16`). These figures are for the 16-bit machine, which has 65,536 cells (128KB) and a 512 by 342 display.

| | words | bytes of code |
|---|---|---|
| The kernel, in a 4,096-byte ROM | 269 | 3,522 |
| All drawing: fills, copying, text | 156 | 5,242 |
| Regions | 37 | 1,406 |
| The window manager | 116 | 3,536 |
| The TIFF reader and the dither | 121 | 3,912 |
| Text kept as lines, which two windows scroll | 44 | 1,144 |
| The Forth window | 22 | 692 |
| The block editor | 9 | 386 |
| The picture viewer, and its picture kept as gray | 158 | 4,272 |
| The calculator | 17 | 634 |
| The paint program | 51 | 2,232 |
| The decompiler and the Words window | 134 | 4,784 |
| Files on the disks, found by label, and the boot's console | 47 | 1,540 |
| The compiler's peephole, for typed code | 108 | 3,546 |
| Everything | 1,571 | 40,594 |

The dictionary holds 640 names in 5,984 bytes, at five bits per character.

**The machine on another processor** (`size`). `baremetal/hexaforth.S` is the whole machine in AArch64 assembler, with a console. It is 800 bytes:

| | bytes |
|---|---|
| Starting up | 60 |
| Fetching an instruction and telling what kind it is | 24 |
| Jumps, conditional jumps and calls | 64 |
| Literals and variables | 80 |
| The ALU: choosing its input | 52 |
| The sixteen operations, two instructions each | 128 |
| Moving the stacks, and sending the result where it goes | 116 |
| The ports: the console, the stack depths, device cells | 256 |
| Halting | 20 |

The code that carries out every instruction is 464 bytes, or 116 AArch64 instructions.

## What is in the kernel

The kernel is the first 4KB of page 0, the part that sits in ROM, and it is everything needed to type Forth at the machine and have it compiled. It has about 270 words, of which these have names you can type:

| Group | Words |
|---|---|
| The stacks | `dup drop swap over nip tuck rot -rot ?dup 2dup 2drop 2swap 2over 3dup depth adepth`, and `>r r> r@ rdrop i j unloop` |
| Arithmetic | `+ - * 1+ 1- 2* 2/ negate abs min max um* um/mod u/mod` |
| Bits | `and or xor invert lshift rshift` |
| Comparison | `= <> < > u< u> 0= 0<> 0< 0> true false` |
| Double-cell numbers | `d+ dnegate s>d d2* 2@ 2!` |
| Memory | `@ ! +! or! and! xor! *! not! c@ c! uw@ w! field@ field! ifield >c c> hi32 lo32 hi16 lo16 count cell+ cells aligned align cmove cells-move cells-fill /string bounds i@ i!` |
| The console and the ports | `key key? emit uart-emit 'emit 'console cr space bl type . .x .x2 hex1 accept io@ io!` |
| The interpreter | `source source-id sourceid >in parse parse-name isspace? isnotspace? isnotdelim >number base decimal hex state interpret1 numbered refill evaluate quit execute \ (` |
| The compiler | `: :noname ] [ immediate create does> variable constant , c, w, s, compile, literal 2literal postpone recurse exit ' inline: join-rules`, and `if else then ahead begin again until while repeat do ?do loop +loop leave`. `;` belongs to the peephole, which comes straight after the kernel, and so does `[']` on a machine with a display |
| The dictionary | `forth sfind >key samename? name> words here allot dp cp` |
| Errors | `catch throw -throw` |
| Tasks | `up sbase spawn pause` |
| Blocks | `blk 'blk-read 'blk-write` |

Twelve more words of the nucleus are in the other half of the kernel's page, outside the ROM, because the machine first uses them once the system is in memory: `2rot`, `fill`, `cmove>`, and the double-cell `dabs d>s d= d< du< d- d0< d0= d2/`. Moving them out left room in the ROM for the driver of the disk a machine starts from, and each kind of machine has its own driver file.

## Running it

You need CMake and either Homebrew or apt. Configuring installs the rest: it runs `brew install`, or `apt-get install` as root, for whichever of the twenty programs and libraries it needs are missing, QEMU, Verilator, Yosys and LLVM among them. With `-DINSTALL_NEEDED=OFF` it only checks and reports what is missing. `cmake/needs.cmake` lists them all.

    cmake -S . -B build/cmake
    cmake --build build/cmake --target demo      # the desktop, on QEMU
    cmake --build build/cmake --target demo-boot # the same, from its ROM and a disk: it compiles its desktop
    cmake --build build/cmake --target quick     # the tests on the reference VM alone; the default target runs them too
    cmake --build build/cmake --target check     # every test
    cmake --build build/cmake --target bench     # the benchmarks, on the Verilog CPU
    cmake --build build/cmake --target gates     # the gate counts

`demo` runs the machine the project is meant to show: a million instructions a second, a disk as fast as an 8-inch floppy, and each pixel drawn at twice its size. These settings, which also apply to `run`, `demo-test` and `demo-time`, are given to CMake when it is configured, and stay until they are changed:

| Setting | What it sets | Default |
|---|---|---|
| `MIPS` | millions of instructions a second; empty for as fast as the host can go | 1 |
| `DISK` | thousands of bytes a second the disk reads and writes; empty for full speed | 40 |
| `SCALE` | how many times larger each pixel is drawn | 2 |
| `KERNEL` | `hexaforth`, the small one, or `hexaforth-fast` | `hexaforth` |
| `STRICT` | the kernel halts on a fetch or a store outside memory | `OFF` |

    cmake -DMIPS= -DDISK= -DSCALE=1 build/cmake    # as fast as the host, at actual size
    cmake -USCALE -UMIPS -UDISK build/cmake        # back to the defaults
    cmake --build build/cmake --target demo

The settings belong to the configure step, so one written after `--target demo` on a `cmake --build` line is ignored. The tests that `check` runs set their own.

`demo` needs a QEMU built with a display. Without one:

    cmake --build build/cmake --target hello        # a window typed at the reference VM, and a picture of it
    cmake --build build/cmake --target screenshot   # remakes the picture above

`DESKTOP.md` explains how to use the desktop and how to write a program for it.

## What is in the tree

The machine is implemented three times, and the second and third are tested against the first:

| Where | What it is |
|---|---|
| `vm.c` | the reference: a virtual machine in C |
| `baremetal/` | the machine twice in AArch64 assembler for QEMU, where the desktop is shown: the small one above, and a faster one that decodes by table |
| `rtl/` | the CPU in Verilog, which runs the system in simulation and which the target `synth` places and routes for an FPGA |

The tree also calls the two AArch64 programs kernels (`KERNEL`, `kernel_size`, `build/kernel/`), since they are what QEMU boots. Everywhere else, "the kernel" means the first 4KB of the Forth.

The system is written in Forth (`forth/`) and built by a cross-compiler that is also Forth. The image it builds is called the nucleus, after `nuc.fs`: the kernel, which is the part in ROM, plus the system that file includes. The tests, tools and benchmarks are in C. All of it, at 64, 32 and 16-bit cells, is built by CMake.

**The cross-compiler** (`forth/cross.fs`) runs on gforth and builds the image. Code is split into pages: a branch is relative to its page, and a call to another page goes through an execution token. Its peephole optimizer replaces a sequence of instructions with a single one wherever one does the same work, and it asks the machine which, in place of keeping a table of those sequences. `util/fuse.h`, which gforth builds and calls, finds each answer by stepping the machine on symbols instead of numbers (`vm_step.h`, the same step `vm.c` runs on real values), and each answer is then run on `vm.c` before it is used. A headerless word keeps its name as a label, for the tools and the decompiler. A small word is compiled inline where the inline code is at most as long as a call to it.

**The nucleus** is a Forth system at its prompt: an interpreter and a compiler, `create` and `does>`, `catch` and `throw`, comments, blocks loaded as Forth source, and names stored at five bits per character. It has cooperative tasks, each with its own part of the two stacks, and the Forth window and the picture's dither run as tasks. Code typed at the prompt is compiled through its own peephole optimizer (`forth/join.fs`), which works out constant arithmetic by running it, and keeps only the rules that proved worth their space when measured.

**The desktop** is drawn in the style of QuickDraw, on a one-bit-per-pixel display in the machine's memory: transfer modes and patterns, `copy-bits`, a port for each window, fonts stored as strikes with proportional text, and regions. Each window is repainted only where it is visible, and only where it is out of date. Words keep their working values on the stacks rather than in variables. The two that need too many values for that, `copy-bits` and `draw-text`, save their state if they are called again while running, so drawing can be restarted partway through. The programs on the desktop are the Forth window, a block editor, the Words window and a decompiler that shows each instruction as the words that make it up, a calculator, a viewer that reads a TIFF from disk and dithers it a row at a time, and HexaPaint, whose canvas is stored on disk.

**The CPU** follows the design of the J1a: one memory read port, the top cells of each stack in registers and the rest in two small memories, and the expensive operations taking extra cycles rather than slowing the clock. Its memory can answer in one cycle or two, and the kernel can be in a ROM, which spares a jump the extra cycle that two-cycle memory adds.

## How it is tested

`cmake --build build/cmake --target check` runs every test and prints a line for each:

- Each instruction test is compiled by a small Forth compiler written in C and by `cross.fs`. The two results are compared instruction for instruction, and then the test is run.
- The image built with the peephole optimizer is compared with the image built without it: every straight-line sequence must leave the same result, and every call, jump and branch must go to the same place.
- On the 64, 32 and 16-bit machines, the tests cover the nucleus at its prompt, comments and blocks read as source, regions against their pixels, every drawing word against a model that draws a pixel at a time, the dither and the TIFF reader against a reference dither, and the desktop driven with keys and a pointer and its screen checked.
- Each AArch64 kernel is run against `vm.c` on eight thousand cases. The desktop is run on QEMU with the small kernel and its screens are checked, and it is run again with each kernel started with only its ROM in memory, where the machine reads the system from the disk and compiles the desktop from source.
- The Verilog is run against `vm.c` at every cell size, with one-cycle and two-cycle memory, and with the kernel in ROM, on the same cases and on a session typed at the nucleus.
- Every word in the kernel must be reachable.

`MODEL.md` defines the machine, and the kernels and the Verilog are checked against `vm.c`, the reference implementation. The reports show where the work goes and what a change costs (`profile`, `peephole`, `pairs`, `phrases`, `subtracts` and `examine`), and `CLAUDE.md` describes each one.

## Status

It has yet to be built as hardware. The starting point for that is `rtl/soc.v`: the CPU and its memory, with a clock, a reset, a console that sends and receives bytes with a handshake, and a signal that says the machine has halted. The cell size, the memory size and the memory's latency are parameters, and with `HF_KROM` the kernel is a ROM. The target `synth` places and routes it for an ECP5 and reports the clock it would reach.

A physical machine still needs:

- video output for the display, because the Verilog only keeps the bitmap in memory
- a keyboard, a pointer and a disk, with a driver for each, because the system's drivers are for QEMU's devices (`DESKTOP.md` says what the system needs from each)
- a board

## History

It began in 2020 as an experiment: would a virtual machine with unencoded instructions, in the style of the Novix NC4016 and the [J1](https://excamera.com/files/j1.pdf), be faster than one that decodes bytecode? The instruction format came from the J1, as swapforth's J1b has it, with its single field of fixed routes for where a result goes replaced by two fields: one choosing the input and one the output.

## Further reading

| Document | What it covers |
|---|---|
| `MODEL.md` | the definition of the machine, which all three implementations follow |
| `UNDERSTANDING.md` | why it is designed the way it is |
| `DESKTOP.md` | how to use the desktop, and how to write a program for it |
| `doc/superoptimizations.md` | the ways the machine's work has been made shorter and faster, by hand and by search |
| `CLAUDE.md` | how to build, test and measure, where everything is, and the rules for working on it |
| the commit history | what each commit changed, why, and what it cost or saved |
