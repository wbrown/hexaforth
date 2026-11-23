# The machine as hardware

What the machine would be made of, as chips: how deep its stacks are
compared with other Forths', where a stack can be kept and what each place
costs, how many pins it needs, how the CPU divides among several chips,
and what the parts cost. These are notes of what was looked up and worked
out. None of it is built, and none of it changes the machine (`MODEL.md`).

This file has no figures measured from the tree: the targets it names
print those. A figure here either describes another machine or a part,
with its source at the end, or follows from the machine's definition.

## How deep a stack is

Each stack is `STACK_CELLS` cells (`MODEL.md`), and the tasks share them.
`spawn` gives a task a part of each stack, starting at the cell it is
given. The main task starts at the bottom, and the viewer's tasks and the
Forth window's task start further up (`view.fs`, `term.fs`). So a task can
use only its part of a stack, not the whole stack.

`memory-map-16`, and the same target for each other machine, prints what
the system uses: the highest cell of each stack that a boot and the
desktop reach, what the return stack held then, word by word, and how many
files were being read. A file that is being read keeps cells on the return
stack until it has been read, and a file read from inside another file
keeps its own as well (`files.fs`). That is where the question of depth
came from: a dictionary with several chains would keep a head for each
chain among those cells (`doc/dictionary.md`).

Other Forths:

| | Data stack | Return stack |
|---|---|---|
| Gforth, by default | 16K cells | 15K cells |
| Forth-83, the minimum a system is tested with | 64 bytes: 32 cells of 16 bits | 48 bytes: 24 cells |
| Forth 2012 | no minimum: a system reports its size (`STACK-CELLS`) | the same (`RETURN-STACK-CELLS`) |
| J1 | 33 | 32 |
| WISC CPU/16, built from TTL | 256 | 256 |
| Novix NC4016 | 256, in RAM off the chip | in RAM off the chip |
| Harris RTX 2000 | 256, on the chip | 256, on the chip |

- A Forth that is a program on a computer has thousands of cells. The
  Forths that are hardware have about 32, or 256.
- Koopman measured programs as they ran: "a stack size of 32 will
  eliminate stack buffer overflows for almost all programs". The deepest
  of them reached 52 cells of the data stack and 39 of the return stack.
  Those are running programs. A system that compiles one file from inside
  another keeps more on its stacks than a running program does.
- GreenArrays' chips have circular stacks: when too much is pushed, the
  oldest cell is overwritten.

## Where the Verilog keeps a stack

In `rtl/hexaforth.v`:

- T, N and the cell under N are registers, and so are R and the cell under
  it.
- Below them, each stack is a memory of `DEPTH` cells. It is written at
  one address, from the register above it, and read without a clock at two
  addresses. An instruction that drops two cells takes two neighbouring
  cells from it in one cycle.
- The memory's addresses come from the depth, which is a register, so they
  do not wait for the instruction to be decoded. What is read goes only
  into the registers. The memory is not an input of the ALU, so it is not
  on an instruction's timing path.
- The depths are eight-bit registers whatever `DEPTH` is. So are the depth
  a port is setting, the value a depth port returns, and what the test
  bench compares (`rtl/tb.cpp`). Changing `DEPTH` alone does not make a
  deeper stack.

`vm.c` and the AArch64 kernels keep a stack in the host's memory, and
`STACK_CELLS` sets how much of it.

## What a deeper stack costs

The two stacks together are twice `STACK_CELLS` cells:

| Cell | Both stacks, 256 cells each | 512 cells each |
|---|---|---|
| 16 bits | 8,192 bits | 16,384 bits |
| 32 bits | 16,384 bits | 32,768 bits |
| 64 bits | 32,768 bits | 65,536 bits |

What that costs depends on where the stacks are kept.

- **In the FPGA's LUTs, as now.** On the ECP5 that `synth` places for, a
  memory that is read without a clock is made of RAMs of 16 by 4 bits, six
  LUTs each, and a stack is built once for each address it is read at,
  which is twice. So the stacks' cells double when the depth doubles.
  `synth` prints the size of the whole system, and the first lines of
  nextpnr's log beside it (`nextpnr.log`) give how many of its LUTs are
  RAM. Those are the stacks' cells, and they are a large part of the whole
  system.
- **In the FPGA's block RAM.** A block is 512 cells of 36 bits, with one
  write address and one read address. A stack of up to 512 cells takes one
  block for cells of 16 or 32 bits and two for 64, for each address it is
  read at, so depth is free up to 512. A block is read on a clock, so it
  needs the address a cycle early. That address is the depth as it will
  be, which is the value going into the depth register, so the read need
  not cost a cycle. But a cell that is pushed down and needed back in the
  next cycle must then be passed round the RAM. This comes from reading
  the Verilog, and is not built.
- **On a chip, as RAM cells.** A static RAM cell is six transistors a bit,
  which is one and a half gates of four transistors, and a cell read at
  two addresses takes more. By that count the stacks at 256 cells are
  already larger than the CPU that `gates` counts, at every cell size, and
  at 512 cells they are twice that. The Novix's gate array could not hold
  its stacks in 1985. The RTX 2000 after it had them on the chip, 256
  cells of 16 bits each.
- **On a chip, as flip-flops.** A flip-flop is four gates by the rule
  `gates` counts by, before the logic that selects a cell. That is several
  times the CPU at 256 cells. A stack this deep is not made of flip-flops.
- **In RAM chips beside the CPU.** Depth is free up to a chip's capacity.
  The static RAM of 1980 is 1,024 by 4 bits: the 2114A, at 100 to 250 ns,
  and the 2148, new that year, at about 70 ns. One chip holds four bits of
  a stack 1,024 cells deep. So a stack takes one chip for every four bits
  of its cell, whether it is 256 cells deep or 1,024.

## A machine of 1980

In 1980 a stack this deep is RAM chips beside the CPU, and that need not
cost a cycle. The arrangement is the Novix NC4016's of 1985, with the
parts of five years earlier:

- the top of each stack in the CPU's registers, as in the Verilog
- the rest of each stack in its own static RAM, addressed by its depth

The Novix did everything except a fetch, a store and a long literal in one
cycle, at up to 8 MHz, with its data stack on a bus of its own, read or
written "in parallel with other operations".

The stack RAM costs no cycle because of where the stack memory sits in the
Verilog's cycle (above): its address is known as the cycle starts, its
cell is needed only as the cycle ends, and no part of the instruction goes
through it. A RAM chip of 1980 has no clock: an address goes in and the
cell comes out, which is what the Verilog's stacks already do. The RAM
only has to be as fast as the cycle, and the cycle is at least one read of
the main memory, because an instruction is fetched in every cycle.

A cycle would be lost in three cases:

- **An instruction that drops two cells** (`!`, `2drop`) takes two cells
  from the stack's memory in one cycle, and one RAM gives one. Either the
  RAM is two banks, the even cells and the odd cells, which doubles the
  chips and adds a second data bus to the CPU's pins, or such an
  instruction takes one more cycle. A store took two cycles on the Novix.
- **RAM slower than the cycle.** `LAT` sets how many cycles the main
  memory takes to answer. The stacks have no such setting.
- **Stacks kept in the main memory,** as the 8086 and the 68000 keep
  theirs. Every refill is then a read of memory, and a pop costs what `@`
  costs.

## Pins

With the stacks in RAM, the CPU has three buses, each a cell wide: one for
each stack's RAM and one for the main memory's data. That is 48 data pins
for 16-bit cells, 96 for 32 and 192 for 64. The memory's address and the
two depths add more. For the 16-bit machine, whose memory is all that a
cell can address, the total is about 80 pins before the clock, control,
the console and power.

- The three buses cannot be shared. An instruction is fetched from the
  main memory in every cycle, and `>r` and `r>` use both stacks' memories
  in one cycle: one stack refills from its RAM as the other pushes down
  into its own.
- The depths need not be pins. The WISC CPU/16 kept each as an eight-bit
  counter beside its RAM, which leaves the CPU one line to count up and
  one to count down.
- The kernel's ROM adds no pins. It is on the main memory's bus, or with
  `HF_KROM` it is logic inside the CPU.

Packages of the time: the 68000 of 1979 had 64 pins; the 68020 of 1984,
114; the Novix of 1985, 121; the 80386 of 1985, 132. So:

- 16-bit cells fit one chip in 1980 only with the depths counted off the
  chip and the memory's address and data on the same pins, as on the 8086
- 32-bit cells fit one chip from about 1984. In 1980 they are a board, or
  a set of chips
- 64-bit cells need more pins than a processor of the 1980s had

What a pin costs:

- **A package.** In 1980 the pins are along two sides, a tenth of an inch
  apart, and 64 of them are over three inches long. More pins need a grid
  of pins under the package, which came in the following years and costs
  more: the package, its socket, and the layers of board under it.
- **The edge of the die.** A pin is a pad, and pads go round the edge of
  the die. A pad is far bigger than a gate and does not shrink as gates
  do, so the number of pins sets how small a die can be. A small CPU with
  many pins is a large die that is mostly empty.
- **A driver.** A signal that leaves the chip drives a trace and other
  chips, a load a hundred times a gate's, or more. Its driver is big and
  slow and takes power, and a bus of drivers switching together disturbs
  the chip's own supply.
- **Time.** A stack in RAM off the chip adds to the cycle the time out
  through a pad, the RAM's access time, and the time back in through a
  pad.
- **The board.** Every pin is a trace and a joint.

So with three buses, the pads set the chip's cost, and the gates do not.
Each way to reduce the pins costs something else: a bus shared between two
uses costs cycles, and stacks on the chip cost transistors.

## Several chips

The CPU is divided across the cell, not by the function of its parts. A
bus is a cell wide whatever is at the end of it, so a chip that holds one
part of the CPU has nearly as many pins as a chip that holds all of it.
Divided into slices of a few bits, a chip's pin count does not grow with
the cell. The Am2901 of 1975 divides a register machine this way, with
four bits and 40 pins a chip, and the Lilith of 1980 was built from them.

| Part | How many | What is in it |
|---|---|---|
| A slice of four bits | one for every four bits of the cell | its bits of T, N and the cell under N, of R and the cell under it, and of the program counter; its bits of the choice of IN, the logic and the adders; and its lines of each bus: the two stacks' RAM, the memory's data and the memory's address |
| Control | one | when an instruction is done: the waits for memory, for a shift and for the console; the two depths, which are the stack RAMs' addresses; the ports |
| A stack's RAM | one chip of 1,024 by 4 bits for each slice | its four bits of every cell under the registers |

What the machine's own design provides:

- The instruction is the control word. Each slice reads the same 16 lines
  of the instruction and derives its own selects from the instruction's
  fields, as `dec` does in the Verilog. `dec` is a function of the
  instruction alone. There is no microcode, and no bus of control signals
  to carry.
- Cells of 16, 32 and 64 bits use the same chips: 4, 8 or 16 slices, as
  the Verilog is one text with the parameter `W`.
- The RAM is already in slices.

What passes from slice to slice:

- the carry of the adder, which serves `+`, `<` and `u<`, and the carry of
  the add of a literal. It ripples, or a look-ahead chip takes it. The
  carry is what makes the cycle longer as the cell gets wider
- "all zero", for `=` and for the T that a conditional jump tests: each is
  a line that any slice can pull down
- the flag, which goes back to every bit, because true is every bit set
- a literal, which is 12 bits placed at bit 0, 12 or 24. Each slice takes
  its own part of the literal from the instruction's lines
- the program counter and R, which meet only in the low 16 bits, at a call
  and a return

What does not divide into slices:

- A shift by T. In one chip, the Verilog computes it in a cycle of its
  own. Across slices it takes one bit a cycle through the neighbouring
  slices, or a shifter that has the whole cell on its pins.
- The multiply. It is a step for each bit, as the kernel's `um*` is, or a
  chip of its own. `gates` also counts the CPU without it.

An instruction that takes one cycle still takes one cycle, but the cycle
is longer: the pads and the carry are in it. A shift and a multiply cost
cycles in proportion to their size unless they have chips of their own.

Counting the Verilog's signals gives about 56 pins for a slice of four
bits and about 71 for a slice of eight. So in a package of 1980 a slice is
four bits. That is a count, not a design.

## What the chips cost

TTL costs nothing before the first machine and a good deal for each
machine. A custom chip costs a good deal before the first one and little
for each. A source from 1981 has a gate array starting to save money after
about a thousand pieces.

| | Before the first | Each |
|---|---|---|
| TTL, in British advertisements of 1980 | nothing | a package of gates 11p to 18p; a RAM of 1,024 by 4 bits, 300 ns, £2.50 to £3.00 |
| A gate array, Ferranti's in 1982 | £5,000 a design on Ferranti's design system, which cost £46,500, or £15,000 for Ferranti to do the design | £1 to £2 a chip in high volume |
| A chip designed from scratch, as the Z80 and the 68000 were | not found | not found |

- Besides its parts, TTL costs board area, power and joints. In 1984 HP
  had a gate array at a fifth of the space, a quarter of the power and
  half the cost of the discrete circuit it replaced, and the single array
  in Sinclair's ZX81 replaced 18 chips of the ZX80.
- What an array held: about 1,200 gates around 1980, 4,000 as Ferranti
  delivered them in 1983, and nearly 8,000 in an HP CPU in 1985. The Novix
  was one array of under 4,000 gates. By `gates`' count the 16-bit CPU is
  several arrays in 1980, two in 1983 and one by 1985.
- As TTL, a package holds from four gates to a few dozen, so the same CPU
  is between a hundred packages and several hundred. This file does not
  count them. The WISC CPU/16, which is a simpler machine, was under a
  hundred packages with its memory, on two boards, and took three cycles
  of 280 ns for an instruction on average.

Not found: American prices of 1980 for TTL and for the Am2901, the price
of an array in small quantities, and the cost of a chip designed from
scratch before the first one is made.

## Not decided

- Whether the stacks are made deeper, and how much of them each task gets.
- Whether the Verilog's depths follow `DEPTH`.
- Whether a stack becomes something the CPU reads at one address and on a
  clock, and what an instruction that drops two cells costs then.

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
