//
// The desktop's checks, which run on any machine that implements
// deskcheck.h.
//
// A check is a row of the table below: the actions to perform with the
// keys and the pointer, and what must then be on the screen.  The actions
// are written in a small notation of this file's own (does(), below).
// Most of what is checked is text.  The windows that hold text on a grid
// are read back one character cell at a time, by matching each cell
// against the glyphs of the fixed-width font, which is read from the Forth
// source that defines it (tables.h).  Everything else has a function of
// its own:
//
//   the picture, compared with a dither of the same file done here
//   (dither.c), and with the same file stored as gray and drawn through a
//   mask as its menu requests (gray.h)
//   HexaPaint's canvas, compared with what its tools should draw, drawn
//   here one pixel at a time
//   the menu, a window's title, and the areas that must not change
//
// The checks run in order, each starting from the state the previous one
// left the desktop in.  The test tracks where it expects each window to be
// and which window is in front, as the checks move them, and works out
// every position it clicks or examines from that.  The windows' starting
// positions are read from forth/desk.fs.
//
// The screen at each check is saved to a file, for anyone who wants to
// look at it.
//

#include <ctype.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../vm_constants.h"
#include "../vm_opcodes.h"
#include "../bench/tiff.h"
#include "../util/room.h"
#include "deskcheck.h"
#include "dither.h"
#include "gray.h"
#include "tables.h"
#include "../util/bluenoise.h"

#define W       DISP_W
#define H       DISP_H
#define SCREEN  ((size_t)W * H)
#define ROWS    16              // the height in pixels of a line of a window's text, and of a glyph
#define COLS    64              // the most columns a window's text has
#define MOST_LINES (H / ROWS)   // the most lines of text a window can show: the number that fit on the screen
#define SCREEN_ROOM ((size_t)DISP_W_MOST * DISP_H_MOST)     // enough pixels for the screen of any machine
#define LINES_ROOM  (DISP_H_MOST / ROWS)                    // enough lines for the screen of any machine

typedef struct {
    const char* grid;           // which window's text to read (GRIDS, below)
    int row;                    // which row of it
    char how;                   // how the row must match: = equals the text; ^ starts with it; $ ends with it; ~ contains it
    const char* text;           // the text; %1 and %2 stand for the second and third words of the machine's dictionary,
} seen;                         // and %b for the first line of block 1 of its disk

enum { ALWAYS, NAMES, NO_NAMES };       // when a check applies: always, only with the tables of names on the disk, or only without

typedef struct {
    const char* label;
    const char* does;           // the actions to perform first, in the notation of does()
    const char* shot;           // the name the screen is saved under afterwards, or NULL
    seen sees[3];               // the text that must then be on the screen
    bool (*also)(char* why);    // a further test, or NULL; on failure it writes the reason to `why`
    int when;
} check;

#define WHY 400                 // the size of the buffer for a check's reason for failing

static bool came_up(char* why), saved(char* why), dithered(char* why), moved(char* why), bold(char* why);
static bool fused_told(char* why), a_number(char* why), word_told(char* why), box_gone(char* why);
static bool scrolled_down(char* why), words_marked(char* why), more_above(char* why), more_both(char* why);
static bool one_cursor(char* why), prints_behind(char* why), keep(char* why), others_same(char* why);
static bool remember(char* why), lights_right(char* why);
static bool last_dragged(char* why);
static bool sees_all(char* why), memory(char* why), menu_down(char* why), line_shown(char* why);
static bool sees_label(char* why), sees_shared(char* why), data_place(char* why), see_blocks(char* why);
static bool calc_picked(char* why), words_held(char* why), menu_whole(char* why), menu_away(char* why);
static bool menu_gone(char* why), forth_closed(char* why), words_reopened(char* why);
static bool paint_front(char* why), canvas_right(char* why), shape_shown(char* why);
static bool on_disk(char* why), from_disk(char* why), was_found(char* why), went_on(char* why);
static bool as_picked(char* why), picture_marked(char* why), view_on_disk(char* why), outlined(char* why);
static int picture_title(void), picture_line(const char* name);
static uint64_t disk_cell(const uint8_t* bytes, size_t n);
static void picture_picked(int line), picture_made(void);

static check CHECKS[] = {
    {"the desktop comes up", "", "1-desktop", .also = came_up},
    // The machine has two disks, each a floppy, and the bar has a light for each.
    {"the bar has a light for each of the machine's two disks, and for no more", "", NULL, .also = lights_right},

    // The Forth window.  Its text is inset 2 pixels from the edge of its content.
    {"the Forth window opens with a prompt", "'2 3 + .\r' ': sq dup * ; 12 sq .\r' 'nosuch\r'", "2-forth",
     {{"forth", 0, '=', "ok"}}},
    {"the Forth window evaluates", "", NULL, {{"forth", 1, '=', "2 3 + ."}, {"forth", 2, '=', "5  ok"}}},
    {"the Forth window compiles", "", NULL, {{"forth", 4, '=', "144  ok"}}},
    {"the Forth window reports errors", "", NULL, {{"forth", 6, '^', "? -13"}}},

    // The calculator.  A click on its title brings it to the front.  Then
    // keys are typed and two of its buttons are clicked, 7 and *.
    {"the calculator computes (12+34)*7", "front:calc '12\r34+' click:calc+17+108 click:calc+119+132",
     "3-calculator", {{"calc", 3, '~', "322"}}},

    // The editor.  It is brought to the front, text is typed on the second
    // line of block 1, and the block is saved.
    {"the editor shows block 1", "front:editor ^s", "4-editor", {{"editor", 0, '=', "%b"}}},
    {"the editor saves to the disk", "down '( edited )' ^s", "5-edited", .also = saved},

    // The picture.  The viewer is brought to the front, and the picture is
    // compared with a dither done here.
    {"the viewer shows the picture as it is dithered a pixel at a time", "front:viewer away", "6-viewer",
     .also = dithered},
    // Once dithered, the bitmap is written to the disk and painted from there.
    {"and the dithered bitmap is on the disk", "", NULL, .also = view_on_disk},
    // While a window is dragged, a gray outline of its frame follows the
    // pointer, and the window stays where it was until the button goes up.
    {"while a window is dragged, a gray outline of it follows the pointer", "grab:viewer+40+42", "7-dragging",
     .also = outlined},
    {"a window moves when it is dragged by its title", "drop away", "7-dragged", .also = moved},
    // While the viewer is in front, its menu is in the bar after the
    // windows' menu, with a mark beside the way the picture is currently
    // drawn.  Picking a line makes the viewer's task remake the picture, a
    // part on each of its turns, while the desktop keeps running.  The
    // checks pick, in order: stored as gray and drawn through blue noise;
    // darker; through Bayer's mask; coarser; lighter and finer again; and
    // dithered again.  Each result is compared with the same picture made
    // here one pixel at a time (gray.h).
    {"the picture's menu is in the bar, and marks how the picture is drawn", "pmenu away", "7-menu",
     .also = picture_marked},
    {"a line of it has the picture kept as gray and drawn through blue noise",
     "backspace picture:blue away", "7-blue", .also = as_picked},
    {"and darker", "picture:darker away", "7-darker", .also = as_picked},
    {"and through Bayer's mask", "picture:ordered away", "7-ordered", .also = as_picked},
    {"and coarser", "picture:coarser away", "7-coarser", .also = as_picked},
    {"and lighter and finer again", "picture:lighter picture:finer away", "7-finer", .also = as_picked},
    {"and the mark is by the line picked", "pmenu away", "7-marked", .also = picture_marked},
    {"and dithered again", "backspace picture:atkinson away", "7-dithered", .also = as_picked},

    // ^W brings the window at the back to the front, and is repeated until
    // the Words window is in front.  Its list is inset 2 pixels from the
    // edge of its content, and the definition 112 pixels.  The list is the
    // dictionary in the order it is searched.  sq, defined in the Forth
    // window above, is the newest word and so is listed first.  The two
    // after it are whatever the machine's dictionary has next (%1, %2),
    // which the test reads from the machine.  A click on a name shows that
    // word's definition.
    {"the Words window lists the dictionary", "cycle:words click:words+10+8 away", "8-words",
     {{"names", 0, '=', "sq"}, {"names", 1, '=', "%1"}, {"names", 2, '=', "%2"}}},
    // The body of a definition is indented three columns for each level.
    {"the Words window decompiles", "", NULL,
     {{"source", 0, '=', ": sq"}, {"source", 1, '=', "   dup *"}, {"source", 2, '=', ";"}}, words_marked},
    {"a word in a definition is a link", "click:words+140+24 away", "9-link",
     {{"source", 0, '=', ": dup"}, {"source", 1, '=', "   dup"}, {"source", 2, '=', ";"}}},
    {"Backspace goes back", "backspace", "10-back",
     {{"source", 0, '=', ": sq"}, {"source", 1, '=', "   dup *"}, {"source", 2, '=', ";"}}},
    {"Down moves to the next word", "down", "10-down", {{"source", 0, '=', ": %1"}}},
    // A definition longer than the window.  PgDn scrolls it down by one
    // line less than the window's height, or as far as still fills the
    // window, and PgUp scrolls it back to where it was.  The last row ends
    // with [PGDN] while more of the definition is below, and the first row
    // ends with [PGUP] while more is above.  words-key, which has a few
    // lines for each key the Words window handles, is longer than the
    // window on either screen size.
    {"a definition longer than the window is shown from its start", "find:words-key name:words-key",
     "10-long", {{"source", 0, '=', ": words-key"}}, remember},
    {"and says that more of it is below", "", NULL, .also = words_marked},
    {"PgDn scrolls it", "pgdn", "10-pgdn", .also = scrolled_down},
    {"and says that more of it is above", "", NULL, .also = words_marked},
    {"and PgUp scrolls it back", "pgup", "10-pgup", .also = box_gone},
    // A word can have more than one header: 1+ and cell+ share one body
    // (nuc.fs).  The name that is clicked is the one selected, so Down
    // from it shows the word listed after that name, not the word listed
    // after the other name.
    {"a word of two names is chosen by the one clicked, and Down goes on from it", "find:cell+ name:cell+ down",
     "10-two-names", .also = went_on},
    // Two words of the dither, each reached by paging the list.
    // dither-row has fused instructions in it.  dither-start has a
    // variable with no header, and a plain word that is one instruction.
    {"the list is paged to a word, wherever in the dictionary it is", "find:dither-row", "10-list", .also = was_found},
    // Words shown in bold are together one instruction.  With the pointer
    // on them, a box beside them shows that instruction: its value and its
    // fields.
    {"a fused instruction is shown as its words, in bold", "name:dither-row", "10-fused",
     {{"source", 0, '=', ": dither-row"}}, bold},
    {"the pointer on a fused instruction shows the machine's form of it", "", NULL, .also = fused_told},
    {"a headerless variable is shown by its label", "find:dither-start name:dither-start", "10-label",
     {{"source", 0, '=', ": dither-start"}, {"source", 1, '$', " img !"}}, remember, NAMES},
    {"a headerless variable is shown as its number, with no names on the disk", "find:dither-start name:dither-start",
     "10-label", {{"source", 0, '=', ": dither-start"}}, a_number, NO_NAMES},
    {"and on a word that is one instruction", "", NULL, .also = word_told},
    {"and the box goes when the pointer does", "away", "10-hover-off", .also = box_gone},

    // ^W again, until the Forth window is in front.
    {"^W cycles the windows, and the Forth window's stack is as it was",
     "cycle:forth 'depth . 7 8\r' '+ . depth .\r'", "11-cycled",
     {{"forth", 8, '=', "0  ok"}, {"forth", 10, '=', "15 0  ok"}}},
    // The Forth window is filled until it scrolls.  A scroll moves the
    // text on the screen and does not redraw it, so the check reads what
    // is actually there: the last lines, the first line, and that the only
    // solid cell is the cursor.  The first row now ends with [PGUP],
    // because more text is above.
    {"the Forth window scrolls", "'0 .\r' '1 .\r' '2 .\r' '3 .\r' '4 .\r' '5 .\r' '6 .\r' '7 .\r'", "11-scrolled",
     {{"forth", 13, '=', "7 ."}, {"forth", 14, '=', "7  ok"}, {"forth", 0, '^', "0  ok"}}, one_cursor},
    {"and says that more of its text is above", "", NULL, .also = more_above},
    // The Forth window is a task of its own.  The check starts a word in
    // it that waits, yielding its turns, until another window comes to the
    // front, and then prints twenty lines.  Then the viewer is brought to
    // the front.  Because the viewer was dragged earlier, it covers the
    // right side of the Forth window, which now scrolls behind it.  The
    // part of the picture that lies over the Forth window must not change.
    // (The word has no name, so that the Words window's list stays as it
    // is.)
    {"a window behind can print and scroll",
     "':noname top begin pause dup top <> until drop 20 0 do i . cr loop ;\r' 'execute\r' front:viewer away",
     "11-behind", .also = prints_behind},
    {"the Forth window runs as a task", "front:forth", "11-task",
     {{"forth", 12, '=', "18"}, {"forth", 13, '=', "19"}, {"forth", 14, '=', " ok"}}, keep},
    // Printed text is kept as lines of their own lengths, and when the
    // room for it is full the oldest lines are dropped.  Thirty lines of
    // forty-odd characters are more than the window keeps on any machine.
    // Forty short lines are not.  After those, PgUp shows the lines one
    // line less than a window's height further back, with [PGDN] at the
    // end of the last row, and PgDn shows the last lines again.
    {"the Forth window keeps its last lines",
     "':noname 30 0 do i . 40 0 do 42 emit loop cr loop ;\r' 'execute\r'", "11-rolled",
     {{"forth", 12, '^', "28 ****"}, {"forth", 13, '^', "29 ****"}, {"forth", 14, '=', " ok"}}, more_above},
    {"PgUp shows the lines before", "':noname 40 0 do i . cr loop ;\r' 'execute\r' pgup", "11-back",
     {{"forth", 0, '^', "11"}, {"forth", 13, '^', "24"}, {"forth", 14, '^', "25"}}, more_both},
    {"and PgDn the last again", "pgdn", "11-forth",
     {{"forth", 13, '^', "39"}, {"forth", 14, '=', " ok"}}, more_above},
    // The whole screen is invalidated from the prompt.  Every window is
    // repainted, each only where it is visible, so nothing outside the
    // Forth window may change except the disk light in the bar.
    {"a full repaint leaves the other windows as they were", "'0 0 %W %H inval-rect\r'", "12-repainted",
     .also = others_same},

    // The decompiler's tables of names are on the disk.  They are read
    // into a buffer, which is kept only if the tables belong to this
    // image, and which is freed when they are unloaded.  With no tables on
    // the disk, names? is false after loading.
    {"the tables of names load from the disk and unload", "'names-load names? . names-unload names? .\r'",
     "13-names", {{"forth", 14, '=', "-1 0  ok"}}, .when = NAMES},
    {"with no tables of names on the disk, none are loaded", "'names-load names? . names-unload names? .\r'",
     "13-names", {{"forth", 14, '=', "0 0  ok"}}, .when = NO_NAMES},
    // `see` keeps the names loaded for as long as it runs.  Its output
    // scrolls the window, and a scroll needs memory of its own while it
    // lasts, so there must be room for both.  The word decompiled is long
    // enough to scroll the window.  Near its end it calls a word with no
    // header, which is shown by name only if the names are loaded:
    // words-key ends with wall.
    {"see prints a word longer than the window, with the names in memory", "'see words-key\r'", "13-see",
     {{"forth", 14, '$', " ok"}}, sees_all},
    // This check prints the next cell that would be allocated, first with
    // the names unloaded and then with them loaded.  `see` scrolled the
    // window while it had the names loaded, and freed them afterwards.  If
    // it had not freed them, they would still be loaded, and loading them
    // here would take no memory.
    {"see gives the names' memory back, though the window scrolled while it had them",
     "'0 halloc . names-load 0 halloc . names-unload\r'", "13-memory", .also = memory, .when = NAMES},
    {"with no tables of names on the disk, loading them takes no memory",
     "'0 halloc . names-load 0 halloc . names-unload\r'", "13-memory", .also = memory, .when = NO_NAMES},
    // `see` finds a word with no header by its label, which is on the
    // disk.  For a label on data it prints the data's address.
    {"see finds a word with no header by its label", "'see in-port\r'", "13-see-label", .also = sees_label,
     .when = NAMES},
    {"and a label on data by where the data is", "'see img\r'", "13-see-data", .also = data_place, .when = NAMES},
    // A word with no instructions is another name for the word it falls
    // through into: `again` is one, for `code,`, which has no header.
    // `until` ends with that word, and `see` shows it by its label.
    {"see says a word by its label where a header's word runs on into it", "'see until\r'", "13-see-shared",
     .also = sees_shared, .when = NAMES},
    // The number of blocks `see` reads from the disk to show a word, which
    // blk-moved counts.
    {"see reads the disk for the names", "'blk-moved @ see words-key blk-moved @ swap - .\r'", "13-see-blocks",
     .also = see_blocks, .when = NAMES},
    {"with no tables of names on the disk, a word with no header is not found", "'see in-port\r'",
     "13-see-label", {{"forth", 14, '^', "? -13"}}, .when = NO_NAMES},
    // The picture was read and dithered by a task of its own, one row on
    // each turn, and that task removed itself from the ring when the last
    // row was done.  The check counts the tasks in the ring, which must be
    // two: the desktop's and the Forth window's.
    {"the picture's task has left the ring",
     "':noname up @ 0 begin 1+ swap @ swap over up @ = until nip ;\r' 'execute .\r'", "14-tasks",
     {{"forth", 14, '=', "2  ok"}}},

    // The title in the bar is a menu of the windows.  They are listed in
    // the order they were created, newest first, on lines 16 pixels high
    // starting just under the bar.  A window's line is bold if the window
    // is open and plain if it is closed.  A click on the title opens the
    // menu, which stays open.  A click on a line brings that window to the
    // front, reopening it if it was closed, and closes the menu.
    {"the title is a menu of the windows, in bold while they are open", "menu away", "15-menu", .also = menu_down},
    {"the line the pointer is on is shown so", "over:words", "15-menu-line", .also = line_shown},
    {"a click on a line brings its window to the front", "pick:calc away", "15-picked", .also = calc_picked},
    // The same by dragging: the button is pressed on the title and released on a line.
    {"and so does letting the button go on one", "hold:words away", "15-dragged", .also = words_held},
    // A window under the open menu must not draw over it.  The Forth
    // window is brought to the front and given a word that waits until the
    // menu is open and then prints twenty lines, so that the window scrolls
    // under the menu.  The menu must stay intact, and the text beside it
    // must have moved.  When a press outside the menu closes it, the area
    // that was under it is repainted with its current contents.
    {"a window under the menu prints and scrolls without drawing on it",
     "front:forth ':noname begin pause menu-open @ until 20 0 do i . cr loop ;\r' 'execute\r' menu away",
     "15-over", {{"forth", 12, '=', "18"}, {"forth", 13, '=', "19"}, {"forth", 14, '=', " ok"}}, menu_whole},
    {"a press off the menu takes it away, and what was under it is painted", "click:forth-40-40 away", "15-away",
     {{"forth", 0, '^', "6"}, {"forth", 1, '=', "7"}, {"forth", 2, '=', "8"}}, menu_away},
    {"and so does a key", "menu backspace away", "15-key", .also = menu_gone},
    // The x at the right of a window's title bar closes the window when
    // the button is released over it.  ^Q closes the window in front.  In
    // the menu a closed window is plain and an open one is bold, and
    // picking a closed window opens it again.
    {"pressing on the x and sliding off does not close the window", "slide:forth away", "15-slide",
     .also = menu_away},
    {"the x in a window's title bar closes it, leaving the one behind in front", "close:forth away", "15-closed",
     .also = forth_closed},
    {"^Q closes the window in front, and closed windows are plain in the menu", "^q menu away", "15-closed-menu",
     .also = menu_down},
    {"picking a closed window from the menu opens it again", "pick:words menu pick:forth away", "15-reopened",
     {{"forth", 14, '=', " ok"}}, words_reopened},

    // HexaPaint.  It is at the back, and is brought to the front from the
    // menu.  Everything drawn in it with the pointer is also drawn here,
    // one pixel at a time, and every pixel of the canvas on the screen is
    // compared with that.  It starts with the pencil selected, in black.
    {"HexaPaint comes to the front from the menu, with nothing drawn", "menu pick:paint away", "16-paint",
     .also = paint_front},
    {"the pencil draws where the pointer is pulled", "pull:10,10,60,40 away", "16-pencil", .also = canvas_right},
    // The brush, in gray; a rectangle filled with bricks; an oval; an oval
    // filled with light gray; a line; the eraser; and a rectangle, pulled
    // up and to the left.
    {"the brush, the shapes, the line and the eraser draw as they do a pixel at a time",
     "tool:1 pattern:3 pull:20,60,100,70 tool:5 pattern:5 pull:120,20,200,90 tool:6 pull:30,80,110,140 "
     "tool:7 pattern:4 pull:130,100,220,145 tool:3 pull:5,145,230,5 tool:2 pull:60,30,150,120 "
     "tool:4 pull:200,60,150,10 away", "16-drawn", .also = canvas_right},
    // A shape is shown while it is being pulled out, and is not part of
    // the canvas until the button is released.
    {"a shape is shown as it is pulled out", "tool:6 press:40,20,100,50", "16-pulling", .also = shape_shown},
    {"and is drawn when the button goes up", "release away", "16-let-go", .also = canvas_right},
    // The canvas is stored on the disk, and is in memory only while
    // HexaPaint is in front.  When another window comes to the front, the
    // disk must have every pixel that was drawn.  That window is then
    // dragged over the canvas and back, so that part of the canvas is
    // repainted behind it.  The repaint must come from the disk, because
    // the canvas is no longer in memory.
    {"the canvas is written to the disk when another window comes to the front", "front:calc", NULL,
     .also = on_disk},
    {"behind another window it is painted from the disk", "", "16-behind", .also = from_disk},
    {"what was drawn is there when the window has been under another", "menu pick:paint away", "16-again",
     .also = paint_front},
    // The buffer for the names uses memory while the Words window is
    // being drawn, and the canvas uses memory while HexaPaint is in front.
    // The Words window, brought to the front, must still show its names.
    {"there is room for the canvas and the tables of names", "menu pick:words away", "17-names",
     {{"source", 1, '$', " img !"}}, .when = NAMES},

    // A window that goes behind another, or is closed, while the button is
    // held down in it must stop receiving the pointer.  HexaPaint frees its
    // canvas when another window comes to the front.  So if its pencil is
    // held down when ^W or ^Q takes it from the front, and the pointer then
    // moves, a bug here would draw where the canvas was: at address 0.
    // Likewise, if the last window is being dragged by its title when ^Q
    // closes it, a bug would move a window at address 0 when the button is
    // released.  The test's machine stops at any store into the kernel
    // (ROM_BYTES, cmake/machines.cmake), so either bug stops it.
    {"the pencil held down when ^W brings another window in front, and moved",
     "menu pick:paint away tool:0 press:10,10,60,40 ^w point:300,300 release away", "18-held-w"},
    {"the pencil held down when ^Q closes HexaPaint, and moved",
     "menu pick:paint away tool:0 press:10,10,60,40 ^q point:300,300 release away", "18-held-q"},
    {"the last window dragged by its title when ^Q closes it, and let go", "", "18-dragged-q",
     .also = last_dragged},
    // With every window closed, the Forth window is reopened from the menu
    // and creates three windows, placed where they do not cover it.  That
    // makes nine windows created, four of them open, and it reuses the
    // memory freed when HexaPaint was closed.  The Forth window, in front
    // again, must still respond.
    {"three windows made from the Forth window opened again",
     "menu pick:forth away ':noname drop ; constant dr  :noname 2drop ; constant ky\r' "
     "': mk 420 290 80 40 t-words dr ky 0 window drop ; mk mk mk\r' front:forth '1 2 + .\r'", "18-windows",
     {{"forth", 14, '=', "3  ok"}}},
    {NULL}};

// ---- the machine, and its screen

static const desk* machine;     // the facts about the machine the checks run on
static const char* boot;        // what its console printed as the desktop came up
static const uint8_t* shot;     // its screen at the last look()
static char* shot_path;         // the file that screen was saved to, if look() was given a name
static uint8_t* kept;           // a saved copy of a screen, to compare a later screen with
const char* desk_checking;      // the check being run
const char* desk_doing;         // the action of does() being performed

static bool on_screen(int x, int y) { return(x >= 0 && x < W && y >= 0 && y < H); }
static bool black(int x, int y) { return(on_screen(x, y) && shot[(size_t)y * W + x]); }
static int low(int a, int b) { return(a < b ? a : b); }
static int high(int a, int b) { return(a > b ? a : b); }

// Reads the screen, after giving the desktop time to finish what it was
// asked to do.  If a name is given, the screen is also saved to a file as
// a binary PBM.
static void look(const char* name) {
    desk_turns(12);
    shot = desk_screen();
    free(shot_path);
    shot_path = NULL;
    if (!name) return;
    static const char form[] = "%s/%s.pbm";
    shot_path = malloc(strlen(machine->out) + strlen(name) + sizeof(form));
    if (shot_path == NULL) {
        fprintf(stderr, "there is no room for the name of a screen's file\n");
        exit(EXIT_FAILURE);
    }
    sprintf(shot_path, form, machine->out, name);
    FILE* out = fopen(shot_path, "wb");
    if (out == NULL) {
        perror(shot_path);
        exit(EXIT_FAILURE);
    }
    fprintf(out, "P4\n%d %d\n", W, H);
    for(int y=0; y < H; y++) {
        for(int x=0; x < W; x += 8) {
            int byte = 0;
            for(int bit=0; bit < 8; bit++) byte |= black(x + bit, y) << (7 - bit);
            fputc(byte, out);
        }
    }
    fclose(out);
}

static void click(int x, int y) {
    desk_pointer(x, y);
    desk_button(true);
    desk_button(false);
    desk_turns(6);
}

static void drag(int x0, int y0, int x1, int y1) {
    desk_pointer(x0, y0);
    desk_button(true);
    desk_pointer(x1, y1);
    desk_button(false);
    desk_turns(6);
}

static void away(void) {        // moves the pointer to a position that is over nothing
    desk_pointer(W - 12, 24);
}

// ---- text on the screen

static font mono, sans;
static uint8_t glyphs[128][ROWS];       // the fixed-width font: one byte for each row of each character

// Reads the character cell at a screen position into sixteen bytes, one
// per row.  A bit is set where the pixel is black or is off the screen.
static void cell_at_screen(int x, int y, uint8_t* cell) {
    for(int row=0; row < ROWS; row++) {
        cell[row] = 0;
        for(int bit=0; bit < 8; bit++) {
            if (!on_screen(x + bit, y + row) || black(x + bit, y + row)) cell[row] |= 0x80 >> bit;
        }
    }
}

// Returns one row of a glyph as it is drawn in a style: p is plain; b is
// bold, where each pixel is drawn together with the pixel to its right;
// i is italic, where the top half is shifted one pixel right.
static uint8_t styled(int c, int row, char style) {
    uint8_t is = glyphs[c][row];
    return(style == 'b' ? is | is >> 1 : style == 'i' && row < ROWS / 2 ? is >> 1 : is);
}

// Returns the character whose glyph, in a style, matches a cell over its
// first `rows` rows, or 0 if none matches.  If two characters match, it
// returns the one with the higher code.
static int matched(const uint8_t* cell, char style, int rows) {
    for(int c=126; c >= 32; c--) {
        int row = 0;
        while (row < rows && styled(c, row, style) == cell[row]) row++;
        if (row == rows) return(c);
    }
    return(0);
}

// Returns the character in the cell at a screen position.  It tries the
// plain glyph, then the glyph in white on black.  Then, ignoring the
// bottom row, it tries the glyph underlined (a link), in bold (a fused
// word) and in italic (a label).  Returns ? if nothing matches.
static int read_cell(int x, int y) {
    uint8_t cell[ROWS], inverse[ROWS];
    cell_at_screen(x, y, cell);
    for(int row=0; row < ROWS; row++) inverse[row] = ~cell[row];
    int c = matched(cell, 'p', ROWS);
    if (!c) c = matched(inverse, 'p', ROWS);
    if (!c) c = matched(cell, 'p', ROWS - 1);
    if (!c) c = matched(cell, 'b', ROWS - 1);
    if (!c) c = matched(cell, 'i', ROWS - 1);
    return(c ? c : '?');
}

// Reads a row of `cols` character cells starting at a screen position
// into `text`, without trailing spaces.
static void read_row(int x, int y, int cols, char* text) {
    for(int c=0; c < cols; c++) text[c] = (char)read_cell(x + c * 8, y);
    while (cols && text[cols - 1] == ' ') cols--;
    text[cols] = '\0';
}

// Returns true if text is on the screen at a position, in a font.  This
// is for text that is not on a grid.  Every pixel of every glyph must
// match the glyph, drawn in bold if `is_bold`, and with black and white
// swapped if `inverse`.
static bool shows_text(const char* text, int x, int y, const font* f, bool is_bold, bool inverse) {
    for(; *text; text++) {
        int width, start = glyph_of(f, (unsigned char)*text, &width);
        for(int row=0; row < f->rows; row++) {
            for(int k=0; k < width; k++) {
                if (!on_screen(x + k, y + row)) return(false);
                bool ink = strike_pixel(f, start + k, row) || (is_bold && k > 0 && strike_pixel(f, start + k - 1, row));
                if (black(x + k, y + row) != (ink != inverse)) return(false);
            }
        }
        x += width;
    }
    return(true);
}

static bool shows(const char* text, int x, int y, const font* f, bool inverse) {
    return(shows_text(text, x, y, f, false, inverse));
}

// Checks the disk lights (wm.fs).  There is a floppy at the right end of
// the bar for each disk the machine has.  The first disk's is rightmost,
// and each later disk's is one light's width further left; that width is
// the first number of disk-corner.  A floppy is one of the first four
// glyphs of the font spin, which are a quarter turn apart.  It is drawn
// ten pixels in from the left edge of its light and two pixels down.  The
// machine has two disks: the one it is given or starts from, and the
// pictures' disk.  So there must be a floppy for each of those, and no
// black pixel where a third disk's floppy would be.  (A machine once
// showed sixteen lights, and no test noticed.)
static bool lights_right(char* why) {
    enum { IN = 10, DOWN = 2, TURNS = 4, DISKS = 2 };
    static font spin;
    static int wide;
    if (!wide) {
        int64_t* corner;
        spin = font_named("spin");
        table("wm.fs", "disk-corner", &corner, 2);
        wide = (int)corner[0];
        free(corner);
    }
    bool shown[DISKS + 1];
    for(int d=0; d <= DISKS; d++) {
        int x = W - (d + 1) * wide + IN, width, ink = 0;
        shown[d] = false;
        for(int turn=0; turn < TURNS && !shown[d]; turn++) {
            int start = glyph_of(&spin, turn, &width), wrong = 0;
            for(int row=0; row < spin.rows; row++) {
                for(int k=0; k < width; k++) wrong += black(x + k, DOWN + row) != strike_pixel(&spin, start + k, row);
            }
            shown[d] = !wrong;
        }
        for(int row=0; row < spin.rows; row++) for(int k=0; k < width; k++) ink += black(x + k, DOWN + row);
        if (d == DISKS) shown[d] = ink != 0;
    }
    if (shown[0] && shown[1] && !shown[DISKS]) return(true);
    snprintf(why, WHY, "a floppy is shown for the first disk: %s; for the second: %s; and where a third's would be "
             "there is %s", shown[0] ? "yes" : "no", shown[1] ? "yes" : "no", shown[DISKS] ? "something" : "nothing");
    return(false);
}

// ---- where the windows are

// The windows, in the order desk.fs lists their positions; their titles;
// and the test's record of where each window should be, which windows are
// open, and their back-to-front order, kept up to date as the checks move,
// close and open them.
enum { VIEWER, EDITOR, WORDS, CALC, FORTH, PAINT, WINDOWS };
static const char* NAMED[] = { "viewer", "editor", "words", "calc", "forth", "paint" };
static const char* TITLES[] = { "Mandrill", "Editor", "Words", "Calculator", "Forth", "HexaPaint" };
static const int BACK_TO_FRONT[] = { PAINT, VIEWER, EDITOR, WORDS, CALC, FORTH };
static struct { int x, y, w, h; } rect[WINDOWS];
static int order[WINDOWS];      // the open windows, from the back to the front
static int open_wins;

// Returns the index of the window with a given name, and exits if there
// is none.
static int window(const char* name, size_t len) {
    for(int n=0; n < WINDOWS; n++) {
        if (strlen(NAMED[n]) == len && !strncmp(NAMED[n], name, len)) return(n);
    }
    fprintf(stderr, "deskcheck: there is no window called %.*s\n", (int)len, name);
    exit(EXIT_FAILURE);
}

// Return the top left corner of a window's content: the area inside its
// frame, under its title.
static int content_x(int n) { return(rect[n].x + 1); }
static int content_y(int n) { return(rect[n].y + 19); }

static int in_front(void) { return(order[open_wins - 1]); }

static bool is_open(int n) {
    for(int at=0; at < open_wins; at++) {
        if (order[at] == n) return(true);
    }
    return(false);
}

static void to_front(int n) {
    int at = 0;
    while (at < open_wins && order[at] != n) at++;
    if (at == open_wins) {
        order[open_wins++] = n;
        return;
    }
    for(; at < open_wins - 1; at++) order[at] = order[at + 1];
    order[open_wins - 1] = n;
}

static void close_win(int n) {
    int at = 0;
    while (at < open_wins && order[at] != n) at++;
    if (at == open_wins) return;
    open_wins--;
    for(; at < open_wins; at++) order[at] = order[at + 1];
}

// Returns the line of the menu a window is on.  The menu lists the
// windows in the order they were created, newest first.
static int menu_line(int n) {
    int at = 0;
    while (BACK_TO_FRONT[WINDOWS - 1 - at] != n) at++;
    return(at);
}

// Returns true if a screen position is under a window, or just outside
// its frame.
static bool under(int x, int y, int n) {
    return(x >= rect[n].x - 2 && x < rect[n].x + rect[n].w + 4 && y >= rect[n].y - 2 && y < rect[n].y + rect[n].h + 4);
}

// Finds a point on a window's title bar, left of its x, that no window in
// front covers, and from which the pointer can move a further dx and dy
// and stay on the screen.  The title bar is the 18 rows under the top of
// the frame, and a press on any of them is on the title.  The middle row
// is tried first, then the others.
static void title_of(int n, int dx, int dy, int* tx, int* ty) {
    static const int ROWS_OF_TITLE[] = { 8, 4, 12, 2, 16 };
    int at = 0;
    while (order[at] != n) at++;
    for(int r=0; r < (int)(sizeof(ROWS_OF_TITLE) / sizeof(ROWS_OF_TITLE[0])); r++) {
        *ty = rect[n].y + ROWS_OF_TITLE[r];
        for(*tx = rect[n].x + rect[n].w - 16; *tx > rect[n].x + 4; *tx -= 4) {
            if (!(*tx >= 0 && *tx < W - 8 - dx && *ty >= 20 && *ty < H - 12 - dy)) continue;
            bool covered = false;
            for(int ahead=at + 1; ahead < open_wins; ahead++) {
                int a = order[ahead];
                if (*tx >= rect[a].x && *tx < rect[a].x + rect[a].w && *ty >= rect[a].y && *ty < rect[a].y + rect[a].h) {
                    covered = true;
                }
            }
            if (!covered) return;
        }
    }
    fprintf(stderr, "deskcheck: no part of the title of %s can be seen\n", NAMED[n]);
    exit(EXIT_FAILURE);
}

// These return true if a window's title, and its x, are drawn the way
// they are for the window in front.
static bool title_shown(int n) {
    return(shows(TITLES[n], rect[n].x + 8, rect[n].y + 2, &sans, true));
}
static bool x_shown(int n) {
    return(rect[n].x + rect[n].w > W || shows("x", rect[n].x + rect[n].w - 12, rect[n].y + 2, &sans, true));
}
static bool shown_in_front(int n) {
    return(title_shown(n) && x_shown(n));
}

// Finds where the windows start.  desk.fs has two arrangements, one for a
// large screen and one for a small screen, each with a row of left, top,
// width and height for every window.  This machine uses the arrangement
// whose Forth window is at a position where the screen shows a window in
// front.  HexaPaint starts at the back, and the other windows are stacked
// from the back in the table's order.
static bool windows_start(void) {
    static const char* ARRANGED[] = { "big-desk", "small-desk" };
    for(int which=0; which < (int)(sizeof(ARRANGED) / sizeof(ARRANGED[0])); which++) {
        int64_t* at;
        table("desk.fs", ARRANGED[which], &at, 4 * WINDOWS);    // each window's corner, and its width and height
        for(int n=0; n < WINDOWS; n++) {
            rect[n].x = (int)at[4 * n], rect[n].y = (int)at[4 * n + 1];
            rect[n].w = (int)at[4 * n + 2], rect[n].h = (int)at[4 * n + 3];
            order[n] = BACK_TO_FRONT[n];
        }
        open_wins = WINDOWS;
        free(at);
        if (shown_in_front(FORTH)) return(true);
    }
    return(false);
}

// ---- the windows' text

typedef struct {
    const char* name;
    int window;
    int dx, dy;                 // the offset of its first cell from the top left of the window's content
    int cols, rows;             // its size in cells; rows is 0 for as many rows as fit in the window
} grid;

static const grid GRIDS[] = {
    {"forth", FORTH, 2, 2, 50, 16},
    {"calc", CALC, 6, 4, 14, 5},
    {"editor", EDITOR, 2, 2, 64, 16},
    {"names", WORDS, 2, 0, 13, 0},          // the Words window's list
    {"source", WORDS, 112, 0, 45, 0},       // the definition beside the list
    {NULL}};

static const grid* grid_named(const char* name) {
    for(const grid* g = GRIDS; g->name; g++) {
        if (!strcmp(g->name, name)) return(g);
    }
    fprintf(stderr, "deskcheck: there is no text called %s\n", name);
    exit(EXIT_FAILURE);
}

static int grid_x(const grid* g) { return(content_x(g->window) + g->dx); }
static int grid_y(const grid* g) { return(content_y(g->window) + g->dy); }

// Returns the number of rows a window's text has: the number in its grid,
// or the number of lines that fit in its content.
static int grid_rows(const grid* g) {
    int rows = g->rows ? g->rows : (rect[g->window].h - 20) / ROWS;
    return(rows < MOST_LINES ? rows : MOST_LINES);
}

static void read_grid(const char* name, int row, char* text) {
    const grid* g = grid_named(name);
    read_row(grid_x(g), grid_y(g) + row * 16, g->cols, text);
}

// ---- HexaPaint's canvas, drawn here the way paint.fs draws it

// The patterns along the bottom of its window, in the order they appear
// there.
static const char* PATTERNS[][2] = { {"display.fs", "p-white"}, {"display.fs", "p-black"},
                                     {"display.fs", "p-dkgray"}, {"display.fs", "p-gray"},
                                     {"display.fs", "p-ltgray"}, {"paint.fs", "p-brick"},
                                     {"paint.fs", "p-diag"}, {"paint.fs", "p-dots"} };
enum { WHITE, BLACK };
#define PATTERNS_THERE ((int)(sizeof(PATTERNS) / sizeof(PATTERNS[0])))
static pattern patterns[PATTERNS_THERE];

// The tools, which are two to a row down the left side of its window.
enum { PENCIL, BRUSH, ERASER, LINE, RECT, RECT_FILLED, OVAL, OVAL_FILLED };

static int cw, ch;              // the canvas's width and height in pixels
static uint8_t* canvas;         // the pixels the canvas should have, 1 for black
static uint8_t* shaped;         // the same, plus a shape that is being pulled out
static uint8_t* drawn;          // whichever of those two is being drawn into
static const pattern* fill;     // the pattern a span is filled with
static int pen;                 // the width and height of a dot in pixels
static int tool, in_hand = BLACK;       // the selected tool and the selected pattern
static int from_x, from_y, to_x, to_y;  // the two corners of a shape that is being pulled out

// Fills the pixels of row y from x0 up to but not including x1 with the
// pattern.
static void span(int x0, int x1, int y) {
    if (y < 0 || y >= ch) return;
    for(int x=high(x0, 0); x < low(x1, cw); x++) drawn[y * cw + x] = pattern_pixel(fill, x, y);
}

// Draws one dot of the pen with its top left at x, y.
static void dot(int x, int y) {
    for(int row=y; row < y + pen; row++) span(x, x + pen, row);
}

// Draws a line of dots from x0, y0 to x1, y1.
static void line_of_dots(int x0, int y0, int x1, int y1) {
    int dx = abs(x1 - x0), sx = x1 < x0 ? -1 : 1;
    int dy = -abs(y1 - y0), sy = y1 < y0 ? -1 : 1;
    int err = dx + dy;
    for(int x=x0, y=y0; ; ) {
        dot(x, y);
        if (x == x1 && y == y1) return;
        int e2 = 2 * err;
        if (e2 >= dy) err += dy, x += sx;
        if (e2 <= dx) err += dx, y += sy;
    }
}

// box() sets up the rectangle between two corners, and the oval inscribed
// in it: half its width and height, its centre, and how far the oval
// extends sideways at each row above or below the centre.
static int bx0, by0, bx1, by1;
static int half_w, half_h, mid_x, mid_y;
static int* reach;              // one entry per row, from the centre row to the edge, plus one more
static void box(int x0, int y0, int x1, int y1) {
    bx0 = low(x0, x1), by0 = low(y0, y1), bx1 = high(x0, x1), by1 = high(y0, y1);
    half_w = (bx1 - bx0) >> 1, half_h = (by1 - by0) >> 1;
    mid_x = bx0 + half_w, mid_y = by0 + half_h;
    free(reach);
    reach = calloc((size_t)half_h + 2, sizeof(int));
    if (reach == NULL) {
        fprintf(stderr, "there is no room for an oval %d rows high\n", by1 - by0);
        exit(EXIT_FAILURE);
    }
    for(int dy=0, s=half_h; dy <= half_h; dy++) {
        while (half_h && s * s > half_h * half_h - dy * dy) s--;
        reach[dy] = half_h ? half_w * s / half_h : half_w;
    }
}

static void rect_frame(void) {
    span(bx0, bx1 + 1, by0);
    span(bx0, bx1 + 1, by1);
    for(int y=by0 + 1; y < by1; y++) {
        span(bx0, bx0 + 1, y);
        span(bx1, bx1 + 1, y);
    }
}

static void rect_fill(void) {
    for(int y=by0; y <= by1; y++) span(bx0, bx1 + 1, y);
}

static void oval_fill(void) {
    for(int dy=0; dy <= half_h; dy++) {
        span(mid_x - reach[dy], mid_x + reach[dy] + 1, mid_y - dy);
        span(mid_x - reach[dy], mid_x + reach[dy] + 1, mid_y + dy);
    }
}

static void oval_frame(void) {
    for(int dy=0; dy <= half_h; dy++) {
        int out = reach[dy], in = dy < half_h ? low(out, reach[dy + 1] + 1) : 0;
        for(int y=mid_y - dy; y <= mid_y + dy; y += dy ? 2 * dy : 1) {
            span(mid_x - out, mid_x - in + 1, y);
            span(mid_x + in, mid_x + out + 1, y);
        }
    }
}

// Draws a shape's outline, which is black and one pixel wide.
static void outline(int how, int x0, int y0, int x1, int y1) {
    fill = &patterns[BLACK], pen = 1;
    box(x0, y0, x1, y1);
    if (how == LINE) line_of_dots(x0, y0, x1, y1);
    else if (how < OVAL) rect_frame();
    else oval_frame();
}

// Draws what a tool draws when it is pulled from one position to another
// with a pattern selected.  The pencil, the brush and the eraser draw a
// line of dots.
static void pulled(int how, int with, int x0, int y0, int x1, int y1) {
    if (how < LINE) {
        fill = &patterns[how == PENCIL ? BLACK : how == BRUSH ? with : WHITE];
        pen = how == PENCIL ? 1 : how == BRUSH ? 4 : 12;
        dot(x0, y0);
        line_of_dots(x0, y0, x1, y1);
        return;
    }
    fill = &patterns[with];
    box(x0, y0, x1, y1);
    if (how == RECT_FILLED) rect_fill();
    if (how == OVAL_FILLED) oval_fill();
    outline(how, x0, y0, x1, y1);
}

// Convert a canvas position to a screen position.  The tools take up the
// 45 pixels to the left of the canvas.
static int canvas_x(int x) { return(content_x(PAINT) + 45 + x); }
static int canvas_y(int y) { return(content_y(PAINT) + y); }

// Returns the number of canvas pixels on the screen that differ from
// `should`.  If `but` is set, a small box around but_x, but_y is ignored.
// If `covering` is a window, the pixels under that window are ignored.
static int canvas_wrong(const uint8_t* should, bool but, int but_x, int but_y, int covering) {
    int wrong = 0;
    for(int y=0; y < ch; y++) {
        for(int x=0; x < cw; x++) {
            if (but && x >= but_x - 1 && x < but_x + 9 && y >= but_y - 1 && y < but_y + 13) continue;
            if (covering >= 0 && under(canvas_x(x), canvas_y(y), covering)) continue;
            if (black(canvas_x(x), canvas_y(y)) != (bool)should[y * cw + x]) wrong++;
        }
    }
    return(wrong);
}

// ---- performing a check's actions

static char names[LINES_ROOM][COLS + 1];        // the Words window's list, as last read
static bool found;                              // true if the name searched for was in that list

static int listed(const char* name) {   // returns the row a name is on, or -1
    for(int row=0; row < grid_rows(grid_named("names")); row++) {
        if (!strcmp(names[row], name)) return(row);
    }
    return(-1);
}

// The machine's dictionary.  The Words window lists it in the order it is
// searched: first the words defined on the machine, newest first, and
// then the image's words in the order cross.fs linked them, which is by
// how often the source uses each name (headers-ordered).  So this file
// cannot know which word is where in the list.  It reads the machine's
// memory and walks the headers the way the tools walk an image's
// (vm_opcodes.c).
static char (*words)[NAME_ROOM];        // the names, in search order
static long nwords, words_room;         // the number of names, and the allocated size

// Reads the dictionary's names from the machine's memory.
static void words_read(void) {
    size_t len;
    const uint8_t* memory = desk_memory(&len);
    nwords = 0;
    for(uint32_t head = first_header(memory, len); head; nwords++) {
        uint64_t xt;
        ONE_MORE(words, nwords, words_room);
        head = read_header(memory, len, head, &xt, words[nwords]);
    }
}

static long word_at(const char* name) { // returns a name's index in the dictionary, or -1
    for(long n=0; n < nwords; n++) if (!strcmp(words[n], name)) return(n);
    return(-1);
}

// Reads the rows of the Words window's list.  Returns the dictionary index
// of the word in its first row, or -1 if the rows are not consecutive
// words of the dictionary.  A row shows only as much of a name as the list
// is wide.
static long list_read(void) {
    const grid* g = grid_named("names");
    const int rows = grid_rows(g);
    for(int row=0; row < rows; row++) read_grid("names", row, names[row]);
    for(long top=0; top < nwords; top++) {
        bool same = true;
        for(int row=0; row < rows && top + row < nwords && same; row++) {
            same = !strncmp(words[top + row], names[row], (size_t)g->cols) && strlen(names[row]) <= (size_t)g->cols;
        }
        if (same) return(top);
    }
    return(-1);
}

// Parses the numbers that follow an action's name.  They are separated by
// commas, or each has a sign.  Returns how many there are.
static int numbers(const char* at, int* out, int most) {
    int n = 0;
    while (n < most && (*at == '+' || *at == '-' || *at == ',' || isdigit((unsigned char)*at))) {
        if (*at == ',' || *at == '+') at++;
        char* end;
        out[n++] = (int)strtol(at, &end, 10);
        at = end;
    }
    return(n);
}

// The window being dragged by grab: its index, the position on its title
// where the button was pressed, the distance it has been moved, and the
// screen just before the press.
static struct { int w, x, y, dx, dy; uint8_t before[SCREEN_ROOM]; } grabbed;

// Performs a check's actions, one word at a time.  The actions are:
//
//   'text'             types the text, one key per character; \r is the Return key
//   ret down backspace pgdn ...    presses a key, given by name
//   ^s                 presses a key with Ctrl held
//   front:NAME         clicks a window's title, which brings it to the front
//   cycle:NAME         presses ^W, which brings the back window forward, until NAME is in front
//   drag:NAME+DX+DY    drags a window by its title
//   grab:NAME+DX+DY    the same, but leaves the button held down
//   drop               releases the button after grab:
//   click:NAME+X+Y     clicks in a window, at an offset from the top left of its content
//   click:NAME-X-Y     clicks at an offset back from the bottom right of its frame
//   point:X,Y          moves the pointer to a screen position
//   at:NAME+X+Y        moves the pointer to an offset from the top left of a window's content
//   away               moves the pointer to a position that is over nothing
//   menu               clicks the title in the bar, which opens the menu
//   pick:NAME          clicks a window's line in the menu
//   hold:NAME          presses the button on the title and releases it on a window's line
//   over:NAME          moves the pointer onto a window's line in the menu
//   pmenu              clicks the picture's title in the bar, which opens its menu
//   picture:LINE       picks a line of the picture's menu (atkinson, ordered, blue, finer,
//                      coarser, darker or lighter), and waits until the picture has been remade
//   find:A             presses Right or Left until a name is in the Words window's list
//   name:A             clicks a name in the Words window's list
//   tool:N pattern:N   clicks one of HexaPaint's tools or patterns
//   pull:X0,Y0,X1,Y1   presses the button at one canvas position, moves to another, and releases
//   press:X0,Y0,X1,Y1  the same, but leaves the button held down
//   release            releases the button after press:
//
// In typed text, %W and %H are replaced by the screen's width and height.
static void does(const char* said) {
    for(const char* at = said; ; ) {
        while (*at == ' ') at++;
        if (!*at) {
            desk_doing = NULL;
            return;
        }
        // The buffer for one word of the actions.  It has room for the
        // whole text, with every %W or %H expanded to a number, which has
        // at most three digits for each byte of an int.
        static char* word;
        const size_t room = strlen(said) * (1 + 3 * sizeof(int)) + 1;
        desk_doing = NULL;
        free(word);
        word = malloc(room);
        if (word == NULL) {
            fprintf(stderr, "there is no room for a word of what is done\n");
            exit(EXIT_FAILURE);
        }
        size_t len;
        if (*at == '\'') {
            len = strcspn(at + 1, "'");
            size_t out = 0;
            for(size_t idx=0; idx < len; idx++) {
                const char* c = at + 1 + idx;
                if (c[0] == '%' && (c[1] == 'W' || c[1] == 'H')) {
                    out += sprintf(word + out, "%d", c[1] == 'W' ? W : H);
                    idx++;
                } else {
                    word[out++] = *c;
                }
            }
            word[out] = '\0';
            desk_doing = word;
            desk_type(word);
            desk_turns(4);
            at += len + 2;
            continue;
        }
        len = strcspn(at, " ");
        memcpy(word, at, len);
        word[len] = '\0';
        desk_doing = word;
        at += len;

        const char* of = strchr(word, ':');     // the action's argument, after the colon: a name, numbers, or both
        of = of ? of + 1 : "";
        size_t name = isdigit((unsigned char)of[0]) ? 0 : strcspn(of, "+-,");
        int n[4] = { 0 }, count = numbers(of + name, n, 4);
        int x, y;
        if (!strcmp(word, "away")) {
            away();
        } else if (!strcmp(word, "menu")) {
            click(20, 8);
        } else if (!strcmp(word, "release")) {
            desk_button(false);
            drawn = canvas;
            pulled(tool, in_hand, from_x, from_y, to_x, to_y);
        } else if (word[0] == '^' && word[1] && !word[2]) {
            desk_key(word + 1, "ctrl");
            if (word[1] == 'q' && open_wins > 0) close_win(in_front());
            if (word[1] == 'w' && open_wins > 1) to_front(order[0]);
        } else if (!strncmp(word, "slide:", 6)) {
            int w = window(of, name);
            drag(rect[w].x + rect[w].w - 8, rect[w].y + 8, rect[w].x + rect[w].w - 24, rect[w].y + 8);
        } else if (!strncmp(word, "close:", 6)) {
            int w = window(of, name);
            click(rect[w].x + rect[w].w - 8, rect[w].y + 8);
            close_win(w);
        } else if (!strncmp(word, "front:", 6)) {
            int w = window(of, name);
            title_of(w, 0, 0, &x, &y);
            click(x, y);
            to_front(w);
        } else if (!strncmp(word, "cycle:", 6)) {
            int w = window(of, name);
            for(int turn=0; turn < open_wins && in_front() != w; turn++) {
                desk_key("w", "ctrl");
                to_front(order[0]);
            }
        } else if (!strncmp(word, "drag:", 5) && count == 2) {
            int w = window(of, name);
            title_of(w, n[0], n[1], &x, &y);
            drag(x, y, x + n[0], y + n[1]);
            rect[w].x += n[0], rect[w].y += n[1];
        } else if (!strncmp(word, "grab:", 5) && count == 2) {
            grabbed.w = window(of, name);
            title_of(grabbed.w, n[0], n[1], &x, &y);
            desk_pointer(x, y);
            look(NULL);
            memcpy(grabbed.before, shot, SCREEN);
            grabbed.x = x, grabbed.y = y, grabbed.dx = n[0], grabbed.dy = n[1];
            desk_button(true);
            desk_pointer(x + n[0], y + n[1]);
            desk_turns(6);
        } else if (!strcmp(word, "drop")) {
            desk_button(false);
            desk_turns(6);
            rect[grabbed.w].x += grabbed.dx, rect[grabbed.w].y += grabbed.dy;
        } else if (!strncmp(word, "click:", 6) && count == 2) {
            int w = window(of, name);
            if (of[name] == '-') click(rect[w].x + rect[w].w + n[0], rect[w].y + rect[w].h + n[1]);
            else click(content_x(w) + n[0], content_y(w) + n[1]);
            to_front(w);                        // a click in a window brings it to the front
        } else if (!strncmp(word, "point:", 6) && count == 2) {
            desk_pointer(n[0], n[1]);
        } else if (!strncmp(word, "at:", 3) && count == 2) {
            int w = window(of, name);
            desk_pointer(content_x(w) + n[0], content_y(w) + n[1]);
        } else if (!strcmp(word, "pmenu")) {
            click(picture_title() + 12, 8);
        } else if (!strncmp(word, "picture:", 8)) {
            int line = picture_line(of);
            click(picture_title() + 12, 8);
            click(picture_title() + 12, 20 + 16 * line + 8);
            picture_picked(line);
            picture_made();
        } else if (!strncmp(word, "pick:", 5)) {
            int w = window(of, name);
            click(32, 20 + 16 * menu_line(w) + 8);
            to_front(w);
        } else if (!strncmp(word, "hold:", 5)) {
            int w = window(of, name);
            drag(20, 8, 32, 20 + 16 * menu_line(w) + 8);
            to_front(w);
        } else if (!strncmp(word, "over:", 5)) {
            desk_pointer(80, 20 + 16 * menu_line(window(of, name)) + 8);
        } else if (!strncmp(word, "find:", 5)) {
            // Find both where the list is and where the word is.  The
            // screen shows which word is in the list's first row, and the
            // machine's dictionary gives every word's position.  A click
            // on the first row selects its word without moving the list.
            // Right and Left move the selection by one window of lines, and
            // the list follows it: Right moves on until the selection is in
            // the last row, and Left moves back until it is in the first
            // (see.fs, wpick).  So the word is in the list after one key
            // press for each whole window of lines between it and the
            // selection.  The pointer is moved off the list before the list
            // is read, because the pointer is drawn on the screen and a row
            // under it would not read as its word.
            const grid* g = grid_named("names");
            const int rows = grid_rows(g);
            away();
            look("10-list");
            words_read();
            long top = list_read(), wanted = word_at(of);
            found = false;
            if (wanted < 0) {                   // the dictionary has no such word, so there is nothing to page to
                fprintf(stderr, "deskcheck: find:%s, and the machine's dictionary has no word of that name\n", of);
                exit(EXIT_FAILURE);
            }
            // If the word is not reached, the check after this action
            // fails.  The state of the dictionary and of the screen is
            // printed here to explain that failure.
            if (top >= 0) {
                click(grid_x(g) + 8, grid_y(g) + 8);
                away();
                if (wanted >= top + rows) for(long at = top; at < wanted; at += rows) desk_key("right", NULL);
                for(long at = top; at > wanted; at -= rows) desk_key("left", NULL);
                look("10-list");
                long now = list_read();
                found = listed(of) >= 0;
                if (!found) {
                    long had = nwords;
                    look("10-not-found");
                    words_read();
                    printf("note the list was gone through to %s and has not got it: the dictionary had it at %ld of %ld "
                           "words, and has %ld words now; the list's first row was word %ld, and is now word %ld\n",
                           of, wanted, had, nwords, top, now);
                }
            } else {
                printf("note the list was not gone through to %s: its rows are not words of the dictionary one after "
                       "another. They read:", of);
                for(int row=0; row < rows; row++) printf(" [%s]", names[row]);
                printf("\n");
            }
            // A program that drives a desktop without checking it
            // (bench/profile.c) has no check that would fail afterwards, so
            // it stops here.
            if (!found && !desk_checking) {
                fprintf(stderr, "deskcheck: find:%s did not come to the word, and no check is being run that would fail for it\n", of);
                exit(EXIT_FAILURE);
            }
        } else if (!strncmp(word, "name:", 5)) {
            const grid* g = grid_named("names");
            int row = listed(of);
            click(grid_x(g) + 8, grid_y(g) + (row >= 0 ? row * 16 + 8 : 30));
            away();
        } else if (!strncmp(word, "tool:", 5) && count == 1) {
            tool = n[0];
            click(content_x(PAINT) + (tool & 1) * 22 + 11, content_y(PAINT) + (tool >> 1) * 22 + 11);
        } else if (!strncmp(word, "pattern:", 8) && count == 1) {
            in_hand = n[0];
            click(content_x(PAINT) + 83 + 18 * in_hand + 8, content_y(PAINT) + ch + 3 + 8);
        } else if (!strncmp(word, "pull:", 5) && count == 4) {
            drag(canvas_x(n[0]), canvas_y(n[1]), canvas_x(n[2]), canvas_y(n[3]));
            drawn = canvas;
            pulled(tool, in_hand, n[0], n[1], n[2], n[3]);
        } else if (!strncmp(word, "press:", 6) && count == 4) {
            from_x = n[0], from_y = n[1], to_x = n[2], to_y = n[3];
            desk_pointer(canvas_x(from_x), canvas_y(from_y));
            desk_button(true);
            desk_pointer(canvas_x(to_x), canvas_y(to_y));
        } else if (!of[0]) {
            desk_key(word, NULL);
        } else {
            fprintf(stderr, "deskcheck: %s is not something a check does\n", word);
            exit(EXIT_FAILURE);
        }
    }
}

// ---- the further tests of the checks

static bool came_up(char* why) {
    snprintf(why, WHY, "the console said: %.200s", boot);
    return(strstr(boot, "desktop") != NULL);
}

static bool saved(char* why) {
    uint8_t block[DISK_BLOCK];
    desk_turns(4);
    desk_block(DISK_SOURCE_BLOCK, 1, block);
    snprintf(why, WHY, "the second line of the block is: %.16s", block + DISK_LINE);
    return(!memcmp(block + DISK_LINE, "( edited )", 10));
}

// The picture as it should be shown.  A window smaller than its picture
// shows the picture at half size, where each pixel is the average gray of
// four, rounded to the nearest.  If even that is larger than the window,
// the window shows the middle of it: the middle rows, and the middle
// columns, starting at the display cell that contains the first of them.
static uint8_t* dither_black;   // the dithered picture: one byte per pixel, 1 for black
static int pw, ph;              // its size in pixels, at the size it is shown
static int kw, kh, px0, py0;    // the part of it the window shows: its size, and its top left in the picture
static bool half;               // true if it is shown at half size
static uint8_t* shown_black;    // the picture as it should be shown now: dithered, or as the menu last drew it

// Computes the picture as it should be shown, the first time it is called.
static void picture(void) {
    if (dither_black) return;
    tiff file = read_tiff(machine->picture);
    int show_w = rect[VIEWER].w - 2, show_h = rect[VIEWER].h - 20;
    half = file.w > show_w || file.h > show_h;
    int* gray = grays_of(file.bytes, file.w, file.h, file.samples, half, &pw, &ph);
    dither_black = calloc((size_t)pw * ph + 1, 1);
    dither(gray, pw, ph, dither_black);
    free(file.bytes), free(gray);
    kw = low(show_w, pw), kh = low(show_h, ph);
    px0 = (pw - kw) / 2 / CELL_BITS * CELL_BITS, py0 = (ph - kh) / 2;
    shown_black = dither_black;
}

// picture_wrong() returns the number of pixels of the picture, inside a
// rectangle of the screen, that differ from what should be shown.
// wrong_at records where they are in the picture: the first wrong pixel,
// and the bounding box of all of them.
static struct { int x, y, x0, y0, x1, y1; } wrong_at;

static int picture_wrong(int x0, int y0, int x1, int y1) {
    int vx = content_x(VIEWER), vy = content_y(VIEWER), wrong = 0;
    for(int y=high(y0, vy); y < low(low(y1, vy + kh), H); y++) {
        for(int x=high(x0, vx); x < low(low(x1, vx + kw), W); x++) {
            int px = x - vx + px0, py = y - vy + py0;
            if (black(x, y) != (bool)shown_black[(size_t)py * pw + px]) {
                if (!wrong) wrong_at.x = wrong_at.x0 = wrong_at.x1 = px, wrong_at.y = wrong_at.y0 = wrong_at.y1 = py;
                wrong_at.x0 = low(wrong_at.x0, px), wrong_at.x1 = high(wrong_at.x1, px);
                wrong_at.y0 = low(wrong_at.y0, py), wrong_at.y1 = high(wrong_at.y1, py);
                wrong++;
            }
        }
    }
    return(wrong);
}

static bool dithered(char* why) {
    picture();
    int wrong = picture_wrong(0, 0, W, H);
    snprintf(why, WHY, "%d pixels differ, of the %d by %d from %d, %d of the picture as %d by %d",
             wrong, kw, kh, px0, py0, pw, ph);
    return(!wrong && kw > 0 && kh > 0);
}

// Checks the viewer's bitmap on the disk, which starts at DISK_VIEW_BLOCK.
// Each block holds as many whole rows as fit, in the display's layout.
// Every pixel is compared with the part of the dither the window shows.
static bool view_on_disk(char* why) {
    picture();
    int stride = (kw + CELL_BITS - 1) / CELL_BITS;
    int rows_a_block = DISK_BLOCK / CELL_BYTES / stride;
    int blocks = (kh + rows_a_block - 1) / rows_a_block;
    if (kw <= 0 || kh <= 0 || blocks > DISK_VIEW_BLOCKS) {
        snprintf(why, WHY, "a bitmap of %d by %d does not fit the %d blocks kept for it", kw, kh, DISK_VIEW_BLOCKS);
        return(false);
    }
    uint8_t* bytes = malloc((size_t)blocks * DISK_BLOCK);
    desk_block(DISK_VIEW_BLOCK, blocks, bytes);
    int differ = 0;
    for(int y=0; y < kh; y++) {
        size_t row = (size_t)(y / rows_a_block) * (DISK_BLOCK / CELL_BYTES) + (size_t)(y % rows_a_block) * stride;
        for(int x=0; x < kw; x++) {
            int bit = x % CELL_BITS;
            bool set = disk_cell(bytes, row + x / CELL_BITS) >> (DISP_LEFT_LOW ? bit : CELL_BITS - 1 - bit) & 1;
            if ((DISP_INK_BLACK ? set : !set) != (bool)dither_black[(size_t)(py0 + y) * pw + px0 + x]) differ++;
        }
    }
    free(bytes);
    snprintf(why, WHY, "%d pixels of the %d by %d bitmap on the disk differ from the dither", differ, kw, kh);
    return(!differ);
}

// The state of the picture's menu (the end of forth/view.fs), updated as
// the checks pick its lines: how the picture is drawn (dithered, through
// Bayer's mask, or through blue noise); the size of a gray sample as
// drawn, as a shift, so that a sample is 1 << pic_d pixels a side; and the
// amount added to the mask's thresholds.  A sample starts as 4 by 4 of the
// picture's pixels.
static const char* PICTURE_LINES[] = { "atkinson", "ordered", "blue", "finer", "coarser", "darker", "lighter" };
enum { PICTURE_LINE_COUNT = sizeof(PICTURE_LINES) / sizeof(PICTURE_LINES[0]) };
static int pic_mode, pic_d = -1, pic_br;

static int text_width(const char* text, const font* f) {
    int w = 0;
    for(; *text; text++) {
        int width;
        glyph_of(f, (unsigned char)*text, &width);
        w += width;
    }
    return(w);
}

// Returns the x position where the picture's title starts in the bar,
// which is after the windows' title.
static int picture_title(void) { return(text_width("hexaforth", &sans) + 16); }

// Returns the index of a line of the picture's menu, by name.
static int picture_line(const char* name) {
    for(int line=0; line < PICTURE_LINE_COUNT; line++) if (!strcmp(name, PICTURE_LINES[line])) return(line);
    fprintf(stderr, "deskcheck: the picture's menu has no line %s\n", name);
    exit(EXIT_FAILURE);
}

// Updates the expected state of the picture's menu for a picked line.
static void picture_picked(int line) {
    picture();
    if (pic_d < 0) pic_d = half ? 1 : 2;
    if (line < 3) pic_mode = line;
    else if (line == 3) pic_d = high(pic_d - 1, 0);
    else if (line == 4) pic_d = low(pic_d + 1, 2);
    else pic_br = high(low(pic_br + (line == 5 ? 1 : -1), 15), -15);
}

// Computes the picture as the menu should have drawn it.  The picture is
// stored as gray from its full size, one 4-bit sample for each square of
// 1 << (d + half) of its pixels a side, and is drawn through the mask with
// each sample 1 << d pixels a side.  The blue noise is 64 by 64, or 32 by
// 32 where a cell is 16 bits.  Its thresholds come from the code that
// makes the disk (util/bluenoise.h), not from the disk itself.
static void picture_expected(void) {
    picture();
    if (pic_mode == 0) {
        shown_black = dither_black;
        return;
    }
    tiff file = read_tiff(machine->picture);
    int fw, fh, gw, gh;
    int* gray = grays_of(file.bytes, file.w, file.h, file.samples, false, &fw, &fh);
    int* level = gray_kept(gray, fw, fh, pic_d + half, 4, &gw, &gh);
    int n = pic_mode == 1 ? 8 : CELL_BITS > 16 ? 64 : 32;
    static uint8_t* masks[65];      // indexed by n; each is made once
    if (!masks[n]) {
        masks[n] = malloc((size_t)n * n);
        if (pic_mode == 1) {
            for(int y=0; y < 8; y++) for(int x=0; x < 8; x++) masks[n][y * 8 + x] = gray_bayer(x, y);
        } else {
            blue_bytes(n, masks[n]);
        }
    }
    const uint8_t* mask = masks[n];
    static uint8_t* gray_black;
    free(gray_black);
    gray_black = calloc((size_t)pw * ph + 1, 1);
    gray_drawn(level, gw, 4, mask, n, pic_br, pic_d, 0, 0, pw, ph, gray_black);
    shown_black = gray_black;
    free(file.bytes), free(gray), free(level);
}

// Waits for the picture to be remade.  The viewer's task remakes it while
// the desktop keeps running.  This reads the screen until the picture
// matches the picked line, which happens when the task has made its last
// rows and left the ring, or until a fixed number of tries have passed.
static void picture_made(void) {
    picture_expected();
    for(int tries=0; tries < 100; tries++) {
        look(NULL);
        if (!picture_wrong(0, 0, W, H)) return;
    }
}

static bool as_picked(char* why) {
    if (pic_mode == 0) return(dithered(why));
    int wrong = picture_wrong(0, 0, W, H);
    snprintf(why, WHY, "%d pixels differ, of the %d by %d from %d, %d of the picture as %d by %d, "
             "kept a sample to %d by %d and drawn %d by %d through %s, %+d; the first at %d, %d, "
             "and all from %d, %d to %d, %d",
             wrong, kw, kh, px0, py0, pw, ph, 1 << (pic_d + half), 1 << (pic_d + half), 1 << pic_d, 1 << pic_d,
             pic_mode == 1 ? "Bayer's mask" : "blue noise", pic_br, wrong_at.x, wrong_at.y,
             wrong_at.x0, wrong_at.y0, wrong_at.x1, wrong_at.y1);
    return(!wrong && kw > 0 && kh > 0);
}

// Returns true if the picture's menu is open, with a mark beside the line
// for the way the picture is drawn and beside no other line.
static bool picture_marked(char* why) {
    picture();
    int x = picture_title(), marks = 0, at = -1;
    for(int line=0; line < 3; line++) {
        bool all = true;
        for(int dy=0; dy < 4; dy++) for(int dx=0; dx < 4; dx++) all = all && black(x + 6 + dx, 20 + 16 * line + 6 + dy);
        if (all) marks++, at = line;
    }
    bool titled = shows("Picture", x + 8, 2, &sans, true);
    snprintf(why, WHY, "%d lines are marked, the last %d, and %d is to be; the title is%s shown as down",
             marks, at, pic_mode, titled ? "" : " not");
    return(titled && marks == 1 && at == pic_mode);
}

// While a window is being dragged, the screen must be the same as before
// the press, except along a one-pixel outline of the window at the
// position it has been dragged to.  On that outline the screen is inverted
// wherever the gray pattern is black (wm.fs, frame-xor).  The comparison
// leaves out the pointer, at both its old and its new position, and the
// disk light.
static bool in_box(int x, int y, int bx, int by, int bw, int bh) {
    return(x >= bx && x < bx + bw && y >= by && y < by + bh);
}
static bool outlined(char* why) {
    int64_t* corner;
    table("wm.fs", "disk-corner", &corner, 2);
    int fx = rect[grabbed.w].x + grabbed.dx, fy = rect[grabbed.w].y + grabbed.dy;
    int fw = rect[grabbed.w].w, fh = rect[grabbed.w].h;
    int differ = 0, inverted = 0;
    for(int y=0; y < H; y++) {
        for(int x=0; x < W; x++) {
            if (in_box(x, y, grabbed.x, grabbed.y, 8, 12) || in_box(x, y, grabbed.x + grabbed.dx, grabbed.y + grabbed.dy, 8, 12)) continue;
            if (x >= W - corner[0] && y < corner[1]) continue;
            bool edge = in_box(x, y, fx, fy, fw, fh) && !in_box(x, y, fx + 1, fy + 1, fw - 2, fh - 2);
            bool flip = edge && pattern_pixel(&patterns[3], x, y);
            bool was = grabbed.before[(size_t)y * W + x];
            if (flip) inverted++;
            if ((bool)shot[(size_t)y * W + x] != (was != flip)) differ++;
        }
    }
    free(corner);
    snprintf(why, WHY, "%d pixels differ from the screen before the press with the outline at %d, %d inverted (%d of its pixels)",
             differ, fx, fy, inverted);
    return(!differ && inverted > 0);
}

static bool moved(char* why) {
    bool titled = title_shown(VIEWER), has_x = x_shown(VIEWER);
    snprintf(why, WHY, "its title at %d, %d is%s shown, and its x at %d, %d is%s shown",
             rect[VIEWER].x + 8, rect[VIEWER].y + 2, titled ? "" : " not",
             rect[VIEWER].x + rect[VIEWER].w - 12, rect[VIEWER].y + 2, has_x ? "" : " not");
    return(titled && has_x);
}

static bool was_found(char* why) {
    snprintf(why, WHY, "the list starts %s, %s, %s, %s", names[0], names[1], names[2], names[3]);
    return(found);
}

// The row and column of the first bold character in the definition, which
// bold() finds.
static int bold_row = -1, bold_col;
static bool bold(char* why) {
    const grid* g = grid_named("source");
    bold_row = -1;
    for(int row=0; row < grid_rows(g) && bold_row < 0; row++) {
        for(int col=0; col < g->cols && bold_row < 0; col++) {
            uint8_t cell[ROWS], any = 0;
            cell_at_screen(grid_x(g) + col * 8, grid_y(g) + row * 16, cell);
            for(int r=0; r < ROWS; r++) any |= cell[r];
            if (any && !matched(cell, 'p', ROWS) && matched(cell, 'b', ROWS - 1)) bold_row = row, bold_col = col;
        }
    }
    snprintf(why, WHY, "nothing in the definition is in bold");
    return(bold_row >= 0);
}

// Moves the pointer onto a word of the definition and reads the box that
// shows the word's instruction.  The box is on the line below the word,
// or on the line above it when the word is in the last two lines.  It
// starts under the word unless that would put it past the edge of the
// window, so its text is searched for from that position leftwards.
// Returns true if the text matches `form`, a regular expression for an
// instruction's value and fields.
static bool told(int row, int col, const char* shot_name, const char* form, char* why) {
    const grid* g = grid_named("source");
    char text[COLS + 1] = "";
    regex_t is;
    regcomp(&is, form, REG_EXTENDED | REG_NOSUB);
    desk_pointer(grid_x(g) + col * 8 + 4, grid_y(g) + row * 16 + 1);
    look(shot_name);
    int y = grid_y(g) + (row < grid_rows(g) - 2 ? (row + 1) * 16 + 1 : row * 16 - 17);
    bool found = false;
    for(int x=grid_x(g) + 1 + col * 8; x > grid_x(g) - 3 && !found; x--) {
        read_row(x, y, low(40, (W - x) / 8), text);
        found = !regexec(&is, text, 0, NULL, 0);
    }
    regfree(&is);
    snprintf(why, WHY, "the box by row %d, column %d says: %s", row, col, text);
    return(found);
}

static bool fused_told(char* why) {
    snprintf(why, WHY, "nothing in the definition is in bold");
    return(bold_row >= 0 && told(bold_row, bold_col, "10-hover", "^[0-9A-F]{4} [^ ]*->IN [^ ]+ ->[^ ]+", why));
}

// Returns true if the definition shown is that of the word that follows
// cell+ in the dictionary.  cell+ was selected, and Down then moved off
// it.
static bool went_on(char* why) {
    char row[COLS + 1];
    long at = word_at("cell+");
    const char* next = at >= 0 && at + 1 < nwords ? words[at + 1] : NULL;
    read_grid("source", 0, row);
    snprintf(why, WHY, "row 0 of the definition is \"%s\", and the word the dictionary has after cell+ is %s", row,
             next ? next : "not there");
    return(next && row[0] == ':' && row[1] == ' ' && !strcmp(row + 2, next));
}

// remember() saves the definition's rows as they are now, for later tests
// to search and to compare with.  keep() saves the whole screen, so that a
// later test can see what changed.
static char source[LINES_ROOM][COLS + 1];
static bool remember(char* why) {
    (void)why;
    for(int row=0; row < grid_rows(grid_named("source")); row++) read_grid("source", row, source[row]);
    return(true);
}

static bool keep(char* why) {
    (void)why;
    memcpy(kept, shot, SCREEN);
    return(true);
}

static bool a_number(char* why) {
    regex_t is;
    regcomp(&is, " [0-9]+ !$", REG_EXTENDED | REG_NOSUB);
    remember(why);
    bool numbered = !regexec(&is, source[1], 0, NULL, 0);
    regfree(&is);
    snprintf(why, WHY, "its first line is: %s", source[1]);
    return(numbered);
}

// A word that is a single instruction on its own also gets a box.  This
// checks the `dup` in the first line of dither-start.
static bool word_told(char* why) {
    const char* dup = strstr(source[1], "dup");
    snprintf(why, WHY, "the first line has no dup: %s", source[1]);
    return(dup && told(1, (int)(dup - source[1]), "10-hover-word", "^[0-9A-F]{4} T->IN IN-> ->T d\\+1", why));
}

// A window that shows part of a text ends its first row with [PGUP] when
// more of the text is above, and its last row with [PGDN] when more is
// below.
static const char PGUP[] = "[PGUP]", PGDN[] = "[PGDN]";

static bool ends_with(const char* text, const char* end) {
    size_t n = strlen(text), e = strlen(end);
    return(n >= e && !strcmp(text + n - e, end));
}

// Returns true if two rows are the same in their first n columns.  A
// shorter row is treated as padded with spaces.
static bool same_to(const char* a, const char* b, int n) {
    for(int col=0; col < n; col++) {
        char ca = *a ? *a++ : ' ', cb = *b ? *b++ : ' ';
        if (ca != cb) return(false);
    }
    return(true);
}

// Returns true if the Words window's definition has its marks exactly
// where more of it is hidden: [PGUP] if its first line (the one with the
// colon) is not shown, and [PGDN] if its last line (the one with the
// semicolon) is not shown.
static bool words_marked(char* why) {
    const grid* g = grid_named("source");
    const int rows = grid_rows(g);
    char text[COLS + 1], first[COLS + 1], last[COLS + 1];
    bool start = false, end = false;
    for(int row=0; row < rows; row++) {
        read_grid("source", row, text);
        if (row == 0) strcpy(first, text), start = text[0] == ':';
        if (row == rows - 1) strcpy(last, text);
        end = end || text[0] == ';';
    }
    bool up = ends_with(first, PGUP), down = ends_with(last, PGDN);
    snprintf(why, WHY, "its first line is%s shown and its last is%s, and its first row is: %s; its last: %s",
             start ? "" : " not", end ? "" : " not", first, last);
    return(up == !start && down == !end);
}

// Returns true if the definition has scrolled down correctly.  If more of
// it is still below, it must have moved by one line less than the
// window's height, so that the old last row is now the first.  Otherwise
// it must have moved as far as still fills the window, with its semicolon
// on the last row.  Rows are compared without the columns where a mark
// can be.
static bool scrolled_down(char* why) {
    const grid* g = grid_named("source");
    const int rows = grid_rows(g), upto = g->cols - (int)strlen(PGUP);
    char now[MOST_LINES][COLS + 1];
    for(int row=0; row < rows; row++) read_grid("source", row, now[row]);
    int by = 0;
    for(int k=1; k < rows && !by; k++) {
        bool same = true;
        for(int row=0; row + k < rows && same; row++) same = same_to(now[row], source[row + k], upto);
        if (same) by = k;
    }
    bool more = ends_with(now[rows - 1], PGDN);
    snprintf(why, WHY, "it moved %d rows of %d, with %s below; its first row is: %s; its last: %s",
             by, rows, more ? "more" : "nothing", now[0], now[rows - 1]);
    return(by > 0 && (more ? by == rows - 1 : now[rows - 1][0] == ';'));
}

// Returns true if the Forth window has [PGUP] and [PGDN] exactly as `up`
// and `down` say.
static bool forth_marked(bool up, bool down, char* why) {
    const grid* g = grid_named("forth");
    char first[COLS + 1], last[COLS + 1];
    read_grid("forth", 0, first);
    read_grid("forth", g->rows - 1, last);
    snprintf(why, WHY, "its first row is: %s; its last: %s", first, last);
    return(ends_with(first, PGUP) == up && ends_with(last, PGDN) == down);
}
static bool more_above(char* why) { return(forth_marked(true, false, why)); }
static bool more_both(char* why) { return(forth_marked(true, true, why)); }

static bool box_gone(char* why) {
    for(int row=0; row < grid_rows(grid_named("source")); row++) {
        char text[COLS + 1];
        read_grid("source", row, text);
        snprintf(why, WHY, "row %d of the definition is now: %s", row, text);
        if (strcmp(text, source[row])) return(false);
    }
    return(true);
}

// Returns true if exactly one cell of the Forth window is solid black,
// which is the cursor, and the last row is empty.
static bool one_cursor(char* why) {
    const grid* g = grid_named("forth");
    char last[COLS + 1];
    int solid = 0;
    for(int row=0; row < g->rows; row++) {
        for(int col=0; col < g->cols; col++) {
            uint8_t cell[ROWS], all = 0xff;
            cell_at_screen(grid_x(g) + col * 8, grid_y(g) + row * 16, cell);
            for(int r=0; r < ROWS; r++) all &= cell[r];
            solid += all == 0xff;
        }
    }
    read_grid("forth", 15, last);
    snprintf(why, WHY, "%d cells are solid, and the last row is: %s", solid, last);
    return(solid == 1 && !last[0]);
}

static bool prints_behind(char* why) {
    const grid* g = grid_named("forth");
    int wrong = picture_wrong(rect[FORTH].x, rect[FORTH].y, rect[FORTH].x + rect[FORTH].w, rect[FORTH].y + rect[FORTH].h);
    int cols = low(g->cols, (rect[VIEWER].x - grid_x(g)) / 8);     // the Forth window's columns that are left of the viewer
    char text[3][COLS + 1] = { "", "", "" };
    for(int row=0; cols > 0 && row < 3; row++) read_row(grid_x(g), grid_y(g) + (12 + row) * 16, cols, text[row]);
    snprintf(why, WHY, "%d pixels of the picture differ; %d columns can be seen, and rows 12 to 14 are: %s | %s | %s",
             wrong, cols, text[0], text[1], text[2]);
    return(!wrong && cols > 2 && !strcmp(text[0], "18") && !strcmp(text[1], "19") && !strcmp(text[2], " ok"));
}

// The disk light at the right of the bar changes whenever blocks are read
// or written, so the comparison leaves it out.  Its size is disk-corner in
// wm.fs.
static bool others_same(char* why) {
    int64_t* corner;
    table("wm.fs", "disk-corner", &corner, 2);
    int changed = 0;
    for(int y=0; y < H; y++) {
        for(int x=0; x < W; x++) {
            bool in_forth = x >= rect[FORTH].x && x < rect[FORTH].x + rect[FORTH].w &&
                            y >= rect[FORTH].y && y < rect[FORTH].y + rect[FORTH].h;
            bool in_disk = x >= W - corner[0] && y < corner[1];
            if (!in_forth && !in_disk && kept[(size_t)y * W + x] != shot[(size_t)y * W + x]) changed++;
        }
    }
    free(corner);
    snprintf(why, WHY, "%d pixels outside the Forth window and the disk light changed", changed);
    return(!changed);
}

static bool sees_all(char* why) {
    bool error = false, label = false;
    for(int row=0; row < ROWS; row++) {
        char text[COLS + 1];
        read_grid("forth", row, text);
        if (strstr(text, "? -")) error = true;          // an error is printed as ? and its number
        if (strstr(text, "wall")) label = true;
    }
    snprintf(why, WHY, "%s", error ? "an error was printed" : "wall, which has no header, is not shown by its name");
    return(!error && (label || !machine->names));
}

static bool sees_label(char* why) {
    bool error = false, named = false;
    for(int row=0; row < ROWS; row++) {
        char text[COLS + 1];
        read_grid("forth", row, text);
        if (strstr(text, "? -")) error = true;
        if (!strcmp(text, ": in-port")) named = true;
    }
    snprintf(why, WHY, "%s", error ? "an error was printed" : "no line is : in-port");
    return(!error && named);
}

// Returns true if, after the line `: until`, some line contains `code,`.
static bool sees_shared(char* why) {
    bool error = false, within = false, labelled = false;
    for(int row=0; row < ROWS; row++) {
        char text[COLS + 1];
        read_grid("forth", row, text);
        if (strstr(text, "? -")) error = true;
        if (!strcmp(text, ": until")) within = true;
        else if (within && strstr(text, "code,")) labelled = true;
    }
    snprintf(why, WHY, "%s", error ? "an error was printed" : !within ? "no line is : until" :
             "until's last word is not shown as code,");
    return(!error && labelled);
}

// Returns true if the last line is a number followed by ok.
static bool data_place(char* why) {
    char text[COLS + 1];
    regex_t is;
    read_grid("forth", 14, text);
    regcomp(&is, "^[0-9]+  ok$", REG_EXTENDED | REG_NOSUB);
    bool placed = !regexec(&is, text, 0, NULL, 0);
    regfree(&is);
    snprintf(why, WHY, "the last line is: %s", text);
    return(placed);
}

// Returns true if the last line ends with the number of blocks read,
// followed by ok.
static bool see_blocks(char* why) {
    char text[COLS + 1];
    read_grid("forth", 14, text);
    size_t len = strlen(text), at = len;
    long blocks = -1;
    if (len > 4 && !strcmp(text + len - 4, "  ok")) {
        at = len - 4;
        while (at && isdigit((unsigned char)text[at - 1])) at--;
        if (at < len - 4) blocks = strtol(text + at, NULL, 10);
    }
    snprintf(why, WHY, "how many blocks were read is not on the last line: %s", text);
    if (blocks <= 0) return(false);
    printf("note see read %ld blocks of the disk to show words-key\n", blocks);
    return(true);
}

// Reads the two allocation addresses the check printed.  Returns true if
// loading the names took memory when the disk has them, and took none when
// it does not.
static bool memory(char* why) {
    char text[COLS + 1];
    long long out = 0, in = 0;
    read_grid("forth", 14, text);
    if (sscanf(text, "%lld %lld", &out, &in) != 2) {
        snprintf(why, WHY, "where memory has got to was not read: %s", text);
        return(false);
    }
    if (CELL_BITS < 64) out &= (1ll << CELL_BITS) - 1, in &= (1ll << CELL_BITS) - 1;
    printf("note the next cell to be given out is %lld, and with the names in %lld: "
           "they take %lld cells\n", out, in, in - out);
    snprintf(why, WHY, "the next cell is %lld with the names out and %lld with them in", out, in);
    return(machine->names ? in > out : in == out);
}

// Returns how many of the menu's lines are on the screen, each at its
// window's position in the menu, in bold if the window is open and plain
// if it is closed.
static int menu_lines(void) {
    int lines = 0;
    for(int at=0; at < WINDOWS; at++) {
        int w = BACK_TO_FRONT[WINDOWS - 1 - at];
        lines += shows_text(TITLES[w], 12, 20 + 16 * at, &sans, is_open(w), false);
    }
    return(lines);
}

static bool bar_plain(void) { return(shows("hexaforth", 8, 2, &sans, false)); }

static bool menu_down(char* why) {
    int lines = menu_lines();
    bool bar_open = shows("hexaforth", 8, 2, &sans, true);
    snprintf(why, WHY, "%d of its %d lines are where they should be (%s), and the bar title \"hexaforth\" is%s inverted",
             lines, WINDOWS, "bold if open, plain if closed", bar_open ? "" : " not");
    return(lines == WINDOWS && bar_open);
}

static bool line_shown(char* why) {
    int at = menu_line(WORDS), lines = menu_lines();
    bool hot = shows_text(TITLES[WORDS], 12, 20 + 16 * at, &sans, is_open(WORDS), true);
    snprintf(why, WHY, "%d of the other lines are where they should be (expected %d), and line %d (%s) is%s highlighted",
             lines, WINDOWS - 1, at, TITLES[WORDS], hot ? "" : " not");
    return(hot && lines == WINDOWS - 1);
}

static bool picked(int n, char* why) {
    int lines = menu_lines();
    bool front = shown_in_front(n);
    snprintf(why, WHY, "%d lines of the menu are still there, and %s is%s in front at %d, %d",
             lines, NAMED[n], front ? "" : " not", rect[n].x, rect[n].y);
    return(front && !lines);
}

static bool calc_picked(char* why) {
    if (!picked(CALC, why)) return(false);
    bool plain = bar_plain();
    snprintf(why, WHY, "the bar title \"hexaforth\" is%s plain", plain ? "" : " not");
    return(plain);
}
static bool words_held(char* why) { return(picked(WORDS, why)); }
static bool menu_away(char* why) { return(picked(FORTH, why)); }
static bool forth_closed(char* why) {
    if (!picked(WORDS, why)) return(false);
    bool gone = !shown_in_front(FORTH);
    snprintf(why, WHY, "the closed Forth window is%s still shown in front", gone ? " not" : "");
    return(gone);
}
static bool words_reopened(char* why) {
    if (!picked(FORTH, why)) return(false);
    snprintf(why, WHY, "%d of %d windows are open", open_wins, WINDOWS);
    return(open_wins == WINDOWS);
}

static bool menu_whole(char* why) {
    snprintf(why, WHY, "%d of its %d lines are where they should be", menu_lines(), WINDOWS);
    return(menu_lines() == WINDOWS);
}

static bool menu_gone(char* why) {
    int lines = menu_lines();
    bool plain = bar_plain();
    snprintf(why, WHY, "%d lines of the menu are still there, and the bar title \"hexaforth\" is%s plain",
             lines, plain ? "" : " not");
    return(!lines && plain);
}

static bool canvas_right(char* why) {
    int wrong = canvas_wrong(canvas, false, 0, 0, -1);
    snprintf(why, WHY, "%d pixels of the canvas differ", wrong);
    return(!wrong);
}

static bool paint_front(char* why) {
    return(canvas_right(why) && shown_in_front(PAINT));
}

// The pointer is drawn over the shape's corner, so the comparison leaves
// that corner out.
static bool shape_shown(char* why) {
    memcpy(shaped, canvas, (size_t)cw * ch);
    drawn = shaped;
    outline(tool, from_x, from_y, to_x, to_y);
    int wrong = canvas_wrong(shaped, true, to_x, to_y, -1);
    snprintf(why, WHY, "%d pixels of the canvas differ", wrong);
    return(!wrong);
}

static uint64_t disk_cell(const uint8_t* bytes, size_t n) {     // returns cell n of some disk bytes; a cell is stored lowest byte first
    uint64_t cell = 0;
    for(int idx=CELL_BYTES - 1; idx >= 0; idx--) cell = cell << 8 | bytes[n * CELL_BYTES + idx];
    return(cell);
}

// Checks the canvas stored on the disk.  The first block records that a
// canvas is there, and its size.  The rows follow, as many whole rows to
// a block as fit.  A row is a whole number of cells, with its pixels in
// the display's layout.
static bool on_disk(char* why) {
    int stride = (cw + CELL_BITS - 1) / CELL_BITS;
    int rows_a_block = DISK_BLOCK / CELL_BYTES / stride;
    int blocks = (ch + rows_a_block - 1) / rows_a_block;
    uint8_t* bytes = malloc((size_t)(1 + blocks) * DISK_BLOCK);
    desk_turns(4);
    desk_block(DISK_CANVAS_BLOCK, 1 + blocks, bytes);
    if (disk_cell(bytes, 1) != (uint64_t)cw || disk_cell(bytes, 2) != (uint64_t)ch) {
        snprintf(why, WHY, "the disk does not say it has a canvas of %d by %d", cw, ch);
        free(bytes);
        return(false);
    }
    int differ = 0;
    for(int y=0; y < ch; y++) {
        size_t row = (size_t)(1 + y / rows_a_block) * (DISK_BLOCK / CELL_BYTES) + (size_t)(y % rows_a_block) * stride;
        for(int x=0; x < cw; x++) {
            int bit = x % CELL_BITS;
            bool set = disk_cell(bytes, row + x / CELL_BITS) >> (DISP_LEFT_LOW ? bit : CELL_BITS - 1 - bit) & 1;
            if ((DISP_INK_BLACK ? set : !set) != (bool)canvas[y * cw + x]) differ++;
        }
    }
    free(bytes);
    snprintf(why, WHY, "%d pixels of the canvas on the disk differ", differ);
    return(!differ);
}

// Drags the calculator, which is in front, to a position over the middle
// of the drawing, and back again.  Then checks that the canvas is
// repainted correctly where the calculator no longer covers it.
static bool from_disk(char* why) {
    int dx = canvas_x(100) - rect[CALC].x, dy = content_y(PAINT) - 5 - rect[CALC].y;
    int x, y, under_it = 0, seen = 0;
    title_of(CALC, 0, 0, &x, &y);
    drag(x, y, x + dx, y + dy);
    rect[CALC].x += dx, rect[CALC].y += dy;
    for(int cy=0; cy < ch; cy++) {              // count the black pixels of the drawing that the window covers
        for(int cx=0; cx < cw; cx++) under_it += canvas[cy * cw + cx] && under(canvas_x(cx), canvas_y(cy), CALC);
    }
    title_of(CALC, -dx, -dy, &x, &y);
    drag(x, y, x - dx, y - dy);
    rect[CALC].x -= dx, rect[CALC].y -= dy;
    away();
    look("16-behind");
    for(int cy=0; cy < ch; cy++) {
        for(int cx=0; cx < cw; cx++) seen += !under(canvas_x(cx), canvas_y(cy), CALC);
    }
    int wrong = canvas_wrong(canvas, false, 0, 0, CALC);
    snprintf(why, WHY, "%d of %d pixels differ; %d black pixels were under the window pulled over it",
             wrong, seen, under_it);
    return(under_it > 500 && seen > 5000 && !wrong);
}

// ---- running the checks

// The first line of block 1 of the machine's disk, as it was when the
// desktop came up.  The editor opens on that block.  The line is read
// from the disk and not assumed, because a disk the machine starts from
// has its boot source there.
static char first_line[DISK_LINE + 1];

static void block1_read(void) {
    uint8_t bytes[DISK_BLOCK];
    desk_block(DISK_SOURCE_BLOCK, 1, bytes);
    int wide = DISK_LINE;
    while (wide && (bytes[wide - 1] == ' ' || !bytes[wide - 1])) wide--;
    memcpy(first_line, bytes, wide);
    first_line[wide] = '\0';
}

// Copies a check's text to `out`, replacing %1 and %2 with the second and
// third words of the machine's dictionary as it is now (which is read if
// they are needed), and %b with the first line of block 1 of its disk.
static void with_names(const char* text, char* out, size_t len) {
    size_t n = 0;
    bool read = false;
    for(; *text && n + 1 < len; text++) {
        const char* is = text[0] == '%' && text[1] == 'b' ? first_line : NULL;
        if (text[0] == '%' && (text[1] == '1' || text[1] == '2')) {
            if (!read) words_read();
            read = true;
            is = text[1] - '0' < nwords ? words[text[1] - '0'] : "";
        }
        if (is) n += snprintf(out + n, len - n, "%s", is), text++;
        else out[n++] = *text;
    }
    out[n < len ? n : len - 1] = '\0';
}

// Returns true if a row of a window's text matches what a check expects.
// The expected text is cut to the width of the window's text, because a
// name from the dictionary can be longer than the list is wide, and the
// list shows only as much of it as fits.
static bool is_seen(const seen* s, char* why) {
    char text[COLS + 1], want[COLS + 1];
    read_grid(s->grid, s->row, text);
    with_names(s->text, want, sizeof(want));
    if (strlen(want) > (size_t)grid_named(s->grid)->cols) want[grid_named(s->grid)->cols] = '\0';
    size_t len = strlen(text), wanted = strlen(want);
    bool is = s->how == '=' ? !strcmp(text, want) :
              s->how == '^' ? !strncmp(text, want, wanted) :
              s->how == '$' ? len >= wanted && !strcmp(text + len - wanted, want) : strstr(text, want) != NULL;
    if (!is) {
        snprintf(why, WHY, "row %d of %s is to %s \"%s\" and is \"%s\"", s->row, s->grid,
                 s->how == '=' ? "be" : s->how == '^' ? "start with" : s->how == '$' ? "end with" : "have",
                 want, text);
    }
    return(is);
}

// Reads the fonts and the patterns from the Forth source.  This runs
// once, before the screen is examined.
static void begin(const desk* on) {
    if (machine) return;
    machine = on;
    mono = font_named("mono");
    sans = font_named("sans");
    for(int c=32; c < 127; c++) {
        int width, start = glyph_of(&mono, c, &width);
        for(int row=0; row < ROWS; row++) {
            for(int bit=0; bit < 8 && bit < width; bit++) glyphs[c][row] |= strike_pixel(&mono, start + bit, row) << (7 - bit);
        }
    }
    for(int n=0; n < PATTERNS_THERE; n++) patterns[n] = pattern_named(PATTERNS[n][0], PATTERNS[n][1]);
    kept = calloc(SCREEN, 1);
}

bool desk_sees(const desk* on, const uint8_t* on_screen, const char* text_of, int row, const char* text) {
    char is[COLS + 1];
    begin(on);
    shot = on_screen;
    if (!windows_start()) return(false);
    read_grid(text_of, row, is);
    return(!strcmp(is, text));
}

bool desk_begins(const desk* on) {
    begin(on);
    desk_checking = "the desktop comes up";
    boot = desk_boot();
    look(NULL);
    block1_read();
    desk_checking = NULL;
    if (!windows_start()) return(false);
    // The canvas is HexaPaint's content area without the tools down its
    // left side and the patterns along its bottom.
    cw = rect[PAINT].w - 2 - 45, ch = rect[PAINT].h - 20 - 21;
    canvas = calloc((size_t)cw * ch, 1);
    shaped = calloc((size_t)cw * ch, 1);
    return(true);
}

// Closes every window except one.  That window is then held by its title
// as if to drag it, closed with ^Q, and the button is released.  The
// window left is whichever one the earlier checks left in front.  The
// machine must keep running: it must not move a window, and must not
// store through a window that no longer exists.
static bool last_dragged(char* why) {
    while (open_wins > 1) does("^q");
    char said[64];
    snprintf(said, sizeof said, "grab:%s+40+30 ^q drop away", NAMED[in_front()]);
    does(said);
    look("18-dragged-q");
    snprintf(why, WHY, "%d windows are open", open_wins);
    return(open_wins == 0);
}

void desk_does(const char* said) {
    does(said);
}

int desk_checks(const desk* on) {
    int checks = 0, failed = 0;
    if (!desk_begins(on)) {
        printf("FAIL the Forth window is not in front where forth/desk.fs puts it, on a big screen or a small\n");
        return(1);
    }

    for(const check* c = CHECKS; c->label; c++) {
        if (c->when == (machine->names ? NO_NAMES : NAMES)) continue;
        desk_checking = c->label;
        char why[WHY] = "";
        does(c->does);
        if (c->does[0] || c->shot) look(c->shot);
        bool right = true;
        for(int n=0; n < 3 && c->sees[n].grid; n++) right = right && is_seen(&c->sees[n], why);
        if (right && c->also) right = c->also(why);
        checks++;
        if (!right) failed++;
        printf("%s %s%s%s", right ? "ok  " : "FAIL", c->label, right ? "" : ": ", right ? "" : why);
        if (!right && shot_path) printf(" (screen: %s)", shot_path);
        printf("\n");
    }
    desk_checking = NULL;
    if (!checks) failed++;      // no check ran, which is a failure
    return(failed);
}
