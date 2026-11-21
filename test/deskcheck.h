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
#include <stddef.h>
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
// Its memory as it now is, from cell 0, and how many bytes that is: for its
// dictionary, which the Words window lists in the order it is searched in.
// It is the machine's until it is next asked for.
const uint8_t* desk_memory(size_t* bytes);

// What is so of the machine.
typedef struct {
    bool names;                 // its disk has the tables of names
    const char* picture;        // the picture that is on its disk, as a file
    const char* out;            // where the screens are kept
} desk;

// The check and the action step being run, for failure messages when the
// machine stops where it was not sent.
extern const char* desk_checking;
extern const char* desk_doing;

// Every check.  Returns how many failed.
int desk_checks(const desk* machine);

// For what works a desktop and is not checking it (bench/profile.c): the
// desktop is started, and found to have its windows where forth/desk.fs
// puts them, or not; and something is done, in the words the checks say
// what they do in (does(), in deskcheck.c).
bool desk_begins(const desk* machine);
void desk_does(const char* said);

// For what only watches a desktop: is a row of a window's text, on a
// screen the machine gave, this?  It is not while the windows are not up.
// The window's text is named as the checks name it (GRIDS in deskcheck.c).
bool desk_sees(const desk* machine, const uint8_t* screen, const char* text_of, int row, const char* text);

#endif //HEXAFORTH_DESKCHECK_H
