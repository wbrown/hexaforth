# Regions with no state but places

A note of what is being done to `forth/region.fs`, and why, written before
it was started. When it is done this goes, and what is true of it is in
`CLAUDE.md` and the comments of `region.fs`.

## Why

`region.fs` has 45 variables: 15 are arguments stored and fetched back, 22
are the locals of loops (18 of them `rgn-op` and `merge`), 7 are what
`scroll-rect` works in, and one (`scroll-update`) is a result. Three tasks
now run this code (the desktop, the Forth window, the picture's), and it is
right only because nothing in it says `pause`.

Putting the 18 on the stacks as they are would be juggling. They are there
because the data does not say things, so the code keeps them:

- where a list of crossings ends (`ea` `eb` `qan` `qbn`)
- which way a crossing goes (`ina` `inb` `ino`)
- what is above a region's top and below its bottom (`ca`/`cb` of 0, and
  `next-y`'s test for the end)
- which operation it is (`opc`, looked at twice a crossing by `inside`)
- and four that follow from others (`pa` `pb` `qo` `ro`)

## What a region is to be

A count of the cells that follow, and then bands. A band is

    n  x0 .. x(n-1)  T

`n` crossings, and `T`, where the band ends: the row it goes down to, plus
`AS-END` (2^32).

- `T` is further along than any crossing (anything from `END-MIN`, 2^31, up
  is an end), so it ends the list and no count is needed to merge. The nearer
  of two bands' ends is the end of the band they make, so merging writes it
- the first band has no crossings and ends at the region's top; the last
  has none and ends at `NEVER` (2^33). So nothing is special about a top or
  a bottom. Nothing at all is `2 | 0 NEVER`; a rectangle is eight cells,
  `0 T(y0) | 2 x0 x1 T(y1) | 0 NEVER`, and nothing else is eight
- a band is an even number of cells, and a region starts on an even cell
  (`new-rgn` sees to it; nothing else makes one). So a crossing in is at an
  even address and a crossing out at an odd one: a place in a list is
  inside the region if its address is odd. That is so of the two regions
  read and of the one written
- whether the answer is inside is then the ALU's operation on the two
  places: `and` for what is in both, `or` for either, `invert and` for the
  first and not the second. The three differ in one instruction
- coordinates are within 2^31 either way, as `FAR` already took them to be

The count `n` stays, so that the next band is found from a band, and where
the next band of the answer goes from the last one written.

## The words

`merge` has two places to read and one to write: T, N and R.

    the nearer of the two crossings is written where the answer's next
    would go; if it is an end, that is the band's end, and stop
    each place that was at it moves on
    the written one stays (the place to write moves on) if what the ALU
    makes of the two places is not what the place to write says

The sweep (`rgn-op`) has the band of each region on the data stack and the
last band written on the return stack, over the region being made and the
merge to use. It starts with a band of nothing, for the first band to be
the same as. Each turn: merge at the place after the last band; if the
count and crossings are those of the last band, that band's end becomes
this one's, and otherwise this is the last band; each region whose band
ends where the answer's does moves to its next; until the end is `NEVER`.

`test/region_test.py`, run with no machine, is this in Python, with memory
an array and inside taken from the address: 32,000 operations a seed on
regions of up to eleven rectangles, every region the one form of its
pixels.

## Steps

1. `region.fs`: the layout, `new-rgn`, `rgn-none`, `rect-rgn`, `merge`,
   the sweep, `rgn-and` `rgn-or` `rgn-diff` with their short cuts (nothing;
   two rectangles), `rgn-offset`, `rgn-box`. No variables in any of them
2. `display.fs`: `mfind`, `rmask` and `reclip` read the layout, and nothing
   else does. `wm.fs` stores a count of 0 in two places to say nothing:
   `rgn-none`
3. `test/region_test.py VM IMAGE.hex`: random operations at the prompt of
   the reference VM, every cell of every region compared with the one form
   worked out from the pixels. Then `display_test.py`, `prompt_test.py`,
   `demo-test`, the comparison with the peephole off
4. `scroll-rect`: six arguments stored, six regions and the clip saved.
   The data space has under 500 cells left, so its regions stay where
   `halloc` put them; the arguments go to the stacks or to cells borrowed
   at `here`
5. Measure, and say what it did: `make demo-profile` and `vars.py` on
   `region.fs` against the figures below, `make -C rtl bench`, the depth of
   the stacks in each task. Then `CLAUDE.md`, and this file goes

## To be settled by measuring

- how the three merges differ in their one instruction. A loop for each
  that calls a word for the rest of a turn is plain Forth; three copies of
  the whole loop are faster and about 60 instructions more
- whether a turn of merge is better with a branch for which place moves on
  or with none (`shorter.py`)

## Before (the tree at 6e31472, `make demo-profile`)

`region.fs` runs 1,617,696 instructions, 23.8% of them fetches of
variables and 5.6% stores: `merge` 839,803, `rgn-op` 268,372, `inside`
125,409, `band` 102,303, `rect-rgn` 50,848, `same?` 45,260. 18 of its 24
words keep what they work on in variables.

What is expected, not yet measured: a third or more off `merge` and
`inside`, since a merge of two or three crossings has a turn to end it and
a call to pay for as well; the sweep about as many instructions as now and
fewer cycles; and no variables. Not a faster picture: by the averages a
region operation is about a tenth of what putting a row out of date costs,
and the rest is painting.
