# Booting from the disk

How the machine is to start from its ROM, take what it runs on from a
disk, and compile the rest of the system from source on that disk. This
is a plan, and the first of it is built (What is built, below). The
disk's index, which it rests on, is `doc/disk.md`.

The figures here are what the target `examine` and `mkdisk --pack` said
when this was written. An estimate says that it is one.

## What is built

The first of it, on the reference VM, on every size of machine:

- `main` (`nuc.fs`) sets what the console and `throw` need first
  (`afresh`). Then, from a disk whose block 0 is an index for this
  machine's cell, it takes every binary entry and reads each one's blocks
  to where it says, and runs the image's `boot`.
- `boot` (`vmdisk.fs`) makes the disk this VM's and loads block 1, with
  the image's own `load`: `load` is not in the ROM yet.
- The words for the reference VM's disk's ports, `disk-blocks`,
  `vd-read` and `vd-write`, are the kernel's, and `vmdisk.fs` makes them
  the driver.
- `mkdisk --boot` makes such a disk: block 0, the source from block 1,
  and the image above the ROM as a run of each stretch of its blocks
  that has something in it.
- `test/boot_test.c` (`boot-64` and the others) starts a machine from the
  ROM alone and from its whole image, with that disk, with the disk's
  index gone, and with no disk, and stops at a store into the ROM.

Nothing else here is built: the IPL, `load` and `block` in the ROM, what
is said of an error, `.(`, the warm start, the screen console, the disk's
ports on the other machines, several disks, the headers' new place, and
compiling the system from source.

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
- The kernel's headers cannot be in the ROM: they are 4,872 bytes on 64
  bits, 2,572 on 32 and 1,550 on 16, where the ROM has 718, 720 and 770
  left.

## Decided

1. The kernel's RAM is on the disk, as the binary entries of block 0
   (`doc/disk.md`), and the ROM reads it before anything else.
2. Every machine has the disk's ports `$b0`-`$b2`, and can have several
   disks. The AArch64 kernels answer the ports with virtio-blk in
   assembly, the Verilog bench answers them, and MODEL.md says so. The
   bench's disk on the console's line goes.
3. The boot image is the kernel's RAM, `join.fs`, and the screen
   console with mono.
4. With no disk, the ROM says `no disk, ready to receive` and takes
   over the console what a disk would have given it (The IPL, below).
5. An error while the boot loads source stops it, at the prompt.
6. What is printed goes to the console and to the screen.
7. Block 1 is the list of what the boot loads, made from the list
   `system.fs` has.
8. Every word keeps its header. On 64 and 32 bits the headers go in the
   upper 128KB of the first 256KB. Making a header smaller is for later.
9. The system is one source, which `cross.fs` and the nucleus both
   compile.
10. How an application finds its data on the disk is the application's
    to say.
11. Every file of the system is laid out to fit blocks.
12. The source takes about 650 blocks, and the picture may go on a
    second disk.

## The ROM

### Where it starts

All three machines start at instruction 0: `vm.c`'s context is
zeroed, `soc.v` gives the Verilog a `start_pc` of 0, and the AArch64
kernels set the PC to 0 (`hexaforth.S:124`). `cross.fs` lays `main` at
instruction 0 and `quit` at 1, and `throw` starts again at 1.

- Instruction 0 is the cold boot.
- Instruction 1 is the warm start. `throw` sets `'emit` to `uart-emit`
  (`nuc.fs:939`), so the warm start sets it to the screen console again,
  if there is one, and runs `quit`.

`vm.c` stops when it fetches an instruction that is nothing
(`vm.c:360`). A run of the ROM with nothing else in memory stops at the
first call into memory that has not been read.

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

Reading only: `blk-read` as the two stores to the ports, one buffer at
a place of its own, `block` and `cblock`, and `load` and `reading` as
`blocks.fs` has them. On 16 bits the buffer is in the lower half of
memory. Writing, `flush`, `update`, the disk light and the buffer from
`halloc` stay in `blocks.fs`, which is compiled later.

### The IPL

With no disk, the ROM says `no disk, ready to receive` and reads from
the console what it would have read from a disk: block 0, and then the
blocks of each binary entry in order. It is the same walk as from a
disk, with a block taken from the console, 1,024 bytes, in place of the
ports.

The console's port gives each byte as it is (`vm.c:241`,
`hexaforth.S:262`). What can change a byte is the sender's side, such as
a terminal that turns line ends round. A serial line can lose a byte, so
a check of the binary runs (Not decided) is how the IPL would say that a
transfer was bad. After the IPL the machine has its dictionary and the
prompt, and no disk.

### What it tells

- Before the image is read there is no font, so what is said then (`no
  disk, ready to receive`, a disk for another machine) goes to the
  console alone.
- As each file starts: its name and its blocks. When it is done: `cp`
  and `here`.
- An error is said where it happened, before `load` puts `blk` back:
  the code, the block, the line (`>in` over 64), the word (the last
  `parse-name`, which `reading` keeps), and that line of the block. It
  is said once: the `load`s it passes on through do not say it again.
- `.(` is in the ROM, for block 1 and the files to say what they are
  doing.

### Room

Estimates: the cold boot about 55 instructions, blocks about 55, `.(`
and what is said of an error about 25, and the IPL's taking a block
from the console about 15. That is about 300 bytes, which fits in what
the ROM has left on every machine without moving anything out of it.

If it does not, these are the words nothing in the boot uses, with how
many instructions each group is on 64 bits (`disasm_hex-64`):

| What | Words | Instructions |
|---|---|---|
| Doubles that reading a number does not use | `dabs` `2rot` `d>s` `d=` `d<` `du<` `d-` `d0<` `d0=` `d2/` | 47 |
| Division and printing in decimal | `divstep` `um/mod` `u/mod` `(.)` `.` | ~58 |
| Typed lines | `accept` `delchar` `3rd` `3dup` | ~71 |
| Showing names | `code>` `name>` `words` | 70 |
| Tasks | `spawn` `(task)` `idle` `pause` | 85 |
| Characters a byte at a time | `cmove` `cmove>` `fill` | 42 |
| The rest | `lo32` `hi32` `lo16` `hi16` `2over` `abs` `max` `depth` `w,` | 36 |

They could go in the upper half of page 0, which a call reaches from any
page in one instruction, as it reaches the ROM. Three things in the ROM
call into them and would first have to stop: `throw` prints with `.`,
`ckey` calls `pause`, and the prompt, which is all that works without a
disk, uses `accept`.

## The boot image

- `nuc.fs` built with a list of its own, named by `SYSTEM` as
  `picture.fs` is: the kernel, `join.fs` (as the machine with no display
  has it now), and the screen console with mono.
- `cross.fs` writes it. `mkdisk` makes its binary entries: a run for
  each part of it as `util/layout.c` knows them (the data and the
  headers, the pages of code, the upper half of page 0, and on 16 bits
  the data above the display), since memory is not nothing when a
  machine is turned on.

## The screen console

- It is in the boot image, in the upper half of page 0, and not in the
  ROM: it needs the font, which is in RAM in any case.
- It is the first part of `display.fs`, in a file of its own, since only
  `display.fs` knows how the display is laid out: `fb-w`, `fb-h`, `row`,
  `ink`, `paper`, and
  - the bitmap made paper;
  - a glyph of mono at a character's place. Mono is 8 by 16, so a row of
    a glyph lands in one cell of a row of the display: from the strike
    on 32 and 64 bits, and from the font opened out on 16;
  - a column and a line, CR and LF, and scrolling or wrapping (Not
    decided);
  - an emit that sends to the console and draws, which `'emit` is;
  - `key?` read at the end of a line, since QEMU's bridge copies the
    bitmap only when the key's port is read (`hexaforth.S:255-269`).
- The rest of `display.fs` is compiled from source, and is built on
  these.
- About 50 to 70 instructions (estimate).

## Disks and their ports

- `$b0`-`$b2` on every machine, and a port that says which disk the
  three act on. `$b0` gives that disk's size.
- Each disk has its own block 0. The ROM starts from disk 0.
- MODEL.md, `vm.c`, both AArch64 kernels and the Verilog change
  together.

## Headers

Every word keeps its header. Every word with a header does not fit
where the headers are now, by what a header takes now (the last column
is an estimate: every word, by the bytes a header is now):

| Cell | Room | Now | Every word |
|---|---|---|---|
| 64 | 24,576 bytes, from `$a000` | 10,712 (444 headers) | ~27,400 (1,134 words) |
| 32 | 16,384 bytes, from `$c000` | 5,940 (427 headers) | ~15,400 (1,106 words) |
| 16 | 8,192 bytes, from `$6000` | 3,774 (426 headers) | ~9,700 (1,099 words) |

- `mkheader` checks the data pointer and not the header pointer
  (`nuc.fs:1026`), so headers that run out of room run into the code
  after them. It is to check the header pointer as well.
- On 64 and 32 bits the headers go in the upper 128KB of the first
  256KB. The display is at the start of it (byte `$20000` on both, about
  55KB long), and on 64 bits the heap starts at cell `$6000`, byte
  `$30000`. So the headers go after the display and the heap after
  them, or the display moves (Not decided).
- Nothing needs a header to be in any one place: a link and an execution
  token are cells, and `sfind` follows them with `@`. Two things in the
  tree take the headers to be where they are now: the decompiler takes a
  number between `VARS` and the pages of code to be data or a header
  (`see.fs:345-350`), and the tools read headers out of the image's
  128KB.
- On 16 bits (Not decided): a header is used to find a name (the
  interpreter and the compiler, `'`, `[']`, `postpone`, and `main`
  finding `cold`) and to show one (`words`, `see.fs`). Compiled code
  never reads one. So the headers could be on the disk, and read in
  while something is compiled or typed, as the decompiler reads the
  names of words without headers now; a word made at the prompt then
  writes its header back. Or they go above the display, where only what
  is read a cell at a time goes.

## One source

What the system's source says that only `cross.fs` knows:

- Words for numbers and text: `d#`, `h#`, `c#`, `[char]`, `char`, `,"`,
  `s"`, `abort"`, `/`.
- Words for how a file is laid down: `header`, `meta`, `target`, `::`,
  `[if]`, `[else]`, `[then]`, `bulk`, `unbulk`, `high`, `unhigh`,
  `in-place`.
- The machine's sizes and the disk's layout as names (`CELL`, `PXMASK`
  and the rest, 49 of which are used outside the kernel), and the values
  the files' `::` words reckon with (`cell-bits`, `lanes`, `spaces-cell`
  and the like). `cross.fs` refuses a word that has a macro's name, so
  these cannot yet be words.
- What `cross.fs` runs as gforth: the `meta` sections and the bodies of
  `::` words; the builders of fonts, pictures, keys and patterns
  (`cross.fs`, lines 1202-1445); `hdr,` and `at:`; and an instruction
  said by its fields (`display.fs:740`).
- `literal` in the body of a `::` word compiles when the macro runs in
  `cross.fs`, and when it is defined in the nucleus.

Estimates: the two groups of words about 100 instructions, the names
about 130, and the builders about 250, by the kernel's one instruction
to a line of its source.

## Source in blocks

- 23 of the system's files have a line longer than 64 characters, or a
  run of more than 16 lines with no line of nothing in it (`mkdisk
  --pack`). `wrap` is to cover them, and the generators of `fused.fs`
  and `folds.fs` are to write what fits.
- The applications that fit take 250 blocks for 3,421 lines. The whole
  system is 8,972 lines, so about 650 blocks (estimate).
- `DISK_PICTURE_BLOCK` and the others like it leave `vm_constants.h`.
  `mkdisk` lays the parts down one after another and writes the index.

## Order of work, and the check for each

1. The cold boot, to `no disk, ready to receive`. The reference VM
   given the ROM alone (a function in `util/` that cuts an image to
   `KROM_BYTES`), built with `STRICT` and `ROM_BYTES`, says it and waits,
   on every size of machine.
2. Block 0, the boot image, and the ROM reading it, to the prompt.
   `prompt_test`'s checks, typed at a machine started so. Built, with the
   whole image for the boot image, and checked by `boot_test` (What is
   built); `prompt_test`'s checks are not yet typed at it.
3. `load` and `block` in the ROM, what is said of an error, `.(`, the
   warm start, and `include` by label. A disk with known source and
   errors put in it: what is said, word for word.
4. The screen console. The screen in the memory `vm.c` writes when it
   stops (`util/screen.c`) has the glyphs of what was printed.
5. The IPL. The reference VM, given no disk and the blocks on its
   console, comes to the prompt.
6. The disk's ports and several disks, on the AArch64 kernels and the
   Verilog: the same checks on QEMU and on `c64-lat2-krom`.
7. The headers' new place, and `mkheader`'s check of it.
8. Every file laid out for blocks, and the disk laid out by its index.
9. One source: the words, the names and the builders.
10. The system compiled from block 1, compared with what `cross.fs`
    makes of it, and the desktop's checks run on it.

## Not decided

- A check of each binary run (`doc/disk.md`).
- On 64 and 32 bits, whether the headers go after the display, or the
  display moves.
- On 16 bits, whether the headers are read from the disk when they are
  wanted, or go above the display.
- The number of the port that says which disk, and whether a label is
  looked up on every disk or on the one named.
- Whether the screen console scrolls or wraps.
- Smaller headers, later.
