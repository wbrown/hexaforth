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

// Start QEMU on a kernel, with a disk.  It is stopped when the program ends.
void qemu_start(const char* elf, const char* disk);

// What the console has said that has not been asked for yet, added to
// what is there: it does not wait.
void qemu_said(char* text, size_t room);

// Eight bytes of the machine's memory, at an address of the machine's.
uint64_t qemu_cell(uint64_t address);

// With SCALE: how many each pixel is across and down, the screens taken,
// and those that were not in whole blocks.
extern int qemu_scale, qemu_shots, qemu_inexact;

#endif //HEXAFORTH_QEMU_H
