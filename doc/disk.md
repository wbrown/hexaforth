# The disk

A disk is a sequence of blocks of `DISK_BLOCK` bytes (`vm_constants.h`). Block 0 is the disk's index, which records what is on the disk, where each thing is, and what type it is. Everything else on the disk is found through the index.

The index's constants are in `vm_constants.h`, from `DISK_ENTRY` on, where both `mkdisk` and the Forth get them, and this file explains what they are for.

What is built so far:

- `mkdisk --boot` writes an index of one block, and the kernel's ROM reads it when the machine starts (`doc/boot.md`).
- A source, a picture, and the blocks reserved for the names of the words a boot compiles are found by their labels (`files.fs`; Several disks, below).

Index blocks after block 0 are still to be built, and the rest of the desktop's data is still at the block numbers `vm_constants.h` defines.

## Entries

The index is a table of entries. An entry describes a run of blocks: where the run starts, how many blocks it has, what type of thing it holds, and a label to find it by.

An entry is `DISK_ENTRY` bytes, and a block holds `DISK_BLOCK` divided by `DISK_ENTRY` entries. A number in an entry is two bytes or four, lowest byte first, at an even byte, so the layout is the same at every cell size. An entry starts at the start of a cell on every machine, and so does its label, which is compared a cell at a time.

| Where in the entry | Field |
|---|---|
| its first `DISK_LABEL` bytes | the label |
| `DISK_ENTRY_TYPE` | the type |
| `DISK_ENTRY_FIRST` | the first block |
| `DISK_ENTRY_BLOCKS` | the number of blocks |
| `DISK_ENTRY_WHERE` | where it goes in memory, for binary; 0 for the other types |
| `DISK_ENTRY_LENGTH`, four bytes | its length |
| the rest | unused, and 0 |

The label is a count followed by as many characters as the rest of its `DISK_LABEL` bytes hold, with zeros after them. `count type` shows it, and two labels are equal when their cells are equal. The longest file name in the system, `virtio-input.fs`, fills a label. A label is whatever its writer gives it, `display.fs` or `display`, because the type is what records what an entry holds.

A block number is two bytes, so a disk has at most 65,536 blocks.

## Types

| Type | What the blocks hold | Its length is in |
|---|---|---|
| `DISK_FREE` | nothing: the slot is free | |
| `DISK_BINARY` | binary, which is memory, read into place when the machine starts (The boot, below) | bytes |
| `DISK_SOURCE` | source, which is Forth in the form `load` reads and the editor shows: lines of `DISK_LINE` characters, one block's worth to a block | lines |
| `DISK_DATA` | data, which belongs to an application that defines what its bytes mean: a TIFF, the names that go with the image, HexaPaint's canvas, the threshold masks, the viewer's bitmap | bytes |
| `DISK_INDEX` | index, which is more of the table: its blocks are index blocks | |

`DISK_ENTRY_WHERE` of a binary entry gives where the memory goes, counted in blocks from cell 0. The cell is that number times the number of cells in a block (`BLKCELLS`), so a run starts at the start of a block of memory, and the two bytes of the field reach 64MB of memory.

## Block 0

Entry 0 of block 0 describes the disk itself:

| Where in the entry | Field |
|---|---|
| its first `DISK_LABEL` bytes | the disk's label |
| `DISK_ENTRY_TYPE` | `DISK_INDEX` |
| `DISK_ENTRY_FIRST` | 0, this block |
| `DISK_ENTRY_BLOCKS` | 1 |
| `DISK_ENTRY_WHERE` | the cell width, in bits, of the machine the disk's binary is for: 16, 32 or 64 |
| `DISK_ENTRY_LENGTH` | the number of blocks on the disk |

The other slots of block 0 are entries. Every binary entry is in block 0, because block 0 is the only part of the table the kernel's ROM reads.

## More index blocks

An index entry names one or more blocks, and each of them is an index block: a block of slots, each holding an entry or free. An index block can have index entries of its own, so the table continues through as many blocks as it needs. A reader of the table follows an index entry wherever it is in a block.

A writer of the table makes sure a full block can always be extended. Either the last slot of an index block is kept free for an index entry, or the entry in that slot is moved into the new index block and an index entry takes its place.

A block is free when it lies outside every entry, and the table lists only the blocks in use, so allocating blocks leaves the table as it is, and finding free blocks means walking the whole table. A new entry goes in a free slot of the table. If every slot is taken, the writer finds free blocks for a new index block, writes the new block with the new entry in it, and then writes the index entry that names it. A disk that stops between those two writes has the table it had before, and one orphaned block.

## Where things are

Binary goes to memory, at the address its entry gives. Memory holds arbitrary values when a machine is turned on, so a run should be a whole part of the image, including every cell the image needs to be zero: one run for each part of the image as `util/layout.h` defines the parts. What `mkdisk --boot` makes now is one run for each stretch of the image's blocks that has non-zero content, which is enough where memory starts as zero, as the reference VM's does.

Source and data stay on the disk, and are found by their labels. `include display.fs` loads the blocks of the source with that label. Block 1 is a list of such includes, and the boot loads it. An application finds its data the same way (`mandrill.tiff`, `canvas`), so the index is the only place that records where things are on the disk. In this design, `DISK_NAMES_BLOCK` and the constants like it are removed from `vm_constants.h`, and `mkdisk` writes the parts one after another and records where each is. The picture's block constant has been removed already, and the picture is found by its label (Several disks).

## Several disks

A machine can have several disks, each with its own block 0. When the machine starts, it asks each device for an index block. A label is searched for on disk 0 first, then on disk 1, disk 2 and so on, and a label found on disk 0 is the one used, whatever a later disk has under the same label. With several disks to search, a disk is 1,024 blocks, the capacity of an 8-inch floppy, and the pictures are on a second disk (`doc/boot.md`, Decided 12), found by their labels as everything else is.

The kernel's ROM reads the binary of disk 0 only, because the image is on the disk the machine starts from.

A block's number also selects the disk. The number is 16 bits:

| Bits | Field |
|---|---|
| the top four | the disk |
| the other twelve | the block on that disk |

A number that fits a 16-bit cell can therefore name any block of any disk, there are at most sixteen disks, and a number reaches at most 4,096 blocks of a disk, of which a floppy has 1,024. A lookup of a label returns such a number, made of the disk the label was found on and the first block there. Everything that takes a block's number (`block`, `load`, the buffer, `blk`) works as before, since a plain block number means disk 0, and only the code that finds a label and the driver that reads a block know that a number includes a disk. An entry in an index refers to a block of its own disk.

What is built of this:

- The reference VM has as many disks as it is given files (`vm.c`; `main.c` takes them separated by commas), and its controller's block register takes the number with the disk in it.
- When a machine boots, `find-disks` (`files.fs`) asks each disk for its first block and records which disks have an index. `include` and `labelled` search those disks in order, and return a first block whose number includes its disk.
- `mkdisk --disk` makes a disk that is an index and its files.
- `boot_test` gives a machine two disks and checks all of that, at every cell size.

The pictures are on the second disk. A disk has at most 1,024 blocks (`DISK_FLOPPY`), and `mkdisk` refuses to make a larger one. The viewer asks for its picture by its label (`picture-blk`, `view.fs`) and gets it from whichever disk has it. The disk a machine starts from holds the machine's source, the names, the canvas, the masks, the viewer's bitmap and gray, and the image, and leaves the pictures to the second disk. Every machine that shows or checks the desktop is given both disks (`pictures`, `cmake/disks.cmake`).

A disk that a machine boots from, and that is also its desktop's disk, has blocks reserved for the names of the words its boot compiles. Those names are each file's own, and they leave the dictionary when the file has been read. The blocks are a data entry, `own-names`, which `mkdisk --boot-desk` makes and `files.fs` finds by that label. The machine writes each file's names there as the file ends, and the decompiler looks there for a word that the image's tables of names lack (`see.fs`). On a disk that lacks the entry, the names are discarded.

On QEMU each disk is a device of its own, and the ROM's driver drives one device. To read a block of another disk, the driver is pointed at the other device, after it has stopped the device it leaves (`virt-disk.fs`, in the upper half of the kernel's page). That machine's word `disks`, which `find-disks` runs first, stores the words that do this in place of the ROM's two words.

The menu bar has a light for each disk present (`wm.fs`), and the arrow for a read or a write appears beside the light of the disk the block is on.

## The boot

The kernel's ROM reads block 0 of its machine's disk, through that disk's driver, which is in that machine's ROM (`doc/boot.md`, Decided 2). The ROM accepts the disk when entry 0 is an index entry whose cell width is this machine's, and treats any other disk as absent. From an accepted disk it collects every binary entry of block 0, and then reads each entry's blocks to the address the entry gives. It collects them all first because a run may cover the buffer block 0 was read into. The kernel then has its dictionary and its variables, and it runs the system's `boot`, which loads block 1.

With no disk and no image in memory, the ROM prints `no disk, ready to receive`, and takes from the console what it would have read: block 0, and then the blocks of each binary entry, starting with entry 1's (`doc/boot.md`, The IPL). `mkdisk --boot` writes that data to a file beside the disk.

## To be decided

- A check of each binary run: a two-byte sum after the length, which the ROM tests. A damaged disk, or a transfer in which the IPL lost a byte, is then reported in place of being run. It would take about 10 instructions of the ROM.
- With several disks: how the editor shows which disk it is on, which is for later (`doc/boot.md`, To do); and how a driver tells its controller which of the controller's disks is meant.
