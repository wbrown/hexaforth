//
// The desktop's machine under QEMU, for the programs that drive it or
// watch it.
//
// It is QEMU's virt machine with a display, two disks, a keyboard and a
// tablet, running a kernel.  Keys and pointer movements are sent through
// QMP, QEMU's control protocol, and the screen is read from QEMU's screen
// dumps.  qemu.c implements the machine that test/deskcheck.h declares,
// and the functions declared below.
//
// QEMU runs in real time, so the desktop is given time and not turns: a
// request for turns is a wait.
//
// If SCALE is set in the environment, the bridge that shows the display
// draws each pixel SCALE pixels across and down, and the screen is read
// back through that scaling.  The reading is exact if every pixel is a
// whole block.  DISK, if it is set, slows the disk to that many thousand
// bytes a second, for reading and for writing.
//

#ifndef HEXAFORTH_QEMU_H
#define HEXAFORTH_QEMU_H

#include <stddef.h>
#include <stdint.h>

// Starts QEMU on a kernel.  The machine's first disk is a copy of `disk`,
// which is made in the directory `out` (disk.img).  Its second disk is the
// pictures' disk, which is not written to.  Each disk is a device of its
// own: the first is on the first virtio bus slot and the second is on the
// next.  The machine numbers its disks by the order of their block
// devices' slots (forth/virt-disk.fs).  QEMU is stopped when the program
// ends.
void qemu_start(const char* elf, const char* disk, const char* pictures, const char* out);

// Returns everything the console has printed so far, without waiting for
// more.  The text is valid only until the next call.
const char* qemu_said(void);

// Returns eight bytes of the guest's memory at a guest address.
uint64_t qemu_cell(uint64_t address);

// Writes `bytes` bytes of the guest's memory, starting at a guest address,
// to a file.
void qemu_memory(uint64_t address, uint64_t bytes, const char* to);

// Sets the guest address of the Forth machine's cell 0, which the kernel's
// symbol `image` gives.  A program that reads the Forth machine's memory
// (desk_memory) must call this first.
void qemu_image(uint64_t address);

// With SCALE: the number of screen pixels each pixel of the display covers
// across and down, the number of screens read, and the number of those
// that were not in whole blocks.
extern int qemu_scale, qemu_shots, qemu_inexact;

// A machine that compiles its system from its disk prints each file as it
// starts it, and its screen should show that text while it does.  These
// record what happened while desk_boot() waited for the desktop: whether
// the console printed a file, and whether the screen then had text on it,
// black on white, before the desktop appeared.
extern int qemu_boot_said, qemu_boot_shown;

#endif //HEXAFORTH_QEMU_H
