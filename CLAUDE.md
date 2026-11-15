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

Little needs the address of a byte, and before anything is given one it
is asked whether it does. What is asked is how it is reached, not how it
is packed: characters stay packed into cells, as many as a cell holds,
and are said by a cell and how far along they are. A table of characters
that is looked up is so: the key map is 256 characters, the 128 keys and
then the 128 shifted, and the calculator's buttons are a counted string,
each found with `char@`. (For a day the key map was a cell a key, which
needed no byte's address either and was four times the memory with
cells of 32 bits and eight times with 64.) Only the pointer's arrow is a
cell a row (`pcell,`): it is twelve rows, fetched 8,100 times in the
profile, and a cell is two instructions to fetch where a character is
eleven, so packed it would be 73,000 instructions more to be 84 bytes
less, and 12 bytes less with cells of 16 bits. Text that is drawn is
characters packed into cells, a cell's lowest eight bits first, and is
said by a cell and which character of the cells from there is its first:
`draw-chars ( a i u x y -- )` and `chars-width ( a i u -- n )`, and
`ctext ( cstr -- a i u )` gives those for a counted string that starts
where a cell does. So it may be anywhere in memory, and a line of a grid
of characters is the grid's cell and the line's number times its length
(the editor: `eblk @ cblock  line 6 lshift  64`). Along the text a
character is eight bits on from the last, so the loop counts bits: the
cell is that over the bits in a cell and the character is shifted down
by the rest (`char@`), which is what `c@` does and costs what it costs.
`draw-text` and `text-width` take the address of a byte and are those
words. A grid that is moved has each row start where a cell does: the
Forth window's text is rows of `TSPAN` cells, moved up a row with
`cells-move` and a row made empty with `cells-fill` of a cell of spaces.
What still takes the address of a byte is what stores a character
(`c!`), `type` and `count`, which are the kernel's, and what the
interpreter reads (`evaluate`, `load`).

What it was worth, in the profile: the Forth window's text was moved up
a byte at a time, since a row of 50 characters did not start where a
cell does, and that was 31,520 instructions of the 178,600 a scroll of
that window cost; a scroll is now 142,300. `c@` ran 54,702 times and
runs 2,284, and `c!` 27,642 and 1,246. That is 1.18 million of the
desktop's 39.6 million instructions, 3%. A key found in the packed map
with `char@` is 11 instructions, and an event is 218 to hand on; with a
cell a key it was 215, which over the whole profile is 1,200
instructions for 768 bytes. Compare two profiles by word and not by
their totals: the driver waits for a block in a loop that asks the
device, and how often it goes round is the host's doing, so the same
image was 38.43 and 39.81 million instructions on two runs, all of the
difference in `vblk-io`. (The total was first given here as 39.60
million to 37.80, which was a run that waited less.) Reading
the characters of text that is drawn by the bit and not with `c@` is
2% of what noting a string's glyphs costs (1,243 instructions a string
to 1,217) and a quarter of a percent of the drawing benchmark's cycles:
it was done so that text need not be where the address of a byte
reaches, not for speed, and nothing on the machine of 64K cells has
been moved above its display for it, where there are 298 cells to
spare.

Code is addressed by instruction: `cp`, a branch origin and an execution
token are instruction numbers, and `i@` and `i!` fetch and store one.

Dictionary headers are whole cells: a link cell, an xt cell (top bit set for
an immediate word), then the name, so lookup compares a cell at a time. A
name is five bits a character, as many to a cell as a cell holds (3, 6 or
12), the first the lowest, and the top bit of its last cell is set: so
nothing counts its characters, and a name that is the start of another is
not the same cells. The dictionary does not tell a capital from its letter
and never kept one, so a character is a code of five bits: nothing, the
26 letters, `-` `!` `?`, and two codes that say the next code is one of 31
others (the digits and most marks) or one of the last eight, counted from
one, so that no code of a name is nothing and what is left of a cell after
its last is the padding. That is every
character that can be typed in a name; `cross.fs` says which is which
(`n-code`), and lays down the machine's two tables of it. The kernel packs
a name to look it up or to make a header of it (`>key`, with `kcode`, what
each character is). Whoever shows a name has its
characters put in a buffer (`name> ( a -- c-addr u )`, with `kchar`, what
each code is): the text of
one name at a time, from cells that may be anywhere in memory. Both are
the kernel's, as `words` is, and both pack with one word (`pack,`: codes
of five bits into the key, characters of eight into the text). The tools
read a name with `name_at` in `insn_map.py`, and the C that reads an image
(`main.c`, `test/main.c`, `util/disasm_hex.c`) with `header_name` in
`vm.h`; those two and `cross.fs` are the three places the codes are said.
Labels are laid out the same, so `see` searches them with `sfind`.

What that cost and what it saved, counted. A name was a count and its
characters, eight bits each. The headers were 4,104 bytes with cells of
16 bits, 5,924 with 32 and 10,092 with 64, and are 3,182, 5,020 and
9,300. A lookup is what it was: between 1,300 instructions for a word
made lately and 4,600 for one of twelve characters that is not there,
ten for each header passed, since what is compared with each is still
one cell. The same seven names looked up before and after are within 2%
either way with cells of 64 bits; with 16, where a cell is three codes,
a name of twelve letters that is not found is 4% more and one of five
digits 3.6% (a digit is two codes). Packing the key is 144 instructions a
call in the profile where it was 419. Showing a name is where it costs:
`name>` is 219 instructions a name, so painting the Words window, which
shows 81, is 108,400 instructions where it was 91,500, a little under a
fifth more, and
0.6% of the desktop's work. It was first written to go through every
code of a cell, the padding too, and to store each character with `c!`:
that was 708 a name and the window 147,900. `words` and `see` in the
Forth window are no different to use: a character there is thousands of
instructions to show.

Packing and reading names made the kernel longer than its ROM, and for a
commit `words` and `name>` were on page 1 for that. They are core words
and are back in it: the room was found in the kernel, which was looked
through for what was written twice and what nothing used (the sizes of
its words are instructions from each one's start to the next's):

- `cmove` had a way of moving whole cells first, for the Forth window's
  text, which is moved with `cells-move` now; nothing else called it
- `lower` and `digit?` worked out what a character is as a digit; the
  table that says what it is in a name says that too (`kcode@`)
- `parse-name` and `parse` ended with the same twelve instructions
  (`parsed`), and started with the same four (`unparsed`)
- the codes of a key and the characters of a name were packed by two
  words that were one (`pack,`, `pack>`)
- `hex1` and `.` each made a digit a character (`.digit`)
- `create` was `constant` written again with the data's address
- `throw` said `error:` a character at a time
- `2/`, `d<` and `du<`, the flag `sfind` gives, the fields `spawn`
  empties, `;` making a call a jump, and `evaluate` and `refill` both
  setting `>in` after the source, were each a few instructions too many

`c@` could have been `h# ff field@`, 16 bytes less, and was tried so: a
lookup was then up to 6% more, since every character read is read with
it, and it is as it was.

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
task that runs past its part of a stack. The kernel's code is 4,014 bytes
with 64-bit cells, 4,050 with 32 and 3,990 with 16, against the 4,096 the
kernel ROM holds: 46 bytes to spare with cells of 32 bits. When it does
not fit, look in it for what is written twice before moving a word out
of it (Addresses, above, says what was found the last time).

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
out of date four at a time. Painting what is out of date costs about ten
thousand instructions each time, however little there is, and that is
half of what dithering a row is. At 1 MIPS the picture is finished at 18.8
seconds with eight rows at a time, 19.6 with four, 20.8 with two and 23.2
with one: so four is a twenty-fifth longer than eight and shows the
picture twice as often, two is a tenth longer and one a quarter. What it
costs is to be measured again when either the dither or the painting
changes: when a row was 67,000 instructions, two at a time was a
thirtieth longer and was the choice. At 1 MIPS the desktop is ready in 0.9
seconds, where it was 34 seconds before anything was drawn.

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
memory from cell `0x6000`. With cells of 32 bits the 4,096 cells that one
instruction reaches end at byte `0x8000`, what is bulk (below) has the
16KB after them, and the headers start at byte `0xc000` and have the 16KB
after that. They started at `0xa000` there too, which left the bulk 8KB,
of which all but 244 bytes were in use: 288 more ran into the headers
by 44. `unbulk` now says so when the bulk has run
into the headers: before, the next thing to walk the headers failed,
with nothing to say why.

That is the layout where memory is more than 64K cells. Where it is 64K
cells, which is the machine with cells of 16 bits, code has to be in the
lower half, since an execution token is 15 bits there, and the same bytes
are laid out so (`hdr-base`, `code-base`, `code-size` in `cross.fs`):

- bytes `0x4000-0x53ff`: data, in the cells that one instruction fetches
  and stores, and the data made at run time after it
- bytes `0x5400-0x5fff`: data that is bulk (below)
- bytes `0x6000-0x7fff`: dictionary headers
- instructions `0x4000-0x7fff`: four code pages, of which the system
  takes two and definitions made at run time have two
- above the display, cells `0xaac0` up: data that is reached only a cell
  at a time (`high`, below), and then what `halloc` gives out

With cells of less than 64 bits the 4,096 cells that one instruction
reaches are fewer bytes than the data is: a font, a table or a buffer of
text laid down before a variable would put the variable out of reach, and
it would cost three instructions to fetch and not one (with cells of 16
bits the decompiler was 900 bytes longer for it, and did not fit its
page). So what is bulk and not variables is between `bulk` and `unbulk`
in the source: the fonts, the key map, the Forth window's text, the block
buffer, the tools' pictures. `cross.fs` lays it down in a place of its
own after the variables: past the 4,096 cells, or on the machine of 64K
cells in the last of them. With cells of 64 bits the 4,096 cells reach
the headers, the two words do nothing, and the image is the same to the
byte. Data made at run time stops where the bulk starts (`DATA-LIMIT`):
on the machine of 64K cells it has 2.4KB, and `allot` does not look, so
a buffer bigger than that, made at the prompt, runs into the bulk and
nothing says so until the next word is defined.

Nothing has to be in the lower half for being text: the dictionary is
searched a cell at a time (`>key`, `samename?`), and a name is read a
cell at a time, wherever it is, to be shown (`name>`). What is there is
there because the words
that read it take the address of a byte (`c@`, `count`, `type`,
`draw-text`), and because there is no room for it above: on the machine
of 64K cells the 21,824 cells above the display are all the memory there
is for windows, the picture, the names and the canvas. But what is read
only a cell at a time need not be below, and the lower half is where
code has to be. So such data is between `high` and `unhigh`, and on that
machine is laid down from where the display ends (`halloc` starts after
it); on any other it is bulk. The table of instructions as words is
there. The fonts are there too, and not as they are written: a font is
written as a strike and drawn from what it is opened out into, and a
machine that opens it out when it is first used has both from then on,
the strike below and the rest above. On the machine of 64K cells
`cross.fs` opens each font out as it lays it down (`font[`, `]font`), and
the strike is not in the image: 3.3KB less below, and no more above than
there was. That, and the table, left little enough bulk to go in the
page the variables have, and the page it had is the headers', and theirs
is a code page: two for definitions made at run time, where there was
one. What it costs: data made at run time has 2.4KB where it had 5.4KB;
the table is 690 cells above the display, where the names tables and a
scroll together now leave 330 cells; and a program that never draws text
has the fonts' 3,800 cells opened out all the same.

In `nuc.fs`, `compile,` emits a one-instruction call for a kernel or
same-page target and `literal xt` + `call execute` for another page.
`newpage?` starts a fresh page when under 1KB is left, and `code,` throws -8
if a definition runs off the end of its page. Execution tokens are stored in
16 bits in dictionary headers, which is what limits code to 64K words.

### Devices, Blocks and Graphics

`forth/system.fs` (included at the end of `nuc.fs`) is cross-compiled into
code page 1 (`0x2000`) and code pages 8 to 11 (`0x10000` up; page 10 is
HexaPaint and `desk.fs`, and page 11 the nucleus's peephole, `join.fs`).
Where memory is 64K cells it is page 1 and pages 4 and 5, the same files
packed into fewer pages, and HexaPaint in the upper half of the kernel's
page, instructions `0x800-0xfff`, which the kernel leaves empty:
`system.fs` says where each page starts on each. The kernel
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
  `0xc0`-`0xc8`, `halloc` for memory above the dictionary, byte-order
  helpers. `halloc` throws -8 if it is asked for more than is left up to
  the end of memory. What it gives is never given back, but for those
  that take memory and put `himem` back when they are done. One run of
  memory can be: whoever is done for good with memory that others were
  given memory after says so (`hfree`), and whoever does not mind where
  its memory is asks for that first (`hreuse`). The viewer gives back
  what a picture was read and dithered in, which it has between its
  record and its bitmap so that it is one run. What is had from the run
  is had from its start, so cells given back that end where the run now
  starts make it whole again: `hgive ( a cells -- )` is for memory that
  is done with, wherever it was had from. If it was the last given out,
  `himem` comes back down, and if not it is the run's. A scroll's three
  regions and HexaPaint's canvas are had and given back so. `himem` is
  always an even cell, so what gives memory back puts it where it was to
  the cell: the names are given back only if `himem` is where they
  ended, and when it was left where an allocation ended, a scroll after
  names of an odd number of cells left it a cell on and they stayed
- `blocks.fs`: block device interface (`'blk-read`, `'blk-write`), `block`
  `update` `flush` `load`. There is one block buffer; `block` gives the
  address of its first byte and `cblock` the cell it starts at, for
  whoever cannot use the address of a byte. Whoever reads more of the
  disk than a block reads whole blocks, by their numbers, to where they
  are wanted (`blk-read`: the driver is handed that place, so nothing is
  copied and the buffer is left alone): the picture, the tables of names
  and HexaPaint's canvas do. There was a word that copied any run of
  bytes of the device to the address of a byte (`disk-read`); nothing
  called it once they did, and it is gone
- `vmdisk.fs`: the driver for the reference VM's disk. The VM can be
  given a file to be one, as its second argument (`hexaforth IMAGE.hex
  DISK.img`), and it is reached through ports as any device is: port
  `0xb0` holds a block's number and reads as how many blocks there are, a
  cell's address written to `0xb1` has the block read into the 1,024
  bytes from it, and to `0xb2` has it written from them. On a machine
  without one the port reads zero and `vmdisk-init` throws -37
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
  `atkinson ( bitmap img -- )` makes the bitmap; `copy-bits`
  shows it. An image is a record (`tiff-open` and `image-is` make one), a
  dither is a record (`dither-start`), and a viewer is a record with its
  task's record at the end of it, so there may be more than one of each:
  two pictures can load at once, each task taking a row of its own. `img`,
  `dth` and `vw` are the one in hand, and the old variables are words that
  give a cell of it; one is in hand only for as long as the word that took
  it up runs, so every word others use takes its record (`tiff-row`,
  `dither-row`). A viewer's task finds its viewer from where its own
  record is, and a window has a cell for whoever draws it (`win-data`).
  Each viewer's task wants a part of the stacks, and there are two above
  what the desktop reaches, so a third picture waits. `dither_test.py`
  dithers two images a row of each in turn. A picture is dithered a row
  at a time, by something that has other things to do between the rows:
  `dither-start`, and `dither-row` for each row; `atkinson` is the whole
  of it.
  The image is never all in memory, and may be bigger than memory is: an
  image keeps a window, a row and a block long, and `dither-row` asks its
  image for the next row (`tiff-row`), which reads blocks into the window,
  where the last one ended, until the row is all there, first moving the
  cells that have not been taken to the front. A block is read only if a
  row that is wanted has bytes in it. A strip may be anywhere in the file,
  and at the first row of one the window starts again from the block the
  strip starts in (`next-strip`); where the strips are is read from the
  directory then, through the block buffer. Nothing in the reader is
  reached by the address of a byte, and no place in the file is a count of
  bytes, since a file may be longer than a cell can count: a place is a
  block and a byte in it, a number of 32 bits is its two halves, and a
  byte of the directory is shifted out of the cell of the buffer it is in
  (`t8@`). The mandrill is 786KB and the window for it 2.6KB; a machine
  with cells of 16 bits, which has 128KB, dithers a picture of 525KB
  (`test/tiff_test.py`). Reading is 310 instructions a row of the
  mandrill, of the 20,000 the row takes, a third of them moving the cells
  not yet taken to the front of the window: about 100,000 instructions a
  picture more than reading it all into memory first was.
  The bitmap need not be of the whole image either. Every pixel is
  dithered, since what a pixel is depends on those before it, but
  `dither-part ( bitmap img x y w h -- dither )` keeps only the rows and
  the cells of a part (`dither-start` is the whole).
  Nor need it be at the image's size: `dither-half` takes the same and
  makes a bitmap of the image at half its size. A pixel of it is four of
  the image, two of each of two rows, and its gray is theirs added and
  quartered to the nearest. So a row that is dithered is two rows read,
  each added into a row of lanes of its own, two pixels to a lane
  (`apairs`); then a quarter of each lane, which is a shift and a mask of
  the whole cell, is added to the row the dither is owed in
  (`aquarters`). They are added apart because the row that is dithered
  already holds what the rows above gave it, which is not to be
  quartered. Where the image gives its grays a cell at a time it gives
  them so too (`'pairs`, `gray-pairs`): four grays in four lanes are
  folded, the cell added to itself a lane down and the two sums brought
  together, so eight pixels are one cell of sums.
  A viewer whose window is smaller than its picture shows it at half its
  size, and if that is still more than the window shows, keeps the bitmap
  of what it does show, the middle, and does not read the rows below it.
  The Mandrill's bitmap is 32KB, which the machine with cells of 16 bits
  has not to spare; at half it is the 256 by 256 that its window shows,
  all of the picture, in 8KB. Read and dithered so, on the reference VM,
  the Mandrill is 52.5 million instructions with cells of 16 bits, 28.7
  with 32 and 10.6 with 64, against 71.4, 41.5 and 16.7 for all of it at
  its own size; the middle 256 by 256 at its own size, which is what that
  window showed before, was 51.1, 29.9 and 12.0
- `font.fs`, `keymap.fs`: generated by `util/mkfont.py` (the two fonts;
  Linux key codes to ASCII, 256 characters packed into cells, which
  `b8,` lays down eight at a time whatever the cell is: the 128 keys and
  then the 128 shifted, a key found with `char@`, by its cell and how
  far along it is; `mkfont.py --keymap` makes it alone, which wants
  neither font installed); `font.fs` is on page 1
- `virtio-input.fs`: keyboard and tablet events, delivered to `'event`
- `gfx.fs`, `wm.fs`: rectangles in plain white and black; windows,
  pointer, event loop. Nothing draws when something changes: it is added
  to the region that is out of date (`inval-rect`, `win-inval`), and the
  event loop paints that (`refresh`), each window only where it can be
  seen, in its own port. What a window says has changed (`win-inval`) is
  out of date only where the window can be seen: where another lies over
  it there is nothing of it to paint, and the one on top is not painted
  again for it. Keys and pointer events come from whatever the machine
  has: each time round, the event loop runs the word in `'poll`, which a
  driver stores there, and the driver hands what it has to `on-event`.
  With none stored it is `uart-poll`, the console's keys.
  The title in the bar, `hexaforth`, is a menu of the windows, the one in
  front first. The button going down on the title brings it down; going
  up on a line of it brings that window to the front, and going up on the
  title leaves the menu down, for a press on a line to pick or a press
  anywhere else to take away, so it works held and dragged or with two
  clicks; a key takes it away too. The line the pointer is on is shown
  white on black, as the title is while the menu is down. While it is
  down it is counted with what covers the windows (`visible`), so that
  none of them paints or scrolls where it is, and it is painted after
  them where it is out of date (`draw-menu`). `menu-open` says whether it
  is down. It is as wide as the longest title (`text-width`, which until
  the menu used it added each character's width to the wrong cell of the
  stack, and was never run).
  A window is told of the button going down in it (its click routine).
  One that wants to follow the pointer after that stores a word in
  `'track` then: it is run with where the pointer is each time the
  pointer has moved while the button is down, and once more when the
  button goes up, when `held` is no longer true. A device says where the
  pointer is an axis at a time and then that it has said all there is (an
  event of type 0), and it is then that the word is run, so that it is
  never given a place the pointer was not at
- `term.fs`, `edit.fs`, `view.fs`, `calc.fs`: the Forth window, block
  editor, image viewer and RPN calculator
- `paint.fs`: HexaPaint, a window to draw in with the pointer, laid out
  as paint programs of its kind are: the tools down the left, two to a
  row (pencil and brush, eraser and line, rectangle and filled rectangle,
  oval and filled oval), the patterns along the bottom with the one in
  hand at their left, and the rest the canvas. The canvas is a bitmap of
  its own, as the display is, 384 by 256 or on a small screen 240 by 152,
  and the window shows it with `copy-bits`, so the drawing is there
  whatever has been over the window. Everything that draws is a run of
  pixels of a row in the pattern (`span`), a cell at a time under a mask
  of the pixels that are the run's; a dot of the pen is a run for each of
  its rows, a line is a dot at each place along it (`pline`), and a
  rectangle or an oval is runs. An oval is as wide at a row as its half
  width times a root over its half height, and the root is not worked
  out: it is no more than it was for the row before, and comes down a
  step at a time (`xe`). The pencil, the brush and the eraser draw as the
  pointer goes. A line, a rectangle or an oval is pulled out, shown in
  outline as part of what the window draws, by the same words with the
  run going to the screen (`'run`, `prun`), and drawn in the canvas when
  the button goes up. The tools' pictures are `icons.fs`, generated by
  `util/mkicons.py`, where they are drawn in text. It is 2.4KB of code.
  Where there are pages to spare it has a page with `desk.fs`; where
  there are not, on the machine of 64K cells, it is in the upper half of
  the kernel's page, which the kernel, a ROM of 4KB, leaves empty: 4KB of
  code that any page calls with one instruction, of which it uses 2.4KB.
  Where there is a disk the canvas's place is the disk, from block 1152,
  and it is in memory only while HexaPaint is the window in front
  (`paint-front`, a word given to `on-front`). It is read in when the
  window comes to the front (`canvas`); when another does it is written
  back, if it was drawn in, and its memory is given up (`page-out`,
  `hgive`). Behind another window it is painted from the disk
  (`cv-show`): a block is a bitmap of as many whole rows as a block
  holds, shown with `copy-bits`, and only the blocks that have rows in
  what is to be painted are read. A block is read to memory that is taken
  for as long as it is wanted (`with-blk`), not through the block buffer,
  so the block being edited stays where it is. The first block says the
  canvas is there: a mark, its size, and what one pixel is in a cell, so
  a disk written by a machine with another cell or another display has
  no canvas for this one, and a new disk has none; then the canvas is
  not asked for until it is drawn in, as with no disk. If the disk cannot
  be read or written there (`baremetal/mkdisk.py` makes it long enough)
  the canvas stays in memory. So a drawing is there the next time the
  machine is started. With the block that says so the canvas is 14
  blocks with cells of 64 bits or 32 and 6 with 16, which on a disk held
  to a floppy's speed (`DISK=40`) is a third of a second to come to the
  front, and again to leave it after drawing
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
- `desk.fs`: `windows`, which opens the desktop's windows and runs the
  event loop, the picture and the editor being there if a driver has said
  how a block is read; `desktop`, which first brings up the QEMU `virt`
  machine's devices; and `cold`, which the kernel runs at boot. Where
  the windows go is a table (`place`), and there are two: for a screen of
  768 by 576, and for a smaller one down to 512 by 342, where the windows
  are as big but for the picture's, lie over each other more, and each
  has some of its title to be seen
- when all of that is compiled, `name-tables` in `cross.fs` makes two
  tables of the names for the decompiler, the dictionary's words and the
  labels: by execution token, and those that are a number (a word that
  only pushes one, a label for data) by the number. A table is how many
  entries it has, then their keys in order, so `tfind` in `see.fs` finds
  one by halving, and then in the same order where the name of each is (a
  header's address, or for a label how far it is into the labels, with
  the top bit to say so). A key and a name are a cell each, and the cells
  are the machine's, so the tables are the same for a cell of any size:
  an entry was once the two halves of one cell, which a cell of 16 bits
  is too small for.
  The tables and the labels are not in the image: they are
  the file `build/nuc.tables`, which `mkdisk.py` puts on the disk
  from block 1024: 37.3KB with cells of 64 bits, 18.9KB with 32 and
  10.7KB with 16, more than half of it the labels (it was 37.8, 20.4 and
  12.4 with a label's name eight bits a character).
  `names-load` reads it into memory from `halloc`, whole blocks to where
  they are wanted and what is less than a block through the block
  buffer, so that it has the memory it is long and not a block's worth
  more, and `names-unload`
  gives the memory back, if nothing has taken more since. The Words
  window has them in while it is the window in front (a word given to
  `on-front` in `wm.fs` is run when the front window changes, and there
  is room for three such; windows do not close) and
  whenever it is drawn: it takes them in to be drawn if they are not in,
  and gives them back when it has been drawn and is not the one in front.
  So what it shows is the same wherever it is, a word with no header by
  its name, and it is not left to go through the dictionary for every
  name when it is painted behind another window: that made leaving it
  1,147,000 instructions, and it is 666,000. `see` has them in
  for as long as it runs. A stamp in the file must be the image's.
  Without them, or with no disk (the reference VM has none), the
  decompiler searches the dictionary a word at a time and has no labels

`cross.fs` also keeps the name of every
definition that has no header: a label, laid out as a header is, in the
machine's cells. A label for code holds
its execution token, and one for `create`d data holds the data's address
with the top bit set. The labels are for the decompiler and the tools, so
they are not in the image: they are in the file of names beside it
(`build/nuc.tables`, after the two tables), which goes on the disk. While
the decompiler has that file in memory, `labels` heads their chain, a call
to a headerless word and a headerless variable are shown by name, and
`see` accepts a label's name; without it they are shown as numbers. The
tools that want an image's labels (`insn_map.py`, `deskprof.py`,
`loops.py`, `shorter.py`) read the file with the image's name
(`labels_of`), and read only an image with cells of 64 bits; a copy of an
image needs a copy of its `.tables`, and
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

`make demo-time MIPS=1` boots the desktop on such a kernel, leaves it
alone, and says when things happen, in seconds from when the machine was
started (`baremetal/desktime.py`): when the console says the desktop is
up, when the Forth window has its prompt on the screen, and when the
first row of the picture, half its rows and all of them are made. At 1
MIPS those are 0.1, 0.9, 0.9, 10.3 and 19.6 seconds. It is what to measure
before and after a change to how the picture is read, dithered or shown:
how many rows are put out of date at a time was chosen with it. The prompt
is looked for on the screen; the rows made are read out of the machine's
memory, where `vw` in `view.fs` is the viewer in hand and the third cell
of a viewer's record is the rows that are made (`bmh`), so the script
changes if they do, and says so if the image has no `vw`.

`make demo DISK=40` (and `make demo-time DISK=40`) holds the disk to that
many thousand bytes a second, read and written, as a floppy did: about 40
for an 8-inch one at double density, 23 for a 5 1/4-inch one, and half of
each at single density. It is QEMU that holds the drive to it
(`throttling.bps-read` and `-write` on the drive), so nothing is built
differently and nothing in the Forth knows. It is the rate and no more: no
head moves, nothing spins up and nothing has to come round, so a real
drive was somewhat slower. The block driver waits for a block without
pausing, so the desktop stops for the 25 ms a block takes at that rate, as
a machine that moved each byte itself did. With `MIPS=1` the picture
wants 41 thousand bytes a second, so an 8-inch disk just keeps up (the
picture is finished at 20.0 seconds, 19.7 with no limit) and a 5 1/4-inch
one is what is waited for (34.2). The tests and the profile never take it.

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

What is checked is one script, `test/deskcheck.py`, run on a machine that
is given to it as something that can type, click, drag and show its
screen. `baremetal/guitest.py` is QEMU and the AArch64 kernel.
`test/desktop_test.py VM IMAGE.hex OUTDIR DISK.img [CELL_BITS [W H BASE
[TABLES]]]` is the reference VM, with cells of 64 bits, 32 or 16. Its
disk is the VM's own (`vmdisk.fs`), a copy of an image that
`baremetal/mkdisk.py` made as it makes QEMU's: block 1 for the editor,
the Mandrill, and the tables of names that go with the build. So the
picture that is checked is the Mandrill, against a dither of the same
file done on the host, every pixel that the window shows: all of it at
its own size on the screen of 768 by 576, and all of it at half its size,
256 by 256, on the machine with cells of 16 bits, whose 128KB the file is
six times; the host halves it as the machine does. The VM
has no keyboard or pointer, so the test is those: a word in `'poll`,
typed at the prompt before `windows` is run, that takes one thing from
the console each time round (a key going down or up by its Linux code,
the pointer's place, its button, nothing, or to print the screen) and
hands events to `on-event` as a driver does. The VM waits for the console
when asked for a key, so the desktop takes one turn of its loop for each
thing sent and the test is the same every time; the screen is the display
in the VM's memory. The checks do not say where a window is: they are
told how the desktop is laid out, keep track of what is in front, and
find a part of a title bar that can be seen. `test/sizes.sh` makes the
disks and runs it at each size, and keeps the screens it looked at in
`build/sizes/desktop-64`, `-32` and `-16`.

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
  taken and given back (`hreuse`, `hgive`: from the run of memory that
  has been given back, if that is big enough, and if not from what has
  not been given out), with one place on the return stack
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
type at the reference VM's prompt: the nucleus, the desktop
(`test/desktop_test.py`, above: 46 checks, the Mandrill, the menu and
HexaPaint among them; what is drawn in HexaPaint is drawn too in a model
of its canvas in `test/deskcheck.py`, and every pixel compared), the
drawing words against their model, regions a cell at a time, the dither
against a dither a pixel at a time (`test/dither_test.py`, which makes
images in memory with `image-is`, and bitmaps of parts of them), and the
TIFF reader (`test/tiff_test.py`). That last wants files that no disk
image has, so it gives the VM a disk of its own at the prompt: a word in
`'blk-read` that makes a block's bytes from the block's number, and lays
over them the bytes of a file's header and directory where the block has
any. So a disk of any size is had with nothing kept, and a file's pixels
are whatever the disk holds where its strips say they are: files of
either byte order, of one, two and three samples, with strips on blocks
and cells and not, apart and out of order, and one of 525KB.
Run it after changing anything that draws: nothing is cell-
size safe that it has not run. The first time the drawing was built at 32
and 16 bits it found text wrong at both, since a font said how long a row
of its strike is in cells of 64 bits; fonts now say it in runs of 64
pixels (`px-cells,`).

A machine that has a display (`-DDISP_W=...` and the rest, through
`EXTRA` to `build-machine.sh`) has all of `system.fs`, whatever its cell:
the desktop is the same files with cells of 64 bits, 32 and 16, but for
the drivers of QEMU's devices (`virtio*.fs`) and what brings them up
(`desktop`, `cold`), which are for the machine the AArch64 kernels are,
and those are written for 64 bits. With cells of 32 bits its bytes are
laid out as the big machine's are, code pages from page 8. What had to
change for it: a table of bytes
(`keymap.fs`) was laid down as cells of 64 bits and is now laid down as
bytes (`b8,`); the decompiler kept where it drew each instruction in a
cell of more than 32 bits, and a place is now 16 bits and the instruction
a cell of its own; and labels and the tables of names were written in
cells of 64 bits. The table of instructions as words (`fused.fs`) is made
from the image of 64 bits and serves the other: of the 157 ALU
instructions that image uses, the one it has not is the one the big
image has not either.

With cells of 16 bits memory is 65,536 cells, 128KB, of which a display
of 512 by 342 is 21KB, and the desktop runs in that. What it took:

- memory is laid out for it (Code Paging, above): an execution token is
  15 bits, since the top bit of its cell in a header is the immediate
  flag, and the address of a byte cannot say the upper half of memory, so
  code, data and headers are all in the lower half, the display and what
  `halloc` gives out in the upper. `halloc` counts in cells and gives the
  address of a cell, and nothing it gives is reached by a byte's address
- the block buffer, which `block` gives the address of a byte of, is in
  the data and not from `halloc`
- a picture is not read into memory, and a viewer whose window is smaller
  than its picture shows it at half its size, and keeps the bitmap of
  what its window shows (`tiff.fs`, `dither-half`, `dither-part`,
  `view.fs`)
- bulk data is kept out of the cells that variables want (`bulk`)
- where the pointer is, in 32,768ths of the screen, times the screen's
  size is more than a cell holds, and is multiplied into two (`scaled`
  in `wm.fs`)
- the windows are laid out for the smaller screen (`place` in `desk.fs`)
- the decompiler is told where code and data are by the layout, not by
  numbers that were the big machine's (`code?`, `xt?`)
- the tables of names are read a block at a time into cells, with no
  count of bytes on the disk and no address of a byte in memory; and a
  label's name, which is in that memory, above where the address of a
  byte reaches, is read a cell at a time to be shown, as any name is
  (`name>`)

Of the ALU instructions its image uses, the table of instructions as
words lacks three, which are shown as numbers. With the desktop up,
the picture loaded and every window drawn, 15,346 of the 21,824 cells
above the display have been given out (`0 halloc .` in the Forth window
says where the next would be, and `test/deskcheck.py` notes it with the
names out and in): 4,490 that the image has there, the two fonts opened
out and the table of instructions as words, the picture's bitmap of
4,096, fourteen regions of 258, and the rest; 2,350 of them are the run
that the picture was read and dithered in, which the viewer gives back
when the picture is made (`hfree`). That leaves 6,478. The names are
5,356 of those for as long as the Words window is in front, is being
drawn, or `see` runs, which leaves 1,122; a scroll, which wants 774 for
as long as it takes, has them from the run that was given back
(`hreuse`, `hgive`), and `see` of a word longer than the window, and
that the names are given back after it, are two of the desktop's checks
at every size. The names were 5,376 cells when
this was first written, and whole blocks of memory were taken for them,
which was 5,632; HexaPaint's labels made that 6,144, which left less
than a scroll wants and nothing looked; and the day they were 30 cells
more than twelve blocks they wanted a thirteenth and did not load. They
now have the memory they are long, and were 6,180 cells of it until a
name was five bits a character. A label is about four cells there, one
of them its link, which only a search by name follows: some 720 cells to
be had, if the names do not fit again.
HexaPaint's canvas there is 2,280 cells, and is in memory only while
HexaPaint is in front, in that same run (`hreuse`): the names are not in
memory then but for as long as the Words window is drawn behind it,
which is at the top. Brought to the front before the picture is made,
the canvas is new memory, which it gives up when another window comes to
the front. A
shift count is taken modulo 64, so a shift by
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