# Hexaforth

Hexaforth is a stack machine, a Forth system that runs on it, and three implementations of the machine: `vm.c` (the reference VM, in C), two AArch64 kernels in `baremetal/` that run under QEMU, and Verilog in `rtl/`. The Forth system is a nucleus (kernel, interpreter, compiler, dictionary, tasks) and, where the machine has a display, a desktop of windows and applications.

`MODEL.md` specifies the machine, and the kernels and the Verilog are tested against `vm.c`. If code and `MODEL.md` disagree, report it and leave `MODEL.md` as it is. A change to what the machine does changes `MODEL.md`, `vm.c`, both kernels and the Verilog together.

## Rules

- Say that something builds, runs or passes only when a tool result in this session shows it, and quote that result. Quote only output that a tool actually printed. A tool call that returned an error left everything as it was, so make the call again correctly before relying on it.
- Run commands yourself with the tools, including any you would otherwise hand to the user.
- When the user asks a question about work in progress, stop all running and editing, and answer the question: yes or no first, then what was done and why. End the answer there, because the user decides what happens next. "Stop" means the same.
- Report what was done, what failed, and what the user must decide. A decision stays decided once it is made, so act on it.
- The project is written in C and Forth, so a test, a generator or a measuring tool is C in the tree that CMake builds and runs. Python, shell scripts and Makefiles stay out of the tree.
- Each fact (an encoding, a layout, a table, a size) is defined in one place, and everything else is generated from it or reads it there. Refer to a size or an address by its name.
- Measure instead of estimating. Find out what instructions do by running them on `vm.c`, and what a change costs by running the benchmark before and after it. Runtime is counted in cycles, because a memory read, a shift and a multiply each take more than one.
- The ALU's operation field is full and the design depends on the fields composing, so look for speed in the fields the machine has, and leave its operations as they are.

In the shell:

- Edit a file with the edit tool and create one with the write tool. The shell is for running programs.
- A shell call is one bare command. `&&`, `||`, pipes, `<` and here-documents are rejected, and so are `echo`, `cat`, `head`, `tail`, `sed`, `awk`, `grep`, `sort`, `wc`, `diff`, `python` and debuggers. Read and search files with the read and search tools.
- Ask the tree's own tools (Tools, below) about an image, a profile or a program's output, and extend one of them when the question is beyond them all.

## The machine

- A cell is 64, 32 or 16 bits. The system is built and tested at all three sizes (`util/machines`), and 64 is the default.
- Memory is an array of cells. An address is a cell number, and `@` and `!` fetch and store one cell. The machine counts in cells and instructions throughout.
- There are two stacks, data and return, both separate from memory. T is the top of the data stack, N the cell under it, and R the top of the return stack.
- An instruction is 16 bits, and code is addressed by instruction. A wider cell holds several instructions, the first in its low bits.

| Top bits | Instruction |
|---|---|
| `1` | literal |
| `000` | jump |
| `001` | conditional jump, which pops T and jumps if it was zero |
| `010` | call |
| `011` | ALU instruction |

A literal holds a 12-bit number and a shift of 0, 12 or 24, and either pushes the shifted number or adds it to T. The fourth value of its shift field makes it fetch or store one of the 4,096 cells starting at `VAR_BASE`, so a variable kept there costs one instruction to fetch or to store.

An ALU instruction is made of fields in place of an opcode. Its fields select the input (N, T, the cell T points to, or R), one of sixteen operations, where the result goes (T, R, the cell T points to, or port T), how each stack moves, and whether to return. Every combination is an instruction, and a baseword such as `dup` or `+` is a name for one of them (`vm_opcodes.h`).

A literal is unsigned, so adding 1 is one instruction and subtracting 1 is two, and loops count up. A shift count is taken modulo 64 at every cell size. Code is in pages of 4,096 instructions, and a call within a page, or into page 0 where the kernel is, is one instruction, while a call into any other page pushes the address and calls `execute`.

## Writing Forth

- Write standard words and let the peephole join them into instructions. `dup >r` compiles to one instruction as written, so write a phrase as its words.
- Keep working values on the stacks, where T, N and R are inputs of the ALU. A value that is the same on every turn of a loop goes on the return stack.
- A loop over memory uses its addresses as the count: `bounds begin 2dup xor while ... 1+ repeat 2drop`.
- The kernel's `um*`, `u/mod` and `c@` are written to be small. A program that multiplies, divides or reads characters heavily defines its own word and times it.
- A word that another file uses has a header, `header name : name ... ;`. A word used only in its own file is defined bare, `: name ... ;`.

A program for the cross-compiler is a file in `forth/`:

```forth
meta
    4 org
target

header double : double ( n -- 2n ) d# 2 * ;
header main : main d# 21 double drop halt ;
header quit : quit halt ;
```

`main` and `quit` must be defined, because the image starts by running them. A number in a definition is written `d# 123` or `h# ff`, `\` starts a comment that runs to the end of the line, and `emit` sends a character to the console, which is how a program shows a result.

| Step | Command | Run from |
|---|---|---|
| build | `CPATH="$(brew --prefix)/include" gforth ./cross.fs ./basewords.fs ./yourprogram.fs` | `forth/` |
| run | `build/cmake/hexaforth build/yourprogram.hex` | the top of the tree |
| read its code | `build/cmake/disasm_hex build/yourprogram.hex --no-color` | the top of the tree |

The build writes `build/yourprogram.hex` for the 64-bit machine, and `CPATH` is needed where gforth comes from Homebrew. The peephole joins what it can and computes arithmetic on known numbers, so the image may have fewer instructions than the source has words, and instructions are counted in what `disasm_hex` prints. `hexaforth` stops when a stack goes out of range and prints what the return stack held, and `hexaforth-debug` traces every instruction with its fields and both stacks.

## Building and testing

| Command | What it does |
|---|---|
| `cmake -S . -B build/cmake` | configures the build, installing any missing dependency with Homebrew or apt |
| `cmake --build build/cmake` | builds every tool and runs `quick`, the tests that need only the reference VM |
| `cmake --build build/cmake --target check` | runs every test, on the reference VM, QEMU and the Verilog |
| `ctest --test-dir build/cmake -V -R NAME` | runs the tests whose names contain `NAME`, building what they need first |

Run `check` after any change to `cross.fs`, `nuc.fs` or the machine. Code is safe at a cell size only if a test has run it at that size. Run a tool by hand only after `cmake --build build/cmake` on its own, which leaves every tool in `build/cmake` up to date.

## Tools

A target is run with `cmake --build build/cmake --target NAME`.

| Command or target | What it shows |
|---|---|
| `build/cmake/hexaforth IMAGE.hex [DISK.img [MEMORY.out]]` | runs an image on the reference VM, and writes the VM's memory to `MEMORY.out` when it stops |
| `build/cmake/hexaforth-debug IMAGE.hex` | the same run with a trace of every instruction |
| `build/cmake/disasm_hex IMAGE.hex [NAME...]` | an image's code, one instruction to a line, for every word or for the named ones |
| `build/cmake/disasm_hex IMAGE.hex --typed FILE.fs` | what the nucleus compiles when `FILE.fs` is typed at it |
| `build/cmake/disasm_hex IMAGE.hex NAME --counts WORK` | how many times each instruction of a word ran |
| target `examine` | an image's words, headers and bytes, by page and by file |
| target `bench` | what each benchmark costs in cycles on the Verilog CPU |
| target `profile` | where the desktop's instructions go, by file, by word and by kind of instruction |

## Where things are

| Path | What is there |
|---|---|
| `MODEL.md`, `UNDERSTANDING.md`, `DESKTOP.md` | the machine's specification, why it is designed as it is, and how to use and program the desktop |
| `vm.c`, `vm_loop.h`, `vm_step.h` | the reference VM |
| `vm_constants.h`, `vm_instruction.h`, `vm_opcodes.h` | the machine's sizes, the fields of an instruction, and the names of the fields and the basewords |
| `forth/cross.fs` | the cross-compiler, which gforth runs, and its peephole |
| `forth/basewords.fs` | the 64-bit machine's definition for `cross.fs`, which is generated |
| `forth/nuc.fs` | the nucleus: the kernel on page 0, the interpreter, the compiler, the dictionary and tasks |
| `forth/join.fs`, `forth/compose.fs` | the nucleus's own peephole |
| `forth/system.fs`, `forth/apps.fs` | the system's files and the applications, in the order their code is placed |
| `util/` | the tools: the disassembler, the image readers, the generators and the disk maker |
| `test/` | the tests |
| `bench/` | the benchmarks, the profile and the reports |
| `baremetal/` | the AArch64 kernels and the QEMU test harness |
| `rtl/` | the Verilog CPU, its memory and its test bench |
| `cmake/` | the build, one file to a subject |
| `doc/` | the other documents, among them `doc/formatting.md` on how Forth source is laid out |
