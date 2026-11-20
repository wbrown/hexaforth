//
// The desktop on the reference VM, as the machine that deskcheck.h says
// one is: for the desktop's test, and for whatever else works the desktop
// (bench/profile.c).
//
// The machine is the nucleus on vm.c (nucleus.h), which is built into
// whatever has this in it, for a machine of 64 bits, 32 or 16.
//
//   the disk       the VM's own, whose driver the kernel's ROM finds when
//                  the machine starts: a copy of the disk it is given, for
//                  the editor and HexaPaint to write to.
//   keys, pointer  the VM has none, so this is them: a word in 'poll,
//                  typed at the prompt, which the event loop runs each time
//                  round.  It takes one thing from the console: a key going
//                  down or up, by its Linux code, as a virtio keyboard
//                  gives them; the pointer's place; its button; nothing;
//                  or to stop.  It hands events to on-event, as any driver
//                  does.
//   the screen     the display in the VM's memory.  When the screen is
//                  asked for, the last thing sent is to stop: the machine
//                  stops in its event loop when it has taken everything
//                  sent before, and is gone on with from there.
//
// The desktop takes exactly one turn of its loop for each thing sent, so
// what is done is the same every time.  Which key is which character is
// asked of the machine, which has the map.
//

#ifndef HEXAFORTH_DESKVM_H
#define HEXAFORTH_DESKVM_H

#include "deskcheck.h"

// Start the machine from an image, with a copy of a disk made in `out`,
// and have it ready to start its desktop: it does when it is first asked
// what its console said (desk_boot).  `pictures` is its second disk, which
// has the picture (mkdisk --disk) and is not written to, and `picture` is
// that picture as a file.  What is so of the machine is what it returns.
// With `rom_only` the machine has nothing in memory but its ROM, and starts
// from the first disk, which is then one it can start from: it reads the
// rest of the image from it, and compiles what its block 1 lists.
desk deskvm_start(const char* image, const char* disk, const char* pictures, const char* picture, const char* out,
                  bool names, bool rom_only);

// How many turns its event loop has taken: the things sent that it has
// taken.
extern long deskvm_turns;

#endif //HEXAFORTH_DESKVM_H
