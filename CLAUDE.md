# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

This file holds rules and tells where things are. Do not put a size, a
count, a profile figure or the history of a change here. The build prints
the first three, and the history belongs in a commit message. A figure
written here repeats a fact stated somewhere else, and someone then has to
keep the two in step.

The README is the one document with figures. It has the figures the
project exists to show, each next to the target that prints it.

## C and Forth, and nothing else

This project is written in C and Forth. When this branch began it used
only those two. The tests were C: `test/main.c`, an array of
`hexaforth_test`, and the small Forth compiler in `test/compiler.c`, which
reads the same instruction definitions the VM does. The words were
generated from `vm_opcodes.h` by `util/baseword_fs_gen.c`, and `cross.fs`
built the image. Each definition was stated once, and everything else was
generated from it.

- Do not write Python: no test, no generator, no tool and no model of the
  machine. Add none to this repository
- This repository once had Python generators, tests and tools. All of it
  was removed, and none of it comes back
- The C that replaced it is written as C, not as Python translated into C.
  Read `test/main.c`, `test/compiler.c`, `test/vm_test.c` and
  `util/baseword_fs_gen.c` before writing any C, and write in their style.
  Do in Forth the work that belongs to Forth: `cross.fs` already compiles
  and joins the image
- State each fact once. An encoding, a layout or a table lives in one
  place, and everything else that needs it is generated from that place or
  reads it there. If a change has to repeat a fact in another file or
  another language, the change is wrong. The same goes for a number: refer
  to an address or a size by the name it has where it is defined, and do
  not write the number out again
- Contrastive testing is good, and it is different from a second source of
  truth. A source of truth is the place where a fact is defined, and
  everything that needs the fact is built from it. Two sources of truth can
  disagree, nothing tells which one is right, and the build depends on
  someone keeping them in step. A contrastive test is a second version,
  made independently, that nothing is built from. It exists only to be
  compared with the first. When the two disagree, the test has done its
  job: one of them is wrong, and it is already settled which one decides.
  `test/main.c` compiles each test with the Forth compiler written in C
  (`test/compiler.c`) and with `cross.fs`, and compares the instructions.
  The two kernels and the Verilog are run against `vm.c`, which is the
  machine. To tell which kind a second thing is, take it away and see
  whether something stops building or only a check disappears. A model of the machine that the peephole's tables were
  generated from was a second source of truth. The same model, used only to
  check tables generated from `vm.c`, would have been a contrastive test
- Do not ask whether C and Forth can do a thing. They did all of it before
  this project had anything else. "I do not know how much of this they can
  absorb; I have not measured it" was said about exactly this. Measurement
  was not the question: the sentence was a way of keeping code that should
  not have been written. Measure what a change costs. Do not measure
  whether the project's own languages are enough
- A test that finds nothing to test must fail, not pass. Check how many
  things a test reports that it compared
- One directory is neither C nor Forth: `bench/m68k/` is 68000 assembly.
  It is not a tool, and nothing is built from it. It is the same work
  written for another machine, to be timed against this machine, and it
  can only be written in that machine's language

## A tool's flaw is not the product's limit

The code written here to build the product or to check it can be wrong,
and can fall short. That is a bug in that code.

- Fix the tool where the flaw is. Do not add a rule to steer around what a
  tool gets wrong, and do not give up anything the product had because a
  tool cannot do it or cannot confirm it. "It declines these" is not a cost
  of a change: it means the change is not finished
- A replacement must do everything the old thing did before the old thing
  is removed. A new way of building the product starts from what the
  product had and the machine ran. Put the new beside the old and compare
  them on everything the old one produced, before the old one is removed
- Find out what an instruction or a run of instructions does by running it
  on `vm.c`, not by reading its fields. Run every belief about what the
  machine does, however sure, before building anything on it or removing
  anything because of it. If a tool and a belief disagree, trust neither
  until the code has been run on the machine
- Test only from a state a program can be in. A return goes to a return
  address, and a fetch through a cell finds an address in that cell. What
  the machine does from a state no program reaches tells nothing about the
  machine or the product, and is no reason to leave anything out
- Being too careful is a defect like any other. Leaving out something
  right because it could not be checked puts the tool before the product.
  Say what could not be checked and why, and fix that
- When someone questions what was done, stop and examine the premise. Do
  not go on building on it, and do not answer by showing that something
  else is true

## A question is the objection

- A question about the work in progress ("is this a shortcut?", "why are we
  doing this?", "what precisely are we checking?", "why are we not done?")
  is not a request for information, and it is not permission to continue.
  It means the work looks wrong to the person asking, and it usually is
- When such a question is asked, stop. Run nothing and change nothing.
  Answer the question and only the question: yes or no first, then what
  was done and why
- The answer is not a plan. Do not say in the same reply what to do about
  it, and do not begin it. The person who asked decides that after hearing
  the answer
- "Stop" means the same, more strongly. Run, change, commit and push
  nothing until told to continue, whatever else seems to call for it

## Reporting on the work

- Report what was done, what failed, and what the reader must decide.
  Present nothing else as if it were one of those
- A consequence of the work that needs no decision is not a caveat, and
  the reader does not need to be told about it. It is part of the work:
  do it. If it deserves a line, the line says what was done
- Do not bring back something already decided or agreed, as a question or
  as a plan, to have it agreed a second time. Do it
- Say nothing just to have said it: no excuse offered before the work is
  finished, and no warning whose only use is to be pointed to afterwards.
  If something may go wrong, find out whether it does
- Write a decision down in the words it was made in. Do not add "may",
  "could" or "later" to it, and do not list it among the undecided things.
  Hold everything built after it to it
- When offering ways to do something, make sure every one of them includes
  what was decided

## The machine model

`MODEL.md` defines the machine: the stacks, memory and display that
`vm.c`, the AArch64 kernels and the Verilog must all provide in the same
way. Read it before changing any of the three, and keep all three
consistent with it. `UNDERSTANDING.md` explains why the machine is
designed as it is: read it first. `DESKTOP.md` explains how to use the
desktop and how to write a program for it, with an example that is run
(`doc/hello.fs`, the target `hello`).

Names with two meanings:

- The nucleus is the image `cross.fs` builds from `nuc.fs`, and the Forth
  that runs in it. "The nucleus's compiler" and "the nucleus's peephole"
  are the ones on the machine, which compile what is typed. `cross.fs` has
  a compiler and a peephole of its own
- The kernel is the first 4KB of that image, on page 0, which may be a
  ROM. The two AArch64 programs in `baremetal/` are also called kernels
  (`KERNEL`, `kernel_size`, `build/kernel/`, `kernel-hexaforth`).
  `kernel_test` and `examine` are about the first kind, and `size` is
  about the second
- A port of the machine is an address where a device is reached
  (`MODEL.md`). A port in the drawing words is the record of the drawing
  state
- In a comment or a message, "nothing" can mean zero, and a number "below
  nothing" is negative

## The instruction set is the whole encoding

An instruction is a control word. Its fields give where the ALU's input
comes from, which operation to do, where the result goes, how each stack
moves, and whether to return. Every combination is an instruction that
`vm.c`, the kernels and the Verilog already execute. The machine has no
opcodes, and a baseword is only a name for one of these instructions.

- Do not judge an encoding, or a sequence that one encoding replaces, by
  whether it looks useful. Measure where it occurs in the image and how
  often it runs. A site that is rare in the source may be in the hottest
  loop
- A name costs one line in `vm_opcodes.h` and costs the machine nothing.
  At the prompt, an instruction is written by its name, or by a phrase
  that the nucleus's peephole joins. The nucleus has no words for the
  fields
- In source that `cross.fs` compiles, any control word can be written by
  its fields, and the peephole joins it with its neighbours as it joins a
  named one: `meta N->IN [IN] ->[T] d-1 alu target`. `MODEL.md` describes
  each field
- Most instructions are written as a phrase of standard words that the
  peephole joins. A baseword's name must mean what the standard says it
  means: the shift right by one is `u2/`, and `2/`, which keeps the sign,
  is a word of the nucleus
- A name in `vm_opcodes.h` with a space in it is a phrase, not a word.
  `dup >r` is written as its two words, and the compilers make one
  instruction from them. The entry lets a trace, a test or a reader find
  the instruction from the source text. Do not define a word for a phrase,
  in `cross.fs` or on the machine, and do not run words together in source
- When several instructions do the same thing, one represents them all:
  the one whose operation has the lowest number in its field, and among
  those the lowest instruction number (`util/fuse.h`, `plainer`). Both
  compilers produce that one, and names go to that one. `util/joins.c`
  stops the build at a name whose instruction is not the representative,
  and at a phrase whose instruction does something other than its words
  do
- The cell under the top can be both of the ALU's inputs. A cell equals
  itself, so `N->IN IN==N` gives -1 and `N->IN IN^N` gives 0, whatever
  the cell holds. `true` and `false` are those two instructions, and
  `true` pushes -1 in one instruction. The compilers and the search for
  fewer instructions know only the facts about the arithmetic that `vm.c`
  states beside each operation. The step on terms reasons from those
  facts (`util/fuse.h`, `t_made`) and tests each one against the
  arithmetic when it starts. They cannot use a fact that `vm.c` does not
  state
- Do not propose a new ALU operation, or any other change to the machine,
  for speed. The operation field is full, the ALU has two inputs, and the
  design depends on the fields composing. First show what the existing
  fields give, and whether the work is needed at all
- Think of a word as data moving: from where, through what, to where, one
  cell at a time. Bytes, opcodes and per-element loops are habits from
  other machines
- A loop counts up, not down. Adding one takes one instruction.
  Subtracting one takes two, a -1 pushed and an add, because a literal
  cannot hold a negative number. A loop over memory keeps no count:
  `bounds begin 2dup xor while ... 1+ repeat 2drop`,
  and a second address is the first address plus the distance between
  them. A loop that only counts starts at a negative number and counts up
  to zero: `negate begin dup while ... 1+ repeat drop`. Keep a value that
  is the same on every turn on the return stack, where it is an input of
  the ALU. The kernel has `cells-move ( from to n )` and
  `cells-fill ( x a n )`: use them before writing another loop over cells

## Shell code

Change a file with the Edit tool, and create a new one with Write. Do not
write a shell or Python script to edit source. A change made that way
cannot be read as a diff or approved, the harness does not know the file
changed, and a wrong replacement is easy to miss. Read and search with
Read, Grep and Glob, not `cat`, `sed` and `grep`.

When running a suite, keep all of its output in a file and print a summary
from that file. Do not cut the results or take only their first lines.

A missing tool is not permission to write a script. Code written to
investigate something, to measure it or to compare two things is never a
script, inside the tree or outside it. If it is worth writing, write it as
a function in the tree's C that a test or a tool includes (`util/words.h`,
`util/cut.h`, `util/reach.h`, `test/nucleus.c`). CMake builds and runs the
tests.

Answer a question about an image, a profile or a program's output with a
tool of the tree, not at the command line. Do not pipe one program into
another, do not feed a program typed input, and do not run a program whose
job is to search, count, compare or script
(`.claude/hooks/validate-bash.sh` refuses them). Where no tool answers the
question, extend a tool. The questions the tools answer:

- What the nucleus compiles from typed text: `disasm_hex IMAGE.hex
  --typed FILE.fs` types the file at the nucleus and shows the words the
  file defines, as the nucleus compiled them
- How many times each instruction of a word ran: `disasm_hex IMAGE.hex
  NAME --counts WORK` reads the work a counting VM wrote (`RUN_WORK`). The
  target `peephole` writes the drawing benchmark's work
  (`build/peephole/draw.work`). Check there that the work runs a word at
  all before using `bench-draw` to measure a change to that word
- How the nucleus compiles a word of the system, compared with `cross.fs`:
  the target `peephole` writes every word the two compile to different
  lengths to `build/peephole/words.txt`. Then
  `typed word build/peephole/typed.txt NAME...` shows those words one run
  at a time
- What it costs a machine to boot and compile from its disk, and which
  words the cost is in: the targets `compiling-64`, `compiling-32` and
  `compiling-16`
- What that boot looks up in the dictionary: the target `finding-64`, and
  the same target for each other machine (`bench/finds.c`). It reports how
  often a name is looked up, how many headers are examined, how many would
  be examined if the dictionary were several chains (for each way of
  choosing a name's chain), and the headers examined for each name
- What a boot compiled, compared with what `cross.fs` compiles for the
  same words, and which of the boot's instructions are missing from the
  decompiler's table: the target `booted-64`, and the same target for each
  other machine (`bench/booted.c`)
- How many words, headers and bytes an image has: `examine`
- How much of each part of a machine's memory is used and free: the target
  `memory-table-64`, and the same target for each other machine. It
  covers the system as `cross.fs` built it, and the machine after it has
  compiled the applications from its disk. `quick` and `check` print these
  tables
- What a machine compiled, and what its dictionary holds, when it has
  stopped: `disasm_hex IMAGE.hex --memory MEMORY.out` reads the memory the
  reference VM wrote, in place of the image. With `--headers` it prints
  only the dictionary, in search order, from the header a search starts at
  to the header where its chain stops, which shows where a chain was
  broken. If `STOP_AFTER` is set in the environment of `hexaforth`, a
  machine still running after that many seconds is stopped and writes its
  memory. A machine whose stack goes out of range prints what its return
  stack held. Three more environment variables stop the reference VM
  (`vm_stopped.h`): `STOP_AT` stops it before the instruction at that
  address, `STOP_ON_STORE` stops it after a store to that cell, and
  `STOP_BUDGET` stops it when it has run more than that many instructions
  since it last took a key. A machine that stops somewhere unexpected
  prints the address, both stacks and the instructions it ran before, and
  writes its memory. The desktop's test on QEMU (`guitest`) writes that
  machine's memory to the file `MEMORY` names, for the same tools. The
  strict kernel prints `!` and the address where it halted, and the memory
  shows which word that address is in
- How an image is laid out, as a basis for improving the layout: the
  targets `listing-64`, `listing-32` and `listing-16` write
  `build/64/listing.txt`, `build/32/listing.txt` and
  `build/16/listing.txt` (`disasm_hex --layout`). A listing has what each
  page holds and has free; the calls that go to a word on another page,
  with how often the desktop's work runs each and what they cost; the data
  by cell; and then every word in order of address
- What the calls between pages would be if some words were in the kernel's
  page: `disasm_hex build/16/nuc.hex --layout --counts
  build/16/listing/desktop.counts --near NAME`, with `--near` repeated for
  each word. For each word it lists the long calls to it that would become
  short, and the calls it would then make long, by the word called
- What a measurement was before a change: `build/kept/log.txt`, and then
  `compared WAS WAS.work IS IS.work`, which compares the work of two images
  by word

## Writing Forth here

- Lay source out as `doc/formatting.md` describes: lines of 64 characters,
  definitions that fit a block, stack comments after the name, and lines
  broken only where a phrase ends. Read it before formatting Forth, and
  before changing `util/wrap.c`, which follows it
- A comment says what a word does, and only where the word needs one: a
  line after its stack comment (`doc/formatting.md`, Comments). It does
  not explain why the word is written that way, warn, compare the word
  with another way of writing it, or tell its history. Those belong in the
  commit message and in `doc/superoptimizations.md`
- An address is the number of a cell, and `@` and `!` fetch and store one
  cell. A branch target, a call target and an execution token are each the
  number of an instruction (`i@`, `i!`). Nothing in the machine counts
  bytes
- Very little needs the address of a byte. Before giving something a byte
  address, check that it needs one. Characters stay packed in cells, and a
  character is addressed by its cell and its position in that cell
  (`char@`, `draw-chars`, `chars-width`, `ctext`). The words that take a
  byte address are the ones that store a character (`c!`), `type`,
  `count`, and the words the interpreter reads with. Memory from `halloc`
  is above the display, and on a machine of 64K cells no byte there has an
  address, so keep nothing there as bytes (`machine.fs`). A picture's rows
  were once kept there as bytes, and on that machine the stores went into
  the dictionary
- A word keeps its working values on the stacks. The top of the data
  stack, the cell under it and the top of the return stack are directly
  available, and a word that needs more is built from smaller words. A
  word that keeps its working values in variables can only be in use once
  at a time. State that many words read (the port) belongs in variables. A
  value that one word stores for the next word to fetch does not. Where a
  word needs a great many values (`copy-bits`, `draw-text`), see how those
  two save theirs when they are run while they are already running
- A variable is in the cells that one instruction can reach, starting at
  `VAR_BASE`. Put data that is not variables (a font, a table, a buffer)
  between `bulk` and `unbulk`, or between `high` and `unhigh` if it is
  only reached one cell at a time, so that it does not push variables out
  of reach
- A word that another file uses has a header (`header name : name`), so it
  can also be typed at the prompt. A word used only in its own file has no
  header, and `cross.fs` keeps its name as a label. On the machine, an
  included file's own names exist only until the file has been read
  (`files.fs`). So if a file uses another file's word that has no header,
  the nucleus cannot find that word. `.public` lists every such use that
  `cross.fs` saw. A macro that another file uses needs a word with a
  header that expands it (`header FACE : code-face FACE ;`, `text.fs`), as
  `names.fs` has for `cross.fs`'s own macros. A header's name can be found
  only after its file has been read. Inside the file, the name still means
  what it meant before, as it does in `cross.fs`
- A small word is inlined in place of a call only where the result is at
  most as long as the call. Where code runs so often that the call and its
  return matter, write `in-place` before the call to keep the word inline
  there (`in-place -`). Use `in-place` only after measuring how often that
  code runs and what the work costs with it and without it
- Names ignore case, on the machine and in `cross.fs`: `LINE` and `line`
  are the same name. A macro (`::`) and a word are looked up in the same
  place. A word that took a macro's name would hide the macro from every
  later file, so `cross.fs` stops at the file that defines such a word. In
  the other direction, a macro defined after a word of the same name would
  take over the word's name, so `cross.fs` refuses that macro. The one
  exception is a baseword: a word may have a baseword's name, because that
  is how the baseword gets a body
- Defining a name that already exists is allowed. Forth finds the newest
  definition, and code compiled afterwards uses that one. `cross.fs`
  records each such name, grouped by what it hides, in `.shadows` beside
  the image, and `quick` and `check` print the list before their tests
  (`util/again.c`). Read that list to catch a name that was redefined by
  accident, instead of finding it by debugging. Two words with the same
  name are told apart by the order in which the source defines them
  (`.lst`), and the tools pair the words of two builds the same way
  (`util/cut.h`)
- A search goes through the dictionary one header at a time, starting at
  the header `forth` holds and following the links (`sfind`, `nuc.fs`). A
  header created on the machine is linked in front of all the existing
  ones, so a search finds the newest definition of a name. The image's
  headers are linked in order of how often the system's source uses each
  name (`headers-ordered`, `cross.fs`), not in the order they were
  defined. `words` and the Words window list the dictionary in search
  order. Code that needs a word's position in the dictionary reads the
  dictionary itself (`first_header` and `read_header`, `vm_opcodes.c`)
- Code does not draw when something changes. It marks the area that is out
  of date (`inval-rect`, `win-inval`), and the event loop repaints it.
  Three things are drawn straight onto the screen instead: the pointer, a
  dragged window's outline, and a window's overlay (`'overlay`, wm.fs).
  They are removed before anything is painted and put back afterwards.
  Every drawing word takes coordinates in the port, and only `display.fs`
  knows how the display is laid out. (A port here is the record of the
  drawing state: origin, clip, pattern and font. The screen has one and
  each window has one. It is a different thing from a port of the machine)
- A table is written in the source file that defines it, in a form a
  person can read: a font is its strike, one row at a time; the key map is
  rows of keys; an icon is rows of `#` and `.`. Do not generate a table
  from another source
- The kernel's code must fit in the kernel ROM (`KROM_BYTES`) at every
  cell size. The VM prints the kernel's length when it loads an image
  (`CODE=`). When the kernel does not fit, look for code that is written
  twice before moving a word out of the kernel
- The kernel favours size over speed. `um*` takes one step for every bit
  of a cell, whatever the numbers are. `u/mod` has a single shortcut, for
  a small dividend. `c@` goes through `field@`. A name is converted to
  text one code at a time. A program that multiplies, divides or reads
  characters heavily defines its own word for that. Time what such a word
  costs; do not estimate it
- A baseword of two instructions is compiled inline wherever it is
  written. In the kernel it can be made `callable` instead: it is then
  compiled as a call wherever the peephole does not join it with its
  neighbours. The top of `nuc.fs` lists which basewords are `callable`. A
  baseword that frequently run words are built from stays inline. Decide
  by measuring (the target `bench-draw`), not by estimating. `2drop` and
  `io!` are each one instruction that cannot hold the return. They are
  `callable` too: each stays inline except at the end of a definition,
  where it is compiled as a jump to the word
- A shift count is taken modulo 64 at every cell size. So on a 16-bit or
  32-bit machine a shift by the cell's width gives zero, and a shift by 64
  leaves the value unchanged

## The order of the code

A list includes its files in order (`system.fs`, `picture.fs`, the end of
`nuc.fs`), and `cross.fs` places their code in that order. The order
matters. It is not the order things happened to be written in.

- Code past the kernel is in pages. The order decides which page a file
  is on, which files share that page, and which pages have room. A call
  within a page, or from any page into the kernel's page, is one
  instruction. A call to a word on any other page pushes the word's
  address and calls `execute`. That is longer where it is written, and
  slower every time it runs
- Code that everything goes through comes first, straight after the
  kernel. The nucleus's peephole (`join.fs`, `compose.fs`) compiles every
  definition made on the machine, and it uses only the kernel. Do not
  place it after the applications, where it would share the last page
  with them and get the room that page has left
- The file placed last gets the room that is left. So place last the
  file that needs the least
- When a page runs out of room, look at the order first: what is on that
  page, why it is there, and whether it should be. Do not split a file,
  set part of a file aside, or make anything smaller in order to keep an
  order that was wrong
- Every page can call the upper half of the kernel's page with one
  instruction. Do not put code there because it did not fit elsewhere.
  The upper half is for the words that other pages call most. Such a
  word stays in its own file, between `nearby` and `nearby-end`
  (`cross.fs`). It is compiled the way it would be on its own page, not
  the way the kernel is compiled for size
- A word placed there calls its old page the long way. So the words it
  calls move with it, down to the words that call nothing. Otherwise the
  move loses more than it saves. A group stops at a word that is the
  entry to a loop of many words (`rgn-op` to the merges, `draw-chars` to
  the text). `disasm_hex IMAGE.hex --layout --counts WORK --near NAME...`
  shows what the calls would be if the named words were there. Run it
  before moving a word, and run `listing-16` after
- A rule about where one thing goes is not a rule about another thing.
  The decompiler's table is the last data in the image, so that its
  length moves nothing else (`system.fs`). That rule places the table. It
  does not place the code that reads it
- Before changing an order, and again after, run `listing-16` or the
  listing for another machine. It shows where code is, how much room each
  page has left, and which calls go from one page to another. A list
  includes its files in a single order: each file comes after the files
  it needs. Between files, the list names the page that the following
  files go on. A list can return to an earlier page, and code continues
  there from where that page's code ended (`code-page`, `cross.fs`), so
  a file can go on any page that has room for it
- A list of files contains no code of its own. Code that a list would
  define goes in a file, so that every word belongs to a file
  (`examine`)

## Measuring

- Measure before saying what a change will save, and before choosing what
  to change. Multiplying how often something is done by its assumed cost
  leaves out the loop around it, and leaves out whatever is rare and
  expensive
- Runtime is measured in cycles, not instructions. A memory read, a shift
  and a multiply each take more than one cycle. Where memory takes two
  cycles to answer (`LAT = 2`, the tests named `lat2`), so does a jump, a
  call, a return or a taken branch (the top of `rtl/hexaforth.v` gives
  the costs). The target `bench` reports cycles
- The target `bench` runs the benchmarks. Each piece of work is a file
  typed at the prompt, the same file on the Verilog CPU at every cell
  size. A benchmark checks what the work produced before it prints a
  figure for it. `cmake/bench.cmake` lists the benchmarks (`bench-draw`,
  `bench-dither`, `bench-gray`, `bench-m68k`, and `gates`). To find what a
  change costs, run the benchmark before the change and after it. The
  repository keeps no figures
- The target `peephole` measures the nucleus's compiler. It types the
  system's code at the nucleus and compares the result with what
  `cross.fs` compiles, by source file. It reports what each join saves in
  code size, and what each saves when the desktop and the drawing
  benchmark run. It also checks every join against the machine's step.
  Run it before and after a change to `join.fs` or `compose.fs`. A join
  that is rare in the source may be in the hottest loop, so how often a
  join is made does not show what it saves
- Anything the machine is compared with does the same work and is checked
  against the same model. The 68000 in `bench/m68k/` runs the machine's
  own dither, written in 68000 assembly, and produces the same bitmap
- A target that measures keeps its results (`util/keep.c`). They go under
  `build/kept/`, in a directory named for the measured image by a hash of
  its bytes, then the target's name, then the number of the run. The
  image and its names are kept beside them. A kept result is never
  overwritten. `build/kept/log.txt` has one line for each run, in order,
  with the commit the tree was at. To find what a change did, compare the
  results kept before it with the results kept after it. Do not rely on a
  remembered figure. A target that measures and keeps nothing is not
  finished
- Compare two profiles by word, not by their totals: the time the disk
  driver waits depends on the host. `compared WAS WAS.work IS IS.work`
  (`bench/compared.c`) lists two counts of work side by side, by word.
  `WAS` and `IS` are the directories that hold each image and its names:
  a build directory (`build/16`) or a kept one
  (`build/kept/IMAGE/image`). Read its output before believing a total.
  If the same words ran fewer times, the work was different, and the
  lower total is not a saving
- Do not state an estimate as a fact, and do not leave something untested
  to avoid finding out

## Making the kernel smaller

To find what is in the kernel, and what a change to it saves, read the
image, not the source, at every cell size. The peephole and the inlining
of small words make a count taken from the source wrong. The functions
that read an image are in the tree's C, and a test or a tool includes
them:

- `util/words.h`: an image's words, and the length of each
- `util/cut.h`: its calls, jumps and branches, and where each goes
- `util/reach.h`: what reaches each word
- `util/layout.h`: its pages of code, and how much of each page every
  source file has

The target `examine` prints what `util/layout.h` finds in the nucleus's
image: the kernel's length, what is on each page, and each file's words
and code. `examine-16`, `examine-32` and `examine-64` print the
same for each machine in `util/machines`, and `examine-virt` prints it
for QEMU's image. QEMU's ROM includes the virtio disk's driver, so it is
the fullest.

- Before making a word shorter, find what reaches it (`util/reach.h`):
  calls and jumps to it from the kernel and from every other page, its
  address pushed as a number or stored in a cell of data, and the word
  before it falling through into it. `kernel_test`, which the test
  `kernel` runs, fails if a kernel word has no header and nothing reaches
  it. A header that a newer header of the same name shadows does not
  count, because a search finds the newest (`.shadows`). Run by hand
  with `typed`
  (`build/cmake/kernel_test build/64/nuc.hex build/64/nuc.tables typed`),
  it lists the words that are reached only by typing their names. Run
  with `to` and some names, it lists what reaches each of those words,
  and from which word. The count does not decide whether a word that is
  only typed should stay
- A word that ends by running another word can be placed directly before
  that word and fall through into it, with no jump and no return of its
  own. It ends with `;into` and the other word's name, and `cross.fs`
  stops if another definition comes between the two. Only one word can
  be directly before another: choose the one that runs most often
  (`disasm_hex IMAGE.hex NAME --counts WORK` gives the counts), and let
  the others jump. `;fallthru` is different: it ends a definition that
  needs no exit because its last word returns for it (`inline;`,
  `offered`)
- A word with no instructions that falls through is a second name for
  the word after it: `begin` is `landing` and `again` is `code,`
  (`nuc.fs`). Such a word must have a header, and `cross.fs` stops at
  one that has none. Where an address has both a label and a header, the
  label names the word whose code is there. `see` and `disasm_hex` show
  that name, and `disasm_hex` lists the headers under it (`util/words.h`,
  `by_place`)
- Two names for the same instructions share one body, with a header for
  each: a second `header` line before the definition (`1+` and `cell+`,
  `nuc.fs`). Code that goes from a word's address to its name then finds
  only one of them: `see` shows the first, `disasm_hex` shows both, and
  the Words window keeps the one that was chosen. Before giving a word a
  second header, check everything else that goes from a word's address
  to its header
- A word that is reached only through another word receives only what
  that word passes on. `pcompile,` runs before the kernel's `(compile,)`,
  and itself inlines every small word whose body is at most as long as a
  call to it. To find which cases of the second word still receive a
  word, apply the first word's rule to every word in the image. A case
  that receives none is dead code, however much of the second word it
  takes up
- A path that a machine of one cell size can never take is still in that
  machine's kernel, unless the source leaves it out at that size
  (`W64? [if]`). Run that machine to confirm the path is never taken
- Try each change by itself, in a copy of the tree, and build it for
  every cell size. Its saving is the difference in the kernel's length.
  Before adding up what several changes save, build them together and
  measure the total
- A change that must leave a word's behaviour as it was is tested by
  checks in the tree (`test/prompt_test.c`). Each check is the text to
  type and the result it must leave. If no check types a word, write
  checks for it before changing it, and confirm that the unchanged kernel
  passes them. Cover numbers at the limits of a cell as well as small
  ones, a name that fills its cells exactly, and characters that are
  typed and then erased
- To find what a change costs in time, time work that runs the changed
  code. If the benchmarks do not run it, write a file that does. Type the
  file the way a benchmark's file is typed, with a mark around each piece
  (`rtl/tb.cpp`), at the unchanged kernel and at the changed one
- A word of the nucleus does not have to be in the ROM if no word of the
  nucleus calls it and nothing needs it before the image is in memory.
  The end of `nuc.fs` has those words, in the upper half of the kernel's
  page, where a call to one is still one instruction. It also gives the
  test a word must pass before it is put there. The tools measure a
  kernel's length up to where its code ends, and `cross.fs` refuses to
  build an image whose kernel ends outside its ROM (`rom-ends`)
- The targets `shorter-16`, `shorter-32` and `shorter-64`, and
  `shorter-virt` for QEMU's kernel, search a kernel's ROM for code that
  a shorter sequence would replace (`bench/shorter.c`, `--rom`). The
  search takes every straight-line run of each word, whole and in parts,
  and every loop. For each one it reports the shortest sequence it found
  that leaves the same result, and it runs both on `vm.c`. It finds
  adjacent instructions that a shorter sequence can replace while
  everything else stays where it is. It does not find code that is
  written twice. Find that by reading

## Building

CMake builds the C (`CMakeLists.txt`): `hexaforth` (the reference VM,
one program that runs as the machine its image is for), `hexaforth-debug`
(the same, with a trace of every instruction), `hexaforth_test`,
`kernel_test`, `fewer_test`, `basewords-fs-gen`, `generate-test-cases`
and `examine-image`. The target `examine` runs `examine-image` on the
image.

`CMakeLists.txt` names the project, sets the directories the build
writes to, and sets how the C is compiled. Everything else is in the
parts it includes, in this order. Each part covers one subject, describes
that subject at its top, and uses what the earlier parts define:

- `cmake/needs.cmake`: the programs and libraries the project needs from
  outside the tree
- `cmake/kinds.cmake`: the kinds of thing the build makes, each defined
  once
- `cmake/default-machine.cmake`: the two generated Forth files
  (`forth/folds.fs`, `forth/fused.fs`), the function that builds a
  system's decompiler table (`tabled`), the default machine's tests and
  report (`kernel`, `default-machine`, `examine`), and the tests that
  need no image
- `cmake/disks.cmake`: the tools that make a disk, the source that goes
  on one, and the pictures
- `cmake/machines.cmake`: the machines in `util/machines`, and for each
  one its images, disks, tests and reports
- `cmake/reports.cmake`: the first machine's reports
- `cmake/kernels.cmake`: the AArch64 kernels, and the desktop on QEMU
- `cmake/verilog.cmake`: the Verilog, its tests, and `synth`
- `cmake/bench.cmake` and `cmake/gates.cmake`: the benchmarks, and the
  CPU's size
- `cmake/said.cmake` and `cmake/clangd.cmake`: the targets `said`,
  `figures` and `clangd`
- `cmake/check.cmake`: `check` and `quick`. It is last because those two
  targets cover every test and every tool

To CMake the parts are one directory: `CMakeLists.txt` includes each part
and adds no subdirectory. So a file or a target that one part defines has
the same name in every later part. `clangd` checks the C that the parts
before it compile, so include a part that compiles C before
`cmake/clangd.cmake`.

`cmake/kinds.cmake` defines each kind of thing once. Add a new thing with
the function for its kind, not by copying another thing's commands:

- `image` builds an image. It is the only place that runs `cross.fs`
- `system` (`cmake/machines.cmake`) builds a system: a machine's images
  and their disks. It builds QEMU's system as well as the system of each
  machine in `util/machines`
- `tabled` (`cmake/default-machine.cmake`) builds a system's decompiler
  table
- `tool` builds a tool. `machine_tool` builds a tool that includes the
  machine or reads a machine's image. Such a tool is one program for
  every machine, and it gets the code every such tool shares from a
  library. `stepping_tool` builds a tool that includes `util/fuse.h`,
  which is also one program for every machine
- `report` builds a report. A report depends on whatever makes each file
  its commands name, so no report lists its dependencies

A build with no target builds every tool and then runs `quick`. After
such a build, every tool in `build/cmake` is up to date with its source.
Run a tool by hand only after such a build.

`cmake/needs.cmake` is the one list of the programs and libraries the
project needs from outside the tree. For each it gives what it is for,
and the Homebrew package and the apt package that provide it. When CMake
configures, it looks for each one and installs all the missing ones in a
single step, with Homebrew, or with apt as root. With
`-DINSTALL_NEEDED=OFF` it only looks, and reports what is missing. So a
system that has CMake and either Homebrew or apt needs no other setup.
Add a needed program to that file, and run it by the name the file gives
it, not by a name that might be in `PATH`.

CMake also compiles and runs everything else: the machines, the AArch64
kernels, the Verilog, the tests, the reports and the benchmarks. The
parts define what each thing is built from, once. Each part lists at its
top its targets and the options they take. Add a new tool, test or
report to the part it belongs to. Do not add a compiler command line in
a script, a script, or a Makefile. Code that reads what a program
printed, or computes something from it, is a function in the tree's C
that a tool includes, not a pipe of `grep` and `awk`:
`baremetal/symbols.h` finds a kernel's symbols, and `bench/figure.h`
reads what a test bench reported between two marks.

`util/machines` lists the machines the system is tested on, and CMake
reads it. The first machine has cells of 64 bits. The reports, `hello`
and the screenshot use it. It is also the default machine: the one
`vm_constants.h` describes when no machine is given, and the one
`forth/basewords.fs` defines. It is built once, in `build/64/`, with
that file. Its decompiler table is `forth/fused.fs`, and the targets
named for the default machine (`kernel`, `examine`) use that build.
`util/machines` and `vm_constants.h` both state its sizes: the test
`default-machine` generates the definition from the sizes in
`util/machines` and fails if it differs from `forth/basewords.fs`.

For each machine, CMake builds the definition that `cross.fs` reads, the
nucleus and its disks. It writes them to a directory of `build/` named
for the machine's cell width in bits (`build/32/nuc.hex`).

The tools and the tests are not built for a machine. Each is one program
in CMake's build directory (`hexaforth`, `prompt_test`), and it runs as
the machine its image is for. A program gets the machine's sizes as
values when it runs (`THIS_MACHINE`, `vm_constants.h`), not as constants
compiled into it. `cross.fs` writes an image's machine to a file beside
the image (`nuc.machine`), and a program that reads the image becomes
that machine (`machine_from`, `util/image.h`). A program stops if the
image has no such file and the program was given no machine. It also
stops if an image is for a different machine from the one it read
before. A tool built on `util/fuse.h` is one program too (`costs`,
`shorter`, `compared`): the machine it tries code on has a memory of its
own size, and the cell width of the image it is given.

The kernels are for the machine QEMU calls virt. That machine has the
sizes of the 64-bit machine and a disk of its own, so it has its own
image too, built in `build/virt/` (`machine-virt`). The kernels are
built in `build/kernel/`. The Verilog is built in `build/rtl/`. It is
tested on the machine that `vm_constants.h` gives for each cell size
when no machine is given (`build/default-32/`; with cells of 64 bits that
is the default machine).

None of that is built until a target needs it:

- `check` runs every test. Each test has what it needs built first
- `quick` runs the tests that need only the reference VM. A build with
  no target runs it
- the reports: `profile`, `shorter-loops`, `subtracts`, `peephole`,
  `pairs` and `phrases`, and `examine-16` and the same target for each
  other machine
- `screenshot` regenerates the README's picture of the desktop
  (`doc/desktop.png`)
- `hello`
- `seen` builds the tools that show a machine's display from the memory
  the reference VM writes (`screen`, `png`)
- `bench` and `gates`
- `run`, `demo`, `demo-boot`, `demo-test`, `demo-time` and `size` use
  the kernel that `KERNEL`, `STRICT`, `SCALE`, `MIPS` and `DISK` select
  (`cmake -DMIPS=5 build/cmake`)
- `synth`

`demo-boot` starts the machine with only its ROM in memory. The machine
reads its image from the disk and compiles its desktop from the source
there.

`gforth` builds an image from the Forth:

```bash
cd forth
gforth ./cross.fs ./basewords.fs ./nuc.fs      # writes build/nuc.hex and the files beside it
BUILD=../build/32/ gforth ./cross.fs ../build/32/basewords.fs ./nuc.fs   # writes the image there
```

The checked-in `forth/basewords.fs` defines the machine with cells of
64 bits. `BUILD` is the directory the image and the files beside it are
written to. It must already exist, and its name must end in `/`. The
source is a file in `forth/`, written as `./name.fs`, and the output
files take its name.

`BOOT=on` builds the image that a machine boots from a disk with: the
system without the files `apps.fs` lists. CMake builds it with
`boot-system-64` and the same target for each other machine, in
`build/64/boot/`. CMake also makes two disks for it: `apps.img`, which
`boot_test` starts the machine from, and `desk.img`, which the desktop's
checks start it from. `desk.img` also holds what the desktop keeps on
its first disk.

A disk has at most 1,024 blocks, the capacity of an 8-inch floppy, and
`mkdisk` refuses to make a larger one. So the pictures are on a disk of
their own (`build/pictures.img`, the target `pictures`). It is the
second disk of every machine that shows or checks the desktop, and the
viewer finds its picture there by its label (`doc/disk.md`, Several
disks).

`ROM` names the file with the part of the machine's ROM that is not the
nucleus: the driver for the disk the machine starts from
(`ROM=virt-disk.fs`). The default is `port-disk.fs`, the reference VM's
driver. A ROM is built for one machine, and `nuc.fs` names no
controller: a machine with a different disk has a different file, and
its own image.

`PEEPHOLE=off` builds the same image with the peephole off. That build
reads the `.inline` file of the build with the peephole on, and it
writes files with the same names. So it needs a directory of its own,
with that `.inline` copied into it. CMake does this in
`build/64/plain/`.

`FUSED` names the decompiler table to build an image with. The default
is `forth/fused.fs`, the default machine's table. Every other machine
uses the table CMake makes for it (`FUSED=../build/32/fused.fs`).

`build/cmake/hexaforth IMAGE.hex [DISK.img [MEMORY.out]]` runs an image
on the reference VM, as the machine the image is for. If `MEMORY.out` is
given, the VM writes its memory there when it stops. Give several disks
with commas between them; the first is disk 0 (`DISK0.img,DISK1.img`). A
block's number selects its disk (`doc/disk.md`, Several disks).

Beside an image, `cross.fs` writes these files:

- `.lst`: every word that has code, in the order the source defines
  them, each with the instruction it starts at and its name
- `.sizes`: code and data by file
- `.inline`: the calls it inlined and the loops it turned. A build with
  the peephole off reads this file, so build with the peephole on first
- `.machine`: the machine the image is for, as one line of its sizes. A
  tool that is given the image reads it
- `.tables`: the names for the decompiler, which go on the disk

It also writes what it saw of names as it compiled. Read these files
before giving a word a header or removing one:

- `.public`: the names the source gives a header, by the file that
  defines them. After them it lists every name without a header (a word,
  table, constant or macro) that a file other than its own uses. The
  nucleus would not find such a name when it compiles that file
- `.needs`: for each file, the files whose words it uses, and everything
  it needs through those files
- `.shadows`: the names that were defined when a definition of that name
  already existed, grouped by what each hides

`cross.fs` asks the machine three questions:

- which one instruction does the work of two or three
- which shorter run does the work of a run that has numbers or variables
  in it
- which one or two instructions do the work of a 0 pushed after ALU
  instructions, or of four to six ALU instructions (`fuse_few`)

`util/fuse.h` answers them. It includes `vm.c`. gforth compiles it each
time an image is built, using `libtool`, and `cross.fs` calls it through
gforth's C interface. `fuse.h` finds an answer with the machine's step on
terms and then runs the answer on `vm.c`. The build stops if the two
disagree. `fuse.h` saves the answers it found and ran in the tree's
`build/`, whatever `BUILD` is (`build/fuse-*.kept`). The file's name
comes from the text of `fuse.h` and of the machine, so a change to
either starts a new file, and the first image built after such a change
takes longer than the rest. gforth prints the C compiler's warnings
about `fuse.h` each time. Where gforth comes from Homebrew, the compiler
needs the path to Homebrew's headers. `cmake/kinds.cmake` sets it. By
hand it is `CPATH="$(brew --prefix)/include" gforth ...`.

Three files are generated and checked in:

- `forth/basewords.fs`: `basewords-fs-gen` writes it from
  `vm_constants.h` and `vm_opcodes.h`
- `forth/folds.fs`: the rules the nucleus's peephole applies.
  `util/joins.c` writes it, and reads no image
- `forth/fused.fs`: the words the decompiler shows for each instruction.
  `util/fused.c` writes it from a built image

`joins.c` and `fused.c` both include `fuse.h` and ask the machine. Each
of the three files has its own command in `cmake/default-machine.cmake`,
which lists what the file is made from. The target `forth-env` makes
`folds.fs` and `fused.fs`, and every nucleus that includes either is
built after it.

The decompiler's table is part of the image, so it cannot be made from
the image that contains it. The build takes three steps. It builds an
image with an empty table, in `build/64/untabled/` (`FUSED` names the
table to build with). It makes the table from that image. Then it builds
the nucleus with the table. The table is the last data in the image
(`system.fs`), so its length moves nothing else, and the two images have
the same instructions. The test `tables-64` checks that. Each field of
the table is sixteen bits, packed into cells (`w,`, `hdr,`; `ffield`,
`see.fs`). The table has an entry only for an instruction that some phrase
of up to four words names. The decompiler shows any other instruction by
its number, and `fused` lists those instructions when it makes a table.

Each machine has its own instructions, so each system that includes the
decompiler has its own table. `forth/fused.fs` is the default machine's.
The others are made the same way (`tabled`,
`cmake/default-machine.cmake`): for example `build/16/fused.fs`, from
the images in `build/16/untabled/`. A system's nucleus and its boot image
are then built with its table. A table covers three sets of
instructions:

- the instructions in the image, which `cross.fs` compiled
- the instructions the nucleus compiles from the boot image's code. The
  boot image is built a second time with the peephole off, and each run
  of that code is typed at the nucleus (`fused --made`), as the target
  `peephole` does for the whole system. The boot image is used because
  the whole system with the peephole off does not fit the machine of
  64K cells
- the instructions a machine compiles when it boots from its disk and
  compiles the applications there. The reference VM boots that image
  from a disk made from it, and `fused` reads the VM's memory once it
  has booted (`fused --booted`). The reference VM cannot use QEMU's
  disk, so the table of QEMU's system takes this set from the boot of
  the 64-bit machine, which has the same sizes

`basewords.fs` also has two pieces of the program's source that are
generated from the machine's definitions: the basewords as words with
headers, and the names of an instruction's fields for the decompiler.
Each is a macro whose body is ordinary source (`source:` and `;source`,
`cross.fs`). `names.fs` uses each macro at the place where its source is
compiled.

The nucleus's peephole has no table of joined instructions. It computes
what two or three instructions do together (`forth/compose.fs`). The
target `pairs` gives it every pair it can meet and checks each answer
against the machine's step. `peephole` does the same for what it joins
in the system's code.

## Testing

After a change, run the tests that cover it. After any change to
`cross.fs`, `nuc.fs` or the machine, run all of them:
`cmake -S . -B build/cmake` and then
`cmake --build build/cmake --target check` runs every test below and
prints a line for each.

`cmake --build build/cmake`, with no target, runs the target `quick`:
the tests that need only the reference VM (`on_the_vm`, in
`cmake/kinds.cmake`). They include every machine's prompt, boot, desktop
and drawing tests, and the comparison of the two builds. Run `quick`
while changing the source. Before their tests, `quick` and `check` print
the names the system's source redefines, and how much of each machine's
memory is used and free. Neither is an error: they are printed to be
read.

A test declares what it needs built (`needs`, in `cmake/kinds.cmake`),
and those things are built before the test runs, however it is run. Each
target a test names is built by a test of its own, `built-TARGET`, and
the test waits for that one. If a test's needs fail to build, the test
is not run, and it counts as failed. So a test never runs on something
built before the source changed. A new test must declare its needs:
CMake stops at a test that declares none.

- `hexaforth_test`: compiles each test in `test/tests.h` with the Forth
  compiler written in C (`test/compiler.c`) and with `cross.fs`,
  compares the instructions, and runs the test on the VM
- `kernel_test`: checks that something reaches every word in the kernel
  (`util/reach.h`). The test `kernel` runs it, for the default machine
- `tables-64`, and the same test for each other machine: each machine's
  decompiler table matches its image. `fused` regenerates the table from
  the finished nucleus, and the test fails if the file differs, listing
  the entries the file lacks. `tables-64` checks `forth/fused.fs`, the
  default machine's table. `tables-virt` checks the table of QEMU's
  system, and only `check` runs it
- `default-machine`: the first machine in `util/machines` is the default
  machine. `basewords-fs-gen`, given that machine's sizes, must produce
  exactly `forth/basewords.fs`
- `shadows`: `cross.fs` records each kind of redefined name, and records
  only those. `forth/shadowed.fs` is a program with a redefinition of
  each kind, and `test/shadowed.shadows` is the list `cross.fs` must
  write for it
- `loops`: `cross.fs` compiles control structures as the standard
  defines them. `forth/loops.fs` has a `while` ended by `until` and a
  loop with two `while`s. The test runs it on the VM and checks what it
  prints
- `refused-if`, `refused-then`, `refused-into` and `refused-empty`:
  `cross.fs` must refuse each of four programs (`forth/refused-if.fs`
  and the three files named for the other tests) and print the reason: a
  branch that never gets a target, a `then` with no branch to resolve, a
  word that falls through (`;into`) into a definition other than the one
  it names, and a word with no instructions that falls through and has
  no header
- `fewer_test`: checks the search for the shortest sequence that does
  the work of a run, or of one turn of a loop (`util/fewer.h`,
  `util/turns.h`). It gives the search runs whose shortest form is
  known, and runs each answer on `vm.c`. The test `fewer` runs it
- The target `check` runs every test CMake has (`ctest`). It prints one
  line for each test, and writes each test's full output to
  `Testing/Temporary/LastTest.log` in CMake's build directory.
  `ctest -V -R NAME`, run in that directory, runs the tests whose names
  contain `NAME`, building what they need first, and prints their
  output. `check` runs the tests above, and the ones below
- On each machine in `util/machines`, with cells of 64, 32 and 16 bits,
  each with a display and `STRICT`: the prompt, comments and a block read
  as source, the machine started from its ROM and a disk, the desktop,
  the drawing words, regions, the dither and the TIFF reader
  (`prompt-32`, `boot-16`, `desktop-16`, `display-64-3`). The desktop's
  checks run a second time on the machine started from its ROM alone,
  which reads its image from the disk and compiles the applications from
  their source there (`desktop-booted-16`). A test that uses random
  input runs once for each seed, and the seed is the last part of its
  name. The CMake option `SEEDS` selects the seeds. Code is cell-size
  safe only if these tests have run it
- `kernel-hexaforth` and `kernel-hexaforth-fast`: each AArch64 kernel
  against `vm.c`
- `qemu-hexaforth-display-strict`: the desktop on QEMU with the small
  kernel. The test drives the desktop and checks its screen. The target
  `demo-test` does the same with the kernel selected in CMake
- `qemu-hexaforth-rom-display-strict` and
  `qemu-hexaforth-fast-rom-display`: the same checks on QEMU, with each
  kernel started with only its ROM in memory. The machine reads its
  image from the disk through the driver in that ROM, and compiles the
  applications and the desktop from their source there. This is the only
  test of the fast kernel's desktop
- `verilog-cases-c64` and `verilog-session-c64` (and `c32`, `c16`; each
  with `-lat2`, where memory takes two cycles to answer; and
  `c64-lat2-krom`, with the kernel in a ROM): the Verilog against `vm.c`,
  on the cases and on a session typed at the nucleus; and `verilog-memory`
- `builds`, which `check` also runs: run it after any change to a
  compiler or a peephole. It compares the image with the image built
  with the peephole off. Every straight-line run must leave what the
  plain run leaves, and every call, jump and branch must go where the
  plain one goes. It fails if the two images have the same number of
  instructions, because then the plain build did not finish
- To check that no code stores into the kernel, or goes outside memory
  or a stack: build the VM with `-DSTRICT` and with `-DROM_BYTES` set to
  the size of the kernel ROM, or use the small kernel with `STRICT`

The target `profile` shows where the desktop's instructions go. It works
the desktop on the reference VM that counts, through a table of things a
person would do, and reports the work of each by file, by word and by
kind of instruction (it writes `build/profile/desktop.counts`). Then it
reports what that work costs, in three files (`bench/costs.c`):

- `paths.txt`: each word charged with everything it calls. Read it
  before saying what a change will save
- `loops.txt`: the loops, by what they cost
- `vars.txt`: what the variables cost, for the files `PROFILE_FILES`
  names, or for all files

The profile ends with the encoding set against the work
(`util/survey.c`): the instructions the image uses, and the runs that a
shorter run would replace, which `cross.fs` left as they are.

The target `phrases` prints the phrase book. For every instruction that
some run of the machine's words does the work of, it gives the shortest
such run, whether `cross.fs` makes one instruction of it, and whether
each name means what it means in gforth.

The target `shorter-loops` is a separate step (`bench/shorter.c`). For
each loop that ran in the profile's work, it inlines the words the loop
calls and searches for the shortest sequence that does one turn,
wherever the values are kept. The peephole cannot find these. It runs
the profile first. It is slow: a loop takes many searches, and each
search looks at up to `SEARCH_STATES` states
(`cmake/default-machine.cmake`).

## Where things are

The machine, in C:

- `vm_constants.h`: the machine's sizes, the display, the kernel ROM's
  size, and the layout of the disk
- `vm_instruction.h`: the fields of an instruction
- `vm_opcodes.h`: the fields' names, the basewords (`FORTH_OPS`), and the
  codes of a name (`NAME_CODES_REPR`)
- `vm.c`: the machine
- `vm_loop.h`: the machine's loop and its operations on a cell. `vm.c`
  includes it once for each cell width, with the width a constant in
  each copy. `vm()` checks the machine's width once, when it starts, and
  runs the loop for that width. `vm.c` includes it once more with no
  fixed width, for code outside the loop that handles cells
- `vm_step.h`: what a literal or an ALU instruction does, defined once.
  `vm.c` runs it on cells, and `util/fuse.h` runs it on terms
- `main.c`: loads an image and runs it
- `vm_debug.c`: the trace
- `vm_stopped.h`: why the reference VM stopped, what stops it besides a
  halt, the environment variables that set those stops, and the report
  on a machine that stopped somewhere unexpected
- `vm_opcodes.c`: builds instructions from the definitions, and reads an
  image's dictionary in search order (`first_header`, `read_header`)

The tools, in `util/`:

- `baseword_fs_gen.c`: writes `basewords.fs`
- `disasm_hex.c`: prints an image's code word by word, formatted and
  named the way `see` does it, with every instruction's address, number
  and fields. On a terminal it prints in colour
- `stack.c`: prints the stacks, for the trace
- `hex2bin.c`: converts an image, or only its ROM, to the bytes a kernel
  includes
- `memimage.c`: converts an image to the files the Verilog loads
- `image.c`: reads an image
- `mkdisk.c`: makes a disk, with source formatted as its blocks. With
  `--boot` it makes a disk that a machine with only its ROM starts from
  (`doc/boot.md`), and with `--boot-desk` that disk is the desktop's
  disk as well. With `--disk` it makes a disk that is an index and its
  files, for the disks a machine has besides the one it starts from
- `lines.h`: parses a line of Forth source into words, strings and
  comments, for the tools that format source
- `wrap.c`: fits source to the lines of a block (the target `wrap`)
- `fuse.h`: finds the instruction that does the work of others. It finds
  each answer with the machine's step on terms and then runs the answer
  on `vm.c`. Its top explains what it trusts and why
- `joins.c` and `fused.c`: the two generators, which include `fuse.h`
- `phrases.c`: the phrase book. It includes `fuse.h` too
- `survey.c`: the encoding set against a profile. It includes `fuse.h`
  too
- `room.h`: storage that grows: a list, a file or a line of any length,
  and text. No tool or test has a fixed limit on what it reads
- `named.h`: a word of `vm_opcodes.h` by its name, an instruction's name,
  and the form of an instruction that the decompiler's table stores
- `pairs.c`: checks what the nucleus computes for two and for three
  instructions against the machine's step: every pair it can meet, and
  what it joined in the system's code
- `words.h`: an image's words, by header and by label, for the tools
  that read an image; the decompiler's table, read from an image; and
  how much of a word's code a booted machine compiled
- `typed.h`: the instructions the nucleus compiled from typed code, read
  from the output of `bench/typed.c`
- `cut.h`: cuts two builds of an image into the same straight-line runs
- `reach.h`: what reaches each word of an image: calls and jumps, its
  address pushed or stored in a cell, and the word before it falling
  through into it
- `kinds.h`: groups the instructions by their effect, identifies the
  instructions with a number in them, and formats an instruction as text
- `loops.h`: an image's loops, and one turn of a loop
- `fewer.h`: searches for the shortest sequence that leaves a wanted
  result. It searches on the step's terms and runs each answer on `vm.c`
- `turns.h`: searches for a shorter sequence that does one turn of a
  loop, in each of the ways one could
- `layout.h`: how an image is laid out: its pages of code, what each
  file has, the dictionary and the data. `examine.c` prints it
- `map.c`: where a machine's memory goes. It lists every part of an
  image, or of the memory the reference VM wrote when it stopped, in
  memory order, with the free space between the parts. The target
  `memory-map-64`, and the same target for each other machine, prints it
  for the image, the machine at its prompt, the boot from a disk and the
  desktop. With `--table` it prints one line for each part, with the
  space used and the space free (`memory-table-64`, and the same target
  for each other machine)
- `screen.c`: a machine's display, from the memory the reference VM
  writes when it stops
- `png.c`: writes a screen as a PNG. The writer itself is `png_file.h`,
  which a tool with a bitmap of its own includes
- `clangd.c`: reports what clangd, the language server, found in the C,
  from what it printed for each file (the target `clangd`). Its report
  must be empty
- `said.c`: finds the file names the tree's text mentions and checks
  that each file exists (the target `said`). Run it when a file is
  renamed or removed
- `keep.c`: saves what a target measured, filed under the measured
  image, where the next measurement does not overwrite it
- `again.c`: lists the names a source redefines, from the `.shadows`
  file beside each machine's image, each name once. `quick` and `check`
  print this list

The tests, in `test/`:

- `tests.h`: the tests
- `compiler.c`: the small Forth compiler
- `vm_test.c`, `main.c`, `generate_test_cases.c`
- `nucleus.c`: runs the nucleus on `vm.c` and types at its prompt, from
  a fresh start or from the state the last typing left it in, and reads
  what the typing left on its stack and in its memory. A test that types
  at the nucleus is built with it
- `dither_test.c`: checks the machine's dither, read out of the
  machine's memory, against a dither that works one pixel at a time
- `dither.c`: that dither, which the test and the benchmarks share
- `gray_test.c`: checks a picture kept as gray and drawn through a mask
  against the same work done one pixel at a time in `gray.h`. The
  desktop's checks of the viewer's menu also use `gray.h`
- `source_test.c` and `source.fs`: comments, at the prompt and in a
  block of a disk that `mkdisk` made from that file
- `boot_test.c`, with `boot.fs`, `boot-more.fs` and `boot-error.fs`:
  the machine started from its ROM alone, and from its whole image. The
  cases are a disk that `mkdisk --boot` made from the image and those
  files; no disk, with the image sent to the console; and an error in a
  block. A store into the ROM stops the machine
- `prompt_test.c`: the nucleus at its prompt: its compiler, its
  arithmetic, variables, tasks and names. Each check is the text to type
  and the result it must leave
- `kernel_test.c`: the kernel's words and what reaches each. It includes
  `reach.h`
- `fewer_test.c`: the search for a shorter sequence. Its cases are runs
  and turns written as words, each with the shortest length the search
  must find
- `region_test.c`: regions against their pixels. The text typed at the
  machine is also applied to a plane of pixels, and every cell of every
  region is compared with the one form those pixels have as a region
- `display_test.c`: the drawing words. The text typed at the machine is
  also drawn one pixel at a time, and every pixel is compared after
  every step
- `tables.c`: reads the system's tables from the Forth source that
  defines them, for a test that needs a font, a pattern or the windows'
  positions. No test has its own copy of a table
- `shadowed.shadows`: the list `cross.fs` must write for
  `forth/shadowed.fs`, a program with a redefinition of each kind that
  is recorded (the test `shadows`)
- `deskcheck.c`: the desktop's checks. They are a table of actions with
  the keys and the pointer, and what must then be on the screen, for
  whatever machine runs them. To find a word's position in the Words
  window's list, it reads that machine's dictionary from the machine's
  memory (`desk_memory`)
- `deskvm.c`: the reference VM as that machine, with its two disks. It
  starts with its image in memory, or from its ROM alone and the first
  disk. The test stops it in its event loop to examine it, and then
  resumes it
- `desktop_test.c`: runs the checks on `deskvm.c`'s machine
- `tiff_test.c`: the TIFF reader. Each file is only a header and a
  directory, on a disk of random bytes. The machine dithers the file,
  and the test dithers the same bytes one pixel at a time
- `builds_test.c`: compares the two builds, run for run. It includes
  `fuse.h` and asks the machine
- `session_test.c`: compares what the Verilog sent in a session with
  what `vm.c` sent

The benchmarks, in `bench/`. The target `bench` runs them, and
`cmake/bench.cmake` describes each one:

- `draw.fs`, `dither.fs` and `gray.fs`: the work, as it is typed
- `pictures.fs`: finds the pictures the work uses, by their labels on
  the machine's disks
- `grays.c`: writes the picture's grays as files. They are on a disk of
  their own beside the pictures' disk
- `tiff.c`: reads a picture from a TIFF, for the tools here
- `check.c`: checks what the work produced, and then prints what the
  work cost. It also prints the benchmarks' table
- `figure.h`: prints a line of that table, and reads what a test bench
  reported between two marks
- `m68k/`: the same dither for a 68000, in 68000 assembly, and the 68000
  that runs it. It is the one directory that is neither C nor Forth,
  because it is what the machine is measured against
- `gates.c`: the CPU's size beside a Z80's and a 68000's, from Yosys's
  counts (the target `gates`)
- `packed.cmake`: converts the 68000's Verilog to a form Yosys reads

The measurement of the nucleus's compiler, also in `bench/` (the target
`peephole`):

- `runs.c`: cuts the image into straight-line runs, and prints each run
  as it was typed and as `cross.fs` compiled it
- `typed.c`: types the runs at the nucleus, and reports what the nucleus
  compiled from them
- `counting.c`: the reference VM, counting how many times the
  instruction at each address runs. It runs by itself, or as the VM in a
  test
- `idle.fs`: starts the machine and stops it. Its counts are subtracted
  from the counts of the work
- `weigh.c`: weighs the runs, and the nucleus's joins, by those counts

The profile and the other reports, also in `bench/`:

- `profile.c`: where the desktop's instructions go
- `work.h`: the work by address, in the format of the file the profile
  writes
- `files.h`: the source files an image is made of, from its `.sizes`
  file. It is the only C that reads that file
- `costs.c`: what the work costs: each word with everything it calls,
  the loops, and the variables
- `compared.c`: lists two counts of work, from two images, side by side
  by word
- `booted.c`: what a machine compiled when it booted, beside the same
  words as `cross.fs` compiles them, and the boot's instructions that
  are missing from the decompiler's table
- `finds.c`: the reference VM, watching `sfind`: the names looked up in
  the dictionary, and the headers examined for each name
- `shorter.c`: the loops that ran, each with a shorter sequence that
  does one turn (the target `shorter-loops`). With `--rom` it searches
  every run and every loop of a kernel's ROM (the targets `shorter-16`,
  `shorter-32`, `shorter-64` and `shorter-virt`)
- `uses.c`: where the work uses a small word, called or inlined, how
  often, and the value the word is passed. The target `subtracts` runs
  it for `-`, `1-` and `negate`. Its top explains how to run it for any
  other word
- `heap.c`: the reference VM, watching the heap. It runs by itself, or
  as the VM in a test. It reports what was allocated and to which word,
  how much of each piece was ever stored to, what was freed, and how far
  the heap reached. It also reports the highest cell of each stack that
  was used, what the return stack held then, word by word, and how many
  files were being read. The target `memory-map-64`, and the same target
  for each other machine, prints its report after each map
- `cycles.h`: the cycles the Verilog takes for each instruction

The toolchain and the nucleus, in Forth (`forth/`):

- `cross.fs`: the cross-compiler. It reads the machine from
  `basewords.fs` and derives the rest. It has the addresses of data,
  bulk, headers and code pages (`data-base`, `hdr-base`, `code-base`,
  `code-size`), which differ with the machine's size; headers and
  labels, and the search order of the image's headers
  (`headers-ordered`); the record of redefined names (`.shadows`); the
  peephole and the inlining of small words; and the tables of names
- `loops.fs`, and `refused-if.fs`, `refused-then.fs`, `refused-into.fs`
  and `refused-empty.fs`: test programs for `cross.fs`, separate from
  the system. `loops.fs` has control structures that must compile as the
  standard defines them (the test `loops`), and `cross.fs` must refuse
  the other four (the tests named for them). `shadowed.fs` is another
  test program (the test `shadows`)
- `nuc.fs`: the nucleus: the kernel on page 0, the interpreter and
  compiler, the dictionary, and tasks (`spawn`, `pause`). After the
  kernel it includes `join.fs`, the first thing on page 1, and then
  `system.fs` if the machine has a display. It includes the file `ROM`
  names into its ROM. At its end are its words that are in the upper
  half of the kernel's page and outside the ROM, with the test a word
  must pass before it is put there
- `port-disk.fs`, `virt-disk.fs`: the part of a machine's ROM that is
  not the nucleus, which is its disk driver. Each defines `disk?`, which
  the nucleus calls. `port-disk.fs` drives the controller at ports
  `$b0`-`$b2`. `virt-disk.fs` is for QEMU's virt: it gives the addresses
  of that machine's virtio devices and includes `virtio-disk.fs`, the
  driver for a virtio block device, written for any cell size. The
  ROM's driver is for the disk the machine starts from. `virt-disk.fs`
  also has, in the upper half of the kernel's page, the words that reach
  that machine's other disks with the same driver. Each disk is a device
  of its own. `reg.fs` has the three port words a driver is built from.
  The ROM includes it if its driver needs them, and otherwise
  `machine.fs` includes it
- `join.fs`: the nucleus's own peephole, for text typed at the prompt
  and for source read from blocks. It does these things:
  - joins two or three instructions into one. `compose.fs` computes the
    result by stepping them on a machine whose cells hold where each
    value came from
  - folds numbers, by running the instruction (`folds.fs`)
  - compiles a 0 as `false` and a -1 as `true`, which it joins like any
    other ALU instruction
  - applies rules that `util/joins.c` generates from the machine's step
    (`folds.fs` too). A rule replaces a `dup`, `r@` or `r>`, a push and
    an instruction with the push and one instruction. Others replace
    three or four instructions with two: `0 =` after an instruction,
    `over 0` before one, and a store after `r@ + r>`. One moves a
    literal add back from before a `+`. The table of rules is data, in
    fields of sixteen bits that `i@` reads (`folds-at`)
  - compiles the one instruction that fetches or stores a variable
    (`var,`)
  - inlines a small word where the inlined code is at most as long as a
    call to it, and leaves every other call to the kernel's `(compile,)`

  The words that move the return stack are in `join.fs` too. Each is an
  immediate word that offers its instruction to the instruction before
  it. The exit is the same
- The words of `cross.fs` that the system's source uses, so that the
  nucleus can compile that source too. Each is a header with the name of
  `cross.fs`'s word, on a word with a different name, because `cross.fs`
  lets no word of the program take the name of one of its own words.
  Four files are one text for both compilers: `cross.fs` compiles each
  for the nucleus, and gforth compiles the same file for `cross.fs`
  itself (`host-included`), which has no copy of its own
  - `literals.fs`: `d#`, `h#`, `c#`, `,"`, `char`, `[char]`
  - `conditional.fs`: `[if]`, `[else]`, `[then]`
  - `divide.fs`: `/` and `mod`, which source between `meta` and `target`
    uses as well
  - `pixels.fs`: `picture[` and its rows, and rows of pixels (`p,`,
    `prow,`, `lefts,`)
- The words each compiler implements in its own way:
  - `placement.fs`: `bulk` and `high`. `meta` and `target`: between
    them the machine compiles code into the scratch area, the end of the
    last code page, and frees it when the file has been read. The words
    that select a page: in a block they continue that page's code from
    where it ended (`pages`, `nuc.fs`), and typed at the prompt they do
    nothing
  - `macros.fs`: `::`, with `literal` and `s"` as they work in its body
  - `names.fs`: the kernel's words under `cross.fs`'s names, and a word
    for each macro, flag and constant that `cross.fs` defines with
    `both::`, `both-flag::` and `both-constant`, generated from that
    list
- `image.fs`: what the image tells the decompiler about itself, which
  may be compiled after the image is made (`forth0`, `tstamp`,
  `fused-at`)
- `apps.fs`: the applications, which are the end of the system's list:
  every file after the window manager and the console, and the page each
  file goes on. The image a machine boots from a disk leaves them out
  (`BOOT=on`). `cross.fs` still reads their source for that image, to
  count the names it uses, for the order of the image's headers (`says`,
  `system.fs`). That disk has the list as block 1, so that the boot
  compiles each file the list includes from source (`doc/boot.md`), at
  any cell size

The system. `forth/system.fs` includes its files and sets where each
code page starts. `nuc.fs` includes the list that `SYSTEM` names, or
`system.fs` if `SYSTEM` names none. The system has two parts. The first
is the minimum that compiles source from a disk and draws: the kernel's
page, the page of code that compiles, and the page of code that draws,
at every cell size. The second is the applications (`apps.fs`).
`forth/picture.fs` is another list: a system with only the picture
viewer, which leaves the 16-bit machine enough memory to dither the
picture at full size.

- `machine.fs`: device registers through the ports, memory above the
  image (`halloc`, `hfree`, `hreuse`, `hgive`), and byte order
- `blocks.fs`: the block interface, built on the kernel's vectors
  `'blk-read` and `'blk-write`: `block`, `cblock`, `blk-read`, `load`
  (which reports where an error in a block happened), `thru` and `.(`.
  When a definition in a block runs off its page of code, `reading`
  compiles it a second time on another page, and zeroes what it compiled
  on the page it ran off. So a block is read with almost none of a page
  held in reserve, where typed source has a kilobyte reserved (`cpr`,
  `nuc.fs`)
- `files.fs`: finds what is on the disks by label. A machine can have
  several disks, and a block's number selects the disk (`doc/disk.md`).
  `find-disks` asks each disk for an index when the machine starts.
  `labelled` returns the blocks of a label from the first disk that has
  it. `include` reads a source file that way. As it reads, it prints the
  file's label, its length in blocks, and a dot as each block starts.
  `header` is here too. An included file's names are its own: they leave
  the dictionary when the file has been read, except the names a header
  gives, which can be found from then on and not before. Where a disk
  has blocks reserved for them (its index's entry `own-names`, which
  `mkdisk --boot-desk` makes), a file's own names are written there as
  the file ends, and the decompiler uses them to show a word the boot
  compiled (`see.fs`). The kernel's ROM stores the words of the disk it
  finds in the vectors, from the driver its machine's build gave it
  (`nuc.fs`, `main`). No code in the nucleus or the system names a
  controller
- `virtio.fs`, `virtio-input.fs`: QEMU's keyboard and pointer
- `display.fs`: the display and all drawing on it: the port, fills,
  `copy-bits`, fonts and text
- `mono.fs` and `sans.fs`: one font each. `spin.fs`: the disk light's
  glyphs. `keymap.fs`: the keys. `icons.fs`: the tools' pictures
- `region.fs`: regions, and `scroll-rect`. Its top describes the form of
  a region. `display.fs` is the only other file that reads that form
- `console.fs`: the screen as the console alongside the serial line, and
  `boot`, which the kernel's ROM runs once it has read the system from a
  disk (`doc/boot.md`)
- `tiff.fs`, `atkinson.fs`: the TIFF reader and the dither. Both work
  one row at a time, and the dither produces black and white or sixteen
  levels
- `gray.fs`: a picture kept as gray in bit planes, which the 16-level
  dither makes. It is drawn through a mask that is stored on the disk,
  also as bit planes (Bayer's, or blue noise; `util/bluenoise.h` makes
  the masks and describes the layout). The viewer keeps its bitmap and
  its gray on the disk, not whole in memory. It makes each a block at a
  time and writes it to the disk, and paints the window from the blocks
  its clip needs (`view.fs`)
- `mandrill.fs`: the picture by itself
- `gfx.fs`: rectangles. `wm.fs`: windows, the pointer, the menu and the
  event loop (`'poll`, `on-event`, `win-front`, `win-back`, `'track`,
  `'moved`)
- `text.fs`: text stored in cells, one line at a time. A line starts at
  the start of a cell and ends at an EOL. Its indent and its faces are
  codes in the line. The file's words insert a line, find a line a cell
  at a time, and draw a line. The Forth window's text and the Words
  window's definition are texts of this kind. The top of the file lists
  the codes
- `term.fs`, `edit.fs`, `view.fs`, `calc.fs`, `paint.fs`: the Forth
  window, the block editor, the picture viewer, the calculator and
  HexaPaint. The viewer's menu is at the end of `view.fs`. Each of its
  lines makes the viewer's task redo the picture, either dithered or
  kept as gray and drawn through a mask
- `see.fs`: the decompiler and the Words window
- `desk.fs`: `windows`, `desktop`, `cold`; `viewdesk.fs`: `windows` for
  the `picture.fs` system, which opens only the picture's window

The AArch64 kernels, in `baremetal/`:

- `hexaforth.S` (the small kernel) and `hexaforth-fast.S` (decoded by
  table), which behave the same
- `ramfb.S`: the bridge that shows the display on QEMU
- `difftest_dump.c`: writes the cases that the kernels and the Verilog
  are run against. `difftest.c` runs a kernel on them
- `symbols.h`: finds the addresses of a kernel's own symbols, for the
  tools that include it
- `size.c`: a kernel's size, and the size of each part of it from one
  label to the next
- `qemu.c`: QEMU as a machine the desktop's checks can run on. It sends
  keys and the pointer, dumps the screen and reads the memory
- `guitest.c`: runs those checks on it
- `desktime.c`: reports when the desktop is ready and when its picture
  is finished, by watching the machine without sending it anything
- `block1.fs`: the contents of block 1 of the disk the desktop is given,
  which is the block the editor opens on

The Verilog, in `rtl/`:

- `hexaforth.v` (the CPU), `hf_mem.v` (the memory), `soc.v`, and the
  test benches
- `tb.cpp`: the test bench, which also times a piece of work. Its top
  explains how marks are made and how it serves as a disk
- `session.fs`: the text typed at the nucleus on the Verilog and on
  `vm.c`. `test/session_test.c` compares the two outputs
- `synth.c`: reports the result of a synthesis

The documents, in `doc/`:

- `desktop.png`: the README's picture of the desktop, which the target
  `screenshot` makes
- `hello.fs`: the window `DESKTOP.md` describes, which the target
  `hello` types at the reference VM
- `formatting.md`: how Forth source is formatted, from *Thinking Forth*:
  what is written by hand and what `util/wrap.c` does
- `boot.md`: how the machine starts from its ROM and a disk, and
  compiles the system from source on the machine: what is built, what is
  decided, what is undecided, and the order of the work. It is a plan,
  and its first part is built
- `disk.md`: block 0, the disk's index: what is on a disk, where, and of
  what type. Part of the design is built
- `dictionary.md`: how a name is found, what a search costs and which
  target reports that, what reads a header, and the changes to the
  dictionary that have not been made: what each is, what it would
  change, and the room it needs in the ROM
- `hardware.md`: the machine as chips: how deep other Forths' stacks
  are, where a stack can be kept and what each place costs, the pin
  count, how the CPU divides among several chips, and what the parts
  cost. It is notes of what was looked up and worked out, and none of it
  is built
- `superoptimizations.md`: each way the machine's work has been made
  shorter or faster, by hand or by the tools: where it is used, what it
  replaced, why in the machine's terms, and the commit that measured it.
  It also has what the search for shorter sequences
  (`bench/shorter.c`) has found: each loop turn before and after, with
  the stacks

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
- `main` and `quit` must be defined: the image starts with them.
  `cross.fs` compiles the image's first two instructions, which run
  `main` and then `quit`. `4 org` starts the program's code after them,
  at byte 4
- a number in a definition is written `d# 123` or `h# ff`
- `emit` sends a character to the console. A program like this one uses
  it to show its result
- an instruction written by its fields goes between `meta` and `target`,
  with the input first and `alu` last (`MODEL.md`, Instructions)
- the peephole joins what it can, and computes arithmetic on known
  numbers, so the image may have fewer instructions than were written

`cd forth && gforth ./cross.fs ./basewords.fs ./yourprogram.fs` writes
`build/yourprogram.hex`, for the machine with cells of 64 bits.
`build/cmake/hexaforth build/yourprogram.hex` runs it.
`build/cmake/disasm_hex build/yourprogram.hex` shows its code, one word
at a time and one instruction to a line. Each of the two is one program,
which runs as the machine the image is for. With words named after the
image, `disasm_hex` shows only those words. With two addresses
(`0 0x20`), it shows only the words with code between them.
`build/cmake/disasm_hex build/16/nuc.hex apixels` shows one word of the
nucleus. `less -R` keeps the colour, and `--no-color` turns it off.

## Adding to the machine

To name an instruction the machine already has, add a line to
`FORTH_OPS` in `vm_opcodes.h`. To name a field, add a line to
`INS_FIELDS`. Then rebuild `basewords-fs-gen` and run it, and the Forth
has the name. A change to what the machine does is a change to
`MODEL.md`, `vm.c`, both kernels and the Verilog together.
