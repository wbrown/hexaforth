//
// The reference VM as a machine the desktop's checks can run on
// (deskcheck.h).  The desktop's test uses it, and so does anything else
// that drives the desktop (bench/profile.c).
//
// The machine is the nucleus on vm.c (nucleus.h), at the cell size of the
// image: 64, 32 or 16 bits.
//
//   the disk       The VM's own disk, whose driver the kernel's ROM finds
//                  when the machine starts.  It is a copy of the disk
//                  given, so that the editor and HexaPaint can write to
//                  it.
//   keys, pointer  The VM has no keyboard and no pointer, so this file
//                  provides them: a word stored in 'poll, typed at the
//                  prompt, which the event loop runs on every turn.  On
//                  each turn it reads one item from the console: a key
//                  going down or up, by its Linux key code, as a virtio
//                  keyboard reports it; the pointer's position; its
//                  button; an item that does nothing; or a request to
//                  stop.  It passes events to on-event, as any driver
//                  does.
//   the screen     The display in the VM's memory.  To read the screen,
//                  this file sends a request to stop as the last item.
//                  The machine stops in its event loop once it has read
//                  everything sent before, and is resumed from there
//                  later.
//
// The desktop takes exactly one turn of its loop for each item sent, so a
// run is the same every time.  The machine has the key map, so this file
// gets from the machine which key gives which character.
//

#ifndef HEXAFORTH_DESKVM_H
#define HEXAFORTH_DESKVM_H

#include "deskcheck.h"

// Starts the machine from an image, with a copy of `disk` made in the
// directory `out`, and gets it ready to start its desktop.  The desktop
// itself starts on the first call of desk_boot().  `pictures` is the
// second disk, which has the picture (mkdisk --disk) and is never written
// to.  `picture` is the path of the same picture as a file.  Returns the
// facts about the machine that the checks need.  With `rom_only`, the
// machine starts with nothing in memory but its ROM, and `disk` must be a
// disk it can start from: the machine reads the rest of the image from it,
// and compiles the files that the disk's block 1 lists.
desk deskvm_start(const char* image, const char* disk, const char* pictures, const char* picture, const char* out,
                  bool names, bool rom_only);

// The number of turns the event loop has taken, which is the number of
// items sent that the machine has read.
extern long deskvm_turns;

#endif //HEXAFORTH_DESKVM_H
