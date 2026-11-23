//
// The interface between the desktop's checks and the machine they run on.
//
// The checks are a table (deskcheck.c): the actions to perform with the
// keys and the pointer, and what must then be on the screen.  Any machine
// that can perform those actions and show its screen can run them, by
// implementing the functions declared here.  desktop_test.c runs them on
// the reference VM, at any cell size.
//

#ifndef HEXAFORTH_DESKCHECK_H
#define HEXAFORTH_DESKCHECK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// The functions a machine implements.  A screen is one byte per pixel,
// DISP_W by DISP_H of them, 1 for a black pixel.  desk_screen() returns
// the screen as it is once the machine has processed everything sent
// before the call.
const char* desk_boot(void);                        // starts the desktop, and returns what the console printed as it came up
void desk_type(const char* text);                   // presses and releases the key for each character
void desk_key(const char* name, const char* with);  // presses a key given by name or character, holding `with` if that is not NULL
void desk_pointer(int x, int y);                    // moves the pointer to a position on the screen
void desk_button(bool down);                        // presses or releases the pointer's button
void desk_turns(int turns);                         // lets the desktop run for that many turns of its event loop
const uint8_t* desk_screen(void);
void desk_block(long n, int count, uint8_t* bytes); // reads blocks of the disk as it is now
// Returns the machine's memory as it is now, from cell 0, and stores its
// size in `*bytes`.  The checks read the dictionary from it, because the
// Words window lists the dictionary in the order it is searched.  The
// pointer is valid until the next call.
const uint8_t* desk_memory(size_t* bytes);

// Facts about the machine that the checks need.
typedef struct {
    bool names;                 // true if its disk has the tables of names
    const char* picture;        // the path of a file with the picture that is on its disk
    const char* out;            // the directory where the screens are saved
} desk;

// The check being run and the action being performed, for the failure
// message when the machine stops somewhere unexpected.
extern const char* desk_checking;
extern const char* desk_doing;

// Runs every check.  Returns how many failed.
int desk_checks(const desk* machine);

// For a program that drives a desktop without checking it
// (bench/profile.c).  desk_begins() starts the desktop, and returns true
// if its windows are where forth/desk.fs puts them.  desk_does() performs
// actions, written in the notation the checks use (does(), in
// deskcheck.c).
bool desk_begins(const desk* machine);
void desk_does(const char* said);

// For a program that only watches a desktop.  Returns true if a row of a
// window's text, on a screen the machine returned, equals `text`.  It
// returns false while the windows are not up yet.  `text_of` names the
// window's text the way the checks name it (GRIDS in deskcheck.c).
bool desk_sees(const desk* machine, const uint8_t* screen, const char* text_of, int row, const char* text);

#endif //HEXAFORTH_DESKCHECK_H
