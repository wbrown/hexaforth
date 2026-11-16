# Understanding hexaforth

`MODEL.md` says what the machine is.  This says what each part of the
design is for and what it costs.  Where a reason is a measurement, the
target that makes it is named, and the figure is what that target prints.
The README has the figures the project is there to show.

## Where it started

The J1 (James Bowman) is a Forth CPU whose instruction is sixteen bits of
fields that drive the datapath directly: four choose what the ALU computes,
three choose one place the result is also to go (the cell under the top of
the stack, the return stack, memory, or a port), one says to return, and
four move the two stacks.

Hexaforth began in 2020 as an experiment with that idea in software: whether
a virtual machine whose instructions are unencoded would be quicker than one
that decodes bytecode.  It took the instruction of swapforth's J1b, and made
the cell 64 bits.

## Two fields for one

The J1's field of three bits names a fixed route: T to the cell under it, T
to the return stack, the cell under T to memory, or to a port.  Hexaforth
has two fields in its place, of two bits each.  One chooses the ALU's
**input**: the top of the stack (T), the cell under it (N), the cell of
memory T points at, or the top of the return stack (R).  The other chooses
where the result **goes**: T, R, the cell of memory T points at, or a port.

    15   14 13   12    11 10    9 8      7 6 5 4     3 2      1 0
     0    1  1   ret   input    output   operation   d-stack  r-stack

That is sixteen routes, each with any operation, and it gives:

- A fetch is a choice of input and a store a choice of output, so either is
  had with any operation
- An operation is done on memory where it lies.  Memory in, add, memory out
  is `+!`, in one instruction.  Memory in, or, memory out sets pixels
- The return stack is an input.  What is the same every turn of a loop is
  kept there, and added, stored or compared from there without being
  brought to the data stack first
- Every setting of the fields is an instruction.  A name in `vm_opcodes.h`
  costs a line and nothing in the machine, and an instruction with no name
  is said by its fields, in source that `cross.fs` compiles (`MODEL.md` has
  the fields and what each does)

The read, the operation and the store as one instruction is the inner step
of drawing a glyph.  OR-ing a mask into a cell of the display without it is
`tuck @ or swap !`: five instructions, and by the Verilog's timings six
clocks where the one instruction is two.

## The sixteen operations

The operation field is four bits, and it is full.  Each operation is had
with four inputs, four outputs and every movement of the stacks.

The J1b's sixteen are T, N, `T+N`, `T&N`, `T|N`, `T^N`, `~T`, `N==T`, `N<T`,
`N>>T`, `N<<T`, R, `[T]`, `io[T]`, the stacks' depths, and `Nu<T`.  Three of
those pass a register through, and here they are one operation with a choice
of input.  The depths are ports.  In the three places that leaves are the
two operations that write N, and the multiply.

- **`IN->`** passes its input through.  The instruction is then its routes
  and the stacks' movements, and it is `dup`, `over`, `drop`, `>r`, `r>`,
  `r@`, `exit`, `@` and `!`.  No operation does more of the ALU's work on
  the desktop (the target `profile` says how much)
- **`T->N,IN->`** also writes T into the cell under it.  In the J1 that is
  one of the routes.  Here the output is T, R, memory or a port, and N is
  otherwise written only by a push, so it is an operation.  It is `swap`,
  `nip` and `swap>r`
- **`T<>N,IN->`** exchanges T and N and delivers its input as it was before
  they were exchanged.  With a
  push that is three cells rearranged in one instruction: `tuck`, and
  `swap r>`
- **`IN+N`** adds two cells, and subtraction is made of it.  A literal with
  its add flag set adds too: that is the add of a number that is known, and
  this is the add of two that are worked out
- **`~IN`** is the only operation of one input.  It makes the adder a
  subtractor (below), makes `and` clear bits, and turns each comparison into
  its opposite
- **`IN&N`, `IN|N`, `IN^N`** on a bitmap are the ways of drawing: clear, set
  and invert.  With memory as input and output each changes a cell of the
  display in one instruction.  `xor` is also what a loop over memory ends
  on: `2dupxor while` leaves nothing when the two addresses have met
- **`IN*N`** is added to the J1's.  Without it a product is a loop of a step
  for every bit.  It takes two more clocks, and the target `gates` counts
  the CPU with it and without
- **`IN==N`** makes a flag that is a value, to be combined with another or
  stored.  A conditional jump asks only whether T is nothing, so `xor` would
  do for a jump alone.  It is also `0=`, which is `0 =`
- **`N<IN`** is an operation because the machine keeps no flags.  The sign
  of a difference is wrong when the difference overflows, and nothing
  records that it did
- **`Nu<IN`** is how a machine with no carry flag has one: the carry out of
  an addition is that the sum is less than what was added, and the kernel's
  arithmetic on two cells is made so.  It is also the test of 0 ≤ i < n in
  one comparison
- **`IN>>T`, `IN<<T`** shift by a count.  Characters and pixels are kept
  packed in cells and taken out by a shift and a mask, and how far is worked
  out.  The right shift brings in zeros, and the one that keeps the sign,
  `2/`, is a word
- **`[IN]`** fetches through whatever the input is.  Through T it is what
  the memory input already does.  Through N it fetches from an address under
  the top and leaves the top; through R, from a pointer kept on the return
  stack; through memory, from a pointer in memory.  With memory as the
  output it moves a cell from one place to another in one instruction
- **`io[IN]`** reads a port.  Devices are on the ports, which are a space
  of their own, and every address of memory is a cell

## Subtracting, and counting

Subtracting is `invert`, `1 +`, `+`: three instructions.  It is done with
the ALU's complement and its add, by choice.  What is complemented can be T,
N, a cell of memory or R, so any of them is subtracted where it is; a − b − 1
is one complement and one add; and a value that is kept negated is
subtracted by adding it, which from memory or from R is one instruction.  A
loop that goes from one place to another ends when the two meet, with no
count to work out (the target `subtracts` says where the work subtracts, how
often, and what).

A literal adds numbers from nothing up, so adding one is one instruction and
taking one away is three.  A loop goes up: one that counts starts below
nothing and goes up to it.

## Literals and variables

A literal is twelve bits, a shift of 0, 12 or 24, and a flag that says
whether it is pushed or added to T.  With the flag every literal is the add
of a number that is known, in one instruction.  A number below nothing is
made with `invert`.

The fourth value of the shift field makes the instruction a variable's.  The
twelve bits pick one of the 4,096 cells from `VAR_BASE`, which is fetched,
or with the flag set has T stored in it.  Without it each use of a variable
is a literal and then a fetch or a store.  The system's variables are kept
within those cells, and other data (a font, a table, a buffer) is laid down
elsewhere.

## Cells

Memory is cells, an address is the number of one, and `@` and `!` move one.

It is one machine with cells of 16, 32 or 64 bits: the same Forth, the
same tests, a different width.  An instruction is 16 bits at every width.  A
machine with cells of 16 bits has 65,536 of them, which is 128KB.

Text is the Forth system's affair.  Characters are packed into cells, and
what deals with whole strings works a cell at a time: the dictionary is
searched so, and glyphs are drawn so.  A name in the dictionary is five bits
a character.

## Code

Code is in pages of 4,096 instructions.  A jump, a call or a branch has
thirteen bits for where it goes: twelve, and one that says whether they are
an address in page 0 or an offset in the page the instruction is in.  Page
0, where the kernel is, is reached from every page with a call, and a page
reaches all of itself.  A return address and an execution token are whole
addresses, so returning and `execute` go anywhere, and a call into another
page is a literal and a call of `execute`.

The first 4KB of page 0 is the kernel, and a machine may have it as a ROM.

## The stacks and the ports

The stacks are apart from memory.  Each is reached by pushing and popping,
and by a port that reads or sets its depth.  Setting a depth changes only
the depth.  A task has a range of depths on each stack, and changing task is
saving two depths and setting two others.  That is all the machine has for
tasks.

Devices are on the ports.  Reading a port is an operation and writing one is
an output.

## The display

The display is a bitmap in memory, a bit to a pixel, and drawing is fetching
and storing cells.  A store is as many pixels as a cell has bits.

Where the bitmap is, how long a row is, which end of a cell is the left and
whether a set bit is black are the machine's, and the Forth system takes
them from the machine's definition.  What changes something says what is
out of date, and the event loop paints it.

## As hardware

The Verilog follows the J1a in how it is built (`rtl/hexaforth.v` says in
what).  The memory has one port to read through, and what comes out of it is
what was asked for the cycle before: usually the next instruction, fetched
while this one is carried out.  T, N and R are registers.  One adder serves
`+`, `=`, `<` and `u<`.

An instruction is one clock, a read of memory one more, a shift one more,
and a multiply two more.  A store is within its instruction's clock: it
lands while the next instruction starts.  The read, the operation and the
store as one instruction is two clocks.

The CPU with cells of 16 bits is fewer gates than a Z80 and than a 68000,
counted the same way with memories left out (the target `gates`, and the
README has what it prints).

## One definition

What an instruction does is said once, in `vm_step.h`.  `vm.c` has it and
runs it on cells.  `util/fuse.c` has it too and runs it on terms: the name
of what would be in each cell, in place of a number, so that what a run of
instructions leaves is known for every state at once.

The cross-compiler asks that which one instruction does the work of two or
three, and lays down the one.  The programmer writes standard words and the
instructions with no name are found for them: `over +` is one, and
`r> @ over !`.  What an instruction does is said only in `vm_step.h`, and
an answer is run on `vm.c` before it is used.

The fields' names, the names of instructions and the machine's sizes are in
the C headers, and the Forth is made from them.  The two kernels and the
Verilog are run against `vm.c`, which is the machine.

## Short before quick

The kernel is to fit its ROM at every width.  A word of the kernel is
written the short way, and the comment at it says what the quick way is and
what it would be felt in.  `um*` takes a step for every bit whatever the
numbers are, and a program that multiplies a great deal has one of its own.

The targets `profile`, `bench` and `examine-16` say where the instructions
go, what a piece of work costs in clocks, and how long each part is.  A
change is kept or dropped by what they print before it and after.
