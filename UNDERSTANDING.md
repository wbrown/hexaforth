# Understanding hexaforth

`MODEL.md` defines the machine. This document explains what each part of
the design is for and what it costs. Where a reason rests on a measurement,
the target that makes it is named, and the figure is whatever that target
prints. The README has the figures the project exists to show.

## Where it started

The J1, by James Bowman, is a Forth CPU whose 16-bit instruction is made of
fields that drive the datapath directly: four bits choose what the ALU
computes, three choose one extra place the result goes (the cell under the
top of the stack, the return stack, memory, or a port), one says to
return, and four move the two stacks.

Hexaforth began in 2020 as an experiment with that idea in software: would a
virtual machine with unencoded instructions be faster than one that decodes
bytecode? It took the instruction format of swapforth's J1b and made the
cell 64 bits.

## Two fields instead of one

The J1's three-bit field names a fixed route: T to the cell under it, T to
the return stack, the cell under T to memory, or to a port. Hexaforth
replaces it with two two-bit fields. One chooses the ALU's **input**: the
top of the stack (T), the cell under it (N), the memory cell T points to, or
the top of the return stack (R). The other chooses where the result
**goes**: T, R, the memory cell T points to, or a port.

    15   14 13   12    11 10    9 8      7 6 5 4     3 2      1 0
     0    1  1   ret   input    output   operation   d-stack  r-stack

That gives sixteen routes, each usable with any operation, and it means:

- A fetch is a choice of input and a store is a choice of output, so either
  can be combined with any operation
- An operation can be done on memory in place. Read memory, add, write
  memory is `+!`, in one instruction. Read memory, OR, write memory sets
  pixels
- The return stack is an input. A value that stays the same on every turn
  of a loop is kept there, and added, stored or compared from there without
  first being brought to the data stack
- A value that is needed twice can be kept there too, and a value that is
  R plus the cell under the top doesn't need to be kept at all.
  `doc/superoptimizations.md` describes what the search for fewer
  instructions has found along these lines, and the other ways the
  machine's work has been made shorter
- Every combination of field values is an instruction. Giving one a name in
  `vm_opcodes.h` costs a line of C and nothing in the machine, and an
  instruction without a name can be written out by its fields in source
  that `cross.fs` compiles (`MODEL.md` lists the fields and what each does)

Reading, operating and storing in one instruction is the inner step of
drawing a glyph. Without it, OR-ing a mask into a cell of the display is
`tuck @ or swap !`: five instructions, and by the Verilog's timings six
clocks, where the single instruction takes two.

## The sixteen operations

The operation field is four bits, and all sixteen values are used. Each
operation can be combined with any of the four inputs, any of the four
outputs and any movement of the stacks.

The J1b's sixteen are T, N, `T+N`, `T&N`, `T|N`, `T^N`, `~T`, `N==T`, `N<T`,
`N>>T`, `N<<T`, R, `[T]`, `io[T]`, the stack depths, and `Nu<T`. Three of
those just pass a register through; here they are one operation with a
choice of input. The stack depths are read through ports instead. That
frees three slots, which hold the two operations that write N, and
multiply.

- **`IN->`** passes its input through. The instruction is then just its
  routes and stack movements, and covers `dup`, `over`, `drop`, `>r`, `r>`,
  `r@`, `exit`, `@` and `!`. No other operation does as much of the ALU's
  work on the desktop (the target `profile` says how much)
- **`T->N,IN->`** also writes T into the cell under it. In the J1 that is one
  of the routes. Here the output is T, R, memory or a port, and N is
  otherwise written only by a push, so this has to be an operation. It
  gives a store that keeps its address, `tuck !`, in one instruction
- **`T<>N,IN->`** swaps T and N and passes on its input as it was before the
  swap. It is `swap`, and `swap >r`. Combined with a push, it rearranges
  three cells in one instruction: `tuck`, and `swap r>`
- **`IN+N`** adds two cells, and subtraction is built from it. A literal
  with its add flag set also adds: that adds a constant, and this adds two
  computed values
- **`~IN`** is the only one-input operation. It turns the adder into a
  subtractor (see below), lets `and` clear bits, and turns each comparison
  into its opposite
- **`IN&N`, `IN|N`, `IN^N`** are the ways of drawing on a bitmap: clear, set
  and invert. With memory as both input and output, each changes a cell of
  the display in one instruction. `xor` is also how a loop over memory
  ends: `2dup xor while` gives zero when the two addresses meet
- **`IN*N`** is new compared to the J1. Without it a multiply is a loop with
  a step for every bit. It takes two extra clocks, and the target `gates`
  counts the CPU with it and without it
- **`IN==N`** produces a flag as a value, which can be combined with another
  or stored. A conditional jump only asks whether T is zero, so for a jump
  alone `xor` would do. It is also `0=`, which is `0 =`. With N as its
  input it compares a cell with itself, which is true whatever the cell
  is: that is `true`, -1 in one instruction, as `IN^N` with N as its input
  is `false`
- **`N<IN`** has to be an operation because the machine keeps no flags. The
  sign of a difference is wrong when the subtraction overflows, and nothing
  records that it did
- **`Nu<IN`** is how a machine with no carry flag gets one: the carry out of
  an addition is whether the sum is less than one of the numbers added, and
  the kernel's double-cell arithmetic works this way. It also tests
  0 ≤ i < n in one comparison
- **`IN>>T`, `IN<<T`** shift by a computed count. Characters and pixels are
  kept packed in cells and extracted with a shift and a mask. The right
  shift brings in zeros; the sign-preserving shift, `2/`, is a word
- **`[IN]`** fetches through whatever the input is. Through T it is the same
  as the memory input. Through N it fetches from an address under the top
  and leaves the top alone; through R, from a pointer kept on the return
  stack; through memory, from a pointer stored in memory. With memory as
  the output it copies a cell from one place to another in one instruction
- **`io[IN]`** reads a port. Devices are on the ports, which are a separate
  address space, so every memory address is a cell

## Subtracting and counting

Subtraction is `invert`, `1 +`, `+`: three instructions. It uses the ALU's
complement and its adder, by design. The value being complemented can be T,
N, a memory cell or R, so any of them can be subtracted where it is;
a − b − 1 is one complement and one add; and a value kept negated can be
subtracted by adding it, which from memory or from R is one instruction. A
loop that runs from one address to another stops when the two meet, with no
count to compute (the target `subtracts` shows where the work subtracts,
how often, and what).

A literal can only add a non-negative number, so adding one is one
instruction and subtracting one is two: a -1, which is the cell under the
top compared with itself, and an add. So loops count up: a loop that
counts starts at a negative number and counts up to zero.

## Literals and variables

A literal is twelve bits, a shift of 0, 12 or 24 bits, and a flag that says
whether it is pushed or added to T. With the flag set, adding any constant
is one instruction. A negative number is made with `invert`; -1 alone is
one instruction, which has no literal in it (`true`).

The fourth value of the shift field turns the instruction into a variable
access. The twelve bits select one of the 4,096 cells starting at
`VAR_BASE`, which is fetched, or, with the flag set, has T stored in it.
Without this, every use of a variable would be a literal followed by a fetch
or a store. The system keeps its variables within those cells, and puts
other data (fonts, tables, buffers) elsewhere.

## Cells

Memory is made of cells, an address is a cell number, and `@` and `!` move
one cell.

It is one machine with 16, 32 or 64-bit cells: the same Forth and the same
tests, at a different width. An instruction is 16 bits at every width. A
machine with 16-bit cells has 65,536 of them, which is 128KB.

Text is up to the Forth system. Characters are packed into cells, and code
that handles whole strings works a cell at a time: that is how the
dictionary is searched and how glyphs are drawn. A name in the dictionary
uses five bits per character.

## Code

Code is in pages of 4,096 instructions. A jump, call or branch has thirteen
bits for its target: twelve bits of address, and one that says whether they
are an address in page 0 or an offset within the current page. So page 0,
where the kernel lives, can be called from any page, and a page can reach
all of itself. Return addresses and execution tokens are full addresses, so
returning and `execute` can go anywhere, and a call into another page is a
literal followed by a call to `execute`.

The first 4KB of page 0 is the kernel, which a machine may have in ROM.

## The stacks and the ports

The stacks are separate from memory. Each is accessed by pushing and
popping, and through a port that reads or sets its depth. Setting the depth
changes only the depth. A task owns a range of depths on each stack, and
switching tasks means saving two depths and setting two others. That is all
the support the machine has for tasks.

Devices are on the ports. Reading a port is an operation, and writing one is
an output.

## The display

The display is a bitmap in memory, one bit per pixel, and drawing is
fetching and storing cells. One store writes as many pixels as a cell has
bits.

Where the bitmap is, how long a row is, which end of a cell is the left and
whether a set bit is black all belong to the machine, and the Forth system
takes them from the machine's definition. Code that changes something
marks what is out of date, and the event loop repaints it.

## As hardware

The Verilog follows the J1a's design (`rtl/hexaforth.v` says how). The
memory has one read port, and what comes out of it is what was requested in
the previous cycle: usually the next instruction, fetched while the current
one runs. T, N and R are registers. One adder serves `+`, `=`, `<` and
`u<`.

An instruction is one clock, plus one for a memory read, one for a shift,
and two for a multiply. A store happens within its instruction's clock: it
completes while the next instruction starts. Reading, operating and
storing in one instruction takes two clocks.

The 16-bit CPU has fewer gates than a Z80 or a 68000, counted the same way
with memories left out (the target `gates`; the README has its figures).

## One definition

What an instruction does is defined once, in `vm_step.h`. `vm.c` includes
it and runs it on real values. `util/fuse.h` includes it too and runs it on
symbolic terms: a name for whatever is in each cell instead of a number, so
that the effect of a sequence of instructions is known for every possible
state at once.

The cross-compiler uses that to ask which single instruction does the work
of two or three, and emits that one. The programmer writes standard Forth
words, and instructions without names are found for them: `over +` is one
instruction, and so is `r> @ over !`. Since what an instruction does is
defined only in `vm_step.h`, every answer is also run on `vm.c` before it is
used.

The field names, the instruction names and the machine's sizes are in the C
headers, and the Forth is generated from them. The two AArch64 kernels and
the Verilog are tested against `vm.c`, which is the definition of the
machine.

## Small before fast

The kernel has to fit its ROM at every cell size. Kernel words are written
the short way, and the comment at each one says what the faster way would
be and where the difference would be noticed. `um*` takes a step for every
bit regardless of the numbers, and a program that multiplies a lot has its
own multiply.

The targets `profile`, `bench` and `examine-16` show where the instructions
go, what a piece of work costs in clocks, and how long each part is. A
change is kept or dropped based on what they print before and after it.
