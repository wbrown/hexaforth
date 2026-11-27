# Booting from the disk

How the machine starts from its ROM, loads the image it runs on from a disk, and compiles the rest of the system from source on that disk. This is a plan, and its first part is built (What is built, below). It depends on the disk's index, which `doc/disk.md` describes.

Sizes and costs are printed by the targets `examine`, `memory-map-64` and `bench`, and `mkdisk` prints a disk's sizes when it makes one.

## What is built

The first part is built, on the reference VM, at every cell size.

### Reading the image

`main` (`nuc.fs`) first sets what the console and `throw` need (`afresh`). Then it looks for a disk whose block 0 is an index for this machine's cell size. From such a disk it takes every binary entry, reads each entry's blocks to the address the entry gives, and runs the image's `boot`.

`boot` (`console.fs`) makes the screen the console alongside the serial line (The screen console, below) and then loads block 1 with the image's own `load`, which is outside the ROM. After an error the prompt continues on the screen (`'console`).

`load` (`blocks.fs`) reports where an error in a block happened, once: the code thrown, the block, the line, the word, and that line of the block. It then throws -1, which `throw` handles silently, as the standard's ABORT does. `.(` prints a block's message, `thru` loads a range of blocks in order, and `include` (`files.fs`) loads a source from the disk by its label in block 0 (block 0 alone, leaving any further index blocks of the disk unread).

### The disk's driver

The ROM reads through the block interface, and the nucleus leaves the controller to the driver. A machine's ROM has the driver for that machine's disk. The driver is a file of its own, which the build names (`ROM`, which `cross.fs` reads, and `include-rom` in `nuc.fs`), and it defines one word that the nucleus calls, `disk?`, which reports whether the disk is present and returns the disk's words for reading and writing a block.

| Driver | The disk it drives |
|---|---|
| `port-disk.fs` | the ports `$b0`-`$b2` of the reference VM. It is the driver a build gets by default |
| `virt-disk.fs` | QEMU's virt. It defines where that machine's virtio-mmio slots are and includes `virtio-disk.fs`, the driver |

The ROM reads the image with the driver's read word, which it keeps on the stack during the read, because the image's data is read over the vectors. Afterwards it stores the driver's words in `'blk-read` and `'blk-write`, which belong to the kernel, and the system builds on them. The system starts with the ROM's driver already working, so `vmdisk.fs` and `virtio-blk.fs` are gone.

A driver's memory, where it needs any, is the last cells of memory, which are kept out of the memory allocated at run time (`top-kept`, `cross.fs`). The virtio driver's queue is there. (It used to be the last kilobyte of the kernel's page, which that page's upper half has a better use for.) Every image leaves those cells free, so a block of an image is always read clear of the queue that is reading it.

### Words outside the ROM

Words of the nucleus that are called only from outside it, and are first needed once the image is in memory, are in the upper half of the kernel's page, outside the ROM (`nuc.fs`, at its end; `upper-half` in `cross.fs`): the double-cell words other than those that read a number, `fill` and `cmove>`. They made room for QEMU's driver, which is longer than the driver for the ports. `cross.fs` refuses to build a kernel that ends outside its ROM (`rom-ends`).

### With no disk

With no disk and no image in memory, the ROM prints `no disk, ready to receive` and takes the image from the console (The IPL, below). The ROM knows the image is in memory when the image's first task is in a ring of one, which it tests before `afresh` makes it so. The text the ROM prints is in the ROM, as numbers in its code (`say"`, `emits`).

### The boot disk and its test

`mkdisk --boot` makes such a disk: block 0, the source from block 1, and the image above the ROM, as one run for each stretch of its blocks that has non-zero content. It also writes a file beside the disk with the data the IPL takes.

`test/boot_test.c` (`boot-64`, and the same test for each other machine) starts a machine from the ROM alone and from its whole image: with that disk, with the disk's index removed, diskless, and diskless with the image sent to the console, both with and without a non-index block sent first. A store into the ROM stops the machine.

### What the boot compiles

The system's files that are compiled from source at boot are every file after the window manager and the console, at every cell size. `forth/apps.fs` is the end of the system's list (`system.fs`), and `cross.fs` with `BOOT=on` builds the image without it (the target `boot-system-64`, and the same target for each other machine, in `build/64/boot/`). That image is the kernel's page, the page of code that compiles and the page of code that draws. Its disk has `apps.fs` as block 1, followed by each file `apps.fs` includes (`mkdisk --boot` follows the includes), so the boot compiles them: the TIFF reader and the dither, text, the Forth window, the editor, the decompiler, the gray picture, the viewer with its menu, the calculator, HexaPaint, on 64 bits QEMU's drivers, and `desk.fs`. What the image tells the decompiler about itself, which is compiled after the image is made, belongs to the image (`image.fs`).

The words of `cross.fs` that such a file uses are words of the nucleus too. The ones that are one text for both compilers are in files of their own, which gforth also compiles for `cross.fs`: `d#` and `,"` (`literals.fs`), `[if]` (`conditional.fs`), `/` (`divide.fs`), and the builders of pictures and pattern rows (`pixels.fs`). The ones each compiler implements in its own way are `meta` and `bulk` (`placement.fs`), and `::`, in whose body `literal` compiles a call, with `s"` (`macros.fs`). The machine's sizes and flags are generated from `cross.fs`'s own definitions (`names.fs`; `both::`, `HOST`). The basewords are words of the nucleus too (`baseword-words`, which `basewords.fs` gets from `vm_opcodes.h`).

`boot_test` starts the ROM alone on that disk, runs the desktop the boot compiled, and leaves it. The desktop must have opened its windows.

### A file's own names

A word of an included file can be found by its name until the file has been read, and after that only if a header came before it (Decided, 8). A header is cells, written from the address `hdp` holds, as far as `hdl` (`nuc.fs`), which are cell addresses, so while a file is read, its own names are written in memory taken from above the display, a piece at a time, and `header` writes a header in the dictionary, which stays. A name that stays can be found only once its file is done, as in `cross.fs`, where a header only names a word for the prompt: inside the file, a name means what it meant before. When the file is done, its own names are removed from the chain and their memory is freed (`files.fs`).

`cross.fs` gives a header only to a name the source gives one, on every machine, so the image is 128KB again, with the headers below the code, and the memory allocated at run time starting after the display. `mkheader` checks the header pointer. `boot_test` checks all of this: a headerless word is lost after its file, and a header's name is found, whatever the definition was called.

The names that leave are kept for the decompiler, where the disk has blocks for them (its index's entry `own-names`, which `mkdisk --boot-desk` makes). Each file's names are written there as the file ends, in the form labels have in the tables of names, and `see` and the Words window use them to show a word the boot compiled (`files.fs`, `see.fs`).

### Code pages

When a definition runs off its page of code while a block is being read, it is compiled a second time on another page, from where it began (`reading`, `blocks.fs`), so a block being read can fill a page almost to its end. Typed source still has a kilobyte reserved (`cpr`, `nuc.fs`). What the definition compiled on the page it ran off is zeroed, and the page has that room again.

The machine puts a file on the page its list names, as `cross.fs` does (`placement.fs`). The image records where each page's code ended (`pages`, `nuc.fs`), and `code-page`, `sys-page`, `big-page` and `small-page` in a block continue the code from there. When a page is full, code continues after all the existing code. When the first block read is done, the code pointer is left there, where the kernel's `newpage?` finds the following page free.

### What is compiled and then discarded

What a file needs only while it is compiled is discarded afterwards (`placement.fs`, `files.fs`). That is code between `meta` and `target`, and a word of the file's own that only pushes a number, which is replaced by the number wherever it is used. Both are compiled into the scratch area, the end of the last code page, which is zeroed when the file has been read. The record kept of such a number's name holds the number alone. The target `booted-64`, and the same target for each other machine, shows what a boot compiled beside what `cross.fs` compiles for the same words.

Some code that the machine compiles for itself, and that is absent from the image `cross.fs` builds, is still in the code pages when the boot is done: the bodies of a file's `::` words, which run only while the file is read, and a file's own words that only push a number and had to go outside the scratch area. `booted-64`, and the same target for each other machine, totals that code by kind. `memory-map-16`, and the same target for each other machine, prints the size of the boot's code beside the image's.

### Looking a name up

The boot looks a name up one header at a time (`sfind`, `nuc.fs`), so the image's headers are linked in order of how often the system's source uses each name, most used first (`headers-ordered`, `cross.fs`). The count includes the applications' source, which the image a machine boots from is built without (`says`, `system.fs`). A header created on the machine is linked in front of the image's, so a search finds the newest definition of a name. A number after `d#` or `h#` is read as a number, skipping the lookup (`numbered`, `nuc.fs`). The target `compiling-64`, and the same target for each other machine, reports what a boot costs and where the cost goes. `finding-64`, and the same target for each other machine, reports the names the boot looks up and how many headers were examined for each.

### The desktop on a machine started this way

`mkdisk --boot-desk` makes a disk that a machine starts from and that is also the desktop's disk. The source comes first, all of it, from block 1, which is the list the boot loads, so a person paging through the disk in the editor sees only source until the source ends. After the blocks reserved for source come the names, the canvas, the masks and the viewer's bitmap and gray, each starting where the one before ends (`vm_constants.h`). The image comes after all of those, at the blocks the index gives (`build/64/boot/desk.img`). The picture is on the pictures' disk, the machine's second, where the viewer finds it by its label.

The desktop's checks run on the reference VM started with only its ROM and that disk, at every cell size (`desktop-booted-64`, and the same test for each other machine): every check the desktop has, on the desktop the boot compiled. The editor's check reads the text it expects from block 1 of the disk it is given, which on such a disk is `apps.fs`.

### QEMU started this way

A kernel can be built with only its ROM in it (`kernel(... ROM)` in `cmake/kernels.cmake`; `hex2bin --rom` writes the first 4KB of the image the machine boots with). The memory after the ROM is the machine's, and the image arrives there only when the ROM reads it. QEMU's virt has an image of its own (`build/virt/`, `machine-virt`), because its ROM has its own disk's driver. The desktop's checks run on QEMU started from that ROM and the disk, with each kernel (`qemu-hexaforth-rom-display-strict`, `qemu-hexaforth-fast-rom-display`), and the target `demo-boot` shows it.

### What is still to build

The rest of this plan is still to build: a disk for the Verilog, and compiling the rest of the system from source (the display, regions, the window manager and the files before them, which also need the font builder, `keys[` and `px,`).

The virtio drivers are written in 64-bit device cells, for the one machine that has those devices. A 16- or 32-bit machine reaches a device through the same four ports, in cells of its own size (`MODEL.md`, The ports), and a driver for one is still to be written.

Once the image is read, the code in the ROM is in the ROM only because the machine started from it. The image has its own `load` and `block`, and they stay in the image (Blocks, below).

## Where things stood

When this was planned:

- The kernel was all of `nuc.fs`, on page 0, in the ROM's 2,048 instructions. It had the outer interpreter and the compiler: `main`, `quit`, `accept`, `interpret`, `sfind`, the reading of numbers, `evaluate`, `catch`, `throw`, `:` and the rest.
- All of it needed RAM beyond the ROM, which `cross.fs` writes into the image: the headers; the variables, with the values the end of `nuc.fs` gives them (`up`, the ring of tasks, `'compile`, `dp`, `hdp`, `cp`); and the tables (`kcode`, which reading a name or a number depends on). With `up` zero, `main`'s first store went to cell 6, in the ROM. `afresh` sets `up` now.
- `'compile` was `pcompile,`, in `join.fs` on the last page of code, outside the ROM.
- All three machines started with the whole image in memory: `main.c` loads it, the AArch64 kernels carry it, and the Verilog reads it with `$readmemh`. Only `boot_test` started one from a disk.
- Each machine reached a disk in its own way: the reference VM through ports `$b0`-`$b2`, the Verilog bench over the console's line (`bench/tb-disk.fs`), and QEMU through virtio, driven by Forth (`virtio.fs`, `virtio-blk.fs`).
- `load` and `block` were in `blocks.fs`, on page 1, outside the kernel.
- The kernel's headers were too large for the ROM: on every machine they exceeded the room the ROM had left.

## Decided

1. The kernel's RAM is on the disk, as the binary entries of block 0 (`doc/disk.md`), and the ROM reads it before anything else.
2. A disk is a device, not part of the machine. The reference VM, QEMU, the Verilog and hardware each have their own controller. A disk's driver is written in Forth and reaches the controller through ports. The driver is a file of its own, and a ROM is built for one machine: a machine's ROM has the driver for that machine's disk, the build of its image names the driver's file, and the nucleus's source names no controller. Not everything in a machine's kernel space comes from the nucleus's source. A driver assumes no cell size. The ROM boots through the block interface, and the system uses the driver the ROM has: the ROM and the image do not each have a reader. With no disk the ROM takes the image from the console (4), on every machine, as a machine with a monitor in its ROM does. (This was first written down wrongly, and not by the person who decides. The first version had the AArch64 kernels answer the reference VM's ports with virtio in assembly. The second had one ROM with a driver for every controller, each tried in turn. That version was built in `nuc.fs`, with QEMU's driver written for 64-bit cells.)
3. The boot image is the kernel's RAM, `join.fs`, and the screen console with mono.
4. With no disk, the ROM prints `no disk, ready to receive` and takes over the console what a disk would have given it (The IPL, below).
5. An error while the boot loads source stops the boot, at the prompt.
6. What is printed goes to the console and to the screen.
7. Block 1 is the list of what the boot loads, made from the list in `system.fs`.
8. Not every word keeps a header. A name the source gives a header (`header`) is public, and stays. A word without one belongs to its file only, and its name ends with the file. The 64-bit and 32-bit machines must fit in 256KB. (This was first written down as every word keeping its header, with the headers after the display and an image of 256KB. It was built that way, with run-time allocation starting at byte $40000, which nobody had decided. The headers were then more than twice the size of the headers the source asks for.)
9. The system is one source, which `cross.fs` and the nucleus both compile.
10. Each application decides how it finds its data on the disk.
11. Every file of the system is formatted to fit blocks.
12. A disk is 1,024 blocks, the capacity of an 8-inch floppy, and the pictures are on a second disk. (This was written here as "the picture may go on a second disk", which is not what was decided, and a disk longer than a floppy, with the picture on it, was built from that wording. It is now built as decided: `doc/disk.md`, Several disks.)

## The ROM

### Where it starts

All three machines start at instruction 0: `vm.c`'s context is zeroed, `soc.v` gives the Verilog a `start_pc` of 0, and the AArch64 kernels set the PC to 0 (`hexaforth.S`). `cross.fs` puts `main` at instruction 0 and `quit` at instruction 1, and `throw` restarts at 1.

Instruction 0 is the cold boot, and instruction 1 is the warm start. `throw` sets `'emit` to `uart-emit` (`nuc.fs`), so the warm start sets it back to the screen console, if there is one, and runs `quit`.

`vm.c` stops when it fetches an all-zero instruction, so a run of the ROM with the rest of memory zero stops at the first call into memory that is still to be read.

### The cold boot

1. Set what the console and `throw` need: the depths of the stacks, `up` to `main-task` and its ring to itself, `base`, `'emit` to `uart-emit`, `handler`, `sbase`, `state`, `blk` and `sourceid`.
2. Ask disk 0 how many blocks it has (`$b0`). If it has zero, run the IPL.
3. Read block 0. Go on only if its entry 0 is an index entry for this machine's cell size, and otherwise treat the disk as absent. (The disks the desktop and the tests are given have yet to get an index.)
4. Collect every binary entry of block 0, and then read each entry's blocks to the address it gives, printing its label. Collect them all first, because a run may cover the buffer block 0 was read into.
5. Run `boot` if the image has it, found by its name as `main` finds `cold` now. `boot` starts the screen console.
6. `1 load`, within `catch`. An error is reported (What it tells, below), and the boot stops at the prompt.
7. Run `cold` if it is now defined, and then `quit`.

### Blocks

The ROM reads a block through the driver of the controller it found, into a buffer of its own in the last block of memory, and it reads blocks only to load the image. `load`, `block` and the rest stay in `blocks.fs`, which is in the boot image. The dictionary arrives with the image, so a `load` in the ROM would be useless to a machine before the image is in memory, and once the image is in memory its own `load` is available. The ROM therefore needs only the code that reads the image, and the report of an error in a block, `.(`, and `include` belong to the image too. (This was first planned the other way round, with `load`, `block` and the error report in the ROM.)

### The IPL

With no disk, the ROM prints `no disk, ready to receive` and reads from the console what it would have read from a disk: block 0, and then the blocks of each binary entry in order, starting with entry 1's. The walk is the same as from a disk, except that each block comes from the console instead of the ports: `DISK_BLOCK` bytes packed into cells, the first byte of each cell the lowest. Until the block it receives is an index for this machine, it prints the message again and takes another block 0. `mkdisk --boot` writes the data to send to a file beside the disk it makes.

The console's port delivers each byte unchanged (`vm.c`, `hexaforth.S`), and only the sender's side can change a byte, for example a terminal that converts line ends. A serial line can lose a byte, so a check of the binary runs (To be decided) is how the IPL would report a bad transfer. After the IPL the machine has its dictionary and the prompt, and is still diskless.

### What it tells

- The font arrives with the image, so the messages printed before then (`no disk, ready to receive`, and the message for a disk made for another machine) go to the console only.
- A file gets one line: its label, its length in blocks, and a dot as each of its blocks starts (`include`, `files.fs`). This is built. A file read from inside another file gets a line of its own, so the outer file's line is ended first, and the dots of the outer file's later blocks are on separate lines. A line is ended only if something has been printed on it. The screen console asks for a key after every character, which is when QEMU's bridge copies the screen, so a line is visible as it grows. A finished file's line leaves out `cp` and `here`.
- While the ROM reads the image, the screen stays as it is, because the font and the code that draws text are in the image, and arrive with the rest of it.
- An error is reported where it happened, before `load` restores `blk`: the code, the block, the line (`>in` divided by 64), the word (the last `parse-name`, which `reading` keeps), and that line of the block. It is reported once, and the `load`s it passes up through stay silent.
- `.(` is in the image (`blocks.fs`), so that block 1 and the files can print what they are doing.

### Room

The target `examine` prints how much room each machine's ROM has left. Where that is too little, these are the words the boot can do without (`disasm_hex` prints the length of each):

| What | Words |
|---|---|
| Double-cell words other than those that read a number | `dabs` `2rot` `d>s` `d=` `d<` `du<` `d-` `d0<` `d0=` `d2/` |
| Division and printing in decimal | `divstep` `um/mod` `u/mod` `(.)` `.` |
| Typed lines | `accept` `delchar` `3rd` `3dup` |
| Showing names | `code>` `name>` `words` |
| Tasks | `spawn` `(task)` `idle` `pause` |
| Characters a byte at a time | `cmove` `cmove>` `fill` |
| The rest | `lo32` `hi32` `lo16` `hi16` `2over` `abs` `max` `depth` `w,` |

They can go in the upper half of page 0, which a call reaches from any page in one instruction, as it reaches the ROM. The double-cell words, `fill` and `cmove>` are there now (What is built). Three things in the ROM call into the rest, and would have to stop doing so first: `throw` prints with `.`, `ckey` calls `pause`, and the prompt, which is the one thing that works on a diskless machine, uses `accept`.

## The boot image

The boot image is `nuc.fs` built with a list of its own, named by `SYSTEM` as `picture.fs` is: the kernel, `join.fs` (as the displayless machine has it now), and the screen console with mono. `cross.fs` writes it, and `mkdisk` makes its binary entries, one run for each part of it as `util/layout.h` defines the parts (the data and the headers, the pages of code, the upper half of page 0, and on 16 bits the data above the display), because memory holds arbitrary values when a machine is turned on.

## The screen console

`console.fs` is built. While the boot image is the whole system, the console is built from the words of `display.fs` and `region.fs`, so only `display.fs` knows how the display is laid out:

- `console` makes the screen white with `pat-rect` and sets `'emit` and `'console` to `con-emit`. `con-emit` sends a character to the serial line and draws it with `draw-char`, in the port's font, at the width the font gives the character.
- CR goes to the start of the line and LF to the next line. Backspace moves back by the width of a blank. When the last line is full, the screen scrolls up a line (`scroll-rect`).
- `key?` is read after every character, because QEMU's bridge copies the bitmap only when the key's port is read (`hexaforth.S`).
- `boot` starts the console before it loads block 1. The warm start is `throw` setting `'emit` from `'console` (`nuc.fs`), so after an error the prompt is drawn on the screen as well.

When the boot image is the kernel and little else, the plan is for its console to be the first part of `display.fs`, in a file of its own: `fb-w`, `fb-h`, `row`, `ink`, `paper`, the bitmap filled with paper, and a mono glyph drawn at a character's position. Mono is 8 by 16, so a row of a glyph fits in one cell of a row of the display: taken from the strike on 32 and 64 bits, and from the expanded font on 16. The rest of `display.fs` is then compiled from source, and is built on these.

## Disks and their controllers

The driver in a machine's ROM reaches a disk through its controller's ports, and everything else reaches it through the block interface (Decided, 2):

| Machine | Its disk controller | Driver |
|---|---|---|
| the reference VM | the ports `$b0`-`$b2` | `port-disk.fs` |
| QEMU | virtio | `virt-disk.fs`, `virtio-disk.fs` |
| the Verilog's bench | on the console's line, for now | `bench/tb-disk.fs` |

A disk is 1,024 blocks and the machine has two: the pictures are on the second (Decided, 12). Each disk has its own block 0, and the ROM starts from disk 0. `doc/disk.md` (Several disks) describes how a block of another disk is numbered and how each machine reaches it.

## Headers

A header belongs to a name the source gives one. The headers are where they were, below the code (`build/64/nuc.sizes`, and the same file for each other machine, gives their size; their room is what `cross.fs` reserves, `hdr-base`).

- Among them, the headers of `names.fs`, with the basewords and sizes it defines, and the headers of `placement.fs`, exist only so that the nucleus can compile the system's source.
- `mkheader` checks the header pointer against the cell that headers must stop before (`hdp` and `hdl`, `nuc.fs`), which for the dictionary's own headers is where the code starts, and for a file's own names is the end of the piece of memory they are in while the file is read.
- A lookup goes through the headers one at a time, so fewer headers make a shorter boot. `build/64/map/boot.out`, which `memory-map-64` writes, has the number of instructions a machine runs from its start to its prompt. With a header for every word, the 64-bit machine ran nearly twice as many.
- A file's own names are outside the tables of names `cross.fs` makes. The machine keeps them itself, on a disk that has blocks for them (What is built), and the decompiler uses them to show a word the boot compiled. On a disk that lacks those blocks it shows such a word as a number.
- A header can be anywhere in memory: a link and an execution token are cells, and `sfind` follows them with `@`. Two things in the tree assume the headers are where they are now: the decompiler treats a number between `VARS` and the pages of code as data or a header (`see.fs`), and the tools read headers out of the image's 128KB.

## One source

The system's source uses these things that only `cross.fs` knows:

- Words for numbers and text: `d#`, `h#`, `c#`, `[char]`, `char`, `,"`, `s"`, `abort"`, `/`.
- Words that control how a file is compiled: `header`, `meta`, `target`, `::`, `[if]`, `[else]`, `[then]`, `bulk`, `unbulk`, `high`, `unhigh`, `in-place`.
- The machine's sizes and the disk's layout as names (`CELL`, `PXMASK` and the rest, many of them used outside the kernel), and the values the files' `::` words compute with (`cell-bits`, `lanes`, `spaces-cell` and others). `cross.fs` refuses a word that has a macro's name, so these have to stay macros for now.
- Code that `cross.fs` runs as gforth: the `meta` sections and the bodies of `::` words; the builders of fonts, pictures, keys and patterns (`cross.fs`); `hdr,` and `at:`; and an instruction written by its fields (`display.fs`).
- `literal` in the body of a `::` word compiles when the macro runs in `cross.fs`, and when the macro is defined in the nucleus.

## Source in blocks

Many of the system's files had a line longer than 64 characters, or more than 16 lines between one blank line and the next (`mkdisk --pack`). Done so far, with each machine's image byte for byte the same after each step:

- `wrap` covers every file in `forth/` that goes on a disk (CMake's `DISK_SOURCES`), and has formatted all of them.
- A table longer than a block is broken between two of its entries (`doc/formatting.md`), which `wrap` now does. `spin.fs`'s pictures are pieces side by side that fit a line.
- A conditional (`[if]` ... `[then]`) longer than a block is split in two, because skipping stops at the end of a block. A conditional is also kept free of blank lines, so that it goes in one block, and `mkdisk` reports a conditional that crosses a block. It reports this because `desk.fs` once ended in a conditional of three runs: the boot on 32 and 16 bits skipped the first run and then compiled the rest.
- `fused.c` and `joins.c` write `fused.fs` and `folds.fs` to fit.
- An `s" ..." evaluate` too long for a line is split into two.

Left to do: `mkdisk --pack` still reports two files, both outside what a boot compiles so far. `display.fs` has a conditional that crosses a block, and `wm.fs` has a run of lines longer than a block. A definition longer than a block is factored by hand, and its lines can be broken only once it is. The cold ones were done first (`desk.fs`, `viewdesk.fs`, `virtio-input.fs`, `join.fs`, `tiff-open` and `selem`, and those in `wm.fs`). The hot ones are factored into words that are correct when called, measured with `bench-draw`, `bench-dither` and `bench-gray` before and after, and given `in-place` only where a call shows in the figures: those in `region.fs`, `display.fs` and `atkinson.fs`, and `tiff.fs`'s `gray-lanes`, `gray-pairs` and `pixels`.

Some of the desktop's checks used a system word that is longer than its window (`tiff-open`, `scroll-rect`). Once every definition fits a block, any system word may be shorter than the window, so those checks must type a long word of their own.

`mkdisk` prints the number of blocks the source takes when it makes a disk, and it refuses to make a disk longer than a floppy.

`DISK_NAMES_BLOCK` and the constants like it are to be removed from `vm_constants.h`: `mkdisk` writes the parts one after another and writes the index. The picture's block constant has been removed (`doc/disk.md`, Several disks).

## Order of work, and the check for each

| | Work | Its check | State |
|---|---|---|---|
| 1 | The cold boot, as far as `no disk, ready to receive` | The reference VM, given the ROM alone and built with `STRICT` and `ROM_BYTES`, prints the message and waits, at every cell size | Built: `boot_test` zeroes the memory above the ROM itself, which takes the place of a tool that cuts an image |
| 2 | Block 0, the boot image, and the ROM reading it, as far as the prompt | `prompt_test`'s checks, typed at a machine started this way | Built, with the whole image as the boot image, and checked by `boot_test` (What is built). `prompt_test`'s checks are still to be typed at it |
| 3 | The report of an error in a block, `.(`, the warm start, and `include` by label, in the image (Blocks, above) | A disk with known source and deliberate errors, and the exact text that is printed | Built and checked by `boot_test` |
| 4 | The screen console | The screen, in the memory `vm.c` writes when it stops (`util/screen.c`), has the glyphs of the text that was printed | Built: `boot_test` reads the screen out of the machine's memory and compares block 1's output, and an error report, with mono's glyphs |
| 5 | The IPL | The reference VM, started diskless with the blocks on its console, reaches the prompt | Built, and checked by `boot_test` |
| 6 | The ROM booting through the block interface, with the driver for its machine's disk | | Built for the reference VM and for QEMU, and the desktop's checks run on QEMU started this way. Still to build: the Verilog's disk, and the same checks on `c64-lat2-krom` |
| 7 | A file's own names, which end with the file, and `mkheader`'s check of the header pointer | | Built, and checked by `boot_test` |
| 8 | Every file formatted for blocks, and the disk laid out by its index | | |
| 9 | One source: the words, the names and the builders | | |
| 10 | The system compiled from block 1 | It is compared with what `cross.fs` compiles from it, and the desktop's checks are run on it | Built for what the boot compiles now, the applications and `desk.fs`: the desktop's checks run on it (`desktop-booted-64`). The target `booted-64`, and the same target for each other machine, prints what the boot compiled beside what `cross.fs` compiles for the same words |

## To do

For later: the editor on a machine with several disks. It opens on block 1, which is on disk 0, and disk 0 is all it can reach (`doc/disk.md`, Several disks).

Next, now that QEMU starts from its disk:

- Text on 16 bits. With a font stored as lanes, `bench-draw` shows text slightly slower on 16 bits than it was: drawing a glyph takes the glyph's lane and a multiply where it took two fetches. On that machine a lane is a cell, so the lane for a glyph can be the cell in front of its rows, and the value recorded for a glyph can be its address. Drawing a glyph then takes a single fetch.
- The desktop's profile after the fonts. It was taken only before (`build/paths-before.txt`), so the present cost of the text that goes straight to the screen is still to be measured. `bench-draw` measures the text that is gathered.
- Data compiled at run time is checked against its limit only before a definition (`mkheader`). A table compiled after `create` can run into the dictionary, and the machine reports it only at the next definition. The fonts exposed this, when they were more data than the boot had room for on 64 bits.
- The virtio driver for the keyboard and the pointer, `virtio.fs` and `virtio-input.fs`, is included on every machine with 64-bit cells (`apps.fs`). That uses the cell width to stand for which machine it is, the fault the disk's driver had. It also has three faults that `virtio-blk.fs` had and the disk's driver has since shed: one phrase written three times for a queue's three addresses, the status read back in order to set bits whose values are known, and a count of slots where a loop over their addresses would do.
- Two things the driver omits when it talks to QEMU's disk, which works all the same, as run on QEMU with each kernel and the desktop's checks: which queue is meant (it has one), and that its interrupt was seen. The driver's status writes are the four the device's protocol defines.
- The ROM of QEMU's virt is almost full (`examine`, on `build/virt/boot/nuc.hex`).
- The test `kernel` runs `kernel_test` for the default machine only. For the other cell sizes, what reaches a word was checked, before words were moved out of the ROM, only by building them: a call from the kernel to a word placed after it fails to compile.
- The memory layout, and the headers when they outgrow their room, which the next two sections describe.

### The memory layout

`memory-map-64`, and the same target for each other machine, shows the memory layout: where everything is, how far the heap reaches while the desktop's checks run, which words its memory is allocated to, and how much of each piece is ever stored to. Run-time allocation starts after the display again, and the desktop is in the first 256KB on 64 and 32 bits, and in the machine's 128KB on 16:

- Everything the heap holds is counted in cells, so it is four times as many bytes on 64 bits as on 16. Most of it is regions, of which little of each is ever stored to, and the Words window's buffers.
- The fonts have left the heap. A font used to be expanded the first time it was used, into a cell for every row of every glyph. `cross.fs` now stores a font as its rows in lanes of a cell, which takes as many bytes as the font has pixels, rounded to a lane, and text is drawn from that (`cross.fs`, `display.fs`). Text is faster for it on 64 and 32 bits (`bench-draw`). On 16 bits, where a cell is one lane, a font takes the space it took before and text is slightly slower.
- The picture has left the heap. Its bitmap used to be there: all of it was in memory while it was dithered, it was freed once it had been written to the disk, and that left a hole under the memory allocated in the meantime. The menu allocated it again to draw into, and the picture kept as gray took as much again, above all of them. Now the dither makes one block of the bitmap at a time and writes each block, the gray is made and kept on the disk the same way, and the window is painted from the blocks its clip needs (`view.fs`), which is what brought the heap's reach down to where it is.
- The boot's own heap, at its prompt with the applications compiled, holds an expanded font and the blocks' buffer. The memory `picture[` keeps a picture's rows in is freed when the picture has been compiled.
- Below the display the slots are fixed, each by a constant in `cross.fs` for the machine's size, and each is partly empty: after the ROM, after the data, where the headers are again, and in the code pages reserved for definitions made at run time, of which the desktop uses little.
- `hfree` keeps one freed run, the largest, and all other freed memory is lost.

The display is just past the first 65,536 instructions, which is as far as `cross.fs` lays code out, and on 16 bits just past the first 32K cells, which is as far as the dictionary's execution tokens and the Forth system's byte addresses reach. Both are the Forth system's limits, and the machine's program counter is a cell (`MODEL.md`, Code).

### Headers that outgrow their room

When the headers that stay outgrow their room below the code, they continue past it, in stretches taken from the heap: when the room is full, `mkheader` takes a stretch and continues in it. The limit is then the end of that stretch, in place of a number compiled into the kernel (`HDR-LIMIT`). A header can be anywhere in memory, since a link and an execution token are cells, which `sfind` follows. A stretch can be written to the disk and its memory freed, and read back when a name is looked up or shown, as the decompiler reads the names of headerless words (`nm-buf`). Headers are read only when a name is looked up or shown. The design has to deal with these:

- A link from one stretch to another points to a place that is absent while the stretch is on the disk. Either such a link records which stretch and how far into it, or each stretch is read into a buffer of its own.
- A name that misses reads every stretch from the disk, and every number is looked up as a name first, and compiling the system at boot has many numbers. That cost must be measured. The usual answer is an index of the names, which can tell when a name is absent.
- A diskless machine keeps its headers in memory.
- A stretch is a run of blocks with an entry in the index (`doc/disk.md`).
- Code that frees memory by setting `himem` back to an earlier value (`with-room`, the tables of names, the picture's memory) must allow for a stretch that was taken while it had the memory.

## To be decided

- A check of each binary run (`doc/disk.md`).
- The number of the port that selects the disk. (A label is looked up on every disk, disk 0 first: `doc/disk.md`, Several disks.)
- Whether the screen console scrolls or wraps.
- Smaller headers, later.
