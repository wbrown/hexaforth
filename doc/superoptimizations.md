# Optimization patterns

The ways this machine's work has been made shorter or faster, whether a
person found them or a tool did. Each entry says where the pattern is used,
what it replaced, and why it takes fewer instructions or cycles on this
machine. How much a change saved is recorded in the commit that made it,
which is named; the figures are not repeated here.

The last five sections record what the tools, and a reading of the ROM,
found:

- two sections on what the search for shorter sequences found, each with
  the code before and after and the stacks after every instruction
- what reading the kernel's ROM found
- a cell with itself: what the ALU computes when the cell under the top is
  both of its inputs, why the compilers and the search did not use it, and
  what the search found once the step knew it
- what the profile's survey of an image found: runs that `cross.fs` left
  long, and how it now compiles them short

The search is `bench/shorter.c` (the program `shorter`, which runs as the
machine its image is for; the target `shorter-loops` runs it on the work
the target `profile` writes). It takes one turn of a loop, inlines the
words the loop calls, and searches for the shortest sequence that leaves
the same result, wherever the values are kept (`util/loops.h`,
`util/turns.h`, `util/fewer.h`). Run on a kernel's ROM (`--rom`; the
targets `shorter-16`, `shorter-32`, `shorter-64` and `shorter-virt`), it
also takes every run of the ROM's words, with everything else left where
it is. Every answer is run on `vm.c` before it is reported.

Changes to the Verilog that made the CPU itself faster are not included:
they are not ways of writing code for the machine.

## Notation

An instruction is written by its name if it has one, and otherwise by its
fields in brackets (`MODEL.md`, Instructions): the input, the operation, the
output, and how each stack moves. A number followed by `+` (`1 +`) is a
single literal-add instruction. Stack pictures show the data stack with its
top on the right; what the return stack holds is noted alongside.

Much of what follows depends on the order in which an instruction is
carried out (`MODEL.md`). In short: the operation reads N as it was before
the stack moves; a push leaves the old top in the cell under the new one;
and when the result goes to memory or to R and the data stack moves down,
the new top is taken from the cell underneath.

On the Verilog CPU an instruction takes one clock, plus one for a memory
read, one for a shift, and two for a multiply. With two-cycle memory, a
jump, call, return or taken branch also costs an extra clock
(`rtl/hexaforth.v`).

## The instruction set everything else builds on

### A fetch is an input, a store is an output

The J1 had one field naming a fixed route. Hexaforth has two: the input is
N, T, the memory cell T points to, or R; the result goes to T, R, the memory
cell T points to, or a port (14c38a9). So a fetch or a store can be combined
with any operation and any stack movement in a single instruction. Every
fused instruction below is a particular choice of those fields.

### Operations combine the input with N

Operations take the input and N (51a6058): `IN+N`, `IN&N` and the rest.
When the input is the memory cell T points to, T is the address, so the
other operand has to be N. That is what makes `+!` one instruction:
`[T]->IN IN+N ->[T] d-2` reads the cell, adds N, and writes it back to the
address still in T. It also means R and T are never added in one
instruction: `R->IN IN+N` adds R to N. With the value under the top,
`over r@ +` is one instruction (`R->IN IN+N ->T d+1`); with the value on top
it takes two.

The stack depths used to be an operation and are now read through a port
(be583fd, bee7834), and writing a port became an output, replacing `->N`
(5aa2d94). The two freed slots hold `T<>N,IN->`, which makes `tuck`,
`swap >r` and `swap r>` one instruction each, and `IN*N`, the multiply,
which replaced a 32-step loop in the nucleus (37e9578).

### Operating on memory in place

`+!`, `or!`, `and!`, `xor!`, `*!` and `not!` read a cell, apply the
operation, and write it back, all in one instruction (51a6058, 3c73365,
23a56ff). Written the usual Forth way, `tuck @ or swap !` is five
instructions and six clocks; the single instruction takes two. Reading a
cell and combining it with N without writing it back (`@ and`, `@ +`,
`@ xor`, `@ *`) is also one instruction. `ink` is `or!`, and `x!` is
`xor!`.

Setting pixels in a display cell is `or!`; clearing them is `and!`; ORing a
glyph row onto the screen is one `or!` per row (`or-plain`); and inverting
a cell in place while keeping its address is `dup not!`, one instruction
(`white-row` in `tiff.fs`).

### Stack shuffles folded into one instruction

`dup @`, `over @`, `dup @ @`, `r@ @`, `tuck`, `swap >r`, `swap r>`,
`over >r`, `dup >r`, `2drop`, `rdrop` and `r@ exit` are each one
instruction, and `-rot` is two, `swap >r swap r>` (188eff1, be583fd,
4692757, 5c4b487). The J1b's `2dupxor`, `over+`, `2dup<`, `overand` and
`over=` are an operation with `d+1` (keep both operands and push the
result) or `d+0` (replace the top with the result), and are written here
as the words they are made of: `2dup xor`, `over +`. None of these phrases
is a word. Each is written as its words, and the compilers make one
instruction from them. `vm_opcodes.h` has an entry for each, and the build
checks each entry against what its words do. `2drop` routes R back to R, leaving it
unchanged, so that the value it writes doesn't take the new top's place.

### Literals that add

A literal is twelve bits with a shift of 0, 12 or 24, and a flag that adds
it to T instead of pushing it (c56f4e4). So `1+`, `d# 8 +` and any add of up
to 4095 is one instruction, and a larger number is built from pieces, each
after the first added on (9b93b30). A negative number is the push of its
complement followed by an `invert` (2b62346). Subtracting one takes three
instructions: a push, an invert and an add. Adding one takes one. A lot of
what follows is arranged so that loops add.

### A variable in one instruction

The literal's fourth shift value turns it into a variable access: the
twelve bits name one of the 4,096 cells from `VAR_BASE`, which is fetched,
or with the add flag set, stored (b0e851f). `var @` and `var !` are one
instruction, where they were a literal and a fetch, and the fetch still
reads memory just as `@` does. To keep variables within reach of that one
instruction, headers and labels go above the data (4d698ee), and data that
isn't variables goes past those cells (`bulk`, `high`). The fields of a port
are defined with `at:`, which compiles the field's address inline, so
`oy @` is one instruction rather than a call to a word that adds an offset
(8deea76).

This was a change to the machine itself. It predates the rule that the
machine is not changed for speed.

### The return folded into the previous instruction

An exit is `T->IN ->T RET r-1`. Since it returns to R as R was at the start
of the instruction, it can be merged into any ALU instruction that leaves R
alone and doesn't write R or a port (f7340c4). Most words end with an
instruction that has the return built in. A call immediately before an
exit becomes a jump, both in `cross.fs` (`shortcut`, `exit-here`, 771d2c9)
and in the nucleus (`texit`).

## Loops

### Loops count up, and loops over memory keep no count

```
bounds begin 2dup xor while ... 1+ repeat 2drop
```

The address goes up until it reaches the end address. `2dup xor` is one
instruction that keeps both addresses (`T->IN IN^N ->T d+1`); the
conditional branch pops the result and only asks whether it is zero; `1+`
is one instruction; and `bounds` runs once. The old way kept a count and
subtracted one from it each turn with a call to `1-` (a74ac05):

```
begin dup while >r over @ over ! 1+ swap 1+ swap r> 1- repeat drop 2drop
```

`cells-move`, `cells-fill`, `same?`, the TIFF reader's directory loop and
`grayn` all have this shape, as do most loops in the system.

### A loop that only counts goes from negative up to zero

```
negate begin dup while ... 1+ repeat drop
```

`negate` runs once; each turn is `dup`, the branch, and `1+` (a74ac05). The
dither's pixel loops (`agrays`, `apairs`), the row loops of `copy-bits` and
`plain-rect`, and `sspaces` and `nth` in the decompiler work this way.

### Keep the end, not the count

A loop from one address to another needs only the end address. Working out
how many there are costs a subtraction before the loop and an addition
inside it. `glyph-or` keeps the end of a glyph's rows (`rend`), and whether
any row is drawn at all is whether the first comes before the last, which
is one instruction (`2dup <`) (3f47c72).

### Loops test at the bottom

`cross.fs`'s `repeat` rotates a loop whose test, right after `begin`, is a
single instruction with operation `IN^N` or `IN==N` (9d92cbb). It repeats
the test at the bottom using the other of the two operations (they are
`h# f0` apart), followed by a branch back while the result is non-zero:

```
begin 2dup xor while BODY repeat        is compiled as
2dup xor if begin BODY 2dup = until then
```

Each turn is then the body, the test and the branch, where it used to be the
test, a branch out, the body and a jump back: one instruction fewer per
turn, for one more instruction in total. Every loop above that `cross.fs`
compiles gets rotated. The nucleus's own `repeat` is `again then` and does
not rotate.

### Values that don't change go on the return stack

R is an ALU input, so a value that is the same on every turn is kept there
and used directly, never brought to the data stack. `r@ over !` is one
instruction (`R->IN ->[T]`): a turn of `cells-fill` is that store, `1+`,
the test and the branch. That is four instructions, the shortest turn the
search finds (e310347). `fast-slab` stores the pattern row into every whole cell
of a fill this way, and `col!` goes down a column of a plain rectangle with
the value on R. Where the store is `or!` or `and!`, the value has to be N,
because the input is the memory cell, and from R each cell is `r@ over
or!`, three instructions. So `col-and!` and `col-or!` keep the value under
the address instead, and the end on R (What the search has found, below).

### A second address is the first plus a distance on R

When a loop walks two addresses that stay the same distance apart, only one
is kept. The distance goes on R, and the second address is computed from
the first whenever it's needed, with `over r@ +`, one instruction
(`R->IN IN+N ->T d+1`):

```
>r over - r> swap >r bounds               ( end from ) ( R: distance from source to destination )
begin 2dup xor while  dup @  over r@ + !  1+  repeat  2drop rdrop
```

That is `cells-move`. `same?` compares two bands this way, and the dither
reaches the rows below with it (`adown1`, `adown2`, `aquarters`,
`gray1-pairs`).

### Moving a cell in two instructions

`move-row` copies the cells between the two ends of a scrolled row with two
instructions written out by their fields (a50020a; the search tool of the
time, `util/shorter.py`, found it):

```
over  (R->IN IN+N ->R d-1 r+1)          the source address, made on R: N is the address, R the distance
(R->IN [IN] ->[T] r-1)                  the cell at R, stored at T, and R popped
```

The first adds the distance on R to the address `over` put under the top
and pushes the sum onto R; the data stack moving down drops the copy. The
second fetches through R (operation `[IN]`) and stores at T, and its `r-1`
pops the source address. A turn is those two, `1+`, the test and the
branch: six. `r> @ over !` is the same instruction, and a rule in the
nucleus's peephole produces it from that phrase when it is typed (88eee3c).

### One test for both directions

`+loop` is finished going up when the old index is above the new one, and
going down when it is below. `(+loopnext)` puts whether the step is negative
onto R and XORs it with a single comparison, so one body handles both
directions with no branch on which way it is going (771d2c9).

### A call to the next instruction runs the rest twice

`DOUBLE` is a call to the instruction right after it. The code after it
runs, returns to it, and runs again: two turns from one body, with no count
and no branch, and each `DOUBLE` in a row doubles the number of turns.
`mulstep` runs 16, 32 or 64 steps depending on how many it is given, and
`divstep` has a label partway down the chain, so that entering there runs 16
steps (`u/mod` for a small dividend, 492b608). The trick comes from
swapforth.

## The stacks, R and variables

### Working values live on the stacks

Fetching a variable is one instruction, just like using a stack cell; what
the stacks save is the store, the extra cycle a fetch spends reading memory,
and the instruction needed to get the value to where the ALU can use it
(e4353f7). So a word keeps what it is working on in T, N and R, and a word
that needs more than that is split into smaller words that each need no
more. `fast-slab` keeps the pattern row and the cell address on R and the
bit offset and count on the data stack, and `under!` handles an end cell
(afd120a, 0c6df02, d483fd1, 97c9dd8, d065a2e). The dither keeps a whole row
on the stacks (708238f), and the region merge keeps its two read pointers on
the data stack and its write pointer on R, replacing eighteen variables
(2e80e85).

### Many values: variables, saved only on re-entry

`copy-bits` and `draw-text` each have a great many values in play. A record
reached through a pointer would cost a fetch of the pointer, an add and a
fetch for each value, against one instruction for a variable; that was
tried, and cost the drawing benchmark a fifth of its cycles (6ef65be). So
they are variables in one contiguous block, with a busy flag that says
whether the word is already running. If it is, the block is copied aside
with `cells-move` (`borrow`) and copied back afterwards. This costs nothing
unless it actually happens (056331f).

### A task saves its state on its own return stack

Each task owns a range of return stack depths (`MODEL.md`), so whatever a
task pushes before its depth is switched is still there when the depth is
switched back. `pause` pushes the loop offset and the data stack depth onto
R, and keeps only the return stack depth in the task's record, where before
each field was found from `up`, stored, and fetched again (ef4ae70).

### Returning into a vector

`execute` is just `>r`, and `emit` is `'emit @ >r`: the address goes onto R
and the word returns into it, and that word's own return goes back to the
original caller. No call is made, and nothing returns twice.

### A value fetched once onto R and used twice

`bsrc` fetches the shift it needs twice only once, and keeps it on R
(6ef65be). The search found the same thing in `gray1-pairs` (below).

## Branches

### Flags as numbers

A flag is either all ones or all zeros, so ANDing it with a number gives
either that number or zero, which can then be added in, with no branch.

- `.digit`: `dup 9 > 7 and +`, the seven characters between `9` and `A`
- `d+`: the carry is `u<` of the low cells, which `negate` turns into 1 to
  add (`UNDERSTANDING.md`: `Nu<IN` is how a machine without a carry flag
  gets one)
- `sfind`: `0< invert 1 or` gives 1 or -1
- `2/`: the sign, shifted to the top bit, ORed into the shifted value
  (4bfc710)
- the dither's threshold: `dup 896 < h# 80ff and + 1+` adds `h# 8100` when
  the pixel is black and 1 when it is white, the two arms of what used to be
  an `if` (708238f)
- the region merge advances its write pointer by `r@ xor 1 and`, which is
  whether the result changed (2e80e85)
- `mulstep` chooses what to add by ANDing the top bit with what is on R;
  smaller, but slower (ef4ae70)

### A branch only asks whether a value is zero

The conditional branch pops T and branches if it is zero. So `<> if` becomes
`xor if`, `0<> if` becomes `if` (44a704d), `2dup = if` becomes `2dup xor
if` with the two branches swapped (4bfc710), and `0= if A else B then` becomes
`if B else A then` (771d2c9).

### Branches that end in exit

`drop exit` is one instruction, since the return is merged into the `drop`.
So `2dup < if drop else nip then` becomes `2dup < if drop exit then nip`,
and the jump over the `else` disappears (771d2c9). In the image, `min` is four
instructions: the test, the branch, and two instructions that each return.

### 0 ≤ i < n is one comparison

Treated as unsigned, a negative number is larger than any positive one, so
`i n u<` checks both bounds at once (`inside?`, `menu-line`, `glyph`).

### Inside a region is an odd address

A region's band is an even number of cells, and a region starts on an even
cell, so a crossing into the region is at an even address and a crossing out
is at an odd one. Whether a position is inside is the low bit of its
address, and the result of combining two regions is one instruction applied
to the two addresses: `and`, `or`, or `invert and`. The three merges are
that one instruction with the same code before and after it (`MERGE[`,
`]MERGE`, 2e80e85). A band ends with its row number plus `AS-END`, which
is larger than any crossing, so it ends the list of crossings and nothing
has to count them. `AS-END`, `END-MIN` and `NEVER` are powers of two that a
single instruction can push.

## Cells and lanes

### Pixels a cell at a time

The display is one bit per pixel, and a cell holds as many pixels as it has
bits (3cd68e6). A horizontal run is one store per cell, and only the two
cells at its ends are fetched and merged under a mask. A pattern row is the
pattern's byte repeated across a cell, so a filled row is just that cell
stored (ed3ac8c). A copy builds each destination cell from two source cells,
two shifts and an OR, with the shift worked out once for the whole copy
(cd42183). When the source and destination are aligned, the copy is a plain
move (7782b54).

A cell entirely inside the area being drawn is stored without being fetched,
since its old contents aren't needed (`whole?`, d2ed909; `fast-slab`,
251f5ea).

### Text a cell at a time, a glyph into all its rows at once

Eight-pixel glyph rows used to be stored a byte at a time. They were then
gathered into a cell and stored when the cell was full (126567d), and then a
glyph went into all its rows at once: the rows of the screen cell are a band
of cells (`tband`), and the glyph's rows are ORed into them with `or!`, one
instruction per row (`+rows`, dae91c3):

```
tend @ tband
begin 2dup xor while  dup gd @ + @ hi @ PIXR over or!  1+  repeat  2drop
```

`gd` is the distance from the band to the glyph's rows, so one add finds the
source row from the position in the band. Where the glyph goes, how wide it
is and whether it is styled are worked out once per glyph, not once per
row.

Text that only adds black, with nothing to mask, goes straight to the
screen: the screen address is on R and is stepped by the display's stride,
and each row is one `or!` (`or-plain`, 88bceec).

### A pixel is a lane

The dither keeps each pixel in a 16-bit lane, as many lanes to a cell as
fit, and before a pixel is looked at, everything it is thresholded on has
already been added into its lane: its gray value, the error from the rows
above, and the error from the two pixels before it (708238f). The cell is on
the stack and the current pixel is its lowest lane; `16 rshift` moves to the
next one. Nothing else is kept alongside. `atkinson.fs` explains it at its
top.

The error a pixel passes on is its error divided by eight, plus 128, so
that it is never negative and one lane never borrows from the next; nothing
is ever subtracted (6ad072c). Whether the pixel is black goes in bit 12 of
what it passes on, and a lane's own value is in the twelve bits below, so
the lanes of a finished row hold both the errors for the rows below and the
pixels themselves.

### One multiply for many lanes

- `tuck h# 10001 * +` gives the next two pixels their share, in both lanes
  at once
- `SPREAD`, `dup h# 10001 * swap 16 rshift +`, adds each lane of a cell to
  the lanes on either side, which are the three neighbours on the row below
- `anib` gathers bit 12 of every lane into place, in the display's pixel
  order, with one multiply by a constant made at build time
- `pair` makes two pixels' gray values at once: the two reds are masked out
  where they lie (`h# ff0000ff`) and multiplied by 77, the greens and blues
  are shifted to the same positions and multiplied by their own weights, and
  the three are added; each product is at most 255 times 256, and they are
  24 bits apart, so nothing carries from one to the other (6cb1918)
- a row's gray values are added into its lanes a cell at a time with `+!`

### Reading the image a cell at a time

The TIFF reader fetches a cell and shifts what it needs out of it on the
stack (`take`), rather than reading a byte at a time (6ad072c). Where a row
is aligned to cells, three cells of colour become two cells of gray lanes
(`lanes-a`, `lanes-b`), at every cell size (0ef1b3c), and for a gray image
the bytes are spread into lanes by halves (`spread`) and pairs of pixels are
added in every lane at once (`fold`, `gray1-pairs`, 540957c). Which method
reads a row is chosen once per image (`'grays`, `'pairs`).

### When a cell is one lane

At 16 bits a cell is a single lane, so the code that kept lanes apart is not
compiled (35af94a):

- `GRAY-PAIR`: after `8 rshift` the high byte has nothing above it, so only
  the low byte needs masking
- `CLEAN`: the mask for a lane's value, `$fff`, is a literal pushed in one
  instruction, not a variable that has to be read from memory
- `BLACK-BITS`: the black bit is moved to where `acell-bits` wants it with an
  AND and a shift; nothing is multiplied
- `LOW>TOP`, `TOP>LOW`, `SPREAD`: a lane has no neighbours in its cell, so
  nothing is compiled
- `apixels`: what a pixel passes on can't ride along in the cell, so what
  the next pixel is owed is added into its lane with `+!` as soon as it is
  known, and only what the last pixel gave is carried, on R

### Names at five bits a character

A name is packed into cells at five bits per character, and the first cell
holds the first characters plus whether that is the whole name. A lookup
compares that cell first, with the key's first cell on R, which settles
almost every entry with one fetch (492b608, 12fb034, 24c033a).

## Doing work once

### Look it up instead of computing it

- `lefts`: the mask of the leftmost n pixels of a cell is a table, one entry
  per bit of a cell plus one, made by `cross.fs` in the display's bit order
  (2181ab6). `leftn` is an add and a fetch, where it was eleven instructions
  and a call
- a font is unpacked once into one cell per row of each glyph, with the
  glyph at the left of the cell, where reading a row from the strike costs a
  shift and a mask across two cells (8deea76); the first and last rows with
  any black are recorded too, so text skips empty rows and a space draws
  nothing (b2284f1). On the smallest machine this is done when the image is
  built
- the decompiler finds a word by binary search in tables the build makes
  (`tfind`), not by walking the dictionary (fca9e2c, 648b5d1)

### Decide outside the loop

- the transfer mode is a vector, so the drawing loops don't test it
  (ed3ac8c)
- `slab` picks `fast-slab` or `slow-slab`; `tflush` picks `or-rows` or
  `put-rows`; `glyph-or` picks `or-plain`, `or-right` or `or-left`
  (a3aebd1, 88bceec)
- `CELL-DONE` is written out once per lane when the image is built, so no
  call or loop is paid per pixel (708238f)
- the pixel at each end of a dither row is given, when the row is set up,
  what its missing neighbour would have passed it (`apreset`), so no pixel
  has to be tested for being at an end
- the rounding term for averaging four pixels is put in the lanes the sums
  are made in, which are filled once per row, so it is added once per row
  rather than once per pixel (`AROUND`, 35af94a)

### Clip once

- a rectangle is clipped to the clip rectangle before its rows are filled,
  rather than row by row (a50020a)
- a plain white or black rectangle is filled a column at a time: where it
  starts within a cell and the masks for its two ends are worked out once,
  and each column is walked by adding the stride (5ec27d6)
- a clip region that is a rectangle is folded into the clip rectangle, so
  nothing is masked; region operations on two rectangles, or on an empty
  region, look at no bands at all (a4f2219)
- a region band that is a single run is clipped to, not masked (a3aebd1)
- under any other region, a fill is done as one rectangle per run of each
  band, and no mask is built per row (a6db523); a region's mask is only
  rebuilt when a row leaves the band it was built for (0e75479)

## Not doing it at all

- nothing is drawn at the moment something changes: the change marks what
  is out of date, and the event loop repaints only that (b4be2a5), and each
  window only where it is visible (248a82d, a50020a)
- window visibility is recomputed only when a checksum of the layout
  changes, which costs a few instructions per window (a4f2219)
- scrolling moves the existing picture and draws only what comes into view
  (37819cf, e35dbe6); rows that the region doesn't cover are left alone
  (d2ed909)
- the picture's rows are marked out of date four at a time, not one at a
  time (d31b990, c81146e)
- a disk block that is wanted at a cell boundary is read straight into place
  (d31b990)
- text rows start on cell boundaries, so scrolling the Forth window moves
  and blanks whole cells (5f65f1a)

## The compilers

### Asking the machine which instructions combine

`cross.fs` offers each new instruction to the one or two before it, and asks
`util/fuse.h` which single instruction does the work of two or three, and
which two do the work of three (eea6888, ef98284, 4df3ca4, 6019675). A
combined instruction is offered again, so combinations can chain. Nothing is
combined across a branch target. The answers come from stepping the machine
on symbolic terms, and each is run on `vm.c`.

A sequence containing numbers or variables is handled the same way: the
machine supplies any shorter sequence with the same effect (`machine-lit`,
18fe8bb), which replaced a table of rules (2b62346, a34f3d1, 44a704d). `16 -`
becomes a push of 15, an invert and an add; `var @ 0 >` becomes `0 var @ <`,
with no call.

A variable's fetch or store is folded in first, before anything else is
combined with the address (`fold-var`, 29fe9e7, 18fe8bb).

### Inlining small words

A word of up to four instructions, with no calls or branches, is compiled
inline in place of a call to it, and kept that way if the peephole combines
it with its neighbours or if the result is at most as long as the call
(3f2a7a1, 4c7d7a1). Where a call site runs so often that the call and return matter,
`in-place` forces the word inline; it is used only after measuring
(`in-place -` in `fast-slab`, `move-row`, `row-direct`, `glyph` and
`text-body`, 4c7d7a1).

Basewords that the nucleus defines are given as headers for the instruction
rather than colon definitions, so that each use is the instruction itself
and can be combined (eea6888).

### The nucleus's compiler

Code typed at the prompt is combined as it is compiled (ac4f758). The
nucleus computes what two or three instructions do together, and has no
table to look it up in. It steps them on a machine whose cells hold where
each value came from, builds the one instruction that would leave the same
result, and steps that too. If the two machines agree, it compiles that
instruction (`compose.fs`). Where the instructions cancel, as `>r r>` do,
it compiles no instruction. This replaced a table of rules (a1cf834,
88eee3c), and the targets `pairs` and `peephole` check what it computes
against the machine's own step.

It also does these:

- constant arithmetic is computed by running it (`100 30 - 2 *` becomes
  140)
- a `dup`, an `r@` or an `r>` before a push is folded into the instruction
  after the push (`across`)
- a variable is fetched or stored by its one instruction (`var,`)
- a small word is compiled inline so that its instructions can be combined
- the return-stack words are immediate words that offer their instruction
  to the one before, so `r> @` typed at the prompt is one instruction
- a 0 is compiled as `false` and a -1 as `true`, which are ALU
  instructions, so the peephole joins them like any other
- rules that `util/joins.c` generates from the machine's step replace
  three or four instructions with two: `0 =` after an instruction, `over 0`
  before one, and a store after `r@ + r>` (`folds.fs`; What the survey
  found, below)

It does not do everything `cross.fs` does. `cross.fs` searches for a
shorter run, and the nucleus has no search: it joins two or three
instructions into one, and applies those rules. The target `peephole`
reports what the difference costs, and lists the words the two compile to
different lengths.

## The kernel's size

The kernel favours small over fast.

- `callable`: a two-instruction baseword is a call inside the kernel and
  inline everywhere else; `negate` is not, since `-` is built from it
  (771d2c9)
- `c@` is `h# ff field@`; `um*` takes a step for every bit; `u/mod` has one
  fast path, for a small dividend, which printing a number uses; `name>`
  looks every character code up (ef4ae70). Each has a faster form that is
  longer: `c@` written out in full, which repeats most of `field@`; a
  multiply step that adds only for a set bit; a second short path in
  `u/mod` for a dividend of 32 bits; a test for a letter before `name>`'s
  look-up
- `interpret1` tests `sfind`'s flag and the state in turn. A table of jumps
  indexed by the two does the same and is longer
- `sfind`'s loop tests for the end of the chain, and adds two to a
  header's address to reach its name. A last header whose name is the key
  would remove the test, and a link that points at the name would remove
  the add. Each changes the header's form for everything that reads a
  header (`doc/dictionary.md`)
- `inline;`: a word that is a call followed by the instruction to copy, with
  no exit: `r>` takes the return address and doesn't put it back, so the
  copy returns straight to the caller's caller (ef4ae70)
- code nothing can reach is removed, along with cases of a word that are
  never used (`util/reach.h`, ef4ae70)
- `segs,` uses the literal it is building as its own state: adding `h# 1000`
  moves its shift up one step, and ORing in `h# 4000` makes it an add
  (771d2c9)
- code written twice becomes a word, or runs on into the next word
  (`;into`, which was `;fallthru`: 4bfc710)

## What the search has found

Each entry shows a loop's turn as it was in the image and as the search
found it, with the stacks after each instruction, and what changed.
`shorter`'s report gives how often each loop runs, and whether every
search finished.

The earlier search tool, `util/shorter.py`, found `move-row`'s two-instruction
move (above) and `cells-fill`'s fill with the value on R (e310347).
`bench/shorter.c` has shown that the turns of `aquarters`, `adown2`,
`cells-fill`, `cells-move`, `move-row`, `col!`, `column-run` and
`fast-slab` are already at their shortest. It found the turns below. The
first three are from the dither and the TIFF reader, on the 16-bit
machine. The ones after "What the three have in common" are from the
desktop's own loops, on the 64-bit machine. All of them are in the source.

### Using a value twice from the return stack

Found in `gray1-pairs` (`tiff.fs`), on the 16-bit machine. Each cell holds
two bytes of gray, and the loop adds the two into a cell of the row of sums.
The stack is `end a`, the current address and the end; R holds the distance
from the gray row to the sums.

As it was (12 instructions):

    dup @                       end a x
    8 (N->IN IN>>T ->T d+0)     end a x hi        x's high byte; x kept underneath
    swap                        end a hi x        x back on top
    255 and                     end a hi lo
    +                           end a s
    over r@ +                   end a s a+R       (R->IN IN+N ->T d+1)
    +!                          end a             s added to the sum there
    1 + (T->IN IN==N ->T d+1) until

As found (11):

    (T->IN [IN] ->R d+0 r+1)    end a             x, fetched onto R
    255 8                       end a 255 8
    (R->IN IN>>T ->T d+0)       end a 255 hi      x from R, shifted right 8
    (R->IN IN&N ->T d+1)        end a 255 hi lo   x from R ANDed with 255, pushed
    (T->IN IN+N ->T d-2 r-1)    end a s           lo and hi added; hi and the
                                                  255 dropped, and x popped from R
    (R->IN IN+N ->T d+1)        end a s a+R
    +! 1 + (T->IN IN==N ->T d+1) until

The cell is needed twice, by the shift and by the mask. On the data stack it
has to be copied for the first and then swapped back on top for the second.
Fetched onto the return stack, it is an ALU input for both, and the data
stack holds only the other operand each needs: 8 for the shift and 255 for
the mask. The 255 is pushed first and left there, so the mask finds it as N;
and the add that combines the two bytes moves the data stack down two, which
drops both the high byte and the 255, and the return stack down one, which
pops the cell. Nothing is spent bringing the value back or clearing up
after it.

The general rule: when two operations need the same value with something
else in between, fetch it onto the return stack, let each read it from
there, and let the last one pop it. Lay out the other operands on the data
stack in the order the operations will use them, and have the last
operation drop the data stack as far as needed to clear them.

This is now in `tiff.fs`, written in standard words (`GRAY-PAIR`), which
the peephole combines into these instructions.

### Recomputing an address instead of keeping it

Found in `adown1` (`atkinson.fs`), on the 16-bit machine. The loop goes
along a row of errors and adds each to three cells of the row below: the one
before, the one beneath, and the one after. The stack is `end p`; R holds
the distance from a cell to the one before it in the other row, so `t`, the
first of the three, is `p` plus R. At 16 bits there is one lane per cell,
and the error goes to all three cells unchanged.

As it was (15):

    over (R->IN IN+N ->T d+0)   end p t           t computed first
    over @ 4095 and             end p t e         the fetch has to reach past t
    over ([T]->IN IN+N ->[T] d+0)
                                end p t e t       e added at t; t copied
    2 + ([T]->IN IN+N ->[T] d-1)
                                end p t e         e added at t+2
    swap 1 + +!                 end p             e added at t+1; t swapped
                                                  back on top for it
    1 + (T->IN IN==N ->T d+1) until

As found (13):

    dup @ 4095 and              end p e           fetch first, while p is on top
    (R->IN IN+N ->T d+1)        end p e t         t = p (under the top) + R, pushed
    ([T]->IN IN+N ->[T] d+0)    end p e t         e added at t
    2 + ([T]->IN IN+N ->[T] d-1)
                                end p e           e added at t+2
    (R->IN IN+N ->T d+1)        end p e t         t computed again
    1 + +!                      end p             e added at t+1
    1 + (T->IN IN==N ->T d+1) until

`t` is made from a stack value and the return stack, and computing it is
one instruction: R as the input, added to N, the result pushed (`over r@ +`
compiles to it). Copying a kept value is also one instruction (`over`). So
recomputing it costs no more than keeping it, and keeping it costs extra:
a kept value is in the way of everything beneath it. Computed first and
kept, `t` got in the way of the fetch (`over @`, reaching past it), had to be
copied for the first add, and had to be swapped back on top for the last.
Computed where it is used, it is consumed there, and nothing has to reach
past it or be swapped back.

It is one instruction only because `p` is the cell under the top each time:
the add uses N, and `p` sits under the error for the whole turn. So the
fetch, which needs `p` itself, is done first while `p` is on top, and after
that `p` stays where the add can reach it. With `p` on top, the same sum
takes two instructions, and keeping it might be cheaper.

The general rule: an address that is a stack cell plus the top of the
return stack should be computed where it is needed, each time, rather than
kept, as long as that cell is under the top whenever it is computed. Do
whatever needs the cell itself first.

This is now in `atkinson.fs`, written in standard words. The same shape
saves two instructions per cell at 32 and 64 bits too.

### Overwriting the top of the return stack in place

Found in `apixels` (`atkinson.fs`), the 16-bit dither's inner loop, where a
cell is one pixel. Each turn thresholds a pixel, stores what it passes on
(`gives`) back in its lane, and adds what the next pixel is owed into the
next lane. What the next pixel is owed depends on the pixel before as well,
so what each pixel gave is carried to the next turn on R. The stack is
`end p`, with R holding what the last pixel gave.

As it was (22 instructions), from the threshold on:

    ... 3 rshift                end p g           g: what this pixel gives
    (T->IN T<>N,IN-> ->[T] d+0) end g p           g stored in this pixel's lane
    (N->IN T<>N,IN-> ->T d+1)   end p g g
    (R->IN IN+N ->T d+0 r-1)    end p g s         s = g + the last one's, popped from R
    swap >r                     end p s           R: g, pushed back
    4095 and                    end p s'
    over 1 + +!                 end p             s' added to the next lane
    1 + (T->IN IN==N ->T d+1) until

As it is now (20):

    ... 3 rshift                end p g
    4095 over r@ +              end p g 4095 s    s = g + R, pushed (over r@ +)
    and                         end p g s'
    (N->IN T<>N,IN-> ->R d-1)   end p s'          R's top replaced by g: swap rdrop >r
    (R->IN T<>N,IN-> ->[T])     end s' p          g stored at p from R: swap r@ over !
    1 + tuck +!                 end p+1           s' added to the next lane
    (T->IN IN==N ->T d+1) until

The old turn took the carried value off R (`r>`, folded into the add) and
put the new one back (`swap >r`). An instruction whose output is `->R` with
the return stack not moving (`r+0`) does both at once: it writes over R's
top. So `swap rdrop >r` is a single instruction, `(N->IN T<>N,IN-> ->R
d-1)`, which replaces what the last pixel gave with what this one gives,
and the swap that is part of it leaves the data stack in order. The new
value is then stored from R into the pixel's lane while the address is
swapped up for what follows: `swap r@ over !` is also one instruction,
`(R->IN T<>N,IN-> ->[T])`. And adding one to the address before the add
into the next lane, `1 + tuck +!`, makes the step the loop needs, so it
isn't done again at the end.

The search found the same count with R also used as scratch for the add
into the next lane: `([T]->IN IN+N ->R r+1)` reads the lane, adds and parks
the sum on R; `(R->IN T->N,IN-> ->[T] d-1)` stores it while moving the
address into place; and the loop's test pops it, `(T->IN IN==N ->T d+1
r-1)`. That is as short as `tuck +!`, so the source uses `tuck +!`.

The general rule: a value carried from one turn to the next on R is
updated by writing over it, with `->R` and `r+0`, not by taking it off and
putting the new one back. A phrase that ends in `rdrop >r` becomes that
instruction. And where something on the data stack has to be swapped
before a store, see whether the swap can ride in the store's operation
(`T<>N,IN->`), as it does here.

This is now in `atkinson.fs`, written in standard words (`swap rdrop >r`
and `swap r@ over !`), which the peephole combines into these instructions.

### What the three have in common

All three rely on the return stack being an ALU input, and all three clear
the data stack of values that are only there to be used again. In the
first, a value is put on R so it can be read twice; in the second, a value
isn't kept at all, because R and the cell under the top can rebuild it in
one instruction; in the third, the value carried on R is replaced where it
is, rather than taken off and put back. The data stack ends up holding only
what can't be rebuilt from R and the cell under the top, and the `swap`s,
`over`s and `>r`s that kept things in order are gone.

### The value under the address, and the end on R

Found in `col-and!` and `col-or!` (`display.fs`) and `band-along`
(`region.fs`), on the 64-bit machine. The first two go down a column of the
display, one cell in every row, and AND or OR a value into it; a row is 12
cells there. The stack was `end a`, the end and the current address, with
the value `x` on R (Values that don't change go on the return stack,
above).

As it was (6 instructions), for `col-and!`:

    r@                          end a x
    over                        end a x a
    and!                        end a             x ANDed into the cell at a
    12 +                        end a'            the same cell of the next row
    (T->IN IN==N ->T d+1)       end a' f          is a' the end?
    until                       end a'

As found (5), with the value and the end in each other's places. The stack
is `x a`, and R holds the end:

    ([T]->IN IN&N ->R d+1 r+1)  x a a             the cell ANDed with x; the result
                                                  m pushed onto R (R: end m)
    12 +                        x a a'            R: end m
    (R->IN T<>N,IN-> ->[T] r-1) x a' a            m stored at a and popped (R: end)
    (R->IN IN==N ->T d+0)       x a' f            is a' the end? written over a
    until                       x a'

`col-or!` is the same with `IN|N`. So is `band-along` (`region.fs`), which
adds what is on R into every cell of a band, with `IN+N` and a step of one
cell: 6 instructions a turn as it was, and 5 as found.

`and!` takes its value from N, because its input is the memory cell. A
value on R has to be brought there, with the address copied on top of it:
`r@ over`, two instructions before the one that does the work. In the turn
that was found the value is under the address for the whole loop, where the
operation wants it, and the end is on R instead. Only the test reads the
end, and the test can take R as its input as easily as it took T.

That leaves the address on top, and the test needs it as N. The turn gets
it there with no `dup` or `swap` of its own. The AND's result isn't stored
at once: it goes to R. An instruction that pushes leaves the old top in the
cell underneath, so with `d+1` on it there are now two copies of the
address, at no cost. The top one is stepped to the next row. Then one
instruction stores the result from R at the old address and pops it, and
its operation swaps the two addresses on the way: the old one comes to the
top to be stored at, and the new one goes under it. The test writes its
flag over the old address, which is finished with.

The general rule: an operation on memory in place takes its other operand
from N. If that operand is the same on every turn, keep it under the
address and not on R, and put on R what only the test needs. And an
instruction whose result goes to R can still move the data stack up, which
is a copy of the top that costs nothing.

This is now in `display.fs` and `region.fs`, written in standard words. The
words set the loop up with the value under the address and the end on R,
make the test once before the loop, and write a turn as

    begin  2dup and!  DISP-STRIDE +  r@ over =  until

`cross.fs` compiles that to a different turn of the same length:

    ([T]->IN IN&N ->[T] d+0)    x a               2dup and!: x ANDed into the cell at a
    12 +                        x a'
    r@                          x a' end
    (T->IN IN==N ->T d+0)       x a' f            over =: is a' the end?
    until                       x a'

With the value where the operation takes it from, `2dup and!` is one
instruction: the cell ANDed with N and stored back, and the data stack not
moved. Nothing is parked on R, and the test is the end pushed and compared
with the address under it. The search's turn and this one are both five
instructions, and five is the shortest the search found. `col!`, the same
loop with a plain store, is four instructions a turn, which is the
shortest the search finds for it.

### A count under a value: the value goes to R, and the test pops it

Found in `nwords` and `rec-next` (`see.fs`), `code-at` (`text.fs`) and
`lights` (`wm.fs`), on the 64-bit machine. All but `rec-next` kept a count
under the value they are working along, and added one to it every turn with
`swap 1+ swap`; `rec-next` stepped an address that way.

`nwords` counts the dictionary's headers. The stack is `n h`, the count and
a header, and a turn follows the header's link until there is none.

As it was (7):

    dup                         n h h
    while                       n h               out, if h is 0
    swap 1 + swap               n' h
    @                           n' h'             the next header
    repeat

As found (6), with the test at the bottom:

    >r                          n                 R: h
    1 +                         n'                R: h
    r@ @                        n' h'             the link, fetched through R
    0                           n' h' 0
    (T->IN IN==N ->T d+0 r-1)   n' h' f           is h' 0? and h popped from R
    until                       n' h'

`code-at` looks along the character codes in a cell for the first that is
below a space. The stack is `k w`: how far along it is, and the cell with
the codes already passed shifted out of it.

As it was (12):

    255 over and                k w c             the code at the bottom of w
    32 u<                       k w f             is it below a space?
    invert                      k w ~f
    while                       k w               out, if it is
    8 rshift                    k w'              the next code
    swap 1 + swap               k' w'
    repeat

As found (9), with the test at the bottom:

    >r                          k                 R: w
    1 +                         k'                R: w
    8                           k' 8
    (R->IN IN>>T ->T d+0)       k' w'             w from R, shifted right 8
    255 over and                k' w' c
    32                          k' w' c 32
    (T->IN Nu<IN ->T d-1 r-1)   k' w' f           u<, and w popped from R
    until                       k' w'

`swap 1+ swap` is three instructions to add one to the cell under the top.
With the value put on R (`>r`) the count is on top, where adding one is one
instruction. The value doesn't have to come back as it was: what the turn
does with it next, following its link or shifting it, takes R as its input
and pushes the new value (`r@ @`, and `R->IN IN>>T`). So `>r`, `1 +` and an
operation the turn had anyway do what `swap 1 + swap` and that operation
did, in one instruction fewer. The old value is still on R, and the test
pops it: its comparison has `r-1` on it, so nothing is spent clearing R.

`rec-next` (`see.fs`) is the same thing with an address. It goes along the
cells of a record to the first that is negative, and steps past it. The
stack is the address alone. As it was (7), the cell is fetched and the
address then stepped underneath it:

    dup @                       a x
    swap 1 + swap               a' x
    0 <                         a' f              is x negative?
    until                       a'

As found (6):

    dup >r                      a                 R: a
    1 +                         a'                R: a
    r@ @                        a' x              the cell, fetched through R
    0                           a' x 0
    (T->IN N<IN ->T d-1 r-1)    a' f              <, and a popped from R
    until                       a'

The address is stepped first, on top, and its old value is on R for the
fetch to go through.

`lights` (`wm.fs`) counts how many times a mask of the disks can be shifted
right before it is zero. As it was (8), a turn is `dup`, the branch out,
`1 rshift swap 1 + swap` and the jump back. As found (7), it is `nwords`'s
turn with `code-at`'s shift in it:

    >r                          n                 R: m
    1 +                         n'                R: m
    1                           n' 1
    (R->IN IN>>T ->T d+0)       n' m'             m from R, shifted right 1
    0                           n' m' 0
    (T->IN IN==N ->T d+0 r-1)   n' m' f           is m' 0? and m popped from R
    until                       n' m'

The rest of what `code-at` saves is the loop's shape. Tested at the bottom,
a turn has one branch where it had a branch out and a jump back. And
`until` repeats while the flag is zero, which is the sense `u<` gives
here, so the `invert` that `while` needed is gone. `cross.fs` moves a test
to the bottom by itself only when the test is one instruction of `IN^N` or
`IN==N` (Loops test at the bottom, above). This test is five instructions,
and `nwords`'s is a `dup`. In `nwords` the test at the bottom is one
instruction longer than the `dup` it replaces, because it has to keep the
header while testing whether it is zero, and the turn is still one
instruction shorter.

The general rule: don't count in the cell under the top. Put the value on
R, count on top, bring the value back through the operation that changes
it, with R as that operation's input, and let the test pop R. And where a
loop's test is more than one instruction, look at the loop with the test at
the bottom: a turn has one branch fewer, and loses the `invert` if `while`
needed one.

What the search counts is a turn. In the source each loop is written with
its test at the bottom, and the test is made once more before the loop is
entered, as `cross.fs` does when it rotates one. The four are now in
`see.fs`, `text.fs` and `wm.fs`, in standard words:

    begin  >r 1+ r> @  dup 0=  until                            nwords
    begin  dup >r 1+ r> @  0<  until                            rec-next
    begin  >r 1+ r> d# 8 rshift  dup h# ff and d# 32 u<  until  code-at
    begin  >r 1+ r> u2/  dup 0=  until                          lights

`cross.fs` makes turns of the lengths the search found, 6, 6, 9 and 7. It
pops R in the operation that brings the value back, where the search popped
it in the test: `r> @` is one instruction (`R->IN [IN] ->T d+1 r-1`), and
`r> d# 8 rshift` is the 8 pushed and then one (`R->IN IN>>T ->T r-1`).
`dup 0=` is two, a 0 pushed and `over =`.

## What the search found in the kernel's ROM

Run on a kernel's ROM, the search takes every run of the ROM's words: the
instructions between one call, branch or return and the next. It searches
for a shorter sequence that leaves what the run leaves, with every value
in the same place, first for the whole run and then for each part of it.
For a run that ends a word, it searches with the return in the last
instruction. These results are from QEMU's ROM, which has 64-bit cells and
is the fullest ROM. Each entry gives the run as it was and as found, and
the last part of the section gives how each is written in `nuc.fs` now
(781c1a7).

What the search does not find:

- A turn of most of the ROM's loops. The search cannot take a turn of a
  loop that branches inside, or that calls a word that branches. It could
  take two, `cells-move` and `cells-fill`, and both are already at their
  shortest.
- Anything in a run that reads or writes a port. The step does not model
  a port.
- The same text written in two words. That is found by reading (The same
  text in two words, below).
- The instruction after a call of `inline:` or `inline;`. Such a word
  reads its return address and copies the instruction there into the code
  it is compiling. That instruction is never run where it is, so it is
  not part of a run.

An answer is true of the image the search is given, and two of its answers
were true only for the numbers in that image. In `evaluate` and in `spawn`
it made a 0 by shifting an address right by itself, which gives 0 only
while the address has its present value. `cross.fs` makes a 0 that way too
(`[` is the address of `state` and one instruction). That is safe for
`cross.fs`, because it asks the machine again for every image it builds.
Written into the source by hand, the same code would be wrong as soon as
the address moved. Those two answers are not listed here.

### A shift up and down by the same count pushes the count once

Found in `sfind`, `name>` and `literal`. Each clears the top bits of a cell
by shifting it left and then right by the same amount.

`sfind` has found the name, and extracts the word's execution token from
the cell whose top bit is the immediate flag (`dup 2* u2/`). `x` is that
cell, and it is needed again afterwards. As it was (5):

    1                           x 1
    (N->IN IN<<T ->T d+0)       x x<<1            x kept underneath
    1 rshift                    x xt              the top bit gone
    swap                        xt x

As found (4):

    1                           x 1
    (N->IN IN<<T ->R d+0 r+1)   x 1               x<<1 onto R; the 1 stays
    (R->IN IN>>T ->T d+0)       x xt              R shifted right by the 1,
                                                  and written over it
    (N->IN T<>N,IN-> ->T r-1)   xt x              swapped, and R popped

`name>` takes a cell of a name and clears the bits that are not codes. The
cell is kept on R, because the end of the turn tests its top bit. As it
was (6):

    dup @                       a w
    dup >r                      a w               R: w
    4 lshift                    a w<<4
    4 rshift                    a codes

As found (4):

    (T->IN [IN] ->R d+0 r+1)    a                 the cell fetched onto R
    4                           a 4
    (R->IN IN<<T ->R d+0 r+1)   a 4               R: w w<<4
    (R->IN IN>>T ->T d+0 r-1)   a codes           R: w

A shift takes its count from T. A shift whose result goes to T consumes
the count, so the second shift has to push it again. A shift whose result
goes to R leaves the count on the data stack, and the second shift takes R
as its input and the same count as its T. In `name>` the cell is never on
the data stack at all: it is fetched onto R, shifted from R onto R, and
shifted from R into place. `literal` keeps the low 36 bits of a number with
two shifts by 28, and its second shift drops the cell the number was in.

The general rule: two shifts by the same count need one push of the count,
with the first shift's result on R.

### A value computed from another goes to R before the other is consumed

Found in `field@`, which is `c@` and `uw@`. It takes the address of a byte
and returns the field of the cell from that byte up. The address is needed
twice: for the cell it is in, and for the byte's place in that cell. `ca`
is the address, and R holds the mask. As it was (10, between `>r` and
`r> and`):

    3                           ca 3
    (N->IN IN>>T ->T d+0)       ca c              the cell's address; ca kept
    @                           ca w
    swap                        w ca
    7 and                       w b               the byte's place in the cell
    3 lshift                    w bits
    rshift                      w'

As found (9):

    7                           ca 7
    (T->IN IN&N ->R d-1 r+1)    ca                the byte's place, onto R
    3                           ca 3
    (N->IN IN>>T ->T d+0)       ca c
    (T->IN [IN] ->T d-1)        w                 fetched over ca, which is done
    3                           w 3
    (R->IN IN<<T ->T d+0 r-1)   w bits            the place from R, as bits
    rshift                      w'

As it was, the address was kept under the cell while the cell was fetched,
and then swapped back up so that the byte's place could be computed from
it. As found, the place is computed first and put on R, and the fetch then
consumes the address. The shift that converts the place to bits takes it
from R.

The general rule: where a value is needed for two things, compute the
smaller of them first and put it on R. The other use then consumes the
value itself, and no cell is kept under what follows.

### A number stored twice is pushed once

Found in `pack>`, which starts a cell of packed fields: it stores 0 in the
cell at `a`, keeps `a` in `pto`, and stores 0 in `psh`. As it was (6,
after its first two stores):

    0 over !                    a                 0 stored at a
    pto !
    0 psh !

As found (4):

    0                           a 0
    (T->IN T<>N,IN-> ->[T])     0 a               stored at a, and the two
                                                  swapped: swap 2dup !
    pto !                       0
    psh !

A store consumes its value and its address. A store whose operation swaps
T and N stores at the cell that was under the top, and with the data stack
not moved it leaves both: the address on top for the next instruction, and
the number under it for the one after.

`pack,` does the same with a number it has not pushed at all. When a cell
is full it steps `pto` to the next cell, stores 0 in that cell, and stores
0 in `psh`. As it was (8):

    1 pto +!                                      (3: the 1, the address, +!)
    0 pto @ !                                     (3)
    0 psh !                                       (2)

As found (7), `p` being the address `pto` holds:

    1 pto                       1 pto
    ([T]->IN IN+N ->T d+1)      1 pto p+1         pto @ 1+, pushed
    (T->IN T<>N,IN-> ->[T] d-1) 1 p+1             stored back in pto
    (N->IN IN^N ->[T] d+0)      1 p+1             1 xor 1 is 0: stored at p+1
    (N->IN IN^N ->T d-1)        0                 and again, in place of both
    psh !

The address that `pto` was given is kept and stored through, where before
it was fetched a second time. The 0 is made from the 1 that was pushed for
the step, as the cell under the top combined with itself (`N->IN IN^N`).

`catch` makes the 0 it returns the same way, in the instruction that
returns (below), and `dnegate` makes its 0 as 1 shifted right by 1.

The general rule: a number that is stored in two places is pushed once and
kept by the first store. And a 0 needs no push where there is a cell under
the top to combine with itself.

### A store that takes its value from R and swaps

Found in `deposit`, which the ROM uses to take an image from the console:
it stores `u` at `a`, steps `a`, and adds `u` to a sum. As it was (7):

    tuck                        a u s u
    (T->IN IN+N ->R d-2 r+1)    a u               R: s+u
    over !                      a                 u stored at a
    1 +                         a+1
    r>                          a+1 s'
    exit

As found (5):

    dup >r                      a s u             R: u
    +                           a s'
    (R->IN T<>N,IN-> ->[T] r-1) s' a              u stored at a from R, and
                                                  popped; the two swapped
    1 +                         s' a+1
    swap ;                      a+1 s'

As it was, the sum went to R to be out of the way of the store, and came
back with an instruction of its own and a return of its own. As found, the
value that is stored goes to R instead. The store takes it from there and
swaps the address up as it does (`swap r> over !`; Overwriting the top of
the return stack in place, above). The word then ends in a `swap`, which
can hold the return.

The general rule: of two values, put on R the one that is only stored.
A store reads R as easily as it reads N, and pops it.

### A variable fetched and then stored: its address pushed once

Found in `(do)` and `(?do)`, which begin every counted loop. Each keeps
the previous loop's limit on the return stack, under its own return
address, and makes `rO` the new limit. `ra` is the return address. As it
was (6):

    r>                          limit start ra
    rO @                        limit start ra old
    >r >r                       limit start       R: old ra
    (N->IN T<>N,IN-> ->T d+1)   start limit limit swap dup
    rO !                        start limit

As found (5), `rO` being the variable's address pushed as a number:

    (R->IN T<>N,IN-> ->T d+1)   start limit ra    R copied, the two under it
                                                  swapped
    rO                          start limit ra rO
    (T->IN [IN] ->R d+0 r+0)    start limit ra rO the old limit, fetched
                                                  through the address, written
                                                  over R's top
    swap >r                     start limit rO    R: old ra
    (N->IN IN-> ->[T] d-1)      start limit       the limit stored through
                                                  the address

`rO @` and `rO !` are one instruction each, and each includes the
variable. With the variable's address pushed once, the fetch and the store
are still one instruction each. The fetch writes over the top of R
(Overwriting the top of the return stack in place, above), from which the
previous instruction copied the return address. The store takes its value
from under the address.

`i` has the address pushed for another reason: `@ +` with the address on
top is one instruction, and can drop what is under it. As it was (6):

    r>                          ra
    r@                          ra c              the count
    rO @                        ra c limit
    +                           ra i
    swap >r                     i                 R: c ra
    exit

As found (5):

    r>                          ra
    r@                          ra c
    over >r                     ra c              R: c ra
    rO                          ra c rO
    ([T]->IN IN+N ->T d-2) ;    i                 the limit fetched and added;
                                                  ra and c dropped; the return

The general rule: a variable in one instruction is the shortest way to
fetch it or store it once. Where it is fetched and stored, or fetched into
an operation, push its address: what follows is the same number of
instructions, and each can do more.

### An instruction that does not use R can still move it

Found in `catch`, after the word it ran has returned. The return stack
holds the saved loop limit, the previous handler, and the depth of the
data stack. `catch` drops the first, restores the second, and drops the
third. As it was (6):

    rdrop                                         R: depth handler
    up @ 4 +                    h                 where the handler is kept
    (R->IN IN-> ->[T] d-1 r-2)                    put back, and both popped
    0
    exit

As found (5), `up` being that variable's address:

    up                          up
    (T->IN [IN] ->T d+1 r-1)    up task           fetched, and R popped
    4 +                         up h
    (R->IN IN-> ->[T] d+0 r-2)  up h              put back, and both popped
    (N->IN IN^N ->T d-1) ;      0                 up xor up; the return

The `rdrop` is folded into the fetch, which does not otherwise use R, and
the 0 and the return are one instruction.

### The other operand on R, not swapped for

Found in `dnegate`, `(+loopnext)` and `2/`.

`dnegate` inverts both cells of a number and adds one. As it was (6,
before it goes to `d+`):

    invert                      lo ~hi
    swap                        ~hi lo
    invert                      ~hi ~lo
    swap                        ~lo ~hi
    1 0                         ~lo ~hi 1 0

As found (5):

    dup >r                      lo hi             R: hi
    (N->IN ~IN ->T d-1)         ~lo               lo inverted, over hi
    (R->IN ~IN ->T d+1)         ~lo ~hi           hi from R, inverted
    1                           ~lo ~hi 1
    (T->IN IN>>T ->T d+1 r-1)   ~lo ~hi 1 0       1 shifted right by 1; R
                                                  popped

`(+loopnext)` adds the step to the count and reports whether the count has
gone round. As it was (9):

    over 0                      inc c inc 0
    (T->IN N<IN ->R d-2 r+1)    inc c             R: is the step negative?
    dup >r                      inc c             R: f c
    over +                      inc c'
    (R->IN T->N,IN-> ->T d+1 r-1)
                                c' c' c           nip dup r>
    u<                          c' f2             did it go round?
    r> xor ;                    c' done

As found (7):

    dup >r                      inc c             R: c
    over +                      inc c'
    swap 0 <                    c' f              is the step negative?
    (R->IN Nu<IN ->T d+1 r-1)   c' f f2           c' u< c, c taken from R
    xor ;                       c' done

`2/` halves a number and keeps its sign. As it was (8):

    0                           n 0
    (T->IN N<IN ->T d+0)        n f               is n negative?
    63 lshift                   n s               as the top bit
    swap                        s n
    1 rshift                    s n>>1
    or ;                        n/2

As found (7):

    0                           n 0
    (T->IN N<IN ->R d+0 r+1)    n 0               the flag onto R
    1 +                         n 1               the 0 made into the count
    (N->IN IN>>T ->T d+0)       n n>>1
    63                          n n>>1 63
    (R->IN IN<<T ->T d+0 r-1)   n n>>1 s          the flag from R, as the top
                                                  bit
    (T->IN IN|N ->T d-2) ;      n/2               n dropped with it

In all three, a value was made early and was then in the way. Getting past
it cost two swaps in the first, a flag parked and brought back in the
second, and a swap in the third. On R, the value is an input of the
operation that needs it, and that operation pops it. In `2/` the flag goes
to R without the data stack moving, so the 0 it was compared with is still
there, and adding one to that 0 makes the count of the shift.

### Stores to adjacent cells, and a cell copied to a cell

Found in `spawn` and `mkheader`.

`spawn` fills a task's record, whose address is on R. It stores three
values in three cells of the record. As it was (9):

    r@ 1 + !                    xt dbase          the return stack's depth
    r@ 3 + !                    xt                where its data stack starts
    r@ 2 + !                                      the word it runs

As found (8), `t` being the record:

    r@ 1 +                      xt dbase rdepth t+1
    tuck !                      xt dbase t+1      stored, and the address kept
    (N->IN T->N,IN-> ->R d+0 r+1)
                                xt t+1 t+1        dbase onto R
    2 +                         xt t+1 t+3
    (R->IN IN-> ->[T] d-1 r-1)  xt t+1            dbase stored from R
    1 +                         xt t+2
    !

`mkheader` links the header it is making: the header `forth` holds goes in
its first cell, and the address of its code in the second. As it was (11),
`h` being the header:

    hdp @                       n h
    dup lastword !              n h
    forth @ over !              n h               the link
    cp @ dup thisxt !           n h xt
    over 1 + !                  n h               the address of its code

As found (9), `forth` being that variable's address:

    forth hdp @                 n forth h
    (N->IN T->N,IN-> ->R d+0 r+1)
                                n h h             forth's address onto R
    lastword !                  n h
    (R->IN [IN] ->[T] d+1)      n h h             what forth holds, stored at h:
                                                  a cell to a cell, through R
    cp @                        n h h xt
    (T->IN T<>N,IN-> ->T d+1 r-1)
                                n h xt h xt       R popped
    thisxt !                    n h xt h
    1 + !                       n h

The general rule: an address rebuilt from R for each store (`r@ 1 +`,
`r@ 3 +`) costs three instructions a store. One address stepped along
costs two, because `tuck !` keeps the address and adding a number is one
instruction. And a cell is copied to another cell in one instruction when
the source address is on R (`R->IN [IN] ->[T]`). That instruction can also
push a copy of the destination address.

### The same text in two words

These were found by reading the ROM and `nuc.fs`, not by the search. The
search takes each run where it is, and cannot tell that two runs are the
same. Each of these was the same text written twice, and is now written
once:

- `throw`, when it restarts the prompt, and `afresh`, when the ROM starts,
  both store 0 in `state`, `sourceid` and `sbase`. That is now a word,
  `prompting`, which `throw` calls and `afresh` falls through into.
- `cells-move`, `cells-fill` and `cmove` ended the same way: each drops
  the loop's two cells and the value it kept on R. That ending is now a
  word, `cells-done`. `cells-move` falls through into it, and the other
  two jump to it. It is never called: it drops from R the value that the
  word ending in it put there, and a call would put its own return address
  there.
- `u/mod`, where the dividend does not fit in 16 bits, pushed a 0, took
  the divisor off R and then did everything `um/mod` does. It now ends in
  `um/mod` there.
- `ifield`, which the ROM uses to read the disk's index, ended as `field@`
  does: a cell shifted right by the byte's place and masked. That ending
  is now `(field)`. `field@` falls through into it, with the place and the
  mask on R, and `ifield` puts its own two values there and jumps to it.
- `delchar` had `d# 32 emit`, which is `space`.
- `dup 0<` is the whole of `s>d`. `.` and `literal`, which run once for
  each number printed or compiled, now call it.

A word that ends the way another word does can fall through into it, with
no return and no jump: `;into`, or a `:` before the previous definition
has ended (The kernel's size, above). Only the word placed directly before
it can do that. Any other word jumps there, which costs one instruction
where it is written and one more each time it runs. Code that two words
share in the middle becomes a word that both call.

Not counted among these: `negate` and `2drop`. Each is one word used in
many places, and `cross.fs` compiles it inline. The top of `nuc.fs`
explains why `negate` is not a call. The next section has what was found
about the two since then. `(do)` and `(?do)` begin the same way, and
falling through cannot share a beginning.

### In the source

All of this is in `nuc.fs`. What the search found is written in standard
words, which `cross.fs` compiles into the instructions shown: `2dup lshift
>r` and `r@ swap rshift` are the two shifts in `sfind`, `tuck over !` is
the store that swaps in `pack>`, `swap r> over !` is the one in `deposit`,
and `up dup rdrop @` is the fetch that drops R in `catch`. The decompiler
prints an instruction as such a phrase (`disasm_hex`), which is how to
find the words for an instruction.

Three instructions are written by their fields, with their words in a
comment beside them:

- `N->IN IN^N ->[T]` in `pack,`, and `R->IN [IN] ->[T] d+1` in
  `mkheader`. No phrase of three words compiles to either.
- `[T]->IN IN+N ->T d-2` in `i`, which is `@ + nip`. From those words
  after `over >r`, `cross.fs` compiles the add and then `swap >r`, which
  is the word as it was, with a return of its own.

For two of them `cross.fs` compiles different instructions, the same
number of them:

- `(do)` and `(?do)`: it takes the return address off R as it copies it
  (`swap r>`), and the fetch through the address then pushes the old
  limit onto R, where the search wrote over R's top.
- `spawn`: the second of its three cells is stored with `tuck d# 2 + !`,
  which keeps the address under the value on the data stack, where the
  search put the value on R.

And `(+loopnext)` is one instruction shorter than the search's version. It
tests whether the step is negative with `0<`, which in the kernel is a
call, where the search pushed a 0 and compared.

`examine-16`, the same target for each other machine, and `examine-virt`
print each kernel's present length and the room left in its ROM.

## What reading the kernel's ROM found

The search takes each run where it is. Reading the ROM's disassembly
(`disasm_hex build/virt/nuc.hex 0 0x800`) beside `nuc.fs` finds what the
search cannot: code that two words have in common, code that is never
reached, and code that is shorter written another way. Every instruction
used here was already in the kernel. `examine-virt`, and the same target
for each machine, print the kernel's size with these changes.

### A word that ends in another word is placed before it

A word whose last word is another word ends with a jump to it. `c@` is a
mask followed by `field@`. As it was (2):

    : c@      255                   ca 255
              jump field@

Placed directly before `field@`, with no return and no jump of its own,
`c@` is the mask alone, and the instruction after it is `field@`'s first.
As it is now (1):

    : c@      255                   ca 255
    : field@  >r                    ca              R: 255
              ...

That saves one instruction where the word is, and one jump every time it
runs. Words can fall through in a chain: `count` into `c@`, `c@` into
`field@`, and `field@` into `(field)`.

Such a word names the word it falls through into (`;into field@`,
`cross.fs`), and the build stops if another definition comes between the
two. A definition that only needs no exit, because its last word returns
for it (`inline;`), still ends with `;fallthru`.

Only one word can be directly before another. Of the words that end in
`field@`, that word is `c@`, which runs most often, and `uw@` jumps. These
words are placed this way in the kernel: `0>` before `>`, `0<>` before
`<>`, `space` before `emit`, `hi16`
before `lo16`, `count`, `c@` and `field@`, `c!` before `field!`, `depth`
before `-`, `d2*` before `d+`, `samename?` before `same-cells?`, `,` before
`allot`, `parse-name` before `parsed`, `-throw` before `throw`, `landing`
before `near`, `mkheader` before `sync`, `:` before `]`, `else` before
`then`, `loop` before `resolveleaves`, `main` before `quit`; and in the
virtio disk's driver `vstatus` before `v!` and `vwrite` before `vio`.

Twelve more are placed away from their neighbours in the source, because
the word each one ends in is defined elsewhere: `prev` before `1-`; `i@`
before `uw@` and `i!` before `w!`, on machines where a cell is wider than
an instruction; `source` before `2@`; `unparsed` before `/string`;
`isvoid` before `-throw`, which is before `throw`; `inline;` before
`code,`; `recurse` before `compile,`, which is before `execute`; `create`
before `constant`, which is before `exit,`; and `afresh` before
`prompting`.

Where several words end in the same word, the one that runs most often
during a boot is placed before it. `disasm_hex IMAGE.hex NAME... --counts
WORK` reports how often each is entered, from the work that
`compiling-32`, and the same target for each other machine, writes. So
`prev` is before `1-` and `adepth` jumps; `isvoid` is before `-throw` and
`newpage?` jumps; `inline;` is before `code,` and `if` jumps. `i@` matters
most: the nucleus's peephole enters it for every instruction it examines,
and each entry now saves a jump.

### A word that is another word's instructions

`negate` is `invert` followed by an add of one, and `cell+` is an add of
one. As they were (3 and 2):

    : negate  invert
              1 +
              ;
    : cell+   1 +
              ;

As they are now (1 and 2):

    : negate  invert
    : cell+   1 +
              ;

`negate`'s body is `invert`, falling through into `cell+`. Both are
compiled inline wherever source uses them. `negate` is never a call: `-`
is `negate` and an add, so a call there would make every subtraction a
call that makes a call. These bodies are what a header finds, for the
prompt and for the nucleus's compiler, which reads a small word's
instructions from the word's body. Read from `negate`'s address, the
instructions are the same as before.

- `here` is `dp @` followed by the whole of `c>`. It is now `dp @`,
  falling through into `c>`.
- `aligned` ends as `c>` does, and jumps to it.
- `1+` and `cell+` are the same two instructions, because an address is
  the number of a cell and the next cell is one more. They are one body
  with a header for each name. Code that finds a name from a word's
  address finds one of them: `see cell+` begins `: 1+`, the first header a
  search by address reaches, and `disasm_hex` prints both. The Words
  window used to find its selection in the list from the word shown, so
  selecting `cell+` selected `1+`, and the selection could not move down
  the list past it. The window now keeps the header that was selected
  (`wchosen`, `see.fs`), and the desktop's checks select `cell+` and move
  on from it.
- `begin` does exactly what `landing` does, and `again` exactly what
  `code,` does. A loop comes back to the address where code is about to be
  compiled, and an unconditional jump's instruction is just its target, so
  compiling the target compiles the jump. Each was one instruction, a jump
  to the other word. Each is now a word with no instructions that falls
  through into the other (`: tagain ;into code,`), so its header finds the
  other word's code.
- `idle`, which a task runs after its word has thrown, was a lone return.
  `cells` also does nothing, so the task is given `cells` instead.

A word with no instructions has a header. The word it falls through into
may have none, as `code,` and `landing` have none. That word keeps its own
name: `cross.fs` gives it a label, as it does every word without a
header, and where an address has both a header and a label, the label
names the word whose instructions are there. The table of names lists the
label first for such an address (`xt-table`, `cross.fs`), and the tools do
the same (`util/words.h`): `see` and `disasm_hex` show `code,` where it is
called, and `see again` begins `: code,`. `cross.fs` stops at a word with
no instructions and no header, because its label would be mistaken for the
name of the next word.

### A header that a newer one hides

The kernel had a `;` and a `[']`. `join.fs` defines `;` again, and
`placement.fs` defines `[']` again. A search finds the newest header of a
name, and no code reached the kernel's two words in any other way. So the
ROM carried their instructions without using them, and they are gone.
`[']` is gone only where the machine has a display, because a machine
without one has only the kernel's `[']`.

`.shadows` lists every name that is redefined. `kernel_test` now counts a
header as a way to reach its word only if no newer header of the same name
hides it (`util/reach.h`). So a kernel word that is hidden and not called
fails the test `kernel`.

### A loop that tests at its head

`delchar` steps back over the bytes of a character that was typed. It stops
when no bytes are left, or at a byte that starts a character. As it was
(13):

    begin
      0 over =                      a n f
      if exit then                  a n             a branch, and a return
      1-  2dup +  c@                a n c
      192 and  128 xor              a n f
    until
    ;

As it is now (11):

    begin
      dup                           a n n
    while                           a n             a branch to the return
      1-  2dup +  c@                a n c
      192 and  128 xor              a n f
    until
    then ;

`dup 0= if exit then` is four instructions: a 0 pushed, the compare, the
branch and a return of its own. `dup while` is two, and its branch goes to
the return the word already has.

`>number` leaves its loop at the end of the text and at a character that is
not a digit. The second exit was `0= if drop exit then`: a call of `0=`, a
branch and a return. It is now a second `while`, whose branch goes past
`repeat` to a `drop` before the `then`. The call is gone, for every digit
of a number. `binaries`, which goes down the entries of a disk's index,
tested for the last entry with `dup 0= if drop exit then`, and is now
`dup if ... then drop`.

`cross.fs`'s `while` now leaves its two addresses in the order the
standard gives. It used to leave them the other way round, which only
`repeat` handled correctly. So a `while` ended by `until`, or by a second
`while`, was compiled with a branch that had no target, and `cross.fs`
printed no error. A branch that never gets a target now stops the build.

### More of the same text in two words

- `'` and `postpone` both took a name, looked it up and threw -13 if it was
  not found. That is now a word, `sought`, which leaves the flag that
  `postpone` tests and `'` drops.
- `is'c'` tested twice whether a character is a quote, and combined each
  answer with the result so far. That is now `tick?`.
- `vdisk?`, in the virtio disk's driver, read two registers and tested
  whether each was 2. That is now `also-2`.
- `prompting` stored 0 in `state`, which is the whole of `[`.
- `abs` had `dup 0<`, which is `s>d`.
- `quit` printed its ` ok` one character at a time. `say"`, which
  `no-disk` gets its text from, makes a number that holds as many
  characters as a cell holds, and `emits` sends them: one number where a
  cell is 32 or 64 bits.

### Facts that hold where a word is used

`throw` tested whether the value thrown is -1 with `dup d# -1 = if`: a 0
pushed, inverted, and compared. -1 is the cell with every bit set, so its
`invert` is zero. `dup invert if`, with the two branches exchanged, is one
instruction where there were three.

`afresh` makes the first task the running task, and then stored the
console's word in `'emit`, which is a cell of the running task: `up`
fetched and 6 added. At that point the task is `main-task`, so the cell is
one the image is built with, and it is now stored like a variable, in one
instruction.

`isnotspace?` was `isspace? 0=`, a call and a jump: six instructions run
for each character of a name. A character is not a space when it is
greater than 32, so the word is now the 32 pushed, falling through into
`u>`.

### An ending that is a word is a jump to it

`2drop` writes R to R in order to drop two cells, so it cannot hold the
return: `2drop ;` is two instructions. `2drop` is also a word of the
kernel, made of those two instructions, and a jump to it is one
instruction. As it was, at the end of `type` (2):

    2drop                           R->IN IN-> ->R d-2
    ;

As it is now (1):

    jump 2drop

`2drop` is `callable`, as the two-instruction basewords are. `cross.fs`
holds back a word that is one instruction and cannot take the return until
it knows what follows (`alone?`). At the end of a definition, the word is
called and the call becomes a jump. Anywhere else it is compiled inline as
before. Source still says `2drop ;`. This applies only on the kernel's
page, where `callable` applies: a jump is one more instruction to run, and
the kernel favours size over speed. `type`, `s,`, `(`, `interpret`,
`by-name` and `ipl` end this way.

A pushed number cannot take the return either, and a word that is one
number is held back the same way. `false` was, at the end of
`same-cells?`, while it was a 0 pushed. (It is now an instruction that
takes the return: A cell with itself, below.)

`kernel_test IMAGE.hex IMAGE.tables to 2drop` lists every place that jumps
to it, in the ROM and in the rest of the kernel's page.

`io!` is handled the same way. It is one instruction, a store to a port,
and cannot hold the return. It is `callable` too, so a kernel word that
ends in `io!` ends in a jump to it: `uart-emit`, which is the port's
number and `io!`; the reference machine's two disk words (`pd-read` and
`pd-write`, `port-disk.fs`); and `reg32!` (`reg.fs`) in QEMU's ROM, the
one ROM that has it. The jump costs one more instruction for every
character sent, every block and every register written. `compiling-16`,
and the same target for each other machine, report what a boot costs with
it.

## A cell with itself

An ALU instruction has two inputs: the one its input field selects, and
the cell under the top. The input field can select the cell under the top
too, and then the operation gets the same cell twice. A cell equals
itself, so `N->IN IN==N ->T d+1` pushes the true flag, -1, which is every
bit set. It does so whatever the cell holds, and with an empty stack too,
because the value read is only compared with itself. `N->IN IN^N ->T d+1`
pushes 0 in the same way.

A literal cannot hold a negative number, so -1 was a 0 pushed and inverted
(2):

    0                               0
    invert                          -1

It is now one instruction (1):

    (N->IN IN==N ->T d+1)           -1

Subtracting one was those two instructions and an add, and is now two
instructions. And where cells are dropped and then a flag is pushed, the
one instruction drops them as well: `2drop false` is `N->IN IN^N ->T d-1`,
because the cell it reads is one of the cells it drops.

### Why it was not found

The compilers did not produce it and the search did not find it, for one
reason. They learn about the machine from the step on terms
(`util/fuse.h`). The step gives the same term to values that are the same
cell, using the facts `vm.c` states about the arithmetic: which operations
take their inputs in either order, which can be regrouped, and which undo
themselves when done twice. `vm.c` also stated that xor of two equal cells
is zero. The terms used that fact to decide whether two cells are equal,
but not for a cell with itself: `x xor x` was a term like any other, and
so was `x = x`.

`t_made` now removes two equal terms from the operands of a xor. Where the
difference of two cells is a number, whether they are equal is a number
too. `fuse.h` also tests each fact `vm.c` states about the arithmetic
against the arithmetic when it starts, for every operation and for each
cell size. A fact that fails stops the program that includes `fuse.h`.

`vm.c` states two more facts about a cell with itself. `and` and `or` of a
cell with itself give that cell. And `<` and `u<` of a cell with itself
always give the false flag. `t_made` uses both: it
drops a repeated term from an `and` or an `or`, and it makes a compare of
a term with itself the number 0. Both facts are tested against the
arithmetic like the rest. With them, `dup and` is an instruction that
does nothing, and `dup <` is the instruction that zeroes the top
(`fewer_test`).

### The two names

`true` and `false` are names in `vm_opcodes.h` for the two instructions,
and the nucleus's word for each is the one instruction with its return. So
the decompiler shows `true`, where without the name it showed the shortest
phrase that does the same, `r@ r@ =`. A `true` typed at the prompt, or in
source the machine compiles from its disk, is the one instruction.

A -1 typed there, or worked out by the nucleus, is the one instruction
too. The nucleus's `literal` used to compile a negative number as its
complement and an `invert`, which for -1 is a 0 pushed and inverted; with
16-bit cells it compiled -1 as two literals. It now compiles the one
instruction for -1, and every other number as before (`tliteral`,
`nuc.fs`). That costs a few instructions in the ROM and saves one
wherever the machine compiles a -1. `examine-virt`, and the same target
for each machine, report the ROM's size with it. `booted-16`, and the same
target for each other machine, report what a boot compiles beside what
`cross.fs` compiles.

Source still says `d# -1`, `true`, `1-` and `2drop false`. `cross.fs` asks
the step for a shorter run that does a run's work, and the step now
answers with these instructions.

On the machine, `true` was a call to a word of two instructions. It is now
one instruction, which the nucleus's peephole tries with its neighbours
like any other.

The nucleus computes a number by running the code that computes it. It
recognised that code by its literals and by the instructions `folds.fs`
lists, each of which makes one number from one or two numbers. `1-` was a
0, an `invert` and an add, so the nucleus folded `16 1-` to 15. `1-` is
now `true` and an add, and `true` is neither a literal nor one of the
listed instructions: it makes a number from no input. `joins` now asks the
step which named instructions do that, and `folds.fs` stores the two in
cells the nucleus keeps (`of-none`, `join.fs`). Where the nucleus looks
for a number, one of those instructions counts as a number, and `16 1-`
folds to 15 again. An add after `true` with no number before it stays an
add: -1 as an added literal takes at least as many instructions as `true`
and the add, and an added literal does not join with the instruction after
it (`add-to`).

Inline, `1-` is now two instructions where it was three. So the nucleus
inlines it where its add joins the word after it. That is as long as a
call to `1-` and runs two fewer instructions: there is no call, and the
following word is folded into the add. Where the word after that would
also have joined, the result is one instruction longer: `1- rdrop dup` was
the call followed by `rdrop dup`, and is now `true`, `+ rdrop` and `dup`.
`booted-16`, and the same target for each other machine, show what a boot
compiles beside what `cross.fs` compiles. `compiling-16` reports what a
boot costs, and `peephole` reports what the nucleus's code would cost
beside `cross.fs`'s.

### Room for the decompiler's table

The decompiler's table of the words for each instruction (`fused.fs`) has
an entry for every instruction the image uses, and this change added
entries. Each of its fields used to take a cell: the instruction, the
number of words, and each word's header. None of those values needs more
than sixteen bits. With 64-bit cells and no bulk area, the table took four
times the bytes it needed, in the cells that one instruction can reach.
The fields are now sixteen bits each, packed one after another into cells
(`w,` and `hdr,` compile them; `ffield` in `see.fs` reads one through the
kernel's `ifield`). `memory-table-64` prints how much room those cells
have left.

### What the decompiler has words for

The decompiler's table was made from the instructions in an image, which
are the ones `cross.fs` compiled. The nucleus's own compiler makes
different instructions from the same source in places: it joins some
instructions that `cross.fs` leaves apart, and leaves apart some that
`cross.fs` joins. When a machine compiled a word with one of those
instructions, the decompiler had no words for it and showed its number.

The table now also covers what the nucleus compiles. It gets that from
the boot image, which is the system without its applications, in two
ways.

The boot image's own code is typed at the nucleus. The boot image is
built again with the peephole off, the two builds are cut into the same
straight-line runs (`bench/runs.c`), each run is typed at the nucleus on
the reference VM (`bench/typed.c`), and `fused` takes the instructions the
nucleus compiled (`--made`). The boot image is used because the whole
system with the peephole off does not fit the 16-bit machine: its code
would run past the last instruction that a 16-bit header can point to.

The applications are compiled by a booted machine. The reference VM boots
the boot image from a disk made from it and is stopped once it has
booted, and `fused` takes the instructions it compiled (`--booted`). That
is the nucleus's compiler as a machine runs it, on the source as the disk
has it.

Each machine has its own instructions. A variable's address, a number and
a field of a cell are different instructions with 16-bit cells than with
64-bit cells, so one machine's table lacks words for another's
instructions. There used to be one table, the default machine's, in every
image. Now each system has its own, made from its own image, its own
typed runs and its own boot (for example `build/16/fused.fs`).
`forth/fused.fs` is the default machine's table, and the only one checked
in. The reference VM cannot boot QEMU's system, which has a different
disk, so QEMU's table takes its boot from the 64-bit machine, which has
the same sizes.

Where several phrases of the same length name an instruction, the table
prefers one that uses the instruction's own operation. Once the step knew
about a cell with itself, `dup <` left the same result as `dup xor`, and
the table would otherwise have shown that xor as a compare.

`peephole` lists the instructions the nucleus compiles from the system's
code that are missing from the table, and `booted-16`, and the same target
for each other machine, list the ones a boot compiled. The test
`tables-64`, and the same test for each other machine, checks each table
against its image. The instructions still missing are those that no
phrase of four words names; `fused` lists them when it makes a table.

### What the search then found

The search uses the same step, so it can now use a 0 made from any cell
that is available. `shorter-virt` then found these in the ROM. Each holds
for any image, and does not depend on the numbers in this one.

`mulstep` runs once for every bit of a multiply. It put the multiplier on
R and tested whether the product's high cell is negative. As it was (5):

    >r                              lo hi           R: u
    0                               lo hi 0
    over >                          lo hi f         f: hi is negative
    r@                              lo hi f u
    and >r                          lo hi           R: u u&f

The 0 it compares with now goes on the return stack, made from the cell
under the top, where it is an ALU input. The compare pushes its flag and
pops the 0, and the multiplier goes onto R afterwards. As it is now (4):

    false >r                        lo hi u         R: 0
    over r> <                       lo hi u f
    over >r                         lo hi u f       R: u
    and >r                          lo hi           R: u u&f

(The search left the 0 on R and stored the multiplier over it,
`N->IN IN-> ->R`, which takes the same number of instructions.)

`pack>` stored 0 in the cell a name is packed from, and in `psh`: a 0
pushed, a store that kept it, and two stores to variables (7). Now the
first 0 is stored without a push, and the second is the last instruction,
a store through `psh`'s address that also holds the return (6):

    pfull !  pw !                   a
    false over !                    a               0, in the cell at a
    pto !
    psh                             psh
    false swap ! ;                                  and in psh

`evaluate` begins by saving `blk` and storing 0 in it: a fetch, a `>r`, a
0 and a store (4). With `blk`'s address pushed once, the fetch goes
straight to the return stack and the 0 is stored through the same address
(3):

    blk                             blk
    dup @ >r                        blk             R: what blk had
    false swap !

Both of these are shorter only where a variable's address is one literal,
as it is with 64-bit cells. With 16-bit or 32-bit cells the address is two
literals, and `cross.fs` compiles the same source to code of the old
length: in `pack>` it uses the variable's own instruction again.

`sfind`, when it has found the name, has four cells on the data stack that
it no longer needs, and the next header's address on the return stack. It
dropped those first, and then fetched the header's cell and removed the
flag from it with a shift up and a shift down. As it was (8):

    nip nip rdrop                   c-addr name
    true                            c-addr name -1
    + nip                           name-1
    @                               x
    1                               x 1
    2dup lshift >r                  x 1             R: x<<1
    r@ swap rshift                  x xt
    swap rdrop                      xt x

Now every instruction that drops a cell also does other work. The cell is
fetched to the return stack, over the address that was there, and the
instructions that do the work drop the cells that are finished with. As it
is now (7):

    true                            c-addr u key name -1
    + nip                           c-addr u name-1
    nip rdrop @ >r                  c-addr          R: x
    1                               c-addr 1
    r@ over lshift >r               c-addr 1        R: x x<<1
    nip r> swap rshift              xt              R: x
    r>                              xt x

The search's seven instructions were different. It kept three of the cells
to the end and dropped two with the shift down,
`R->IN IN>>T ->T d-2 r-1`. No phrase of four words names that instruction,
so the decompiler would have shown it by its number. These seven drop the
cells sooner, and the decompiler has a phrase for each.

In `nuc.fs` each is written in standard words, which `cross.fs` compiles
to those instructions: `false >r`, `over r@ <` and `rdrop over >r` in
`mulstep`; `false over !` and `false swap !` in `pack>` and `evaluate`;
and in `sfind` `true + nip`, `@ rdrop >r drop`, `r@ over lshift >r`,
`r> swap rshift` and `nip r>`.

## What the survey found

The target `profile` ends with a survey of the image (`util/survey.c`). It
takes every straight-line run of up to six instructions and asks the step
whether one instruction, or two, would leave the same result. `cross.fs`
has already joined what it can, so the survey lists the runs it left: each
run, what would replace it, every place it occurs, and how often that
place ran in the desktop's work (`build/profile/survey.txt`, part 3).

Every run it listed came from ordinary source: `over 0<`, `u< 0=`,
`drop d# 0`, `@ 0=`. The source is unchanged. `cross.fs` left these runs
long because of three limits in how it asked the machine, and those are
what changed (Why `cross.fs` left them, below). The listings are from the
64-bit image.

### A flag compared with zero is the flag inverted

Found in `glyph#`, `rmask`, `across`, `span`, `while?`, `pline`,
`(view-show)` and `lay-at`. Each makes a comparison and then asks whether
the flag is zero (`u< 0=`). In `glyph#` the stack is `i a`: an index, and
the address of a count.

As it was (3):

    @ over u>                   i f               f: is i below the count?
    0                           i f 0
    =                           i g               g is true when f is false

As it is (2):

    @ over u>                   i f
    invert                      i g

A flag is all ones or all zeros, so comparing it with 0 gives the same
cell as inverting it. `cross.fs` already did this after a comparison that
is a baseword: `u< 0=` was `u<` and `invert`. Here the comparison had
first been joined with the fetch before it, into `[T]->IN Nu<IN ->T`, and
after that the `0=` was left as it was written.

`pushes` keeps the flag and tests a copy of it, with `dup 0=`. That was a
0 pushed and `over =`. It is `dup invert`, one instruction
(`T->IN ~IN ->T d+1`).

### Zero after a drop is made by the drop

Found in `over8`, `glyph#`, `nm-blk-of`, `data>lbl`, `lit>hdr`,
`picture-start` and `hover`. Each drops one cell or more and then pushes a
0. In `over8` the stack is `a b`.

As it was (2):

    2drop
    0                           0

As it is (1):

    (N->IN IN^N ->T d-1)        0                 2drop false

An instruction can take the cell under the top as both of the ALU's
inputs, and a cell xored with itself is 0 (A cell with itself, above). The
instruction reads that cell before the stack moves, so the same
instruction can drop cells and leave the 0. `drop 0` is
`N->IN IN^N ->T d+0`, `2drop 0` is the same with `d-1`, and `2drop drop 0`
with `d-2`. `hover` returns two zeros with `2drop 0 0 exit`, which was
three instructions and is two: `2drop false`, and `false` with the return
in it.

### A compare with zero takes the zero from the return stack

Found in `nth` and `jmade`. `over 0<` asks whether the cell under the top
is negative. The stack is `n h`.

As it was (3):

    over                        n h n
    0                           n h n 0
    <                           n h f

As it is (2):

    (N->IN IN^N ->R r+1)        n h               R: 0        false >r
    (R->IN N<IN ->T d+1 r-1)    n h f             over r> <

The compare wants `n` as N and the 0 as its input. `n` is already under
the top, so only the 0 is missing. The first instruction makes it from the
cell under the top and puts it on the return stack. The second compares N
with R, pushes the flag and pops R. No cell is copied. `mulstep` has had
these two instructions since the search found them there (What the search
then found, above), written in `nuc.fs` as `false >r` and `over r> <`.
`cross.fs` now makes them from `over 0<`.

### A fetched cell compared with zero: xor with N, compare with N

Found in `text-back`, `tiff-window`, `tiff-row`, `gray1`, `asums`,
`gray-in` and `block-kept`, which say `@ 0=`, and in `band-mask`, which
says `dup @ 0=`. The stack is `n a`: an address, and whatever cell is
under it.

As it was (3):

    @                           n x
    0                           n x 0
    =                           n f

As it is (2):

    ([T]->IN IN^N ->T)          n x^n             @ over xor
    (T->IN IN==N ->T)           n f               over =

`x xor n` equals `n` exactly when `x` is 0. So the fetched cell is xored
with the cell under the top, and the result is compared with that same
cell. It is correct whatever `n` holds, and also when the stack has no
cell under the address, because whatever is read as `n` is the same cell
both times.

`dup @ 0=` keeps the address, so the xor goes to the return stack and the
compare takes it from there: `[T]->IN IN^N ->R r+1`, then
`R->IN IN==N ->T d+1 r-1`.

### An address taken from R once, added to and stored through

Found in `band-at`. The stack is `x`, and R holds an address `a`. The
word stores `x + a` at `a`.

As it was (4):

    r@                          x a
    +                           s
    r>                          s a
    ! ;

As it is (2):

    r>                          x a
    (T->IN IN+N ->[T] d-2) ;                      tuck + swap !

The address was read from R twice, once to add and once to store through.
Taken off R once, it is on top, where one instruction adds the cell under
it and stores the sum at it.

### Why `cross.fs` left them

`cross.fs` asks the machine for a shorter run in several ways, and each of
these runs fell between them.

- A run with a number in it goes to `fuse_lit` (`util/fuse.h`), which
  searches for a shorter run. It tried every single instruction, but a
  pair of instructions only when both were basewords: trying every
  instruction with every other, for every question, is too slow. `X 0 =`
  needs `X invert`, and `X` was a joined instruction, not a baseword.
  `fuse_two` already had a table of every pair by what the pair leaves
  (`list_pairs`). `fuse_lit` now looks a pair up in that table before it
  goes on to the longer search.
- `fuse_lit` was asked only when an ALU instruction, an add, a variable's
  instruction or a conditional branch was compiled. A 0 pushed last in a
  run, before a `then`, a call or the end of the word, was never asked
  about. A 0 pushed after an ALU instruction is now asked about too.
- A run of ALU instructions alone was looked up two and three at a time
  (`fuse_one`, `fuse_two`). `r@ + r> !` is four. A run of four to six is
  now looked up the same way.

The last two use one new question, `fuse_few`: the one instruction, or the
two, that leave what a run leaves. It is answered as the survey answers
it, by stepping the run on terms and looking the result up among the
single instructions and then among the pairs. It involves no search, so it
is cheap enough to ask at every instruction. Each answer is run on `vm.c`
before it is used, as every answer is.

### The nucleus's compiler

The nucleus compiles the same phrases when they are typed, and when a
machine compiles its applications from its disk. It has no table of pairs:
it computes what two or three instructions do together (`compose.fs`), and
applies rules that `util/joins.c` generates from the machine's step
(`folds.fs`). It already turned `0=` after a flag into `invert` by such a
rule. Each of the other runs stayed long for a different reason, and each
reason is fixed by a different change.

**A cell with itself.** `compose.fs` steps instructions on a model whose
cells hold where each value came from. The model knew an operation of two
cells only when one of them was the cell that started under the top. After
a `drop`, the cell under the top is a different cell, so the model lost
`false` after a drop, which is that cell xored with itself. The model now
has the facts `vm.c` states about a cell with itself: xor, `<` and `u<`
give 0 whatever the cell is, and `=` gives -1. So `drop false`,
`2drop false`, `drop true` and `2drop rdrop false` are each one
instruction, as `rdrop false` already was. The target `pairs` checks every
answer of the model against the machine's step.

**A 0 is `false`.** A typed 0 went from the kernel straight into the code
as a literal (`segs,`, `nuc.fs`), and the peephole never saw it. An
instruction after it could not join it either, because a literal is not an
ALU instruction. The kernel now compiles a 0 as the word `false`, through
`compile,`, and -1 as the word `true`. `false` is an ALU instruction, so
the peephole offers it to the instructions before it and joins later
instructions to it, like any other:

    drop 0                      N->IN IN^N ->T          one cell dropped, 0 on top
    2drop 0                     N->IN IN^N ->T d-1      two cells dropped
    0 >r                        N->IN IN^N ->R r+1
    0 ;                         N->IN IN^N ->T d+1 with the return

The nucleus also compiled `2drop` as a call, because the instruction
passes R through the ALU and the inliner refused every instruction that
reads or writes R. That instruction writes back the value it read, so it
does the same whatever R holds, and the inliner now accepts it
(`quiet?`). Without that, `2drop 0` on a machine was a call and a 0.

A literal 0 in a small word that is compiled inline becomes `false` the
same way (`emit0`, `join.fs`). The rules that looked for a pushed number
before an instruction (`across`, and the rules for `0 =`) count `false` and
`true` as a push, and `joins.c` checks every rule with each of them as the
push.

**`0 =` after any instruction.** `folds.fs` had one set of rules for
`0 =`: the instructions that leave a flag. `joins.c` now asks the machine,
for each named instruction X, for the two instructions that do the work of
X, `false` and `=` (`pair_for`, `util/fuse.h`). Each answer is a rule in a
set: X is changed by the rule's bits, and the set's own instruction takes
the place of `0 =`. Where X can stay as it is, the rule leaves it so. The
flags are the set whose instruction is `invert`, with X unchanged. The
fetches are another set:

    @ 0=                        [T]->IN IN^N ->T        @ over xor
                                T->IN IN==N ->T         over =

**`dup 0=` after a flag.** A set can have one fixed instruction between X
and the 0. After an instruction that leaves a flag, `dup`, `false` and `=`
are `dup invert`, which is one instruction. The nucleus tries these sets
first, because they replace the longer run.

**`over 0` before a compare.** One more kind of set holds the two
instructions for `over`, `false` and an instruction p: one instruction
takes the place of `over 0`, and p is changed by the rule's bits.

    over 0 <                    N->IN IN^N ->R r+1      false >r
                                R->IN N<IN ->T d+1 r-1  over r> <

**A store after `r@ + r>`.** No two of `r@`, `+`, `r>` and `!` join, so
the nucleus had the four. `joins.c` asks the machine for the two
instructions that do the work of A, X, B and `!`, for two pairs of A and
B: `r@` and `r>`, and `r>` and `over`. One instruction takes the place of
A, X and B, and the rule's bits change X into the instruction that also
stores. Where B can be that first instruction, the rule has it so: the
second then leaves the return stack alone, and a return folds into it.

    r@ + r> !                   r>
                                T->IN IN+N ->[T] d-2    tuck + swap !

    r> or over !                over
                                R->IN IN|N ->[T] d-2 r-1

The second is in `under!`.

**A literal add before `+`.** `band-at` says `r@ CELL2 + +`, so a literal
add stands between the `r@` and the `+`, and the store's rule does not see
`r@ +`. The sum is the same if the add is done to the cell under the top
before `r@` pushes. `joins.c` lists the instructions X for which X, a
literal add and `+` do the work of the add, X and `+`, whatever number is
added. The nucleus moves the add back across such an X. X and `+` are then
adjacent, and it joins them where one instruction does both: `over 1+ +`
is `1+` and `over +`.

**The rules are data.** The table of rules was laid as instructions at the
end of the page of code that draws, and no code could follow it on that
page. With these rules it is three times as long, and on the 32-bit
machine it no longer fitted there. It is now data in fields of sixteen
bits, as the decompiler's table is, and `i@` reads it there as it read it
among the code. The page of code that draws has that room back. The code
in `cross.fs`, `nuc.fs` and the tools that handled a table on a code page
is removed.

`peephole` reports what the nucleus's code costs beside `cross.fs`'s, and
lists every word the two compile to different lengths. It also checks
every join the nucleus made in the system's code against the machine's
step, however many instructions the join has and whether or not it has
numbers in it. `booted-16`, and the same target for each other machine,
report what a boot compiles beside what `cross.fs` compiles.
`compiling-16` reports what the boot costs with the extra work the
peephole does.
