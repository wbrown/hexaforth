//
// What the desktop is checked for, whatever machine it is running on.
//
// The checks are a table (deskcheck.c): what is done with the keys and the
// pointer, and what is then to be on the screen.  A machine they are run
// on is what can do those things and show its screen, and says so here:
// desktop_test.c is the reference VM, with cells of any size.
//

#ifndef HEXAFORTH_DESKCHECK_H
#define HEXAFORTH_DESKCHECK_H

#include <stdbool.h>
#include <stdint.h>

// What the machine does.  A screen is a byte a pixel, DISP_W by DISP_H of
// them, 1 if the pixel is black; it is the screen as it is when everything
// done before it was asked for has been taken in.
const char* desk_boot(void);                        // what the console said as the desktop came up
void desk_type(const char* text);                   // each character's key, down and up
void desk_key(const char* name, const char* with);  // a key by its name or its character; held with another, or NULL
void desk_pointer(int x, int y);                    // the pointer goes to a place of the screen
void desk_button(bool down);                        // its button goes down or up
void desk_turns(int turns);                         // the desktop is given time: so many turns of its loop
const uint8_t* desk_screen(void);
void desk_block(long n, int count, uint8_t* bytes); // blocks of the disk, as it now is

// What is so of the machine.
typedef struct {
    const char* newest[2];      // the two newest words of its dictionary as the desktop starts,
    const char* ending;         // and how the definition of the first of them ends
    bool names;                 // its disk has the tables of names
    const char* picture;        // the picture that is on its disk, as a file
    const char* out;            // where the screens are kept
} desk;

// Every check.  Returns how many failed.
int desk_checks(const desk* machine);

#endif //HEXAFORTH_DESKCHECK_H
