# The disk

A disk is blocks of `DISK_BLOCK` bytes (`vm_constants.h`). Block 0 is
the disk's index: what is on the disk, where it is, and what kind of
thing it is. Everything else on the disk is found through it.

The numbers of the index are in `vm_constants.h`, from `DISK_ENTRY` on,
where `mkdisk` and the Forth both have them; this file says what they are
for. What is built of it so far: `mkdisk --boot` writes an index of one
block, and the kernel's ROM reads it when the machine starts
(`doc/boot.md`). Index blocks after block 0, data entries, and finding an
entry by its label are not built yet.

## Entries

The index is a table of entries. An entry is a run of blocks: where it
starts, how many blocks it is, what kind of thing is in it, and a label
to find it by.

An entry is `DISK_ENTRY` bytes, and a block is as many entries as that
goes into `DISK_BLOCK`. A number in an entry is two bytes or four, its
lowest byte first, at an even byte: the layout is the same whatever a
machine's cell is. An entry starts where a cell does on every machine,
and so does its label, which is compared a cell at a time.

| Where in the entry | Field |
|---|---|
| its first `DISK_LABEL` bytes | the label |
| `DISK_ENTRY_TYPE` | the type |
| `DISK_ENTRY_FIRST` | the first block |
| `DISK_ENTRY_BLOCKS` | how many blocks |
| `DISK_ENTRY_WHERE` | where it goes in memory, for binary; 0 for the rest |
| `DISK_ENTRY_LENGTH`, four bytes | how long it is |
| the rest | nothing yet, and 0 |

The label is a count and as many characters as the rest of its
`DISK_LABEL` bytes hold, with zeros after them. It is shown with `count
type`, and two labels are the same when their cells are. The longest
name of a file of the system, `virtio-input.fs`, fills it. A label is
what its writer gives it, `display.fs` or `display`: the type says what
an entry holds, and the label need not.

A block number is two bytes, so a disk is at most 65,536 blocks.

## Types

| Type | What the blocks are |
|---|---|
| `DISK_FREE` | nothing: the slot is free |
| `DISK_BINARY` | binary |
| `DISK_SOURCE` | source |
| `DISK_DATA` | data |
| `DISK_INDEX` | index |

**Binary** is memory, read into place when the machine starts (The boot,
below). `DISK_ENTRY_WHERE` is where it goes: memory counted in blocks
from cell 0, so the cell is that number times the cells a block is
(`BLKCELLS`), and a run starts where a block of memory does. Two bytes of
it reach 64MB of memory. Its length is in bytes.

**Source** is Forth as `load` reads it and the editor shows it: lines of
`DISK_LINE` characters, a block's worth to a block. Its length is in
lines.

**Data** belongs to an application, and its bytes are what the
application says they are: a TIFF, the names that go with the image,
HexaPaint's canvas, the threshold masks, the viewer's bitmap. Its length
is in bytes.

**Index** is more of the table: its blocks are index blocks.

## Block 0

Entry 0 of block 0 is the disk's own:

| Where in the entry | Field |
|---|---|
| its first `DISK_LABEL` bytes | the disk's label |
| `DISK_ENTRY_TYPE` | `DISK_INDEX` |
| `DISK_ENTRY_FIRST` | 0, this block |
| `DISK_ENTRY_BLOCKS` | 1 |
| `DISK_ENTRY_WHERE` | the bits of a cell of the machine the disk's binary is for: 16, 32 or 64 |
| `DISK_ENTRY_LENGTH` | how many blocks the disk has |

The other slots of block 0 are entries. Every binary entry is in block
0, since block 0 is all the kernel's ROM reads of the table.

## More index blocks

An index entry names one or more blocks, and each is an index block: a
block of slots, each an entry or free. An index block may have index
entries of its own, so the table goes on through as many blocks as it
needs. Whoever reads the table follows an index entry wherever it is in
a block.

Whoever writes the table keeps a way on from a full block. The last slot
of an index block is kept free for an index entry; or, where it is not,
the entry in it is moved into the new index block and an index entry
takes its place.

A block that no entry has is free, and free blocks have no entry of
their own: taking blocks writes nothing to the table, and finding free
blocks is a walk of all of it. An entry is added where the table has a
free slot. Where it has none, free blocks are found for a new index
block, the new block is written with the new entry in it, and then the
index entry that names it is written. A disk that stops in between has
the table it had before, and a block that nothing uses.

## Where things are

Binary goes to memory, where its entry says. Memory is not nothing when a
machine is turned on, so a run is to be a whole part of the image, with
every cell the image needs to be nothing in it as well: a run of each
part of the image as `util/layout.c` knows them. What `mkdisk --boot`
makes now is a run of each stretch of blocks of the image that has
something in it, which is enough where memory starts as nothing, as the
reference VM's does.

Source and data stay on the disk, and are found by their labels.
`include display.fs` loads the blocks of the source of that label, and
block 1 is a list of such includes, which the boot loads. An application
finds its data the same way (`mandrill.tiff`, `canvas`). So what is where
on the disk is said in the index and nowhere else: `DISK_PICTURE_BLOCK`
and the others like it leave `vm_constants.h`, and `mkdisk` lays the
parts down one after another and writes where each is.

## Several disks

A machine can have several disks, each with its own block 0. How they are
reached, and which the kernel's ROM starts from, is not decided
(`doc/boot.md`).

## The boot

The kernel's ROM reads block 0 of its machine's disk, through the driver
of that disk, which is in that machine's ROM (`doc/boot.md`, Decided 2).
If entry 0 is not an index
entry, or its cell is not this machine's, the ROM goes on as if there
were no disk. Otherwise it takes every binary entry of block 0, and then
reads each one's blocks to where it says. It takes them all first, since
a run may cover the buffer block 0 was read into. The kernel then has
its dictionary and its variables, and runs the system's `boot`, which
loads block 1.

With no disk and no image in memory, the ROM says `no disk, ready to
receive`, and takes from the console what it would have read: block 0,
and then the blocks of each binary entry, entry 1's first (`doc/boot.md`,
The IPL). `mkdisk --boot` writes that beside the disk.

## Not decided

- A check of each binary run: a sum of two bytes, after the length, which
  the ROM tests, so that a damaged disk, or a transfer the IPL lost a
  byte of, is said to be so and is not run. It would be about 10
  instructions of the ROM.
- Whether a label is looked up on every disk, or on the one named.
