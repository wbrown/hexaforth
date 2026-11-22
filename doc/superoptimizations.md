# Optimization patterns

The ways this machine's work has been made shorter or faster, whether a
person found them or a tool did. Each entry says where the pattern is used,
what it replaced, and why it takes fewer instructions or cycles on this
machine. How much a change saved is recorded in the commit that made it,
which is named; the figures are not repeated here.

The last section covers what the search for fewer instructions has found,
each with the loop before and after and the stacks after every
instruction. The search is `bench/shorter.c` (the program `shorter`, of
the machine whose image it is given, run at the end of the target
`profile`). It
takes one turn of a loop, puts the words it calls inline, and looks for the
fewest instructions that leave the same result, wherever the values are
kept (`util/loops.h`, `util/turns.h`, `util/fewer.h`). Every answer is run
on `vm.c` before it is reported.

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
is a word. Each is written as its words, the compilers make the one
instruction of them, and `vm_opcodes.h` has an entry for each, which the
build holds to what the words do. `2drop` routes R back to R, leaving it
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
the test and the branch, four instructions, and the search finds nothing
shorter (e310347). `fast-slab` stores the pattern row into every whole cell
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
it with its neighbours or if the result is no longer than the call (3f2a7a1,
4c7d7a1). Where a call site runs so often that the call and return matter,
`in-place` forces the word inline; it is used only after measuring
(`in-place -` in `fast-slab`, `move-row`, `row-direct`, `glyph` and
`text-body`, 4c7d7a1).

Basewords that the nucleus defines are given as headers for the instruction
rather than colon definitions, so that each use is the instruction itself
and can be combined (eea6888).

### The nucleus's compiler

Code typed at the prompt is combined as it is compiled (ac4f758). What two
instructions or three are together is worked out on the machine, not
looked up: they are stepped on a machine whose cells hold where each value
came from, the one instruction that would leave the same is made from what
is left and stepped too, and if the two machines agree that instruction is
compiled (`compose.fs`). Where they come to nothing, as `>r r>` does,
nothing is compiled. It replaced a table of rules (a1cf834, 88eee3c), and
the targets `pairs` and `peephole` hold what it works out to the machine's
own step. Constant arithmetic is worked out by running it (`100 30 - 2 *`
becomes 140); a `dup`, an `r@` or an `r>` before a push is folded into the
following instruction (`across`); and a variable is fetched or stored by
its one instruction (`var,`). A small word is compiled inline so its
instructions can be combined. The return-stack words are immediates that
offer their instruction to the one before, so `r> @` typed is one
instruction.

It does not do all that `cross.fs` does: it makes one instruction of two or
three, and does not replace three with two, or know that a comparison left
a flag. The target `peephole` says what that comes to.

## The kernel's size

The kernel favours small over fast. Each of these is explained in a comment
at the word, along with what the other way would cost or save.

- `callable`: a two-instruction baseword is a call inside the kernel and
  inline everywhere else; `negate` is not, since `-` is built from it
  (771d2c9)
- `c@` is `h# ff field@`; `um*` takes a step for every bit; `u/mod` has one
  fast path, for a small dividend; `name>` looks every character code up
  (ef4ae70)
- `inline;`: a word that is a call followed by the instruction to copy, with
  no exit: `r>` takes the return address and doesn't put it back, so the
  copy returns straight to the caller's caller (ef4ae70)
- code nothing can reach is removed, along with cases of a word that are
  never used (`util/reach.h`, ef4ae70)
- `segs,` uses the literal it is building as its own state: adding `h# 1000`
  moves its shift up one step, and ORing in `h# 4000` makes it an add
  (771d2c9)
- code written twice becomes a word, or falls through into the next word
  (`;fallthru`, 4bfc710)

## What the search has found

Each entry shows a loop's turn as it was in the image and as the search
found it, with the stacks after each instruction, and what changed. How
often the loop runs, and whether every search for something shorter
finished, is in `shorter`'s report.

The earlier search tool, `util/shorter.py`, found `move-row`'s two-instruction
move (above) and `cells-fill`'s fill with the value on R (e310347).
`bench/shorter.c` has shown that `aquarters`, `adown2`, `cells-fill`,
`cells-move`, `move-row`, `col!`, `column-run` and `fast-slab` can't be
made shorter, and found those below: first what it found in the dither and
the TIFF reader, on the 16-bit machine, and after What the three have in
common what it found in the desktop's own loops, on the 64-bit machine.
All of them are in the source.

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
address, for nothing. The top one is stepped to the next row. Then one
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
make the test once before the loop, and say a turn as

    begin  2dup and!  DISP-STRIDE +  r@ over =  until

Of that `cross.fs` makes another turn of the same length:

    ([T]->IN IN&N ->[T] d+0)    x a               2dup and!: x ANDed into the cell at a
    12 +                        x a'
    r@                          x a' end
    (T->IN IN==N ->T d+0)       x a' f            over =: is a' the end?
    until                       x a'

With the value where the operation takes it from, `2dup and!` is one
instruction: the cell ANDed with N and stored back, and the data stack not
moved. Nothing is parked on R, and the test is the end pushed and compared
with the address under it. The search's turn and this one are both five,
and the search found none of four. `col!`, the same loop with a plain
store, is four instructions a turn and the search finds nothing shorter.

### A count under a value: the value goes to R, and the test pops it

Found in `nwords` and `rec-next` (`see.fs`), `code-at` (`text.fs`) and
`lights` (`wm.fs`), on the 64-bit machine. All but `rec-next` kept a count
under the value they are working along, and added one to it every turn with
`swap 1+ swap`; `rec-next` stepped an address that way.

`nwords` counts the dictionary's headers. The stack is `n h`, the count and
a header, and a turn follows the header's link until there is none.

As it was (7):

    dup                         n h h
    while                       n h               out, if h is nothing
    swap 1 + swap               n' h
    @                           n' h'             the next header
    repeat

As found (6), with the test at the bottom:

    >r                          n                 R: h
    1 +                         n'                R: h
    r@ @                        n' h'             the link, fetched through R
    0                           n' h' 0
    (T->IN IN==N ->T d+0 r-1)   n' h' f           is h' nothing? and h popped from R
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
right before it is nothing. As it was (8), a turn is `dup`, the branch out,
`1 rshift swap 1 + swap` and the jump back. As found (7), it is `nwords`'s
turn with `code-at`'s shift in it:

    >r                          n                 R: m
    1 +                         n'                R: m
    1                           n' 1
    (R->IN IN>>T ->T d+0)       n' m'             m from R, shifted right 1
    0                           n' m' 0
    (T->IN IN==N ->T d+0 r-1)   n' m' f           is m' nothing? and m popped from R
    until                       n' m'

The rest of what `code-at` saves is the loop's shape. Tested at the bottom,
a turn has one branch where it had a branch out and a jump back. And
`until` goes round while the flag is nothing, which is the sense `u<` gives
here, so the `invert` that `while` needed is gone. `cross.fs` puts a test
at the bottom by itself only when the test is one instruction of `IN^N` or
`IN==N` (Loops test at the bottom, above); this test is five instructions,
and `nwords`'s is a `dup`. In `nwords` the test at the bottom is an
instruction longer than the `dup` it replaces, since it has to keep the
header while asking whether it is nothing, and the turn still comes out one
ahead.

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
