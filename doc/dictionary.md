# The dictionary

How a name is found, what a search costs and where that is asked, what has
been done about it, and what has not: each change that was held back, what
it is, what it would save, and everything it would change. No measured
figure is here. The targets that are named print them, and what a change
saved is in the commit that made it.

## A header

A header is cells: a link to the header looked at after it, or nothing;
the word's execution token, with the top bit set if the word is immediate;
and the name. A name is codes of five bits packed into cells, and the top
bit of its last cell is set (`cross.fs` lists the codes).

`forth`, the first cell of the data, holds the header a search starts at.

- A header made on the machine is laid where `hdp` says (`mkheader`,
  `nuc.fs`) and linked to what `forth` held. It becomes the header `forth`
  holds when its definition ends (`attach`, which `;` runs), so a word is
  not found while it is being defined, and of a name the newest is found.
- While a file is read from a disk its own names are laid in memory taken
  from the heap, and are out of the chain when the file is done; those a
  `header` names stay, and are linked in then (`files.fs`).
- The image's headers are linked by `cross.fs`, in the order of how often
  the system's source says each name, the one said most first
  (`headers-ordered`). It counts every word of the source it reads, and
  of the applications' source where the image is built without them
  (`says`, `system.fs`). Headers of one name keep their order.

So a link may go on in memory as well as back, and the dictionary is not in
the order its words were defined in. `words` and the Words window list it
as it is searched.

## The search

`sfind` (`nuc.fs`) packs the name it is given as a header's is packed
(`>key`), and goes along the links comparing the first cell of each name
with the first cell of that. Where they are the same it compares the rest
(`same-cells?`). This is what it does at each header it goes past, an
instruction a line, as `disasm_hex build/16/nuc.hex sfind` shows it:

```
0239  dup            is this the end?
023a  0jump 0251
023b  dup @ >r       the link, put aside
023c  2 +            on to the name
023d  2dup @ =       its first cell against the key's
023e  0jump 024f
024f  r> nip         the link, in place of the name's address
0250  jump 0239
```

The compare is one instruction. The rest is the header's form. Two
instructions test for the end. Three put the link aside and bring it back,
because the link is two cells before the cell that is compared, and on this
machine a number is added in one instruction and taken away in three: from
the name there is no short way back to the link.

A number said with `d#`, `h#` or `c#` is not looked up: it goes to
`numbered` (`nuc.fs`, `literals.fs`). A number said bare is looked up
first, as any word is, and goes past every header before it is tried as a
number.

## What reads a header

A change to the dictionary changes these. Nothing else knows what a header
is.

What goes along the links, to the end or to a header it is told:

- `nuc.fs`: `sfind`, `words`
- `see.fs`: `xt>hdr`, `lit-walk`, `nwords`, `nth`, `hdr>i`, which are how
  the decompiler names a word and how the Words window lists them
- `files.fs`: `own-done`, which takes a file's own names out by going from
  `forth` to the header that was first when the file began
- the C: `first_header` and `read_header` (`vm_opcodes.c`), and what
  starts from them: `main.c`, `test/main.c`, `util/words.h`,
  `util/reach.h`, `util/disasm_hex.c`, `bench/finds.c`,
  `test/deskcheck.c`

What links a header in:

- `nuc.fs`: `mkheader`, `attach`
- `files.fs`: `name-stays`, `stays-in`
- `cross.fs`: `(header)`, `headers-ordered`, and where it sets `forth`
  and `forth0`

What knows where a header's link, token and name are:

- `nuc.fs`: `sfind`, `samename?`, `mkheader`, `words`, and `immediate`
  (`timmediate`)
- `blocks.fs`: `begins?`, which asks whether the last header made is the
  one `forth` holds
- `files.fs`: `name-cells`, `rec-key`, `own-rec`, `keep-run`
- `see.fs`: `hxt`, and where a header's name is shown
- `cross.fs`: where it lays a header, and the tables of names
- the C: `read_header` and `header_end`, which are all of the C that
  knows a header is two cells and then the name

## What it costs, and where that is asked

- `compiling-16` and the like: what it costs a machine to boot and compile
  its applications from its disk, each word with all it calls, and the
  loops
- `finding-16` and the like (`bench/finds.c`): how often the dictionary is
  asked, how many headers are looked at, by what was found; what the best
  order of the image's headers would look at; what the same searches would
  look at were the dictionary several chains, for each way of choosing a
  name's chain and each number of chains, beside what that would be if
  names fell evenly; and each name by what was looked at for it
- `disasm_hex build/16/boot/nuc.hex sfind --counts
  build/16/compiling/work`: how often each instruction of the loop ran

Two changes were made, and what each saved is in 6e3253d: the image's
headers linked by use, and a number after `d#` not looked up. Before them
the search was most of what a boot's compiling cost. After them:

- `sfind` is still the largest single cost of it, and is no longer most
  of it. Reading the source's text (`parse-name`) and the peephole
  (`kept,`) are next, and neither depends on the dictionary.
- Most of the headers it goes past are headers made on the machine since
  the image, which are a file's own names and those that stay, on the way
  to a word of the image. They are searched first, so no order of the
  image's headers takes them away.
- The order of the image's headers is close to the best there is for that
  work: `finding-16` says both.
- The names most often not found are numbers said bare, `0` and `1`
  first. Each goes past every header.

## What has not been done

Three changes, the first two of which were one step when they were first
put. None is built, and none has been run: the loops below are written
from instructions the present loop already has, and what each saves is a
count of those, not a measurement.

### A last header that always matches

The chain ends in a header whose first name cell is stored with the key's
first cell before each search. The compare is then true there at the
latest, so the loop has no test for the end, and a match at that header is
a name not found. With the header's form as it is, and the branch at the
foot of the loop:

```
M:  r> nip           the link, in place of the name's address
C:  dup @ >r         the link, put aside        (the loop is entered here)
    2 +              on to the name
    2dup @ =         its first cell against the key's
    0jump M          not this one
```

It saves three of the eight instructions the search spends on a header it
goes past.

It changes `sfind`; where the chain ends, which is `cross.fs`'s to lay,
and is a header in memory that can be stored to; and everything that goes
along the links to the end, which is to stop a header sooner. The form of
a header and what reads its fields are as they were.

### The link in the cell after the first name cell

Links point at a header's first name cell, and the link is the cell after
it, so that from the cell compared the next header is one number added and
a fetch:

```
N:  1 +              the cell after the first name cell
    @                the link: the next header's first name cell
C:  2dup @ =         its first cell against the key's   (entered here)
    0jump N          not this one
```

With a last header that always matches, that is four instructions of the
eight.

The link then comes between the first cell of a name and the rest of it.
Either a name is in two parts, or its first cell is said twice, which is a
cell more for every header. Whichever it is, it changes everything that
knows where a header's link, token and name are, as well as what the first
change does. Over the first change alone it saves one more instruction of
the five.

### Several chains

`forth` is several cells, each the head of a chain, and a name's chain is
chosen from the first cell of its packed name. A search goes along one. If
names fall evenly among them, it looks at one header in as many as there
are chains: the image's, those made since the image, and the whole of a
chain for a name that is not found. That is the only one of the three that
looks at fewer of the headers made since the image.

The form of a header is as it was, and so is everything that reads its
fields. What changes is what links and what goes along the links:

- `sfind` chooses the chain; `mkheader` and `attach` link into the chain
  of the name being defined
- `words`, the walkers of `see.fs` and the C go along every chain, and the
  order that `words` and the Words window list is the chains one after
  another
- `files.fs` takes a file's own names out of every chain when the file
  is done (`own-done`), and so is told where each chain was when the
  file began. For the one chain now that is the header `forth` held,
  which `own-names` keeps on the return stack among the cells a file
  being read has there. A head for each chain kept there would be a
  cell a chain for every file being read, out of the part of the stack
  that the task reading it has (`spawn`), and a deeper stack is dear
  (`doc/hardware.md`). The heads need not be there: `own-names` keeps
  where the file's own names begin (`base`), and they can be laid
  there, in the memory the names are laid in. `memory-map-16` and the
  like say the highest cell of that stack a boot uses, what it held
  then, word by word, and how many files were being read
- `cross.fs` links each of the image's headers into its chain, and orders
  each chain by use

How names fall is measured, and `finding-16` and the like say it: what the
searches of a boot would look at for each way of choosing a chain and each
number of chains, beside what they would look at if names fell evenly. The
low bits of the first cell are the name's first code, and by those alone
names do not fall evenly: few letters begin most of them. With the second
code mixed in (`dup 5 rshift xor`) they fall nearly evenly, and a third
code, or a multiply, adds little to that. Choosing a chain is a few
instructions, once for a search and once for a header that is made, since
the name is packed before either: `disasm_hex IMAGE.hex --typed FILE.fs`
shows what the nucleus makes of a way of choosing one.

### Taken together

With the chains in, the two changes to the loop save little: what they
make shorter is run that many times less. Without the chains, the first is
most of what the two give, for much the less change.

## The room in the ROM

All three change `sfind`, and the chains change `mkheader` and `attach` as
well. These are in the kernel's ROM, and a kernel is to fit its ROM on
every machine: an image whose kernel does not end inside it is not built
(`rom-ends`, `cross.fs`).

The reference machines' ROMs have room. QEMU's has the least: its ROM has
the driver of a virtio disk in it (`virt-disk.fs`, `virtio-disk.fs`),
which is longer than the one the reference machines have
(`port-disk.fs`). `examine-16` and the like say how long each reference
machine's kernel is and what its ROM has left, with each file's words and
code, and `examine-virt` says the same of QEMU's image.

By a count from the source, and not from a build:

- a last header that always matches takes two instructions out of the
  loop, and adds the store of the key and the test for that header: about
  as long as it was
- the chains add the choice of a chain to `sfind`, to `mkheader` and to
  `attach`, and `words` goes along each: longer, by more than QEMU's ROM
  had left when this was first written

So room is made in QEMU's ROM before either, and that is by making what is
in it shorter. How that is done is in `CLAUDE.md` (Making the kernel
smaller), and it is asked of the image and not of the source:

- what is written twice in the kernel is looked for first, before a word
  is moved out of it
- a word that nothing in the ROM runs before the image is read need not be
  in the ROM: it goes in the upper half of the kernel's page, where the
  end of `nuc.fs` has those that are there now. `kernel_test`, run with
  `typed` after the image and its names, says which words of the kernel
  that have a header are there only to be typed
- a case of a word that is handed nothing, and a path that a machine of
  one size never takes, are in the ROM all the same until the source
  leaves them out
- the driver is the ROM's as much as the nucleus is, and it is what makes
  this one full

The first has been done, twice: with what the search for fewer
instructions finds of the ROM's runs (`shorter-virt`), and then by reading
the ROM beside its source, which finds what the search does not: a word
that ends in another, a word that is another's instructions, a header that
a newer one hides. `doc/superoptimizations.md` says each, and every word
that was in the ROM and is still called is still there.
Whether the room that made is enough is known when a change is written.
The rest have not been looked at for this. A word is on the kernel's page
to be one instruction from every page, for what the machine compiles
later as well as for the image: that nothing in the image calls it is no
reason for it to leave.

## Not decided

- Whether the dictionary is changed again, and which of the three it is.
- Whether a number said bare is to go on being looked up first.
