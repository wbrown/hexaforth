# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

It is rules, and where things are. It is not where a size, a count, a
profile figure or the story of a change goes: the build prints the first
three, and the last is a commit message. A figure written here is a fact
said a second time, which someone then has to keep in step.

## C and Forth, and nothing else

This project is C and Forth. Where this branch began it was nothing
else: the tests were C (`test/main.c`, an array of `hexaforth_test`, and
the small Forth compiler in `test/compiler.c` that reads the same
instruction definitions the VM does), the words were generated from
`vm_opcodes.h` by `util/baseword_fs_gen.c`, and `cross.fs` built the
image. One definition, said once, and everything else made from it.

- Do not write Python. Not a test, not a generator, not a tool, not a
  model of the machine. None is to be added to this repository
- The Python that is here is to be removed, all of it: `util/*.py`,
  `test/*.py`, `baremetal/*.py`. The generators go first. Until a file is
  gone it is not somewhere to add to and not a pattern to follow, and what
  this document says of those tools is of what is being taken out
- What replaces it is not Python written in C. Read `test/main.c`,
  `test/compiler.c`, `test/vm_test.c` and `util/baseword_fs_gen.c` before
  writing any, and write as they are written. What is Forth's to do is
  done in Forth: `cross.fs` already compiles and joins the image
- A fact is said once. An encoding, a layout or a table is in one place,
  and whatever else needs it is generated from that place or reads it
  there. If a change has to say the same thing again in another file or
  another language, the change is wrong. That goes for a number too: an
  address or a size is a name from where it is defined, not the number
  written out again
- Contrastive testing is good, and it is not that. A source of truth is
  the place a fact is defined, which everything that needs the fact is
  built from: two of them can disagree, nothing says which is right, and
  the build depends on someone keeping them in step. A contrastive test
  is a second thing, made independently, that nothing is built from: it
  is there only to be compared with the first, and when the two disagree
  that is what it was for, one of them is wrong and it is already
  settled which has the say. `test/main.c` compiles each test with the C
  compiler and with `cross.fs` and compares the instructions; the two
  kernels and the Verilog are run against `vm.c`, which is the machine.
  To tell which a second thing is: take it away, and see whether
  something stops building or only a check goes. A model of the machine
  that the peephole's tables were made from was a second source of
  truth. The same model, used only to check tables made from `vm.c`,
  would have been a contrastive test
- Do not ask whether C and Forth can do a thing. They did all of it
  before there was anything else here. "I do not know how much of this
  they can absorb; I have not measured it" was said of exactly this, and
  it was not a question of measurement: it was a way of keeping what
  should not have been written. Measuring is for what a change costs, not
  for whether the project's own languages are enough
- A test that finds nothing to test is to fail, not pass. Look at how
  many things a test says it compared
- One thing here is neither: `bench/m68k/` is 68000 assembly. It is not a
  tool and nothing is built from it. It is another machine's work, there
  to be timed against this one's, and it can only be said in that
  machine's language

## A tool's flaw is not the product's limit

What is written here to make the product or to check it can be wrong, and
can fall short. That is a bug in it.

- It is fixed where it is. No rule is added to keep away from what a tool
  gets wrong, and nothing the product had is given up because a tool
  cannot do it or cannot confirm it. "It declines these" is not a cost of
  a change: it is the change not being finished
- What replaces a thing does all that the thing did before the thing is
  taken out. What the product had, and the machine ran, is where a new way
  of making it starts from: put the new beside the old and ask each of
  everything the old one said, before the old one goes
- What an instruction or a run of them does is found by running it on
  `vm.c`, not by reading its fields. A belief about what the machine does,
  however sure, is run before anything is built on it or taken away for
  it. If a tool and a belief disagree, neither has the say until the
  machine has been asked
- A state that something is tested from is one a program can be in. A
  return goes to a return address, and a fetch through a cell finds an
  address there. What the machine does from a state no program reaches
  says nothing of the machine or of the product, and is no reason to leave
  anything out
- Being too careful is a defect like any other. To leave out what is right
  because it could not be checked is to put the tool before the product.
  Say what could not be checked and why, and fix that
- When what was done is questioned, stop and look at the premise. Do not
  go on building on it, and do not answer by showing that some other thing
  is so

## A question is the objection

- A question about what is being done ("is this a shortcut?", "why are we
  doing this?", "what precisely are we checking?", "why are we not done?")
  is not asked for information, and it is not leave to go on. It means
  that what is being done looks wrong to the one asking, and it usually is
- When one is asked, stop. Run nothing and change nothing. Answer what was
  asked and only that: yes or no first, then what was done and why
- The answer is not a plan. What to do about it is not said in the same
  breath and is not begun. It is decided after the answer has been heard,
  by the one who asked
- "Stop" is the same and more so. Nothing is run, changed, committed or
  pushed until the word to go on, whatever else is asking for it

## The machine model

`MODEL.md` says what the machine is: the stacks, memory and display that
`vm.c`, the AArch64 kernels and the Verilog must all provide in the same
way. Read it before changing any of the three, and keep them to it.
`UNDERSTANDING.md` says why the machine is as it is: read it first.

## The instruction set is the whole encoding

An instruction is a control word: where the ALU's input comes from, the
operation, where the result goes, how each stack moves, and whether to
return. Every combination is an instruction that `vm.c`, the kernels and
the Verilog already execute. There are no opcodes, and a baseword is only
a name for one of them.

- Do not judge an encoding, or a sequence that one encoding equals, by
  whether it looks useful. Measure where it occurs in the image and how
  often it runs. A site that is rare in the source may be in the hottest
  loop
- A name costs a line in `vm_opcodes.h` and nothing in the machine. An
  encoding with no name is one the programmer cannot say and the nucleus's
  compiler cannot emit
- Any control word can be said in source by its fields, and the peephole
  joins it to its neighbours as it does a named one:
  `meta N->IN [IN] ->[T] d-1 alu target`
- Most instructions are said by a phrase of standard words that the
  peephole joins. A baseword's name must mean what the standard says: the
  shift right by one is `u2/`, and `2/`, which keeps the sign, is a word
  of the nucleus
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
  and a second place is the first plus how far apart they are. A loop
  that only counts starts below nothing and goes up to it:
  `negate begin dup while ... 1+ repeat drop`. What is the same every turn
  is kept on the return stack, where it is an input of the ALU. The kernel
  has `cells-move ( from to n )` and `cells-fill ( x a n )`: use them
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

## Writing Forth here

- An address is the number of a cell, and `@` and `!` fetch and store one
  cell. A branch, a call and an execution token are the number of an
  instruction (`i@`, `i!`). Nothing in the machine counts bytes
- Little needs the address of a byte, and before anything is given one it
  is asked whether it does. Characters stay packed into cells and are said
  by a cell and how far along they are (`char@`, `draw-chars`,
  `chars-width`, `ctext`). What takes the address of a byte is what stores
  a character (`c!`), `type`, `count`, and what the interpreter reads
- A word keeps what it is working on on the stacks: the top of the data
  stack, the cell under it and the top of the return stack are to hand,
  and a word that needs more is made of smaller words. A word that keeps
  it in variables can be in use only once. State that many words read (the
  port) is rightly variables; what one word stores for the next to fetch
  is not. Where a word needs a great many values (`copy-bits`,
  `draw-text`), see how those two set theirs aside when run while busy
- A variable is in the cells one instruction reaches, from `VAR_BASE`.
  Data that is not variables (a font, a table, a buffer) is between
  `bulk` and `unbulk`, or `high` and `unhigh` if it is reached only a cell
  at a time, so that it does not push variables out of reach
- A word that another file uses has a header (`header name : name`), so
  that it can be typed at the prompt. A word used only in its own file has
  none, and `cross.fs` keeps its name as a label
- A small word is compiled in place of a call to it only where what is
  left is no longer than the call. Where a place is run so often that the
  call and its return cost, `in-place` before the call keeps the word
  there (`in-place -`). It is said after measuring how often the place is
  run and what the work costs with it and without, and not before
- A name has one case, on the machine and in `cross.fs`: `LINE` and
  `line` are one name. A macro (`::`) and a word are looked up in the
  same place, so a word that took a macro's name would take it from every
  file after: `cross.fs` stops at the file that does. A baseword is the
  one macro a word may be named for, since that is how it is given a body
- Nothing draws when something changes: it says what is out of date
  (`inval-rect`, `win-inval`) and the event loop paints it. Everything
  that draws takes coordinates in the port, and nothing outside
  `display.fs` knows how the display is laid out
- A table is written where it is said, as source that can be read: a font
  is its strike a row at a time, the key map is rows of keys, an icon is
  rows of `#` and `.`. It is not generated from somewhere else
- The kernel's code is to fit in the kernel ROM (`KROM_BYTES`) at every
  cell size; the VM says how long it is when it loads an image (`CODE=`).
  When it does not fit, look in it for what is written twice before moving
  a word out of it
- A baseword of two instructions is in line wherever it is written. In
  the kernel one can be made `callable` instead: a call wherever the
  peephole does not join it to its neighbours. The top of `nuc.fs` says
  which are and which are not, and why. One that words run all the time
  are made of is not, and which is which is measured
  (`bench/bench.sh draw`), not judged
- A shift count is taken modulo 64 whatever the cell, so a shift by the
  width of a cell of 16 or 32 bits gives zero, and a shift by 64 shifts
  by nothing

## Measuring

- Before saying what a change will save, and before choosing what to
  change, measure. A count of how often a thing is done, times what one is
  thought to cost, leaves out the loop that does them and whatever is rare
  and dear
- Runtime is cycles, not instructions: a memory read, a shift, a multiply
  and a jump each take more than one. `make -C rtl bench` says cycles
- `sh bench/bench.sh` is the benchmarks: a piece of work is a file typed
  at the prompt, the same file on the Verilog CPU of every size, and what
  it made is checked before a figure is printed for it. It says at its top
  what there is. What a change costs is what it prints before the change
  and after; no figure is kept in the repository
- `sh bench/peephole.sh` is what the nucleus's compiler is worth: what it
  makes of the system's code as typed beside what `cross.fs` makes, by
  source file, and what each of its rules saves in the code and when the
  desktop and the drawing are run. A rule is kept or cut by what that
  prints, and a change to `join.fs` is measured by it before and after. A
  rule that is rare in the source may be in the hottest loop: how often it
  is used does not say what it saves
- A thing the machine is compared with does the same work and is checked
  against the same model: the 68000 in `bench/m68k/` runs the machine's
  own dither, written for it, and makes the same bitmap
- Compare two profiles by word, not by their totals: how long the disk
  driver waits is the host's doing
- Do not give an estimate as a fact, and do not leave a thing untested so
  as not to find out

## Building

The C is built with CMake (`CMakeLists.txt`): `hexaforth` (the reference
VM), `hexaforth-debug` (with a trace of every instruction),
`hexaforth_test`, `basewords-fs-gen`, `generate-test-cases`, and the
target `tests`, which runs `hexaforth_test`.

An image is built by `gforth` from the Forth:

```bash
cd forth
gforth ./cross.fs ./basewords.fs ./nuc.fs      # build/nuc.hex and what goes with it
PEEPHOLE=off gforth ./cross.fs ./basewords.fs ./nuc.fs   # the same with the peephole off
```

`util/build-machine.sh CELL_BITS [ADDR_BITS [WIDTH HEIGHT BASE]]` builds
the generator, the nucleus, the reference VM and the tests that have that
VM in them for a machine of another size, into `build/`, with a display if
one is given; `EXTRA` carries more definitions to it (`-DSTRICT`).
`util/machines` is the machines the system is tried on, which
`test/sizes.sh` and `bench/bench.sh` read.
`./hexaforth IMAGE.hex [DISK.img [MEMORY.out]]` runs an image, and writes
its memory when it has stopped if it is told where.

Beside an image `cross.fs` writes `.lst` (symbols), `.sizes` (code and
data by file), `.inline` (the calls it compiled in place, which a build
with the peephole off reads, so build with it on first) and `.tables`
(the names for the decompiler, which go on the disk).

`cross.fs` asks the machine which instruction does the work of two or
three, and what shorter run does the work of a run that has numbers or
variables in it: `util/fuse.c`, which has `vm.c` in it, is built by gforth
each time an image is and called through gforth's C interface. gforth
builds C with `libtool`, which has to be installed. An answer is found by
the machine's step on terms and then run on `vm.c`, and the build stops if
the two do not agree. What was found and run is kept beside the images
(`build/fuse-*.kept`, named for the text of `fuse.c` and of the machine,
so that a change to either starts another), so the first image built
after such a change takes longer than the rest. Where gforth is
Homebrew's, the compiler is told where Homebrew's headers are:
`CMakeLists.txt` and `build-machine.sh` do, and by hand it is
`CPATH="$(brew --prefix)/include" gforth ...`.

Generated, and checked in: `forth/basewords.fs` (by `basewords-fs-gen`,
from `vm_constants.h` and `vm_opcodes.h`); `forth/folds.fs` (what the
nucleus's peephole is told of numbers, by `util/joins.c`, which has
`fuse.c` in it and reads no image: `gcc -O2 -w -fcommon -o build/joins
util/joins.c -lm && build/joins forth/folds.fs`); and `forth/fused.fs`
(the decompiler's table, made from a built image: build until it stops
changing), which is still made by `util/insn_map.py`.

`forth/joins.fs`, the rules of the nucleus's peephole, is not generated:
it is the rules that are kept, and says at its top which and why. Do not
give `insn_map.py --fused` a third file, or it writes all of its rules
over them. The nucleus counts each rule's uses; `sh test/hits.sh` works
the desktop and prints the counts.

## Testing

After a change, run what covers it, and after anything in `cross.fs`,
`nuc.fs` or the machine, all of it.

- `hexaforth_test`: each test of `test/tests.h` compiled by the C compiler
  and by `cross.fs` and the instructions compared, then run on the VM
- `sh test/sizes.sh [SEEDS]`: the machine with cells of 64, 32 and 16
  bits, each with a display and `STRICT`: the prompt, comments and a
  block read as source, the desktop, the drawing words, regions, the
  dither and the TIFF reader. Nothing is cell-size safe that it has not
  run
- `make -C baremetal test` (and `KERNEL=hexaforth-fast`): each AArch64
  kernel against `vm.c`
- `make -C baremetal demo-test` (and `STRICT=1`, `KERNEL=`): the desktop
  on QEMU, driven and its screen checked
- `make -C rtl test W=64` (32, 16; `LAT=2`; `KROM=1`): the Verilog against
  `vm.c`; `test-mem`, `bench`, `synth` are there too
- `python3 util/insn_map.py --compare plain.hex optimised.hex`: after any
  change to a compiler or a peephole, every straight-line run of the image
  must behave as the build with the peephole off does. If the two have the
  same number of instructions, the plain build did not finish
- To check that nothing stores into the kernel or leaves memory or a
  stack: the VM built with `-DSTRICT` and `-DROM_BYTES` the size of the
  kernel ROM, or the small kernel with `STRICT=1`

`baremetal/Makefile` says in its first lines what else it does: `demo`,
`demo-profile` (where the desktop's instructions go; it leaves
`build/desktop.counts`), `demo-time`, and `MIPS=`, `DISK=`, `SCALE=`.
The reports made from a profile are `util/survey.py` (the whole encoding
against it), `loops.py`, `paths.py` (each word charged with all it
calls), `shorter.py`, `vars.py` and `phrases.py`; each says at its top
what it is given and writes to `build/`.

## Where things are

The machine, in C:

- `vm_constants.h`: the machine's sizes, the display, the kernel ROM's
  size, and what is where on the disk. `vm_instruction.h`: the fields of
  an instruction. `vm_opcodes.h`: the fields' names, the basewords
  (`FORTH_OPS`), and the codes of a name (`NAME_CODES_REPR`)
- `vm.c`: the machine. `vm_step.h`: what a literal or an instruction of
  the ALU does, said once: `vm.c` runs it on cells, and `util/fuse.c` on
  terms. `main.c`: loads an image and runs it.
  `vm_debug.c`: the trace. `vm_opcodes.c`: makes instructions of the
  definitions, and reads an image's dictionary (`read_header`)
- `util/`: `baseword_fs_gen.c` (writes `basewords.fs`), `disasm_hex.c`,
  `hex2bin.c` and `memimage.c` (an image as bytes for the kernels, and as
  what the Verilog loads; `image.c` reads one), `mkdisk.c` (a disk),
  `fuse.c` (which instruction does the work of others: found by the
  machine's step on terms, and each answer then run on `vm.c`; what it
  trusts and why is said at its top)
- `test/`: `tests.h` (the tests), `compiler.c` (the small Forth compiler),
  `vm_test.c`, `main.c`, `generate_test_cases.c`; `nucleus.c` (the
  nucleus on `vm.c`, typed at its prompt, and what that left on its stack
  and in its memory: a test that does that is built with it);
  `dither_test.c` (the dither against a dither a pixel at a time, read
  out of the machine's memory); `dither.c` (that dither a pixel at a
  time, which the test and the benchmarks share); `source_test.c` and
  `source.fs` (comments, at the prompt and in a block of a disk that
  `mkdisk` made of that file); `prompt_test.c` (the nucleus at its
  prompt: its compiler, variables, tasks and names, each a check that is
  what is typed and what that is to leave)
- `bench/`: `bench.sh` (the benchmarks, and what each is); `draw.fs` and
  `dither.fs` (the work, as it is typed); `disk.c`, `check.c` and `tiff.c`
  (the disk the picture is read from, and what looks at a bitmap);
  `m68k/` (the same dither for a 68000, in its assembler, and the 68000
  to run it on: the one place here that is not C or Forth, since it is
  what is measured against); `gates.sh` (the CPU's size beside a Z80's
  and a 68000's); `peephole.sh` (what the nucleus's compiler is worth),
  with `runs.c` (the image cut into straight-line runs, as typed and as
  `cross.fs` made them), `typed.c` (the runs typed at the nucleus, and
  what it made of them), `counting.c` (the reference VM, counting how
  often each place is run), `weigh.c` (the runs and the rules weighed by
  that) and `words.c` (an image's words, by header and by label)

The toolchain and the nucleus, in Forth (`forth/`):

- `cross.fs`: the cross-compiler. It reads the machine from
  `basewords.fs` and derives the rest: where data, bulk, headers and code
  pages are (`data-base`, `hdr-base`, `code-base`, `code-size`), which
  differs with the machine's size; headers and labels; the peephole and
  the trying of small words in place of calls; the tables of names
- `nuc.fs`: the nucleus: the kernel on page 0, the interpreter and
  compiler, the dictionary, tasks (`spawn`, `pause`). It includes
  `system.fs`, or with no display `join.fs` alone
- `join.fs`: the nucleus's own peephole, for what is typed at the prompt
  and for source read from blocks: the rules (`joins.fs`), and numbers,
  which it works out by running the instruction (`folds.fs`)

The system (`forth/system.fs` includes it, and says where each code page
starts):

- `machine.fs`: device registers through the ports, memory above the
  image (`halloc`, `hfree`, `hreuse`, `hgive`), byte order
- `blocks.fs`: the block interface (`'blk-read`, `'blk-write`, `block`,
  `cblock`, `blk-read`, `load`). `vmdisk.fs`: the reference VM's disk.
  `virtio.fs`, `virtio-blk.fs`, `virtio-input.fs`: QEMU's devices
- `display.fs`: the display and all drawing on it: the port, fills,
  `copy-bits`, fonts and text. `font.fs`, `keymap.fs`, `icons.fs`: the
  fonts, the keys, the tools' pictures
- `region.fs`: regions, and `scroll-rect`. The form of a region is said at
  its top; only `display.fs` reads it besides
- `tiff.fs`, `atkinson.fs`: the TIFF reader and the dither, a row at a
  time. `mandrill.fs`: the picture by itself
- `gfx.fs`, `wm.fs`: rectangles; windows, the pointer, the menu, the
  event loop (`'poll`, `on-event`, `on-front`, `'track`, `'moved`)
- `term.fs`, `edit.fs`, `view.fs`, `calc.fs`, `paint.fs`: the Forth
  window, the block editor, the picture's viewer, the calculator,
  HexaPaint
- `see.fs`: the decompiler and the Words window
- `desk.fs`: `windows`, `desktop`, `cold`

The other two machines:

- `baremetal/`: `hexaforth.S` (the small kernel) and `hexaforth-fast.S`
  (decoded by table), which behave the same; `ramfb.S`, the bridge that
  shows the display on QEMU; `difftest_dump.c`, the cases they and the
  Verilog are run against
- `rtl/`: `hexaforth.v` (the CPU), `hf_mem.v` (the memory), `soc.v`, and
  the test benches. `tb.cpp` is also what times a piece of work: it says
  at its top how marks are made and how it is a disk

## Writing a program for the cross-compiler

```forth
meta
    4 org
target

header double : double d# 2 * ;
header main : main d# 21 double drop halt ;
header quit : quit halt ;
```

- `meta` and `target` switch between the host and the program
- `header name` before a definition gives it a dictionary header
- `main` and `quit` must be defined: they are where the image starts
- a number in a definition is `d# 123` or `h# ff`

`cd forth && gforth ./cross.fs ./basewords.fs ./yourprogram.fs` writes
`build/yourprogram.hex`, and `disasm_hex` shows it.

## Adding to the machine

A name for an instruction the machine already has is a line in
`FORTH_OPS` in `vm_opcodes.h`. A field's name is a line in `INS_FIELDS`.
Rebuild `basewords-fs-gen` and run it, and the Forth has it. Anything
that changes what the machine does is a change to `MODEL.md`, `vm.c`, both
kernels and the Verilog together.
