//
// The desktop's machine under QEMU, for what drives it or watches it.
//
// QEMU's virt machine with a display, a disk, a keyboard and a tablet,
// running a kernel.  Keys and the pointer are sent through QMP, QEMU's own
// way of being told things, and the screen is what QEMU dumps of it.  It
// is what test/deskcheck.h says a machine is (qemu.c has those), and what
// is here besides.
//
// QEMU runs in its own time, so the desktop is given time and not turns:
// a turn that is asked for is a wait.
//
// With SCALE in the environment the bridge that shows the display draws
// each pixel that many across and down, and the screen is looked at
// through that: it is exact if every pixel is a whole block.  DISK is the
// thousand bytes a second the disk gives and takes, if it is to be slow.
//

#ifndef HEXAFORTH_QEMU_H
#define HEXAFORTH_QEMU_H

#include <stddef.h>
#include <stdint.h>

// Start QEMU on a kernel, with a copy of a disk that is made in a
// directory (disk.img), and the pictures' disk as its second, which is
// not written to.  Each is a device of its own, the first in the first
// place for one and the second in the next: the machine's disks are its
// block devices in the order of their places (forth/virt-disk.fs).  It is
// stopped when the program ends.
void qemu_start(const char* elf, const char* disk, const char* pictures, const char* out);

// All the console has said so far, however much: it does not wait for
// more.  The text is where it is only until this is next asked.
const char* qemu_said(void);

// Eight bytes of the machine's memory, at an address of the machine's.
uint64_t qemu_cell(uint64_t address);

// As many bytes of its memory as are said, from an address of the
// machine's, written to a file.
void qemu_memory(uint64_t address, uint64_t bytes, const char* to);

// Where the kernel has the Forth machine's cell 0, which its symbols say
// (image): what asks for the Forth machine's memory (desk_memory) says it
// first.
void qemu_image(uint64_t address);

// With SCALE: how many each pixel is across and down, the screens taken,
// and those that were not in whole blocks.
extern int qemu_scale, qemu_shots, qemu_inexact;

// A machine that compiles its system from its disk says each file as it
// starts, and its screen is to show that while it does.  While the desktop
// was waited for (desk_boot): whether the console said a file, and whether
// the screen then had text on it, black on white, before the desktop came.
extern int qemu_boot_said, qemu_boot_shown;

#endif //HEXAFORTH_QEMU_H
