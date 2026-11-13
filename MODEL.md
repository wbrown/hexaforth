# The machine model

Hexaforth is one machine with three implementations: `vm.c`, the AArch64
kernels in `baremetal/`, and the Verilog in `rtl/`. This file says what the
machine is. A program may rely on what is written here and on nothing else,
and each implementation must behave the same way wherever this file defines
the behaviour.

Sizes are properties of a machine and are set in `vm_constants.h`, from
which they reach the toolchain and the other implementations.

| Name | Meaning | Default |
|---|---|---|
| `CELL_BITS` | width of a stack cell, the ALU, and a fetch or store: 64, 32 or 16 | 64 |
| `ADDR_BITS` | memory is 2^`ADDR_BITS` bytes | 17 |
| `STACK_CELLS` | cells in each of the two stacks | 256 |

Where this file says a thing is undefined, implementations may differ, and a
program that does it is wrong. The strict builds stop at the first such
thing: `-DSTRICT` for `vm.c`, `make STRICT=1` for the small AArch64 kernel.

## Stacks

1. There are two stacks, the data stack and the return stack. Each is an
   array of `STACK_CELLS` cells and is not part of memory: `@` and `!`
   cannot reach it. A program reaches a stack by pushing, by popping, and
   through its depth port.
2. Each stack has a depth. Port `0xd0` is the data stack's and `0xd1` the
   return stack's. Reading a port gives the depth as the instruction starts,
   so `port io@` counts the port number it has just pushed.
3. Writing a port sets the depth and changes nothing else. The cells that a
   lowered depth hides keep their values, and are there again when the depth
   is raised. An implementation may keep the top cells of a stack in
   registers; it writes them back before the depth moves, so a program never
   sees the difference.
4. A cell that is popped is gone. Its value is undefined from then on, even
   if the depth is later raised over it.
5. Pushing onto a full stack, popping an empty one, and setting a depth
   outside 0 to `STACK_CELLS` are undefined.

Rule 3 is what lets several tasks share the stacks: each task owns a range
of depths, and changing task is saving two depths and setting two others.
It also gives access to a cell by its position without any further port:
set the depth to that position, push or pop, and set the depth back.

## Memory

Decided in outline; the layout question is open.

1. Memory is exactly 2^`ADDR_BITS` bytes, addressed from 0. Nothing else is
   reached with `@` and `!`.
2. A fetch or store outside memory is undefined.
3. Devices are not in memory. They are reached through the ports.

Not yet true of the implementations. The AArch64 kernels run with all the
RAM QEMU gives them, and the desktop allocates from `0x100000` upward with
`halloc`; `vm.c` has no memory there and does not check; the Verilog memory
wraps. Under this model the desktop is a machine with a larger `ADDR_BITS`.

Open: `ADDR_BITS` also fixes the layout of the first 128KB, where code must
stay because an execution token is stored in 16 bits. Raising it must not
move the code pages, so the size of memory and the layout of the image need
to be separate things in `cross.fs`.

## Display

Decided in outline; the details are open.

The display is a bitmap in memory, one bit to a pixel, of a size set in the
machine definition (which may be none). Drawing is fetching and storing
cells. Each implementation shows the bitmap however it can: `vm.c` may write
it out, the Verilog scans it out as video, and on QEMU a bridge copies it
into the true-colour buffer that `ramfb` wants. The bridge is a separate
assembler file from the kernel, so that another display can take its place.

Open: the resolution and the cells in a row; which bit of a cell is the
leftmost pixel; whether a set bit is black or white; and where in memory
the bitmap is.

Not yet true of the implementations. The Forth system draws through
`display.fs` into a driver, `ramfb.fs`, that keeps four bytes for each pixel
in memory above the image.

## Other devices

The keyboard, the pointer and the disk are reached through QEMU's virtio
devices, by drivers written in Forth. They are not yet part of this model.
