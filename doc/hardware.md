# The machine as hardware

What the machine would be made of, as chips: how deep its stacks are
beside other Forths', where a stack can be kept and what each place costs,
what the pins come to, how the CPU divides among several chips, and what
the parts cost. These are notes of what was looked up and worked out. None
of it is built, and nothing here changes the machine (`MODEL.md`).

No figure of the tree's own is here: the targets that are named print
them. A figure here is of another machine or of a part, and where it is
from is at the end; or it follows from what the machine is defined to be.

## How deep a stack is

Each stack is `STACK_CELLS` cells (`MODEL.md`), and the tasks share them:
`spawn` gives a task a part of each stack, from the cell it is told. The
main task has from the bottom, and the viewer's tasks and the Forth
window's start further up (`view.fs`, `term.fs`). So what a task may use
is its part, and not the stack.

`memory-map-16` and the like say what the system uses: the highest cell
of each stack that a boot and the desktop reach, what the return stack
held then, word by word, and how many files were being read. A file that
is being read keeps cells on the return stack until it has been read, and
a file read from inside another keeps them again (`files.fs`). The
question of depth came from there: a dictionary of several chains would
keep a head for each chain among those cells (`doc/dictionary.md`).

Other Forths:

| | Data stack | Return stack |
|---|---|---|
| Gforth, unless it is told | 16K cells | 15K cells |
| Forth-83, the least a system is tested with | 64 bytes: 32 cells of 16 bits | 48 bytes: 24 cells |
| Forth 2012 | no least: a system says what it has (`STACK-CELLS`) | the same (`RETURN-STACK-CELLS`) |
| J1 | 33 | 32 |
| WISC CPU/16, of TTL | 256 | 256 |
| Novix NC4016 | 256, in RAM off the chip | in RAM off the chip |
| Harris RTX 2000 | 256, on the chip | 256, on the chip |

- A Forth that is a program on a computer has thousands of cells. The
  Forths that are hardware have about 32, or 256.
- Koopman measured programs as they ran: "a stack size of 32 will
  eliminate stack buffer overflows for almost all programs". The deepest
  of them reached 52 cells of the data stack and 39 of the return stack.
  Those are programs running. A system that compiles one file from inside
  another keeps more than a running program does.
- GreenArrays' chips have stacks that go round: when too much is pushed,
  the oldest is written over.

## Where the Verilog keeps a stack

In `rtl/hexaforth.v`:

- T, N and the cell under N are registers, and so are R and the cell
  under it.
- Under them each stack is a memory of `DEPTH` cells. It is written at
  one address, from the register above it, and read without a clock at
  two. An instruction that drops two cells takes two neighbouring cells
  from it in the one cycle.
- The memory's addresses come from the depth, which is a register, and do
  not wait for the instruction to be decoded. What is read goes only into
  the registers. The memory is not an input of the ALU, so it is not on
  an instruction's path.
- The depths are registers of eight bits whatever `DEPTH` is, and so are
  the depth a port is setting, what a depth port gives, and what the test
  bench compares (`rtl/tb.cpp`). `DEPTH` by itself does not make a deeper
  stack.

`vm.c` and the AArch64 kernels keep a stack in the host's memory, and
`STACK_CELLS` is how much of it.

## What a deeper stack costs

The two stacks are twice `STACK_CELLS` cells:

| Cell | Both stacks, 256 cells each | 512 cells each |
|---|---|---|
| 16 bits | 8,192 bits | 16,384 bits |
| 32 bits | 16,384 bits | 32,768 bits |
| 64 bits | 32,768 bits | 65,536 bits |

What that is depends on where it is kept.

- **In the FPGA's LUTs, as it is now.** On the ECP5 that `synth` places
  for, a memory read without a clock is made of RAMs of 16 by 4 bits,
  each six LUTs, and a stack is laid once for each address it is read
  at, which is twice. So the stacks' cells double with the depth. `synth`
  says what the system comes to, and the first lines of what nextpnr said
  beside it (`nextpnr.log`) say how many of its LUTs are RAM: those are
  the stacks' cells, and a good part of the whole system.
- **In the FPGA's block RAM.** A block is 512 cells of 36 bits, with one
  address that is written and one that is read. A stack of up to 512
  cells is one block for cells of 16 or 32 bits and two for 64, for each
  address it is read at, so depth is free up to 512. A block is read on
  a clock: it wants the address a cycle early. That is the depth as it
  will be, which is what goes into the depth register, so it need not
  cost a cycle; but a cell that is pushed down and wanted back in the
  next cycle has then to be passed round the RAM. This is from reading
  the Verilog, and is not built.
- **On a chip, as RAM cells.** A cell of static RAM is six transistors a
  bit, which is one and a half gates of four, and one read at two
  addresses is more. By that count the stacks at 256 cells are already
  more than the CPU that `gates` counts, at every size of cell, and at
  512 they are twice that. The Novix's gate array could not hold its
  stacks in 1985; the RTX 2000 after it had them on the chip, 256 cells
  of 16 bits each.
- **On a chip, as flip-flops.** A flip-flop is four gates by the rule
  `gates` counts by, before what chooses a cell: several times the CPU
  at 256 cells. A stack this deep is not made of flip-flops.
- **In RAM chips beside the CPU.** Depth is free up to what a chip
  holds. The static RAM of 1980 is 1,024 by 4 bits: the 2114A, at 100 to
  250 ns, and the 2148, new that year, at about 70. One chip is four
  bits of a stack 1,024 cells deep, so a stack is a chip for every four
  bits of its cell, whether it is 256 cells or 1,024.

## A machine of 1980

A stack this deep is RAM chips beside the CPU in 1980, and that need not
cost a cycle. The arrangement is the Novix NC4016's of 1985, with the
parts of five years before:

- the top of each stack in the CPU's registers, as the Verilog has it;
- the rest of each stack in its own static RAM, with its depth as the
  address.

The Novix did all but a fetch, a store and a long literal in one cycle,
at up to 8 MHz, with its data stack on a bus of its own, read or written
"in parallel with other operations".

No cycle is lost because of where the memory is in the Verilog's cycle
(above): its address is known as the cycle starts, its cell is wanted
only as the cycle ends, and nothing of the instruction goes through it. A
RAM chip of 1980 has no clock: an address goes in and the cell comes out,
which is what the Verilog's stacks do already. It has only to be as quick
as the cycle, and the cycle is at least a read of the main memory, since
an instruction is fetched in every one.

A cycle would be lost in three cases:

- **An instruction that drops two cells** (`!`, `2drop`) takes two cells
  from the stack's memory in one cycle, and one RAM gives one. Either the
  RAM is two banks, the even cells and the odd, which is twice the chips
  and a second data bus on the CPU's pins, or such an instruction takes a
  cycle more. A store took two cycles on the Novix.
- **RAM slower than the cycle.** `LAT` says how many cycles the main
  memory takes to answer. Nothing says it of a stack.
- **Stacks kept in the main memory,** as the 8086 and the 68000 keep
  theirs. Every refill is then a read of memory, and a pop costs what `@`
  does.

## Pins

With the stacks in RAM the CPU has three buses, each a cell wide: one for
each stack's RAM and one for the main memory's data. That is 48 data pins
for cells of 16 bits, 96 for 32 and 192 for 64. The memory's address and
the two depths are more: for the 16-bit machine, whose memory is all a
cell can address, about 80 pins before the clock, control, the console
and power.

- None of the three can be shared. An instruction is fetched from the
  main memory in every cycle, and `>r` and `r>` use both stacks' memories
  in the one cycle: one stack refills from its RAM as the other pushes
  down into its own.
- The depths need not be pins. The WISC CPU/16 had each as a counter of
  eight bits beside its RAM, which leaves the CPU a line to count up and
  one to count down.
- The kernel's ROM adds none. It is on the main memory's bus, or with
  `HF_KROM` it is in the CPU as logic.

What packages had: the 68000 of 1979, 64 pins; the 68020 of 1984, 114;
the Novix of 1985, 121; the 80386 of 1985, 132. So:

- cells of 16 bits are one chip in 1980 only with the depths counted off
  the chip and the memory's address and data on the same pins, as the
  8086 has them;
- cells of 32 bits are one chip from about 1984, and in 1980 a board, or
  a set of chips;
- cells of 64 bits are more pins than a processor of the 1980s had.

What a pin costs:

- **A package.** In 1980 the pins are along two sides, a tenth of an inch
  apart, and 64 of them are over three inches. More needs a grid of pins
  under the package, which came in the years after and costs more: the
  package, its socket, and the layers of board under it.
- **The edge of the die.** A pin is a pad, and pads go round the edge. A
  pad is far bigger than a gate and does not shrink as gates do, so the
  pins say how small a die can be. A small CPU with many pins is a large
  die that is mostly empty.
- **A driver.** A signal that leaves the chip drives a trace and other
  chips, a load a hundred times a gate's or more. Its driver is big, slow
  and takes power, and a bus of them switching together disturbs the
  chip's own supply.
- **Time.** A stack in RAM off the chip is in the cycle as out through a
  pad, the RAM's access, and back in through a pad.
- **The board.** Every pin is a trace and a joint.

So with three buses it is the pads, and not the gates, that say what the
chip costs. The ways out each cost something else: a bus shared between
two uses costs cycles, and stacks on the chip cost transistors.

## Several chips

The CPU is cut across the cell and not by what its parts do. A bus is a
cell wide whatever is at the end of it, so a chip that has one part of
the CPU has nearly as many pins as one that has all of it. Cut into
slices of a few bits, a chip's pins do not grow with the cell. The Am2901
of 1975 is this cut for a machine of registers, four bits and 40 pins a
chip, and the Lilith of 1980 was built of them.

| Part | How many | What is in it |
|---|---|---|
| A slice of four bits | one for every four bits of the cell | its bits of T, N and the cell under N, of R and the cell under it, and of the program counter; of the choice of IN, the logic and the adders; and its lines of each bus: the two stacks' RAM, the memory's data and the memory's address |
| Control | one | when an instruction is done: the waits for memory, for a shift and for the console; the two depths, which are the stack RAMs' addresses; the ports |
| A stack's RAM | one chip of 1,024 by 4 bits for each slice | its four bits of every cell under the registers |

What the machine's own form gives:

- The instruction is the control word. Each slice reads the same 16 lines
  of the instruction and makes its own selects of its fields, as `dec`
  does in the Verilog, which is a function of the instruction and of
  nothing else. There is no microcode, and no bus of controls to carry.
- Cells of 16, 32 and 64 bits are the same chips: 4, 8 or 16 slices, as
  the Verilog is one text and `W`.
- The RAM is in slices already.

What goes from slice to slice:

- the carry of the adder, which is `+`, `<` and `u<`, and the carry of
  the add of a literal. It ripples, or a chip that looks ahead takes it,
  and it is what makes the cycle longer as the cell is wider;
- "all zero", for `=` and for the T a conditional jump tests: each a line
  that any slice can pull down;
- the flag, which goes back to every bit, since true is every bit set;
- a literal, which is 12 bits put at bit 0, 12 or 24: each slice takes
  what of it is its own, from the instruction's lines;
- the program counter and R, which meet only in the low 16 bits, at a
  call and a return.

What does not cut into slices:

- A shift by T. In one chip the Verilog works it out in a cycle of its
  own. Across slices it is a bit a cycle through the neighbours, or a
  shifter that has the whole cell on its pins.
- The multiply. It is a step for each bit, as the kernel's `um*` is, or a
  chip of its own. `gates` counts the CPU without it as well.

An instruction that is one cycle is one cycle still, and the cycle is
longer: the pads and the carry are in it. A shift and a multiply cost
cycles by their size unless they have chips of their own.

A count of the Verilog's signals makes a slice of four bits about 56 pins
and one of eight about 71, so in a package of 1980 it is four. That is a
count and not a design.

## What the chips cost

TTL costs nothing before the first machine and a good deal for each one.
A chip of one's own costs a good deal before the first and little for
each. A source of 1981 has a gate array begin to save after about a
thousand pieces.

| | Before the first | Each |
|---|---|---|
| TTL, in British advertisements of 1980 | nothing | a package of gates 11p to 18p; a RAM of 1,024 by 4 bits, 300 ns, £2.50 to £3.00 |
| A gate array, Ferranti's in 1982 | £5,000 a design on Ferranti's design system, which was £46,500, or £15,000 for Ferranti to do it | £1 to £2 a chip in high volume |
| A chip designed from nothing, as the Z80 and the 68000 | not found | not found |

- Besides its parts, TTL costs board, power and joints. HP in 1984 had a
  gate array at a fifth of the space, a quarter of the power and half the
  cost of the discrete circuit it took the place of, and the one array of
  Sinclair's ZX81 took the place of 18 chips of the ZX80.
- What an array held: about 1,200 gates around 1980, 4,000 as Ferranti
  delivered them in 1983, and nearly 8,000 in a CPU of HP's in 1985. The
  Novix was one array of under 4,000 gates. By `gates`' count the CPU of
  16 bits is several arrays in 1980, two in 1983 and one by 1985.
- As TTL, a package holds from four gates to some tens of them, so the
  same CPU is between a hundred packages and several hundred. Nothing
  here counts them. The WISC CPU/16, which is a simpler machine, was
  under a hundred with its memory, on two boards, and took three cycles
  of 280 ns for an instruction on average.

Not found: American prices of 1980 for TTL and for the Am2901, what an
array cost each in small numbers, and what a chip designed from nothing
cost before the first one.

## Not decided

- Whether the stacks are made deeper, and what part of them a task has.
- Whether the Verilog's depths are to follow `DEPTH`.
- Whether a stack is to be something the CPU reads at one address and on
  a clock, and what an instruction that drops two cells costs then.

## Where the figures are from

- Philip Koopman, *Stack Computers: the new wave* (1989): the WISC CPU/16
  (4.2), the Novix NC4016 (4.4), the Harris RTX 2000 (4.5) and how deep a
  stack needs to be (6.4):
  <https://users.ece.cmu.edu/~koopman/stack_computers/contents.html>
- James Bowman, *J1: a small Forth CPU core for FPGAs*:
  <https://www.excamera.com/files/j1.pdf>
- The FORTH-83 Standard, 7.2:
  <https://www.complang.tuwien.ac.at/forth/fth83std/FORTH83.TXT>
- Forth 2012, Usage requirements:
  <https://forth-standard.org/standard/usage>
- Gforth's manual, Invoking Gforth:
  <https://www.complang.tuwien.ac.at/forth/gforth/Docs-html/Invoking-Gforth.html>
- GreenArrays' stacks: <https://colorforth.github.io/stack.htm>
- The 2114A:
  <https://arcarc.xmission.com/Web%20Archives/ionpool.net%20(Dec-31-2020)/arcade/tech/2114A_SRAM.html>
- The 2148:
  <https://www.computerwoche.de/article/2881222/neues-ram-in-hmos-technologie.html>
- The ECP5's block RAM, as Yosys maps a memory to it:
  `techlibs/ecp5/brams.txt` of Yosys
- Packages: Wikipedia, "Motorola 68000", "Motorola 68020" and "i386"
- The Am2901:
  <https://www.righto.com/2020/04/inside-am2901-amds-1970s-bit-slice.html>
- The Lilith: <https://en.wikipedia.org/wiki/Lilith_(computer)>
- Gate arrays, Ferranti's prices and HP's comparison:
  <https://en.wikipedia.org/wiki/Gate_array>
- The ZX81's array: <https://en.wikipedia.org/wiki/ZX81>
- Prices of 1980: the advertisements of *Practical Electronics*, October
  1980, *Everyday Electronics*, September 1980, and *Electronics Today
  International*, January 1980 (worldradiohistory.com)
