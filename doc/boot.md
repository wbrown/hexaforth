# Booting from the disk

How the machine is to start from its ROM, take what it runs on from a
disk, and compile the rest of the system from source on that disk. This
is a plan, and the first of it is built (What is built, below). The
disk's index, which it rests on, is `doc/disk.md`.

How big a thing is, and what it costs, is not written here: it is what
the targets `examine`, `memory-map-64` and `bench` print, and what
`mkdisk` says when it makes a disk.

## What is built

The first of it, on the reference VM, on every size of machine:

- `main` (`nuc.fs`) sets what the console and `throw` need first
  (`afresh`). Then, from a disk whose block 0 is an index for this
  machine's cell, it takes every binary entry and reads each one's blocks
  to where it says, and runs the image's `boot`.
- The ROM reads through the block interface, and the nucleus names no
  controller. A machine's ROM has the driver of that machine's disk: a
  file of its own that the build names (`ROM`, which `cross.fs` reads,
  and `include-rom` in `nuc.fs`), with one word the nucleus calls,
  `disk?`, which says whether the disk is there and gives its words to
  read and write a block. `port-disk.fs` is the ports `$b0`-`$b2` the
  reference VM has, and what a build has if it names none;
  `virt-disk.fs` is QEMU's virt, which says where that machine's virtio
  devices are and has `virtio-disk.fs`, the driver. The image is read
  with the driver's reader, kept on the stack while it is, since the
  image's data is read over the vectors; then its words are put in
  `'blk-read` and `'blk-write`, which are the kernel's, and the system
  builds on them. There is no driver to bring up after: `vmdisk.fs` and
  `virtio-blk.fs` are gone.
- A driver's memory, where it has any, is the last cells of memory,
  which are kept from what is given out at run time (`top-kept`,
  `cross.fs`): the virtio driver's queue is there. (It was the last
  kilobyte of the kernel's page, which that page's upper half has
  better use for.) No image has anything there, so no block of one is
  read over the queue while the queue is what reads it.
- Words of the nucleus that nothing in it calls, and that nothing needs
  before the image is in memory, are in the upper half of the kernel's
  page and not in the ROM (`nuc.fs`, at its end; `upper-half` in
  `cross.fs`): the doubles that reading a number does not use, `fill`
  and `cmove>`. They made room for QEMU's driver, which is longer than
  the ports'. A kernel that does not end inside its ROM is not built
  (`rom-ends`).
- `boot` loads block 1, with the image's own `load`: `load` is not in the
  ROM.
- With no disk and no image in memory, the ROM says `no disk, ready to
  receive` and takes the image from the console (The IPL, below). That
  the image is in memory is told by its first task being in a ring of
  one, before `afresh` makes it so. What the ROM says is in the ROM, as
  numbers in its code (`say"`, `emits`).
- `mkdisk --boot` makes such a disk: block 0, the source from block 1,
  and the image above the ROM as a run of each stretch of its blocks
  that has something in it. It writes what the IPL takes beside it.
- `test/boot_test.c` (`boot-64` and the others) starts a machine from the
  ROM alone and from its whole image: with that disk, with the disk's
  index gone, with no disk, and with no disk and the image sent to the
  console, with and without a block that is not an index sent first. It
  stops at a store into the ROM.

- `load` (`blocks.fs`) says where an error in a block happened, once:
  what was thrown, the block, the line, the word, and that line of the
  block. It throws it on as -1, which `throw` says nothing of, as the
  standard has ABORT. `.(` says what a block says, `thru` loads blocks
  in turn, and `include` (`files.fs`) loads a source of the disk's by
  its label in block 0 (only block 0, not the index blocks a disk may
  go on to).

- `boot` (`console.fs`) makes the screen the console as well as the
  serial line before it loads block 1 (The screen console, below), and
  after an error the prompt goes on there (`'console`).

- The system's files compiled from source at boot: every file past the
  window manager and the console, on a machine of any size.
  `forth/apps.fs` is the end of the system's list (`system.fs`), and
  `cross.fs` told `BOOT=on` makes the image without it (the target
  `boot-system-64` and the others, in `build/64/boot/`): the kernel's
  page, the page of what compiles and the page of what draws. Its disk
  has apps.fs as block 1 and each file it includes after it (`mkdisk
  --boot` follows the includes), so the boot compiles them: the TIFF
  reader and the dither, text, the Forth window, the editor, the
  decompiler, the gray picture, the viewer with its menu, the
  calculator, HexaPaint, on 64 bits QEMU's drivers, and `desk.fs`.
  What the image says of itself to the decompiler, which is compiled
  after it is made, is the image's (`image.fs`).
  The words of `cross.fs` that such a file says are the nucleus's too.
  Those that are one text for both compilers are in files of their own,
  which gforth compiles for `cross.fs` as well: `d#` and `,"`
  (`literals.fs`), `[if]` (`conditional.fs`), `/` (`divide.fs`), the
  builders of pictures and pattern rows (`pixels.fs`).  Those that each
  compiler does its own way are `meta` and `bulk` (`placement.fs`) and
  `::`, in whose body `literal` compiles a call, with `s"`
  (`macros.fs`).  The machine's sizes and flags are laid from
  `cross.fs`'s own definitions (`names.fs`; `both::`, `HOST`).
  The basewords are words of the nucleus too (`baseword-words`, which
  `basewords.fs` has from `vm_opcodes.h`). `boot_test` starts the ROM
  alone on that disk, runs the desktop the boot compiled, and leaves it:
  it is to have opened its windows.
- A file's own names (Decided, 8): a word of a file that is included is
  found by its name until the file has been read, and after that only if
  a header came before it. A header is cells, laid where `hdp` says and
  not past `hdl` (`nuc.fs`), which are cells' addresses: so while a file
  is read its own names are laid in memory had from above the display,
  a piece at a time, and `header` lays one in the dictionary, which
  stays. A name that stays is found only when its file is done, as it
  is to `cross.fs`, which has a header as a name for the prompt and no
  word: in the file a name is what it was. When the file is done its
  own are taken out of the chain and the memory given back (`files.fs`).
  `cross.fs` gives a header only to a name the source gives one, on
  every machine, so the image is 128KB again, with the headers below
  the code and what is given out starting after the display; `mkheader`
  checks the header pointer. `boot_test` holds the machine to it: a
  word with no header is not found after its file, and a header's name
  is, whatever the definition was called.
- The names that go are kept for the decompiler, where the disk has
  blocks for them (its index's entry `own-names`, which `mkdisk
  --boot-desk` makes): each file's are written there as it ends, as
  labels are in the tables of names, and `see` and the Words window show
  a word the boot compiled by them (`files.fs`, `see.fs`).
- A definition that runs off its page of code while a block is read is
  compiled again on another page, from where it began (`reading`,
  `blocks.fs`), so a block is read with next to nothing of a page held
  back; what is typed still has a kilobyte (`cpr`, `nuc.fs`). What it
  laid on the page it ran off is made zero, and the page has that room
  again.
- The machine lays a file on the page its list says, as `cross.fs` does
  (`placement.fs`): the image has how far each page's code has got
  (`pages`, `nuc.fs`), and `code-page`, `sys-page`, `big-page` and
  `small-page` in a block take code on from there. A page that is full
  sends code on after all there is, and when the first block read is
  done, code is left there, where the kernel's `newpage?` finds the page
  after free.
- What a file needs only while it is compiled is not kept
  (`placement.fs`, `files.fs`): code between `meta` and `target`, and a
  word of the file's own that only pushes a number, which is the number
  in place wherever it is said, are laid in the scratch, the end of the
  last code page, and made zero when the file has been read. The record
  kept of such a number's name has the number, and no place. The target
  `booted-64` and the like say what a boot laid beside what `cross.fs`
  lays of the same words.
- The boot looks a name up a header at a time (`sfind`, `nuc.fs`), so
  the image's headers are linked in the order of how often the system's
  source says each name, the one said most first (`headers-ordered`,
  `cross.fs`): the applications' source as well, which the image a
  machine boots from is built without (`says`, `system.fs`). A header
  made on the machine is linked before the image's, so of a name the
  newest is found. A number after `d#` or `h#` is read as a number and
  is not looked up (`numbered`, `nuc.fs`). The target `compiling-64` and
  the like say what a boot costs and where that goes, and `finding-64`
  and the like what it asks the dictionary for, and how many headers
  were looked at for each name.
- The desktop on a machine started so. `mkdisk --boot-desk` makes a disk
  a machine starts from that is the desktop's as well. The source is
  first, all of it, from block 1, which is the list the boot loads: who
  turns the pages of the disk in the editor sees source and nothing
  else until it has ended. After the blocks kept for source are the
  names, the canvas, the masks and the viewer's bitmap and gray, each
  where the one before ends (`vm_constants.h`), and the image is after
  all of those, where the index says (`build/64/boot/desk.img`). The
  picture is not on it: it is on the pictures' disk, the machine's
  second, where the viewer finds it by its label. The desktop's checks are run on the
  reference VM started with its ROM alone and that disk, on every size
  of machine (`desktop-booted-64` and the others): every check the
  desktop has, on the desktop the boot compiled. The editor's check
  reads what it is to show from block 1 of the disk it is given, which
  on such a disk is `apps.fs`.

- QEMU started so. A kernel can be built with only its ROM in it
  (`kernel(... ROM)` in `CMakeLists.txt`; `hex2bin --rom` is the first
  4KB of the image the machine boots with), and what is after it is the
  machine's memory, with nothing in it. QEMU's virt has an image of its
  own (`build/virt/`, `machine-virt`), since its ROM has its own disk's
  driver. The desktop's checks are run on QEMU started from that ROM and
  the disk, with each kernel (`qemu-hexaforth-rom-display-strict`,
  `qemu-hexaforth-fast-rom-display`), and the target `demo-boot` shows
  it.

Nothing else here is built: a disk for the Verilog, and compiling the
rest of the system from source (the display, regions, the window
manager and the files before them, which need the font builder, `keys[`
and `px,` as well).

What the machine compiles for itself that `cross.fs` does not lay is
still in its pages of code when the boot is done: the bodies of a
file's `::` words and of what it defines between `meta` and `target`,
which are run only while the file is read, and the word the kernel
lays for each of a file's own `create`s. `memory-map-16` and the like
say what the boot's code comes to beside the image's.

Not run: the virtio driver on a cell of 32 or 16 bits. It is written for
any cell, and the only machine that has the device has cells of 64 bits.
A machine address has to fit a cell (`MODEL.md`, the ports), which on 16
bits is so only of the lower half of memory.

What is in the ROM once the image is read is the ROM's only because it
was there: the image has its own `load` and `block`, and they stay there
(Blocks, below).

## Where things stood

When this was planned:

- The kernel is all of `nuc.fs`, on page 0, in the ROM's 2,048
  instructions. It has the outer interpreter and the compiler: `main`,
  `quit`, `accept`, `interpret`, `sfind`, the reading of numbers,
  `evaluate`, `catch`, `throw`, `:` and the rest.
- None of it runs from the ROM alone. It needs its RAM, which `cross.fs`
  lays down in the image: the headers; the variables, with the values
  the end of `nuc.fs` gives them (`up`, the ring of tasks, `'compile`,
  `dp`, `hdp`, `cp`); and the tables (`kcode`, without which neither a
  name nor a number can be read). With `up` nothing, `main`'s first
  store was to cell 6, in the ROM; `afresh` is what sets it now.
- `'compile` is `pcompile,`, in `join.fs` on the last page of code, not
  in the ROM.
- All three machines start with the whole image in memory: `main.c`
  loads it, the AArch64 kernels carry it, and the Verilog reads it with
  `$readmemh`. Only `boot_test` starts one from a disk.
- Each machine reaches a disk its own way: the reference VM through
  ports `$b0`-`$b2`, the Verilog bench on the console's line
  (`bench/tb-disk.fs`), and QEMU through virtio, driven by Forth
  (`virtio.fs`, `virtio-blk.fs`).
- `load` and `block` are in `blocks.fs`, on page 1, not in the kernel.
- The kernel's headers cannot be in the ROM: on every machine they are
  more than the ROM has left.

## Decided

1. The kernel's RAM is on the disk, as the binary entries of block 0
   (`doc/disk.md`), and the ROM reads it before anything else.
2. A disk is a device, not part of the machine: the reference VM, QEMU,
   the Verilog and hardware each have their own controller. A driver for
   it is Forth, reaching the controller through ports. It is a file of
   its own, and a ROM is built for a machine: a machine's ROM has the
   driver for the disk that machine has, which the build of its image
   names, and the nucleus's source names no controller. What is in a
   machine's kernel space is not all the nucleus's source. A driver
   takes no size of cell for granted. The ROM boots through the block
   interface, and the driver it has is the system's too: there is not a
   reader in the ROM and another in the image. With no disk the ROM
   takes the image from the console (4), on every machine, as a machine
   with a monitor in its ROM does.
   (This was first written, wrongly and not by the one who decides, as
   the AArch64 kernels answering the reference VM's ports with virtio in
   assembly; and then as one ROM with a driver for every controller,
   each tried in turn, which was built so in `nuc.fs`, with QEMU's
   written for cells of 64 bits.)
3. The boot image is the kernel's RAM, `join.fs`, and the screen
   console with mono.
4. With no disk, the ROM says `no disk, ready to receive` and takes
   over the console what a disk would have given it (The IPL, below).
5. An error while the boot loads source stops it, at the prompt.
6. What is printed goes to the console and to the screen.
7. Block 1 is the list of what the boot loads, made from the list
   `system.fs` has.
8. Not every word keeps a header. A name the source gives one
   (`header`) is public, and stays; a word with none is its file's own,
   and its name ends with the file. The machines of 64 and 32 bits are to
   fit in 256KB. (This was first written as every word keeping its
   header, the headers after the display and the image 256KB, and it was
   built so, with what is given out starting at byte $40000, which
   nobody decided. The headers were then more than twice what those the
   source says are.)
9. The system is one source, which `cross.fs` and the nucleus both
   compile.
10. How an application finds its data on the disk is the application's
    to say.
11. Every file of the system is laid out to fit blocks.
12. A disk is 1,024 blocks, which is what an 8-inch floppy holds, and the
    pictures are on a second one. (This was written here as "the picture
    may go on a second disk", which is not what was decided, and a disk
    with the picture on it, longer than a floppy, was built on that. It
    is built now as decided: `doc/disk.md`, Several disks.)

## The ROM

### Where it starts

All three machines start at instruction 0: `vm.c`'s context is
zeroed, `soc.v` gives the Verilog a `start_pc` of 0, and the AArch64
kernels set the PC to 0 (`hexaforth.S`). `cross.fs` lays `main` at
instruction 0 and `quit` at 1, and `throw` starts again at 1.

- Instruction 0 is the cold boot.
- Instruction 1 is the warm start. `throw` sets `'emit` to `uart-emit`
  (`nuc.fs`), so the warm start sets it to the screen console again,
  if there is one, and runs `quit`.

`vm.c` stops when it fetches an instruction that is nothing. A run of
the ROM with nothing else in memory stops at the first call into memory
that has not been read.

### The cold boot

1. Set what the console and `throw` need: the depths of the stacks,
   `up` to `main-task` and its ring to itself, `base`, `'emit` to
   `uart-emit`, `handler`, `sbase`, `state`, `blk` and `sourceid`.
2. Ask disk 0 how many blocks it has (`$b0`). With none, the IPL.
3. Read block 0. If its entry 0 is not an index entry for this
   machine's cell, go on as if there were no disk: the disks the desktop
   and the tests are given have no index yet.
4. Take every binary entry of block 0, and then read each one's blocks
   to where it says, saying its label. They are all taken first, since a
   run may cover the buffer block 0 was read into.
5. Run `boot` if the image has it, found by its name as `main` finds
   `cold` now: it starts the screen console.
6. `1 load`, within `catch`. An error is said (What it tells, below),
   and the boot stops at the prompt.
7. Run `cold` if there is one now, and then `quit`.

### Blocks

The ROM reads a block through the driver of the controller it found, into
a buffer of its own in the last block of memory, and that is all it
reads blocks for: the image. `load`, `block` and the rest stay in
`blocks.fs`, which is in the boot image. Without the image there is no
dictionary, so `load` in the ROM would be nothing a machine could use
before the image is in memory, and once it is, the image's `load` is
there. So what the ROM has to have is only what reads the image; what
is said of an error in a block, `.(`, and `include` are the image's too.
(This was first planned the other way: `load`, `block` and what is said
of an error in the ROM.)

### The IPL

With no disk, the ROM says `no disk, ready to receive` and reads from
the console what it would have read from a disk: block 0, and then the
blocks of each binary entry in order, entry 1's first. It is the same
walk as from a disk, with a block taken from the console, `DISK_BLOCK`
bytes into cells, the first byte of each the lowest, in place of the
ports. While what it takes is not an index for this machine, it says so
again and takes another block 0. `mkdisk --boot` writes what is to be
sent, beside the disk it makes.

The console's port gives each byte as it is (`vm.c`, `hexaforth.S`).
What can change a byte is the sender's side, such as
a terminal that turns line ends round. A serial line can lose a byte, so
a check of the binary runs (Not decided) is how the IPL would say that a
transfer was bad. After the IPL the machine has its dictionary and the
prompt, and no disk.

### What it tells

- Before the image is read there is no font, so what is said then (`no
  disk, ready to receive`, a disk for another machine) goes to the
  console alone.
- A file has a line: its label, how many blocks it is, and a dot as
  each of its blocks starts (`include`, `files.fs`). Built. A file read
  from within another has a line of its own, so the outer one's line is
  ended first and the dots of its later blocks are on lines of their
  own; a line is ended only if it has anything on it. The screen
  console asks for a key after every character, which is when QEMU's
  bridge copies the screen, so a line is seen as it grows. `cp` and
  `here` when a file is done are not said.
- While the ROM reads the image nothing is shown: the font and what
  draws text are in the image, and come with all of it.
- An error is said where it happened, before `load` puts `blk` back:
  the code, the block, the line (`>in` over 64), the word (the last
  `parse-name`, which `reading` keeps), and that line of the block. It
  is said once: the `load`s it passes on through do not say it again.
- `.(` is in the ROM, for block 1 and the files to say what they are
  doing.

### Room

What the ROM has left on each machine is what `examine` says of its
image. Where it is not enough, these are the words nothing in the boot
uses (`disasm_hex-64` says how long each is):

| What | Words |
|---|---|
| Doubles that reading a number does not use | `dabs` `2rot` `d>s` `d=` `d<` `du<` `d-` `d0<` `d0=` `d2/` |
| Division and printing in decimal | `divstep` `um/mod` `u/mod` `(.)` `.` |
| Typed lines | `accept` `delchar` `3rd` `3dup` |
| Showing names | `code>` `name>` `words` |
| Tasks | `spawn` `(task)` `idle` `pause` |
| Characters a byte at a time | `cmove` `cmove>` `fill` |
| The rest | `lo32` `hi32` `lo16` `hi16` `2over` `abs` `max` `depth` `w,` |

They can go in the upper half of page 0, which a call reaches from any
page in one instruction, as it reaches the ROM. The doubles, `fill` and
`cmove>` are there now (What is built). Of the rest, three things in
the ROM call into them and would first have to stop: `throw` prints
with `.`, `ckey` calls `pause`, and the prompt, which is all that works
without a disk, uses `accept`.

## The boot image

- `nuc.fs` built with a list of its own, named by `SYSTEM` as
  `picture.fs` is: the kernel, `join.fs` (as the machine with no display
  has it now), and the screen console with mono.
- `cross.fs` writes it. `mkdisk` makes its binary entries: a run for
  each part of it as `util/layout.h` knows them (the data and the
  headers, the pages of code, the upper half of page 0, and on 16 bits
  the data above the display), since memory is not nothing when a
  machine is turned on.

## The screen console

Built: `console.fs`. While the boot image is the whole system, the
console is made of `display.fs`'s words and `region.fs`'s, so nothing
outside `display.fs` knows how the display is laid out:

- `console` makes the screen white with `pat-rect` and sets `'emit` and
  `'console` to `con-emit`, which sends a character to the serial line
  and draws it with `draw-char`, in the port's font, a character as
  wide as the font says;
- CR goes to the start of the line, LF to the next, back-space back over
  a blank, and the screen goes up a line (`scroll-rect`) when the last
  line is full;
- `key?` is read at the end of a line, since QEMU's bridge copies the
  bitmap only when the key's port is read (`hexaforth.S`);
- `boot` starts the console before it loads block 1, and the warm start
  is `throw` setting `'emit` from `'console` (nuc.fs), so that after an
  error the prompt is drawn on the screen as well.

When the boot image is the kernel and little else, the console it has
is to be the first part of `display.fs`, in a file of its own: `fb-w`,
`fb-h`, `row`, `ink`, `paper`, the bitmap made paper, and a glyph of mono
at a character's place. Mono is 8 by 16, so a row of a glyph lands in one
cell of a row of the display: from the strike on 32 and 64 bits, and from
the font opened out on 16. The rest of `display.fs` is then compiled
from source, and is built on these.

## Disks and their controllers

- A disk is reached through its controller's ports by the driver in its
  machine's ROM, and through the block interface by everything else
  (Decided, 2). The ports `$b0`-`$b2` are one controller, the reference
  VM's (`port-disk.fs`); QEMU's is virtio (`virt-disk.fs`,
  `virtio-disk.fs`); the Verilog's bench has its disk on the console's
  line now (`bench/tb-disk.fs`).
- A disk is 1,024 blocks and the machine has two: the pictures are on
  the second (Decided, 12). Each disk has its own block 0, and the ROM
  starts from disk 0 (`doc/disk.md`, Several disks, which says how a
  block of another disk is said and how each machine reaches it).

## Headers

A header is of a name the source gives one, and they are where they
were, below the code (`build/64/nuc.sizes` and the others say what they
take; the room is what `cross.fs` gives them, `hdr-base`).

- Of those, the headers of `names.fs`, with the basewords and sizes it
  lays, and of `placement.fs` are there only for the nucleus to compile the
  system's source with.
- `mkheader` checks the header pointer against the cell headers must
  stop before (`hdp` and `hdl`, `nuc.fs`): where the code starts, for
  the dictionary's own, and the end of the piece of memory a file's own
  names are in while it is read.
- Looking a name up goes through every header, so fewer headers is a
  shorter boot: how many instructions a machine runs from its start to
  its prompt is in `build/64/map/boot.out`, which `memory-map-64` makes.
  With a header for every word the machine of 64 bits ran nearly twice
  as many.
- A file's own names are not in the tables of names `cross.fs` makes:
  the machine keeps them itself, on a disk that has blocks for them
  (What is built), and the decompiler shows a word the boot compiled by
  them. On a disk that has none it shows such a word as a number.
- Nothing needs a header to be in any one place: a link and an execution
  token are cells, and `sfind` follows them with `@`. Two things in the
  tree take the headers to be where they are now: the decompiler takes a
  number between `VARS` and the pages of code to be data or a header
  (`see.fs`), and the tools read headers out of the image's 128KB.

## One source

What the system's source says that only `cross.fs` knows:

- Words for numbers and text: `d#`, `h#`, `c#`, `[char]`, `char`, `,"`,
  `s"`, `abort"`, `/`.
- Words for how a file is laid down: `header`, `meta`, `target`, `::`,
  `[if]`, `[else]`, `[then]`, `bulk`, `unbulk`, `high`, `unhigh`,
  `in-place`.
- The machine's sizes and the disk's layout as names (`CELL`, `PXMASK`
  and the rest, many of them used outside the kernel), and the values
  the files' `::` words reckon with (`cell-bits`, `lanes`, `spaces-cell`
  and the like). `cross.fs` refuses a word that has a macro's name, so
  these cannot yet be words.
- What `cross.fs` runs as gforth: the `meta` sections and the bodies of
  `::` words; the builders of fonts, pictures, keys and patterns
  (`cross.fs`); `hdr,` and `at:`; and an instruction said by its fields
  (`display.fs`).
- `literal` in the body of a `::` word compiles when the macro runs in
  `cross.fs`, and when it is defined in the nucleus.

## Source in blocks

- Many of the system's files had a line longer than 64 characters, or a
  run of more than 16 lines with no line of nothing in it (`mkdisk
  --pack`). Done so far, with each machine's image the same, byte for
  byte, after each:
  - `wrap` covers every file of `forth/` that goes on a disk (CMake's
    `DISK_SOURCES`), and has laid out all of them;
  - a table is cut between two of its entries where it is longer than a
    block (`doc/formatting.md`), which `wrap` now does; `spin.fs`'s
    pictures are pieces side by side that fit a line;
  - a conditional (`[if]` ... `[then]`) longer than a block is made two,
    since skipping one does not go on into the next block loaded; and
    one is lines with none of nothing between them, so that it is laid
    in one block. `mkdisk` says so of one that is not, since `desk.fs`
    ended in one of three runs that the boot of 32 and 16 bits skipped
    the first of and then compiled the rest. It says it now of files
    that no boot compiles yet: `display.fs`, `tiff.fs` and
    `atkinson.fs`;
  - `fused.c` and `joins.c` write `fused.fs` and `folds.fs` to fit;
  - `s" ..." evaluate` too long for a line is two of them.

  Left: the definitions longer than a block, which are factored by
  hand, and the lines in them that cannot be cut until they are. The
  cold ones are done (`desk.fs`, `viewdesk.fs`, `virtio-input.fs`,
  `join.fs`, `tiff-open` and `selem`, and those of `wm.fs`). The hot
  ones are factored into words that are right when called, measured
  with `bench-draw`, `bench-dither` and `bench-gray` before and after,
  and given `in-place` only where a call shows in the figures: those of
  `region.fs`, `display.fs` and `atkinson.fs`, and `tiff.fs`'s
  `gray-lanes`, `gray-pairs` and `pixels`.

  Some of the desktop's checks showed a system word longer than its
  window (`tiff-open`, `scroll-rect`), and once every definition fits a
  block no system word is sure to be: those checks are to type a long
  word of their own.
- How many blocks the source takes is what `mkdisk` says when it lays a
  disk, and it makes no disk longer than a floppy.
- `DISK_NAMES_BLOCK` and the others like it leave `vm_constants.h`.
  `mkdisk` lays the parts down one after another and writes the index.
  The picture's block has left it (`doc/disk.md`, Several disks).

## Order of work, and the check for each

1. The cold boot, to `no disk, ready to receive`. The reference VM
   given the ROM alone, built with `STRICT` and `ROM_BYTES`, says it and
   waits, on every size of machine. Built: `boot_test` makes the memory
   above the ROM nothing itself, in place of a tool that cuts an image.
2. Block 0, the boot image, and the ROM reading it, to the prompt.
   `prompt_test`'s checks, typed at a machine started so. Built, with the
   whole image for the boot image, and checked by `boot_test` (What is
   built); `prompt_test`'s checks are not yet typed at it.
3. What is said of an error in a block, `.(`, the warm start, and
   `include` by label, in the image (Blocks, above). A disk with known
   source and errors put in it: what is said, word for word. Built and
   checked by `boot_test`.
4. The screen console. The screen in the memory `vm.c` writes when it
   stops (`util/screen.c`) has the glyphs of what was printed. Built:
   `boot_test` reads the screen out of the machine's memory and compares
   what block 1 said, and an error, with mono's glyphs.
5. The IPL. The reference VM, given no disk and the blocks on its
   console, comes to the prompt. Built, and checked by `boot_test`.
6. The ROM booting through the block interface, with the driver of its
   machine's disk. Built for the reference VM and for QEMU, and the
   desktop's checks run on QEMU started so; the Verilog's disk, and the
   same checks on `c64-lat2-krom`, are not.
7. A file's own names, which end with it, and `mkheader`'s check of the
   header pointer. Built, and checked by `boot_test`.
8. Every file laid out for blocks, and the disk laid out by its index.
9. One source: the words, the names and the builders.
10. The system compiled from block 1, compared with what `cross.fs`
    makes of it, and the desktop's checks run on it. Built for what the
    boot compiles now, the applications and `desk.fs`: the desktop's
    checks are run on it (`desktop-booted-64`). What the nucleus makes of
    a file is not yet compared with what `cross.fs` makes of it.

## To do

For later: the editor on a machine of several disks. It opens on block
1, which is disk 0's, and has no way to be told another disk
(`doc/disk.md`, Several disks).

Next, now that QEMU starts from its disk:

- Text on 16 bits. With a font laid as lanes, `bench-draw` has text a
  little slower on 16 bits than it was: the glyph's lane and a multiply
  are where two fetches were. On that machine a lane is a cell, so the lane for a glyph
  can be the cell in front of its rows, and what is noted of a glyph its
  address: no multiply and no second fetch when it is drawn.
- The desktop's profile after the fonts. It was taken before
  (`build/paths-before.txt`) and not after, so what the text that goes
  straight to the screen costs now is not measured; `bench-draw` is of
  the text that is gathered.
- Data laid at run time is checked against where it must stop only
  before a definition (`mkheader`): a table laid after `create` can run
  into the dictionary and nothing says so until the next definition. The
  fonts found it, when they were more data than the boot had room for
  on 64 bits.

- The virtio driver of the keyboard and the pointer, `virtio.fs` and
  `virtio-input.fs`, is still written for cells of 64 bits and included
  only where they are (`apps.fs`), which is cell width standing for
  which machine it is: the same faults the disk's driver had. It also
  has what `virtio-blk.fs` had and the disk's driver now has not: one
  phrase said three times for a queue's three addresses, the status read
  back to set bits whose values are known, a count of slots where a loop
  over their addresses keeps none.
- What QEMU's disk is not told, and works without, as run on QEMU with
  each kernel and the desktop's checks: features 0-31 as nothing, the
  high halves of the queue's addresses, which queue is meant (it has
  one), and that its interrupt was seen. The driver's status writes are
  the four the device's protocol has.
- The ROM of QEMU's virt is all but full (`examine`, of
  `build/virt/boot/nuc.hex`).
- `kernel_test` is built for the default machine alone. What comes to a
  word was asked of the other sizes, before words were moved out of the
  ROM, only by building them: a call from the kernel to a word laid
  after it does not compile.
- The memory layout, which `memory-map-64` and the others show: where
  everything is, how far the heap reaches while the desktop's checks
  are run, what it is given out to, and how much of each piece is ever
  stored to. What is given out starts after the display again, and the
  desktop is in the first 256KB on 64 and 32 bits, and in the machine's
  128KB on 16:
  - everything the heap holds is counted in cells, and is four times as
    many bytes on 64 bits as on 16. Most of it is regions, of each of
    which little is ever stored to, and the Words window's buffers;
  - the fonts are not among them. They were: opened out the first time
    each was used, a cell for every row of every glyph. A font is now
    laid down by `cross.fs` as its rows in lanes of a cell, as many
    bytes as it has pixels to the nearest lane, and drawn from that
    (`cross.fs`, `display.fs`), and text is quicker for it on 64 and 32
    bits (`bench-draw`). On 16 bits, where a cell is one lane, a font
    takes what it took and text is a little slower;
  - the picture is not among them. Its bitmap was: all of it in memory
    while it was dithered, given back when it had been written to the
    disk, and then a hole under what was given out meanwhile; the menu
    took it again to draw into; and the picture kept as gray was as
    much again, above them all. Now the dither makes a block of the
    bitmap at a time and writes each, the gray is made and kept on the
    disk the same way, and the window is painted from the blocks its
    clip asks for (`view.fs`). That is what brought the heap's reach
    down to where it is;
  - the boot's own heap, at its prompt with the applications compiled,
    is a font opened out and the blocks' buffer: what `picture[` keeps
    the rows of a picture in is given back when the picture is laid;
  - below the display the slots are fixed, each by a constant in
    `cross.fs` for the size of machine, and part empty: after the ROM,
    after the data, where the headers are again, and in the code pages
    kept for definitions made at run time, of which the desktop uses
    little;
  - `hfree` keeps one run of what is given back, the biggest, and what
    else is given back is not had again.

  The display is right where it is: just past the first 65,536
  instructions, where code can be, and on 16 bits past the first 32K
  cells, where code and what is reached by the address of a byte can be.
- The headers, when those that stay are more than their room below the
  code: past it, stretches taken from the heap. When the room is full,
  `mkheader`
  takes a stretch and goes on in it, the limit being where that stretch
  ends, and no longer a number compiled into the kernel (`HDR-LIMIT`).
  Nothing needs a header to be in any one place: a link and an
  execution token are cells, which `sfind` follows. A stretch may be
  written to the disk and its memory given back, and read again when a
  name is looked up or shown, as the decompiler reads the names of words
  without headers (`nm-buf`); compiled code never reads a header. What
  that has to deal with:
  - a link from one stretch to another is to a place that is not there
    while it is on the disk: such a link says which stretch and how far
    into it, or a stretch is read into a buffer of its own;
  - a name that is not found reads every stretch on the disk, and every
    number is looked up as a name first, of which compiling the system
    at boot has many: what that costs is to be measured, and an index of
    the names, which says when one is not there, is the usual answer;
  - a machine with no disk keeps its headers in memory;
  - a stretch is a run of blocks with an entry in the index
    (`doc/disk.md`);
  - what gives memory back by putting `himem` where it was (`with-room`,
    the tables of names, the picture's memory) is to allow for a stretch
    taken while it had the memory.

## Not decided

- A check of each binary run (`doc/disk.md`).
- The number of the port that says which disk. (A label is looked up on
  every disk, disk 0 first: `doc/disk.md`, Several disks.)
- Whether the screen console scrolls or wraps.
- Smaller headers, later.
