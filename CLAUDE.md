# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

It is rules, and where things are. It is not where a size, a count, a
profile figure or the story of a change goes: the build prints the first
three, and the last is a commit message. A figure written here is a fact
said a second time, which someone then has to keep in step.

The README is the one document that has figures: those the project is
there to show, each beside the target that prints it. When a target prints
otherwise, it is the README that is out of date.

## C and Forth, and nothing else

This project is C and Forth. Where this branch began it was nothing
else: the tests were C (`test/main.c`, an array of `hexaforth_test`, and
the small Forth compiler in `test/compiler.c` that reads the same
instruction definitions the VM does), the words were generated from
`vm_opcodes.h` by `util/baseword_fs_gen.c`, and `cross.fs` built the
image. One definition, said once, and everything else made from it.

- Do not write Python. Not a test, not a generator, not a tool, not a
  model of the machine. None is to be added to this repository
- There was Python here, generators and tests and tools, and all of it has
  been taken out. None of it is to come back
- What replaced it is not Python written in C. Read `test/main.c`,
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
`DESKTOP.md` says how the desktop is worked and how a program for it is
written, with one that is run (`doc/hello.fs`, the target `hello`).

Names that are used for two things:

- The nucleus is the image `cross.fs` builds from `nuc.fs`, and the Forth
  that runs in it. "The nucleus's compiler" and "the nucleus's peephole"
  are the ones on the machine, which compile what is typed; `cross.fs` has
  its own
- The kernel is the first 4KB of that image, on page 0, which may be a
  ROM. The two AArch64 programs in `baremetal/` are called kernels as
  well (`KERNEL`, `kernel_size`, `build/kernel/`, `kernel-hexaforth`).
  `kernel_test` and `examine` are of the first, and `size` of the second
- A port of the machine is where a device is reached (`MODEL.md`). A port
  in the drawing words is the record of how drawing is done
- Nothing is zero, and a number below nothing is negative

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
- A name costs a line in `vm_opcodes.h` and nothing in the machine. At the
  prompt an instruction is said by its name, or by a phrase that the
  nucleus's peephole joins: the nucleus has no words for the fields
- In source that `cross.fs` compiles, any control word can be said by its
  fields, and the peephole joins it to its neighbours as it does a named
  one: `meta N->IN [IN] ->[T] d-1 alu target`. `MODEL.md` says what each
  field is and does
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

## Shell code

Change a file with the Edit tool, and make a new one with Write. Do not
write a shell or Python script to edit source: a change made that way
cannot be read as a diff or approved, the harness does not know the file
changed, and a wrong replacement is easy to miss. Read and search with
Read, Grep and Glob, not `cat`, `sed` and `grep`.

When a suite is run, keep all of its output in a file and print a summary
from that; do not cut or head the results.

That a thing has no tool is not leave to write a script for it. What is
written to look into something, to measure it or to compare two things is
not a script, in the tree or out of it. If it is worth writing it is a
function of the tree's C, which a test or a tool has in it
(`util/words.c`, `util/cut.c`, `util/reach.c`, `test/nucleus.c`), and a
test is CMake's to build and run.

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
  `display.fs` knows how the display is laid out. (A port here is the
  record of how drawing is done, its origin, clip, pattern and font, which
  the screen has and each window has: another thing than a port of the
  machine)
- A table is written where it is said, as source that can be read: a font
  is its strike a row at a time, the key map is rows of keys, an icon is
  rows of `#` and `.`. It is not generated from somewhere else
- The kernel's code is to fit in the kernel ROM (`KROM_BYTES`) at every
  cell size; the VM says how long it is when it loads an image (`CODE=`).
  When it does not fit, look in it for what is written twice before moving
  a word out of it
- The kernel is short before it is quick. `um*` takes a step for every
  bit of a cell whatever the numbers are, `u/mod` has one way in for a
  small dividend and no more, `c@` goes through `field@`, and a name is
  made into text a code at a time. The comment at each says what the
  quicker way is and what it is felt in. A program that multiplies,
  divides or reads characters a great deal has a word of its own for it,
  and what one costs is timed, not judged
- A baseword of two instructions is in line wherever it is written. In
  the kernel one can be made `callable` instead: a call wherever the
  peephole does not join it to its neighbours. The top of `nuc.fs` says
  which are and which are not, and why. One that words run all the time
  are made of is not, and which is which is measured
  (the target `bench-draw`), not judged
- A shift count is taken modulo 64 whatever the cell, so a shift by the
  width of a cell of 16 or 32 bits gives zero, and a shift by 64 shifts
  by nothing

## Measuring

- Before saying what a change will save, and before choosing what to
  change, measure. A count of how often a thing is done, times what one is
  thought to cost, leaves out the loop that does them and whatever is rare
  and dear
- Runtime is cycles, not instructions: a memory read, a shift and a
  multiply each take more than one, and where memory takes two cycles to
  answer (`LAT = 2`, the tests named `lat2`) so does a jump, a call, a
  return or a branch that is taken (`rtl/hexaforth.v` says at its top).
  The target `bench` says cycles
- The target `bench` is the benchmarks: a piece of work is a file typed
  at the prompt, the same file on the Verilog CPU of every size, and what
  it made is checked before a figure is printed for it. `CMakeLists.txt`
  says what there is (`bench-draw`, `bench-dither`, `bench-m68k`, and
  `gates`). What a change costs is what it prints before the change
  and after; no figure is kept in the repository
- The target `peephole` is what the nucleus's compiler is worth: what it
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

## Making the kernel smaller

What is in the kernel, and what a change to it saves, is asked of the
image and not of the source, on every size of machine: the peephole, and
the small words compiled in place of calls to them, make a count from
the source wrong. What asks is functions of the tree's C, which a test or
a tool has in it: `util/words.c` (an image's words, and how long each
is), `util/cut.c` (its calls, jumps and branches, and where each goes),
`util/reach.c` (what reaches each word) and `util/layout.c` (its pages of
code, and what each source file has of them). The target `examine`
prints what the last finds of the nucleus's image: how long the kernel
is, what is on each page, and each file's words, code and tables; and
`examine-16`, `examine-32` and `examine-64` the same of each machine of
`util/machines`.

- Before a word is made shorter, find what reaches it (`util/reach.c`):
  the calls and jumps to it from the kernel and from every other page,
  its place pushed as a number or kept in a cell of data, and the word
  before it running into it. `kernel_test`, which the target `tests`
  runs, fails if a word of the kernel that has no header has nothing that
  comes to it, and run by hand with `typed` after the image and its names
  (`build/cmake/kernel_test build/nuc.hex build/nuc.tables typed`) says
  which words that have one are there only to be typed. Whether one of
  those stays is not the count's to say
- A word that is reached only through another is handed only what that
  one lets through: `pcompile,` stands before the kernel's `(compile,)`,
  and compiles in place itself whatever is small. Put the first one's
  rule to every word of the image and see which cases of the second are
  ever handed anything. A case that is handed nothing is dead, however
  much of the word it is
- A path that a machine of one size can never take is in that size's
  kernel all the same, unless the source leaves it out there
  (`W64? [if]`). That it is never taken is run on that machine
- A change is tried by itself, in a copy of the tree, and built for every
  size: what it saves is how much shorter the kernel is. Changes are built
  together before what they save is added up
- A change that is to behave as before is held to checks in the tree
  (`test/prompt_test.c`): each is what is typed and what that is to leave.
  A word that nothing in the tree types is given its checks before it is
  changed, and the kernel as it was passes them: numbers at the ends of a
  cell as well as small ones, a name that fills its cells exactly, what
  is typed and taken back
- What a change costs in time is asked of work that runs what it changes.
  Where the benchmarks do not run it, a file that does is typed the same
  way, with a mark round each piece (`rtl/tb.cpp`), at the kernel as it
  was and at the changed one
- Why a word of the kernel is written as it is, where another way would
  be the first thought of, is said in the comment at the word: what the
  other way is, and what it would cost or save. Read it before writing the
  word another way, and when a word is changed for its size, say it there.
  The figures are in the commit that made the change

## Building

The C is built with CMake (`CMakeLists.txt`): `hexaforth` (the reference
VM, of the machine `vm_constants.h` gives when nothing is said; each
machine of `util/machines` has its own, `hexaforth-64` and the like),
`hexaforth-debug` (with a trace of every instruction, which is built when
CMake is told `-DDEBUG=ON`),
`hexaforth_test`, `kernel_test`, `fewer_test`, `basewords-fs-gen`,
`generate-test-cases`, and the target `tests`, which runs `hexaforth_test`,
then `kernel_test` on the image, and then `fewer_test`; and
`examine-image`, with the target `examine`, which runs it on the image.
What the project needs that is not in it is said once, at the top of
`CMakeLists.txt`: each program or library, what it is for, and the
package that has it, Homebrew's and apt's. When CMake is configured each
is looked for, and what is not there is asked of Homebrew, or of apt as
root, all at once; `-DINSTALL_NEEDED=OFF` only looks and says. So a
system that has CMake and one of the two needs nothing else done to it.
A program that is needed is added there, and is run by the name it is
given there, not by a name that is hoped to be in `PATH`.

Whatever else is compiled or run is CMake's to compile and run too: the
machines, the AArch64 kernels, the Verilog, the tests, the reports and
the benchmarks. `CMakeLists.txt` says what each thing is made of, once,
and at each part what its targets are and what can be said to them. A
tool, a test or a report that is added is added there: not a compiler's
command line in a script, not a script, and not a Makefile. What looks at
what a program printed, or works something out of it, is a function of
the tree's C that a tool has in it, and not a pipe of `grep` and `awk`:
`baremetal/symbols.c` is where a kernel's symbols are, `bench/figure.c`
what a test bench said passed between two marks.

`util/machines` is the machines the system is tried on, and CMake reads
it. The first of them, which has cells of 64 bits, is the one the reports,
`hello` and the screenshot are of. For each it builds the definition `cross.fs` reads, the nucleus, the
reference VM and the tests, and keeps what is made in a directory of
`build/` named for the bits of the machine's cell (`build/32/nuc.hex`;
what is compiled is where CMake builds, `hexaforth-32`,
`prompt_test-32`). The kernels are of the one with cells of 64 bits, and
are made in `build/kernel/`; the Verilog is made in `build/rtl/`, and
tested on the machine `vm_constants.h` gives each size of cell when
nothing else is said (`build/default-32/`). None of that is built until
a target wants it: `check`, which builds every test and runs them; the
reports, `profile`, `subtracts`, `peephole`, `hits`, `phrases` and
`tables`, and `examine-16` and the like; `screenshot`, which makes the
README's picture of the desktop again (`doc/desktop.png`), `hello`, and
`seen`, which builds what shows a machine's display from the memory the
reference VM writes (`screen-64`, `png`); `bench`
and `gates`; `run`, `demo`, `demo-test`, `demo-time` and `size`, of the
kernel that `KERNEL`, `STRICT`, `SCALE`, `MIPS` and `DISK` say
(`cmake -DMIPS=5 build/cmake`); and `synth`.

An image is built by `gforth` from the Forth:

```bash
cd forth
gforth ./cross.fs ./basewords.fs ./nuc.fs      # build/nuc.hex and what goes with it
BUILD=../build/32/ gforth ./cross.fs ../build/32/basewords.fs ./nuc.fs   # the image written there
```

The checked-in `forth/basewords.fs` is the machine with cells of 64 bits.
`BUILD` is the directory the image and what goes with it are written to: it
is to be there already, and to end in `/`. The source is a file of
`forth/`, said as `./name.fs`, and what is written is named for it.
`PEEPHOLE=off` builds the same with the peephole off. That build reads the
`.inline` of the build with it on, and writes files of the same names, so
it is given a directory of its own with that `.inline` copied into it:
CMake does so, in `build/64/plain/`.

`build/cmake/hexaforth-64 IMAGE.hex [DISK.img [MEMORY.out]]` runs an image
on the reference VM of that machine, and writes its memory when it has
stopped if it is told where.

Beside an image `cross.fs` writes `.lst` (symbols), `.sizes` (code and
data by file), `.inline` (the calls it compiled in place, which a build
with the peephole off reads, so build with it on first) and `.tables`
(the names for the decompiler, which go on the disk).

`cross.fs` asks the machine which instruction does the work of two or
three, and what shorter run does the work of a run that has numbers or
variables in it: `util/fuse.c`, which has `vm.c` in it, is built by gforth
each time an image is and called through gforth's C interface. gforth
builds C with `libtool`. An answer is found by
the machine's step on terms and then run on `vm.c`, and the build stops if
the two do not agree. What was found and run is kept in the tree's
`build/`, whatever `BUILD` says (`build/fuse-*.kept`, named for the text
of `fuse.c` and of the machine, so that a change to either starts
another), so the first image built after such a change takes longer than
the rest. gforth prints the C compiler's warnings of `fuse.c` each time. Where gforth is
Homebrew's, the compiler is told where Homebrew's headers are:
`CMakeLists.txt` does, and by hand it is
`CPATH="$(brew --prefix)/include" gforth ...`.

Generated, and checked in: `forth/basewords.fs` (by `basewords-fs-gen`,
from `vm_constants.h` and `vm_opcodes.h`); `forth/folds.fs` (what the
nucleus's peephole is told of numbers, by `util/joins.c`, which reads no
image); and `forth/fused.fs` (the words the decompiler shows an
instruction as, by `util/fused.c`, from a built image). Both have
`fuse.c` in them and ask the machine. The target `tables` makes the two
again: the decompiler's table is in the image it is made from, so
`util/tables.cmake` builds until the table stops changing.

`forth/joins.fs`, the rules of the nucleus's peephole, is not generated:
it is the rules that are kept, and says at its top which and why. The
nucleus counts each rule's uses; the target `hits` works the desktop and
prints the counts.

## Testing

After a change, run what covers it, and after anything in `cross.fs`,
`nuc.fs` or the machine, all of it:
`cmake -S . -B build/cmake` and then
`cmake --build build/cmake --target check` builds every test below and
runs it, and prints a line for each.

- `hexaforth_test`: each test of `test/tests.h` compiled by the C compiler
  and by `cross.fs` and the instructions compared, then run on the VM
- `kernel_test`: nothing is in the kernel that nothing comes to
  (`util/reach.c`). CMake builds it and the target `tests` runs it, for
  the default machine
- `fewer_test`: the search for the fewest instructions that do the work
  of a run, and of a turn of a loop (`util/fewer.c`, `util/turns.c`),
  held to runs whose fewest is known, and each answer run on `vm.c`. The
  target `tests` runs it
- The target `check` builds every test CMake has and runs them (`ctest`):
  a line a test, and all that each said in
  `Testing/Temporary/LastTest.log` where CMake builds. `ctest -V -R NAME`
  there runs the tests whose names have that in them and prints what
  they say. It runs the three above, and
- on each machine of `util/machines`, with cells of 64, 32 and 16 bits,
  each with a display and `STRICT`: the prompt, comments and a block read
  as source, the desktop, the drawing words, regions, the dither and the
  TIFF reader (`prompt-32`, `desktop-16`, `display-64-3`). A test that
  tries things by chance is run once for each seed its chances start
  from, and the seed is the last part of its name: `SEEDS`, said to CMake,
  is which seeds. Nothing is cell-size safe that these have not run
- `kernel-hexaforth` and `kernel-hexaforth-fast`: each AArch64 kernel
  against `vm.c`
- `qemu-hexaforth-display-strict` and `qemu-hexaforth-fast-display`: the
  desktop on QEMU with each, driven and its screen checked. The target
  `demo-test` is the same of the kernel that is said to CMake
- `verilog-cases-c64` and `verilog-session-c64` (and `c32`, `c16`; each
  with `-lat2`, where memory takes two cycles to answer; and
  `c64-lat2-krom`, with the kernel in a ROM): the Verilog against `vm.c`,
  on the cases and on a session typed at the nucleus; and `verilog-memory`
- `builds`, which `check` runs too: after any change to a compiler or a
  peephole, the image against the image built with the peephole off: every
  straight-line run must leave what the plain one leaves, and every call,
  jump and branch go where the plain one goes. It fails if the two are as
  many instructions, since then the plain build did not finish
- To check that nothing stores into the kernel or leaves memory or a
  stack: the VM built with `-DSTRICT` and `-DROM_BYTES` the size of the
  kernel ROM, or the small kernel with `STRICT`

The target `profile` is
where the desktop's instructions go: the desktop worked on the reference
VM that counts, a table of things a person would do, and the work of each
by file, word and kind of instruction (it leaves
`build/profile/desktop.counts`).
It then says what that work costs, three ways (`bench/costs.c`):
`paths.txt`, each word charged with all it calls, which is what to read
before saying what a change will save; `loops.txt`, the loops by what
they cost; and `vars.txt`, what the variables cost, of the files
`PROFILE_FILES` names or of all. The target `phrases` is the phrase book: for every
instruction some run of the machine's words does the work of, the
shortest run, whether `cross.fs` makes one instruction of it, and whether
each name means what it means in gforth. The profile ends with the
encoding laid out against the work (`util/survey.c`): the instructions
the image uses, and the runs that fewer would do, which are what
`cross.fs` left; and then with the loops that ran (`bench/shorter.c`):
for each that calls nothing, the fewest instructions found that do a turn
of it, wherever its values are kept, which the peephole cannot find.

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
- `util/`: `baseword_fs_gen.c` (writes `basewords.fs`), `disasm_hex.c`
  (an image's code word by word, laid out and named as `see` does it, with
  every instruction's place, number and fields; in colour on a terminal),
  `stack.c` (the stacks
  printed, for the trace),
  `hex2bin.c` and `memimage.c` (an image as bytes for the kernels, and as
  what the Verilog loads; `image.c` reads one), `mkdisk.c` (a disk),
  `fuse.c` (which instruction does the work of others: found by the
  machine's step on terms, and each answer then run on `vm.c`; what it
  trusts and why is said at its top), `joins.c` and `fused.c` (the two
  generators that have it in them), `tables.cmake` (which runs the second
  until its table settles),
  `phrases.c` (the phrase book, which has it in it too),
  `survey.c` (the encoding against a profile, which has it too),
  `room.c` (room for as many of a thing as there are: a list that grows,
  a file or a line of any length, text that grows.  Nothing in the tools
  or the tests has room for only so many of what it reads),
  `named.c` (a word of `vm_opcodes.h` by its name), `rules.c` (the rules
  of `joins.fs`, as it writes them),
  `words.c` (an image's words, by header and by label, for the tools
  that read an image), `cut.c` (two builds of an image cut into the same
  straight-line runs), `reach.c` (what reaches each word of an image:
  calls and jumps, its place pushed or kept in a cell, the word before
  running into it), `kinds.c` (the instructions by what they leave, those
  with a number in them, and an instruction as it is said), `loops.c`
  (an image's loops, and the turn of one), `fewer.c` (the fewest
  instructions that leave what is wanted: a search on the step's terms,
  each answer run on `vm.c`), `turns.c` (a turn of a loop looked at the
  ways fewer could do it), `layout.c` (how an image is laid out: its
  pages of code, the tables after them, what each file has, the
  dictionary and the data) and `examine.c` (which prints it), `screen.c`
  (a machine's display, out of the memory the reference VM writes when it
  stops) and `png.c` (a screen as a PNG)
- `test/`: `tests.h` (the tests), `compiler.c` (the small Forth compiler),
  `vm_test.c`, `main.c`, `generate_test_cases.c`; `nucleus.c` (the
  nucleus on `vm.c`, typed at its prompt, once or again as it was left,
  and what that left on its stack and in its memory: a test that does
  that is built with it);
  `dither_test.c` (the dither against a dither a pixel at a time, read
  out of the machine's memory); `dither.c` (that dither a pixel at a
  time, which the test and the benchmarks share); `source_test.c` and
  `source.fs` (comments, at the prompt and in a block of a disk that
  `mkdisk` made of that file); `prompt_test.c` (the nucleus at its
  prompt: its compiler, its arithmetic, variables, tasks and names, each
  a check that is what is typed and what that is to leave);
  `kernel_test.c` (the kernel's words and what comes to each: it has
  `reach.c` in it); `fewer_test.c` (the search for fewer instructions:
  runs and turns as words, and the fewest that is to be found for each);
  `region_test.c` (regions
  against their pixels: what is typed is done to a plane of pixels too,
  and every cell of every region compared with the one form of them);
  `display_test.c` (the drawing words: what is typed is drawn a pixel at
  a time too, and every pixel compared after every step); `tables.c`
  (the system's tables read from the Forth that says them, for a test
  that needs a font, a pattern or where the windows go: a test has none
  of its own); `hits.c` (how often each rule of the nucleus's peephole
  was used, read out of the memory the VM wrote: the target `hits`);
  `deskcheck.c` (what the desktop is checked for: a table of
  what is done with the keys and the pointer and what is then to be on
  the screen, for whatever machine it runs on), `deskvm.c` (the
  reference VM as that machine: it is stopped in its event loop to be
  looked at, and gone on with) and `desktop_test.c` (the checks run on
  it);
  `tiff_test.c` (the TIFF reader: files that are only a header and a
  directory, on a disk of bytes by chance, dithered by the machine and a
  pixel at a time);
  `builds_test.c` (the two
  builds, run for run: it has `fuse.c` in it, and asks the machine);
  `session_test.c` (what the Verilog sent of a session against what
  `vm.c` sent)
- `bench/`: the benchmarks (the target `bench`, and `CMakeLists.txt`
  says what each is): `draw.fs` and
  `dither.fs` (the work, as it is typed); `disk.c` and `tiff.c`
  (the disk the picture is read from); `check.c` (what looks at what the
  work made, and then prints what it cost: the table of the benchmarks is
  its too) and `figure.c` (a line of that table, and what a test bench
  said passed between two marks);
  `m68k/` (the same dither for a 68000, in its assembler, and the 68000
  to run it on: the one place here that is not C or Forth, since it is
  what is measured against); `gates.c` (the CPU's size beside a Z80's
  and a 68000's, from what Yosys counted: the target `gates`) and
  `packed.cmake` (the 68000's Verilog made what Yosys reads); what the
  nucleus's compiler is worth (the target
  `peephole`): `runs.c` (the image cut into straight-line runs, as typed and as
  `cross.fs` made them), `typed.c` (the runs typed at the nucleus, and
  what it made of them), `counting.c` (the reference VM, counting how
  often each place is run: by itself, or as the VM in a test), `idle.fs`
  (the machine come up and stopped, which is taken from the rest) and
  `weigh.c` (the runs and the rules weighed by that);
  `profile.c` (where the desktop's instructions go), with `work.c` (the
  work by place, as the file the profile leaves), `files.c` (the source
  files an image is made of, from its `.sizes`: the one place the C reads
  that file), `costs.c` (what the
  work costs: each word with all it calls, the loops, the variables),
  `shorter.c` (the loops that ran, and fewer instructions that do a turn
  of each), `uses.c` (where the work uses a small word, called or
  compiled in place, how often, and what it is given: the target
  `subtracts` asks it of `-`, `1-` and `negate`, and `uses.c` says at its
  top how it is asked of any other) and `cycles.h` (the
  cycles the Verilog takes for an instruction)

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
  and for source read from blocks: the rules (`joins.fs`), numbers, which
  it works out by running the instruction (`folds.fs`), and the one
  instruction that fetches or stores a variable (`var,`). Whatever can be
  compiled in place it compiles in place, and the kernel's `(compile,)`
  is left with a call

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
  Verilog are run against, and `difftest.c`, which runs a kernel on them;
  `symbols.c` (where a kernel's own symbols are, for the tools that are
  told); `size.c` (how big a kernel is, and each part of it from one label
  to the next); `qemu.c` (QEMU as a machine the
  desktop's checks can be run on: told keys and the pointer, and its
  screen dumped), `guitest.c` (those checks run on it) and `desktime.c`
  (when the desktop is ready and its picture made, watched and not
  touched); `block1.fs` (what is in block 1 of the disk the desktop is
  given, which is the block the editor opens on)
- `doc/`: `desktop.png` (the README's picture of the desktop, which the
  target `screenshot` makes) and `hello.fs` (the window `DESKTOP.md` is
  about, which the target `hello` types at the reference VM)
- `rtl/`: `hexaforth.v` (the CPU), `hf_mem.v` (the memory), `soc.v`, and
  the test benches. `tb.cpp` is also what times a piece of work: it says
  at its top how marks are made and how it is a disk. `session.fs` is
  what is typed at the nucleus on the Verilog and on `vm.c`, which
  `test/session_test.c` compares; `synth.c` says what a synthesis came
  to

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
- `main` and `quit` must be defined: they are where the image starts.
  `cross.fs` lays down the image's first two instructions, which run `main`
  and then `quit`, and `4 org` starts the program's code after them, at
  byte 4
- a number in a definition is `d# 123` or `h# ff`
- `emit` sends a character to the console, which is how such a program
  shows what it made
- an instruction said by its fields is between `meta` and `target`, the
  input first and `alu` last (`MODEL.md`, Instructions)
- the peephole joins what it can, and works out arithmetic on numbers that
  are known: the image may have fewer instructions than were written

`cd forth && gforth ./cross.fs ./basewords.fs ./yourprogram.fs` writes
`build/yourprogram.hex`, for the machine with cells of 64 bits.
`build/cmake/hexaforth-64 build/yourprogram.hex` runs it, and
`build/cmake/disasm_hex-64 build/yourprogram.hex` shows its code, a word
at a time and an instruction to a line (CMake builds the two for each
machine). Words named after the image show only those, and two places
(`0 0x20`) only the words with code between them.
`build/cmake/disasm_hex-16 build/16/nuc.hex apixels` is a word of the
nucleus; `less -R` keeps the colour, and `--no-color` leaves it out.

## Adding to the machine

A name for an instruction the machine already has is a line in
`FORTH_OPS` in `vm_opcodes.h`. A field's name is a line in `INS_FIELDS`.
Rebuild `basewords-fs-gen` and run it, and the Forth has it. Anything
that changes what the machine does is a change to `MODEL.md`, `vm.c`, both
kernels and the Verilog together.
