# The dictionary

How a name is found, what a search costs, which targets report the cost,
what has been done about it, and what has been left undone. For each change
that was held back, this file describes the change, what it would save, and
everything it would affect. This file has no measured figures. The targets
it names print them, and the commit that made a change records what the
change saved.

## A header

A header is a run of cells:

- a link to the header that a search examines next, or 0 at the end of the
  chain
- the word's execution token, with the top bit set if the word is immediate
- the name. A name is five-bit codes packed into cells, and the top bit of
  its last cell is set (`cross.fs` lists the codes)

`forth`, the first cell of the data, holds the header where a search
starts.

- A header created on the machine is written at the address `hdp` holds
  (`mkheader`, `nuc.fs`) and linked to the header `forth` held. It becomes
  the header `forth` holds when its definition ends (`attach`, which `;`
  runs). So a word cannot be found while it is being defined, and a search
  finds the newest definition of a name.
- While a file is read from a disk, its own names are written in memory
  taken from the heap, and they leave the chain when the file is done. The
  names a `header` gives stay, and are linked in at that point
  (`files.fs`).
- `cross.fs` links the image's headers in order of how often the system's
  source uses each name, most used first (`headers-ordered`). It counts
  every word of the source it reads. Where the image is built without the
  applications, it counts the words of their source too (`says`,
  `system.fs`). Headers with the same name keep their order.

So a link can point forward in memory as well as back, and the dictionary
is not in the order its words were defined. `words` and the Words window
list it in search order.

## The search

`sfind` (`nuc.fs`) packs the name it is given the way a header's name is
packed (`>key`). Then it follows the links, comparing the first cell of
each header's name with the first cell of the packed name. Where the two
are equal it compares the remaining cells (`same-cells?`). This is the code
it runs for each header it passes, one instruction to a line, as
`disasm_hex build/16/nuc.hex sfind` shows it, with labels in place of the
addresses. The code for a header whose first cell matches is between
`0jump M` and `M`:

```
L:  dup              is this the end?
    0jump END
    dup @ >r         the link, saved
    2 +              on to the name
    2dup @ =         its first cell against the key's
    0jump M          not this one
M:  r> nip           the link, in place of the name's address
    jump L
```

The compare is one instruction. The rest comes from the header's layout.
Two instructions test for the end. Three save the link and bring it back,
because the link is two cells before the cell that is compared. On this
machine adding a number takes one instruction and subtracting one takes
three, so there is no short way back from the name to the link.

A number written with `d#`, `h#` or `c#` is not looked up: it goes to
`numbered` (`nuc.fs`, `literals.fs`). A bare number is looked up first, as
any word is, and passes every header before it is tried as a number.

## What reads a header

A change to the dictionary changes the code listed here. No other code
knows the layout of a header.

Code that follows the links, to the end or to a given header:

- `nuc.fs`: `sfind`, `words`
- `see.fs`: `xt>hdr`, `lit-walk`, `nwords`, `nth`, `hdr>i`. The decompiler
  uses them to name a word, and the Words window uses them to list the
  words
- `files.fs`: `own-done`, which removes a file's own names by going from
  `forth` to the header that was first when the file began
- the C: `first_header` and `read_header` (`vm_opcodes.c`), and the code
  that starts from them: `main.c`, `test/main.c`, `util/words.h`,
  `util/reach.h`, `util/disasm_hex.c`, `bench/finds.c`,
  `test/deskcheck.c`

Code that links a header in:

- `nuc.fs`: `mkheader`, `attach`
- `files.fs`: `name-stays`, `stays-in`
- `cross.fs`: `(header)`, `headers-ordered`, and the code that sets `forth`
  and `forth0`

Code that knows where a header's link, token and name are:

- `nuc.fs`: `sfind`, `samename?`, `mkheader`, `words`, and `immediate`
  (`timmediate`)
- `blocks.fs`: `begins?`, which tests whether the last header created is
  the one `forth` holds
- `files.fs`: `name-cells`, `rec-key`, `own-rec`, `keep-run`
- `see.fs`: `hxt`, and the code that shows a header's name
- `cross.fs`: the code that writes a header, and the tables of names
- the C: `read_header` and `header_end`. They are the only C that knows a
  header is two cells followed by the name

## What a search costs, and which targets report it

- `compiling-16`, and the same target for each other machine: what it costs
  a machine to boot and compile its applications from its disk. It charges
  each word with everything it calls, and lists the loops
- `finding-16`, and the same target for each other machine
  (`bench/finds.c`):
  - how often a name is looked up, and how many headers are examined, by
    what was found
  - how many headers the best order of the image's headers would examine
  - how many the same searches would examine if the dictionary were several
    chains, for each way of choosing a name's chain and each number of
    chains, beside the number if names were spread evenly
  - each name, with the headers examined for it
- `disasm_hex build/16/boot/nuc.hex sfind --counts
  build/16/compiling/work`: how many times each instruction of the loop ran

Two changes were made, and 6e3253d records what each saved: the image's
headers are linked by use, and a number after `d#` is not looked up. Before
them, the search was most of the cost of a boot's compiling. After them:

- `sfind` is still the largest single cost, but it is now less than half of
  the total. Reading the source's text (`parse-name`) and the peephole
  (`kept,`) come next, and neither depends on the dictionary.
- Most of the headers a search passes are headers created on the machine
  after the image was built: a file's own names, and the names that stay. A
  search for a word of the image passes them first, so no order of the
  image's headers removes that cost.
- The order of the image's headers is close to the best order for that
  work. `finding-16` prints both.
- The names most often not found are bare numbers, `0` and `1` first. Each
  passes every header.

## What has not been done

Three changes. The first two were one step when they were first proposed.
None is built, and none has been run. The loops below are written from
instructions the present loop already has, and the saving given for each
is a count of those instructions, not a measurement.

### A last header that always matches

The chain ends in a header whose first name cell is set to the key's first
cell before each search. The compare is then true at that header at the
latest, so the loop needs no test for the end, and a match at that header
means the name was not found. With the header's present layout, and the
branch at the bottom of the loop:

```
M:  r> nip           the link, in place of the name's address
C:  dup @ >r         the link, saved            (the loop is entered here)
    2 +              on to the name
    2dup @ =         its first cell against the key's
    0jump M          not this one
```

It saves three of the eight instructions the search spends on each header
it passes.

It changes three things:

- `sfind`
- where the chain ends. `cross.fs` writes that last header, and it must be
  in memory that can be stored to
- all the code that follows the links to the end, which must stop one
  header sooner

The layout of a header and the code that reads its fields stay as they are.

### The link in the cell after the first name cell

Links point at a header's first name cell, and the link is the cell after
it. From the cell that is compared, the next header is then one add and one
fetch away:

```
N:  1 +              the cell after the first name cell
    @                the link: the next header's first name cell
C:  2dup @ =         its first cell against the key's   (entered here)
    0jump N          not this one
```

With a last header that always matches as well, the loop is four
instructions where it is eight now.

The link then sits between the first cell of a name and the rest of the
name. Either a name is in two parts, or its first cell is stored twice,
which costs one more cell for every header. Either way, this changes all
the code that knows where a header's link, token and name are, as well as
everything the first change changes. Compared with the first change alone,
it saves one more instruction of the five.

### Several chains

`forth` becomes several cells, each the head of a chain. A name's chain is
chosen from the first cell of its packed name, and a search follows one
chain. If names are spread evenly among N chains, a search examines one
header in N. That applies to the image's headers, to the headers created
since the image, and to the whole of a chain for a name that is not found.
Of the three changes, only this one reduces how many of the headers created
since the image a search examines.

The layout of a header stays as it is, and so does all the code that reads
its fields. The code that links headers and the code that follows the links
change:

- `sfind` chooses the chain. `mkheader` and `attach` link a header into the
  chain of the name being defined
- `words`, the walkers in `see.fs` and the C follow every chain. `words`
  and the Words window list the chains one after another
- `files.fs` removes a file's own names from every chain when the file is
  done (`own-done`), so it must know where each chain was when the file
  began. With the single chain, that is the header `forth` held.
  `own-names` keeps it on the return stack, among the cells a file being
  read has there. Keeping a head for each chain there would cost one cell
  per chain for every file being read, out of the part of the stack that
  belongs to the task reading the file (`spawn`), and a deeper stack is
  expensive (`doc/hardware.md`). The heads do not have to be on the stack.
  `own-names` keeps the address where the file's own names begin (`base`),
  and the heads can be written there, in the memory the names are written
  in. `memory-map-16`, and the same target for each other machine, prints
  the highest cell of that stack a boot uses, what the stack held then,
  word by word, and how many files were being read
- `cross.fs` links each of the image's headers into its chain, and orders
  each chain by use

How evenly names are spread is measured. `finding-16`, and the same target
for each other machine, prints what the searches of a boot would examine
for each way of choosing a chain and each number of chains, beside what
they would examine if names were spread evenly. The low bits of the first
cell are the name's first code. Chosen by those bits alone, names are
spread unevenly, because a few letters begin most names. With the second
code mixed in (`dup 5 rshift xor`) they are spread nearly evenly, and a
third code, or a multiply, improves that only slightly. Choosing a chain
takes a few instructions, once for a search and once for a header that is
created, because the name is packed before either.
`disasm_hex IMAGE.hex --typed FILE.fs` shows what the nucleus compiles for
a given way of choosing a chain.

### Taken together

With the chains in place, the two changes to the loop save little, because
the loop they shorten runs that many times less often. Without the chains,
the first change gives most of what the two give together, for a much
smaller change.

## The room in the ROM

All three change `sfind`, and the chains also change `mkheader` and
`attach`. These words are in the kernel's ROM, and a kernel must fit its
ROM on every machine: `cross.fs` refuses to build an image whose kernel
ends outside its ROM (`rom-ends`).

The reference machines' ROMs have room. QEMU's has the least. Its ROM
includes the driver for a virtio disk (`virt-disk.fs`, `virtio-disk.fs`),
which is longer than the reference machines' driver (`port-disk.fs`).
`examine-16`, and the same target for each other reference machine, prints
the length of the kernel, the room left in its ROM, and each file's words
and code. `examine-virt` prints the same for QEMU's image.

Counting from the source, and not from a build:

- a last header that always matches takes two instructions out of the loop,
  and adds the store of the key and the test for that header. The kernel
  stays about the same length
- the chains add the choice of a chain to `sfind`, to `mkheader` and to
  `attach`, and `words` follows each chain. The kernel gets longer, by more
  than QEMU's ROM had left when this was first written

So room must be made in QEMU's ROM before either change, by making what is
in the ROM shorter. `CLAUDE.md` (Making the kernel smaller) describes how.
The answers come from the image, not from the source:

- look first for code that is written twice in the kernel, before moving a
  word out of it
- a word that no code in the ROM runs before the image is read does not
  have to be in the ROM. It goes in the upper half of the kernel's page,
  where the end of `nuc.fs` has the words that are there now.
  `kernel_test`, run with `typed` after the image and its names, lists the
  kernel words with a header that are reached only by typing them
- a case of a word that is never taken, and a path that a machine of one
  cell size never takes, are still in the ROM until the source leaves them
  out
- the driver is part of the ROM as much as the nucleus is, and the driver
  is what fills this ROM

The first has been done, twice. The first time used what the search for
shorter sequences finds in the ROM's runs (`shorter-virt`). The second time
was by reading the ROM beside its source, which finds what the search
cannot: a word that ends by running another, a word that is another word's
instructions, and a header that a newer header hides.
`doc/superoptimizations.md` describes each, and every word that was in the
ROM and is still called is still there. Whether that made enough room will
be known when a change is written. The other three have not been examined
for this purpose. A word is on the kernel's page so that every page can
call it in one instruction, for code the machine compiles later as well as
for the image. So a word stays on that page even when no code in the image
calls it.

## Not decided

- Whether the dictionary is changed again, and which of the three changes
  is made.
- Whether a bare number goes on being looked up first.
