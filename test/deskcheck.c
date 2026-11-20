//
// What the desktop is checked for, whatever machine it is running on
// (deskcheck.h says what a machine is).
//
// A check is a row of the table below: what is done, with the keys and
// the pointer, and what is then to be on the screen.  What is done is said
// in a few words of this file's own (does(), below).  What is to be seen is
// mostly text: the windows that hold text on a grid are read back a
// character cell at a time, by matching each cell against the glyphs of
// the fixed-width font, which is read from the Forth that says it
// (tables.h).  The rest is looked at by a function of its own: the picture
// against a dither done here of the same file (dither.c), and against the
// same kept as gray and drawn through a mask as its menu asks (gray.h), HexaPaint's
// canvas against what its tools are to draw, drawn here a pixel at a time,
// and the menu, a window's title, and what may not change.
//
// The checks are in order, each from where the last left the desktop.  The
// test keeps where it expects each window to be and which is in front, as
// the checks move them, and finds every place it clicks or looks from that.
// Where the windows start is read from forth/desk.fs.
//
// The screen as it was at each check is kept as a file, for whoever wants
// to look.
//

#include <ctype.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../vm_constants.h"
#include "../bench/tiff.h"
#include "deskcheck.h"
#include "dither.h"
#include "gray.h"
#include "tables.h"
#include "../util/bluenoise.h"

#define W       DISP_W
#define H       DISP_H
#define SCREEN  ((size_t)W * H)
#define ROWS    16              // the rows of a window's text, and of a glyph
#define COLS    64              // the most columns a window's text has
#define MOST_LINES (H / ROWS)   // the most lines of text a window can show: as many as the screen

typedef struct {
    const char* grid;           // where: a window's text (GRIDS, below)
    int row;                    // which row of it
    char how;                   // = it is the text; ^ it starts with it; $ it ends with it; ~ it has it
    const char* text;           // %1 and %2 are the machine's two newest words, %e how the first ends,
} seen;                         // and %b the first line of block 1 of its disk

enum { ALWAYS, NAMES, NO_NAMES };       // whether the disk has the tables of names

typedef struct {
    const char* label;
    const char* does;           // what is done first: see does()
    const char* shot;           // the name the screen is kept under when it has been done
    seen sees[3];               // what is then to be read on the screen,
    bool (*also)(char* why);    // and what else is to be so: it says why not
    int when;
} check;

#define WHY 400                 // the most a check says of why it failed

static bool came_up(char* why), saved(char* why), dithered(char* why), moved(char* why), bold(char* why);
static bool fused_told(char* why), a_number(char* why), word_told(char* why), box_gone(char* why);
static bool scrolled_down(char* why), words_marked(char* why), more_above(char* why), more_both(char* why);
static bool one_cursor(char* why), prints_behind(char* why), keep(char* why), others_same(char* why);
static bool remember(char* why), lights_right(char* why);
static bool sees_all(char* why), memory(char* why), menu_down(char* why), line_shown(char* why);
static bool sees_label(char* why), data_place(char* why), see_blocks(char* why);
static bool calc_picked(char* why), words_held(char* why), menu_whole(char* why), menu_away(char* why);
static bool menu_gone(char* why), paint_front(char* why), canvas_right(char* why), shape_shown(char* why);
static bool on_disk(char* why), from_disk(char* why), listed_both(char* why);
static bool as_picked(char* why), picture_marked(char* why), view_on_disk(char* why), outlined(char* why);
static int picture_title(void), picture_line(const char* name);
static uint64_t disk_cell(const uint8_t* bytes, size_t n);
static void picture_picked(int line), picture_made(void);

static check CHECKS[] = {
    {"the desktop comes up", "", "1-desktop", .also = came_up},
    // The machine has two disks, each a floppy: the bar has a light for each.
    {"the bar has a light for each of the machine's two disks, and for no more", "", NULL, .also = lights_right},

    // The Forth window: text is inset by 2 from its content.
    {"the Forth window opens with a prompt", "'2 3 + .\r' ': sq dup * ; 12 sq .\r' 'nosuch\r'", "2-forth",
     {{"forth", 0, '=', "ok"}}},
    {"the Forth window evaluates", "", NULL, {{"forth", 1, '=', "2 3 + ."}, {"forth", 2, '=', "5  ok"}}},
    {"the Forth window compiles", "", NULL, {{"forth", 4, '=', "144  ok"}}},
    {"the Forth window reports errors", "", NULL, {{"forth", 6, '^', "? -13"}}},

    // The calculator: its title is clicked to bring it forward, then keys
    // and two of its buttons, 7 and *.
    {"the calculator computes (12+34)*7", "front:calc '12\r34+' click:calc+17+108 click:calc+119+132",
     "3-calculator", {{"calc", 3, '~', "322"}}},

    // The editor: brought forward, typed in on the second line of block 1, and saved.
    {"the editor shows block 1", "front:editor ^s", "4-editor", {{"editor", 0, '=', "%b"}}},
    {"the editor saves to the disk", "down '( edited )' ^s", "5-edited", .also = saved},

    // The picture: brought forward, and compared with a dither done here.
    {"the viewer shows the picture as it is dithered a pixel at a time", "front:viewer away", "6-viewer",
     .also = dithered},
    // Once dithered, the bitmap is written to the disk and painted from there.
    {"and the dithered bitmap is on the disk", "", NULL, .also = view_on_disk},
    // While a window is dragged, a gray outline of its frame follows the
    // pointer, and the window stays where it was until the button goes up.
    {"while a window is dragged, a gray outline of it follows the pointer", "grab:viewer+40+42", "7-dragging",
     .also = outlined},
    {"a window moves when it is dragged by its title", "drop away", "7-dragged", .also = moved},
    // While the viewer is in front its menu is in the bar after the
    // windows', with a mark by how the picture is drawn.  A line of it has
    // the viewer's task make the picture again, a part at each of its
    // turns, while the desktop goes on: kept as gray and drawn through blue
    // noise, darker, through Bayer's mask, coarser, lighter and finer
    // again, and dithered again.  Each is compared with the same made here
    // a pixel at a time (gray.h).
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

    // ^W brings the window at the back to the front, until it is the Words
    // window.  Its list is 2 in from its content and the definition 112.
    // sq, defined in the Forth window above, is the newest word and so the
    // first listed; a click on its name shows it.
    {"the Words window lists the dictionary", "cycle:words click:words+10+8 away", "8-words",
     {{"names", 0, '=', "sq"}, {"names", 1, '=', "%1"}, {"names", 2, '=', "%2"}}},
    // What is inside a definition is indented three columns a level.
    {"the Words window decompiles", "", NULL,
     {{"source", 0, '=', ": sq"}, {"source", 1, '=', "   dup *"}, {"source", 2, '=', ";"}}, words_marked},
    {"a word in a definition is a link", "click:words+140+24 away", "9-link",
     {{"source", 0, '=', ": dup"}, {"source", 1, '=', "   dup"}, {"source", 2, '=', ";"}}},
    {"Backspace goes back", "backspace", "10-back",
     {{"source", 0, '=', ": sq"}, {"source", 1, '=', "   dup *"}, {"source", 2, '=', ";"}}},
    {"Down moves to the next word", "down", "10-down",
     {{"source", 0, '=', ": %1"}, {"source", 1, '$', "%e"}, {"source", 2, '=', ";"}}},
    // A definition longer than the window: PgDn shows it from a window
    // less a line further down, or as far down as fills the window, and
    // PgUp from where it was.  [PGDN] ends its last row while more of it
    // is below, and [PGUP] its first while more is above.  words-key, a
    // few lines for each key the Words window takes, is longer than the
    // window on either screen; it is newer than the words of the dither,
    // which the list is paged on to next.
    {"a definition longer than the window is shown from its start", "find:words-key,words-click name:words-key",
     "10-long", {{"source", 0, '=', ": words-key"}}, remember},
    {"and says that more of it is below", "", NULL, .also = words_marked},
    {"PgDn scrolls it", "pgdn", "10-pgdn", .also = scrolled_down},
    {"and says that more of it is above", "", NULL, .also = words_marked},
    {"and PgUp scrolls it back", "pgup", "10-pgup", .also = box_gone},
    // Two words of the dither, which are next to each other in the list:
    // dither-row has fused instructions in it, and dither-start a variable
    // that has no header and a plain word that is one instruction.
    {"Right pages through the dictionary", "find:dither-row,dither-start", "10-list", .also = listed_both},
    // Words in bold are one instruction between them.  With the pointer on
    // them a box by them says which: its number, and its fields.
    {"a fused instruction is shown as its words, in bold", "name:dither-row", "10-fused",
     {{"source", 0, '=', ": dither-row"}}, bold},
    {"the pointer on a fused instruction shows the machine's form of it", "", NULL, .also = fused_told},
    {"a headerless variable is shown by its label", "name:dither-start", "10-label",
     {{"source", 0, '=', ": dither-start"}, {"source", 1, '$', " img !"}}, remember, NAMES},
    {"a headerless variable is shown as its number, with no names on the disk", "name:dither-start", "10-label",
     {{"source", 0, '=', ": dither-start"}}, a_number, NO_NAMES},
    {"and on a word that is one instruction", "", NULL, .also = word_told},
    {"and the box goes when the pointer does", "away", "10-hover-off", .also = box_gone},

    // ^W again, to the Forth window.
    {"^W cycles the windows, and the Forth window's stack is as it was",
     "cycle:forth 'depth . 7 8\r' '+ . depth .\r'", "11-cycled",
     {{"forth", 8, '=', "0  ok"}, {"forth", 10, '=', "15 0  ok"}}},
    // The Forth window is filled so that it scrolls.  The text is moved on
    // the screen, not drawn again, so what is there is looked at: the last
    // lines, the first, and that the only solid cell is the cursor.  The
    // first row ends with [PGUP] now, since more is above.
    {"the Forth window scrolls", "'0 .\r' '1 .\r' '2 .\r' '3 .\r' '4 .\r' '5 .\r' '6 .\r' '7 .\r'", "11-scrolled",
     {{"forth", 13, '=', "7 ."}, {"forth", 14, '=', "7  ok"}, {"forth", 0, '^', "0  ok"}}, one_cursor},
    {"and says that more of its text is above", "", NULL, .also = more_above},
    // The Forth window is a task of its own.  A word is started in it that
    // waits, taking turns, until another window comes to the front, and
    // then prints twenty lines.  The viewer is brought to the front: since
    // it was dragged it covers the right of the Forth window, which now
    // scrolls behind it.  The picture where it lies over the Forth window
    // must be untouched.  (The word has no name, so that the Words window
    // stays as it is.)
    {"a window behind can print and scroll",
     "':noname top begin pause dup top <> until drop 20 0 do i . cr loop ;\r' 'execute\r' front:viewer away",
     "11-behind", .also = prints_behind},
    {"the Forth window runs as a task", "front:forth", "11-task",
     {{"forth", 12, '=', "18"}, {"forth", 13, '=', "19"}, {"forth", 14, '=', " ok"}}, keep},
    // What is printed is kept as lines of their own lengths, and when there
    // is no more room the first go.  Thirty lines of forty-odd characters
    // are more than the window keeps on any machine.  Forty short lines
    // are not: PgUp shows the lines a window less a line further back,
    // with [PGDN] at the end of the last row, and PgDn the last again.
    {"the Forth window keeps its last lines",
     "':noname 30 0 do i . 40 0 do 42 emit loop cr loop ;\r' 'execute\r'", "11-rolled",
     {{"forth", 12, '^', "28 ****"}, {"forth", 13, '^', "29 ****"}, {"forth", 14, '=', " ok"}}, more_above},
    {"PgUp shows the lines before", "':noname 40 0 do i . cr loop ;\r' 'execute\r' pgup", "11-back",
     {{"forth", 0, '^', "11"}, {"forth", 13, '^', "24"}, {"forth", 14, '^', "25"}}, more_both},
    {"and PgDn the last again", "pgdn", "11-forth",
     {{"forth", 13, '^', "39"}, {"forth", 14, '=', " ok"}}, more_above},
    // The whole screen is put out of date from the prompt.  Every window is
    // painted again, each only where it can be seen, so nothing outside the
    // Forth window may change, except the disk light in the bar.
    {"a full repaint leaves the other windows as they were", "'0 0 %W %H inval-rect\r'", "12-repainted",
     .also = others_same},

    // The tables of names for the decompiler are on the disk, and are read
    // through a buffer that is kept only if they are this image's, and then
    // given back.  A disk without them says so.
    {"the tables of names load from the disk and unload", "'names-load names? . names-unload names? .\r'",
     "13-names", {{"forth", 14, '=', "-1 0  ok"}}, .when = NAMES},
    {"with no tables of names on the disk, none are loaded", "'names-load names? . names-unload names? .\r'",
     "13-names", {{"forth", 14, '=', "0 0  ok"}}, .when = NO_NAMES},
    // see has them in for as long as it runs, and what it prints
    // scrolls the window, which wants memory of its own for as long as a
    // scroll takes: there has to be room for both.  The word decompiled is
    // long enough to scroll, and near its end calls a word with no header,
    // which is shown by its name only if the names are in: words-key ends
    // with wall.
    {"see prints a word longer than the window, with the names in memory", "'see words-key\r'", "13-see",
     {{"forth", 14, '$', " ok"}}, sees_all},
    // Where memory has got to: the next cell that would be given out, with
    // the names out and in.  see scrolled the window while it had the names
    // in, and gave them back after: if it had not, they would be in still,
    // and loading them here would have taken no memory.
    {"see gives the names' memory back, though the window scrolled while it had them",
     "'0 halloc . names-load 0 halloc . names-unload\r'", "13-memory", .also = memory, .when = NAMES},
    {"with no tables of names on the disk, loading them takes no memory",
     "'0 halloc . names-load 0 halloc . names-unload\r'", "13-memory", .also = memory, .when = NO_NAMES},
    // see finds a word with no header by its label, which is on the disk,
    // and for a label on data says where the data is.
    {"see finds a word with no header by its label", "'see in-port\r'", "13-see-label", .also = sees_label,
     .when = NAMES},
    {"and a label on data by where the data is", "'see img\r'", "13-see-data", .also = data_place, .when = NAMES},
    // What it reads of the disk to show a word, which blk-moved counts.
    {"see reads the disk for the names", "'blk-moved @ see words-key blk-moved @ swap - .\r'", "13-see-blocks",
     .also = see_blocks, .when = NAMES},
    {"with no tables of names on the disk, a word with no header is not found", "'see in-port\r'",
     "13-see-label", {{"forth", 14, '^', "? -13"}}, .when = NO_NAMES},
    // The picture was read and dithered by a task of its own, a row at a
    // turn, which took itself out of the ring when the last row was made.
    // The tasks round the ring are counted: the desktop's and the Forth
    // window's.
    {"the picture's task has left the ring",
     "':noname up @ 0 begin 1+ swap @ swap over up @ = until nip ;\r' 'execute .\r'", "14-tasks",
     {{"forth", 14, '=', "2  ok"}}},

    // The title in the bar is a menu of the windows, the one in front
    // first, a line of 16 pixels each from just under the bar.  A click on
    // the title brings it down and it stays; a click on a line brings that
    // window to the front and takes the menu away.
    {"the title is a menu of the windows, the one in front first", "menu away", "15-menu", .also = menu_down},
    {"the line the pointer is on is shown so", "over:words", "15-menu-line", .also = line_shown},
    {"a click on a line brings its window to the front", "pick:calc away", "15-picked", .also = calc_picked},
    // Held down and dragged: the button goes down on the title and up on a line.
    {"and so does letting the button go on one", "hold:words away", "15-dragged", .also = words_held},
    // A window under the menu does not draw where it is.  The Forth window
    // is brought to the front and given a word that waits for the menu to
    // be down and then prints twenty lines, so that it scrolls under it:
    // the menu must be whole, and the text beside it moved.  When the menu
    // is taken away, by a press that is not on it, what was under it is
    // painted as it now is.
    {"a window under the menu prints and scrolls without drawing on it",
     "front:forth ':noname begin pause menu-open @ until 20 0 do i . cr loop ;\r' 'execute\r' menu away",
     "15-over", {{"forth", 12, '=', "18"}, {"forth", 13, '=', "19"}, {"forth", 14, '=', " ok"}}, menu_whole},
    {"a press off the menu takes it away, and what was under it is painted", "click:forth-40-40 away", "15-away",
     {{"forth", 0, '^', "6"}, {"forth", 1, '=', "7"}, {"forth", 2, '=', "8"}}, menu_away},
    {"and so does a key", "menu backspace away", "15-key", .also = menu_gone},

    // HexaPaint.  It is at the back, and is brought to the front from the
    // menu.  What is drawn in it with the pointer is drawn here too, a
    // pixel at a time, and every pixel of the canvas on the screen is
    // compared with that.  The pencil is in hand to start with, and black.
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
    // A shape is shown while it is pulled out, and is not in the canvas
    // until the button goes up.
    {"a shape is shown as it is pulled out", "tool:6 press:40,20,100,50", "16-pulling", .also = shape_shown},
    {"and is drawn when the button goes up", "release away", "16-let-go", .also = canvas_right},
    // The canvas's place is the disk, and it is in memory only while
    // HexaPaint is in front.  Another window comes to the front: the disk
    // has what was drawn, every pixel.  That window is then pulled over the
    // canvas and back, so that some of the canvas is painted again, behind
    // it: which is from the disk, since it is no longer in memory.
    {"the canvas is written to the disk when another window comes to the front", "front:calc", NULL,
     .also = on_disk},
    {"behind another window it is painted from the disk", "", "16-behind", .also = from_disk},
    {"what was drawn is there when the window has been under another", "menu pick:paint away", "16-again",
     .also = paint_front},
    // The names' buffer is memory while the Words window is drawn, and
    // the canvas is while HexaPaint is in front.  The Words window, brought
    // to the front, has its names.
    {"there is room for the canvas and the tables of names", "menu pick:words away", "17-names",
     {{"source", 1, '$', " img !"}}, .when = NAMES},
    {NULL}};

// ---- the machine, and its screen

static const desk* machine;     // what is so of the machine the checks are run on
static const char* boot;        // what its console said as the desktop came up
static const uint8_t* shot;     // its screen, as it was last looked at,
static uint8_t* kept;           // and a screen that was kept, to compare a later one with

static bool on_screen(int x, int y) { return(x >= 0 && x < W && y >= 0 && y < H); }
static bool black(int x, int y) { return(on_screen(x, y) && shot[(size_t)y * W + x]); }
static int low(int a, int b) { return(a < b ? a : b); }
static int high(int a, int b) { return(a > b ? a : b); }

// The screen, when the desktop has had time to do what was asked of it;
// kept as a file if it is given a name: a bitmap, a bit a pixel.
static void look(const char* name) {
    desk_turns(12);
    shot = desk_screen();
    if (!name) return;
    static const char form[] = "%s/%s.pbm";
    char* path = malloc(strlen(machine->out) + strlen(name) + sizeof(form));
    if (path == NULL) {
        fprintf(stderr, "there is no room for the name of a screen's file\n");
        exit(EXIT_FAILURE);
    }
    sprintf(path, form, machine->out, name);
    FILE* out = fopen(path, "wb");
    if (out == NULL) {
        perror(path);
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
    free(path);
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

static void away(void) {        // the pointer where it is off everything
    desk_pointer(W - 12, 24);
}

// ---- text on the screen

static font mono, sans;
static uint8_t glyphs[128][ROWS];       // the fixed-width font: each character's rows, a byte a row

// The sixteen bytes of the character cell at a place of the screen, a bit
// set where a pixel is black or is off the screen.
static void cell_at_screen(int x, int y, uint8_t* cell) {
    for(int row=0; row < ROWS; row++) {
        cell[row] = 0;
        for(int bit=0; bit < 8; bit++) {
            if (!on_screen(x + bit, y + row) || black(x + bit, y + row)) cell[row] |= 0x80 >> bit;
        }
    }
}

// A glyph as it is drawn in a style: plain; bold, each pixel and the one to
// its right; or leaning, the top half a pixel to the right.
static uint8_t styled(int c, int row, char style) {
    uint8_t is = glyphs[c][row];
    return(style == 'b' ? is | is >> 1 : style == 'i' && row < ROWS / 2 ? is >> 1 : is);
}

// The character whose glyph a cell is, in a style, looking at so many of
// its rows; or 0.  Of two with the same rows it is the later.
static int matched(const uint8_t* cell, char style, int rows) {
    for(int c=126; c >= 32; c--) {
        int row = 0;
        while (row < rows && styled(c, row, style) == cell[row]) row++;
        if (row == rows) return(c);
    }
    return(0);
}

// The character in a cell: as it is, or white on black; or, less its
// bottom row, underlined (a link), in bold (a fused word) or leaning (a
// label).  ? where nothing matches.
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

// A row of character cells from a place, less the blanks at its end.
static void read_row(int x, int y, int cols, char* text) {
    for(int c=0; c < cols; c++) text[c] = (char)read_cell(x + c * 8, y);
    while (cols && text[cols - 1] == ' ') cols--;
    text[cols] = '\0';
}

// Is this text on the screen at a place, in a font?  For text that is not
// on a grid: every pixel of every glyph is to be as the glyph has it, or
// with `inverse` the other.
static bool shows(const char* text, int x, int y, const font* f, bool inverse) {
    for(; *text; text++) {
        int width, start = glyph_of(f, (unsigned char)*text, &width);
        for(int row=0; row < f->rows; row++) {
            for(int k=0; k < width; k++) {
                if (!on_screen(x + k, y + row)) return(false);
                if (black(x + k, y + row) != (strike_pixel(f, start + k, row) != inverse)) return(false);
            }
        }
        x += width;
    }
    return(true);
}

// The disk lights (wm.fs): a floppy at the right of the bar for each disk
// the machine has, the first disk's rightmost and each next one a light's
// width to its left, which is the first number of disk-corner.  A floppy is
// one of the first four glyphs of the font spin, a quarter turn apart, ten
// pixels in from the left of its light and two down.  The machine has two
// disks, the one it is given or starts from and the pictures': so there is
// to be a floppy for each of those, and not a pixel where a third's would
// be.  (A machine once showed sixteen, and nothing looked.)
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

// The windows, in the order desk.fs says where each goes; their titles;
// and where each is expected to be, and which are in front of which, as
// the checks move them.
enum { VIEWER, EDITOR, WORDS, CALC, FORTH, PAINT, WINDOWS };
static const char* NAMED[] = { "viewer", "editor", "words", "calc", "forth", "paint" };
static const char* TITLES[] = { "Mandrill", "Editor", "Words", "Calculator", "Forth", "HexaPaint" };
static struct { int x, y, w, h; } rect[WINDOWS];
static int order[WINDOWS];      // from the back to the front

static int window(const char* name, size_t len) {
    for(int n=0; n < WINDOWS; n++) {
        if (strlen(NAMED[n]) == len && !strncmp(NAMED[n], name, len)) return(n);
    }
    fprintf(stderr, "deskcheck: there is no window called %.*s\n", (int)len, name);
    exit(EXIT_FAILURE);
}

// The top left of what is inside a window's frame, under its title.
static int content_x(int n) { return(rect[n].x + 1); }
static int content_y(int n) { return(rect[n].y + 19); }

static int in_front(void) { return(order[WINDOWS - 1]); }

static void to_front(int n) {
    int at = 0;
    while (order[at] != n) at++;
    for(; at < WINDOWS - 1; at++) order[at] = order[at + 1];
    order[WINDOWS - 1] = n;
}

// A window's line of the menu: the one in front is the first.
static int menu_line(int n) {
    int at = 0;
    while (order[WINDOWS - 1 - at] != n) at++;
    return(at);
}

// Is a place of the screen under a window, or just outside its frame?
static bool under(int x, int y, int n) {
    return(x >= rect[n].x - 2 && x < rect[n].x + rect[n].w + 4 && y >= rect[n].y - 2 && y < rect[n].y + rect[n].h + 4);
}

// A point of a window's title bar that no window in front covers, from
// which the pointer can go dx and dy further and still be on the screen.
// The bar is 18 rows under the frame's top, and a press on any of them is
// on the title: its middle row is tried first, then the others.
static void title_of(int n, int dx, int dy, int* tx, int* ty) {
    static const int ROWS_OF_TITLE[] = { 8, 4, 12, 2, 16 };
    int at = 0;
    while (order[at] != n) at++;
    for(int r=0; r < (int)(sizeof(ROWS_OF_TITLE) / sizeof(ROWS_OF_TITLE[0])); r++) {
        *ty = rect[n].y + ROWS_OF_TITLE[r];
        for(*tx = rect[n].x + rect[n].w - 12; *tx > rect[n].x + 4; *tx -= 4) {
            if (!(*tx >= 0 && *tx < W - 8 - dx && *ty >= 20 && *ty < H - 12 - dy)) continue;
            bool covered = false;
            for(int ahead=at + 1; ahead < WINDOWS; ahead++) {
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

// Is a window's title shown as that of the window in front is?
static bool shown_in_front(int n) {
    return(shows(TITLES[n], rect[n].x + 8, rect[n].y + 2, &sans, true));
}

// Where the windows start: desk.fs has two arrangements, for a big screen
// and a small, each a row a window of left, top, width and height.  The
// one this machine has is the one whose Forth window is where the screen
// shows a window in front.  HexaPaint is at the back, and the rest are
// from the back in the table's order.
static bool windows_start(void) {
    static const char* ARRANGED[] = { "big-desk", "small-desk" };
    static const int BACK_TO_FRONT[] = { PAINT, VIEWER, EDITOR, WORDS, CALC, FORTH };
    for(int which=0; which < (int)(sizeof(ARRANGED) / sizeof(ARRANGED[0])); which++) {
        int64_t* at;
        table("desk.fs", ARRANGED[which], &at, 4 * WINDOWS);    // each window's corner, and its width and height
        for(int n=0; n < WINDOWS; n++) {
            rect[n].x = (int)at[4 * n], rect[n].y = (int)at[4 * n + 1];
            rect[n].w = (int)at[4 * n + 2], rect[n].h = (int)at[4 * n + 3];
            order[n] = BACK_TO_FRONT[n];
        }
        free(at);
        if (shown_in_front(FORTH)) return(true);
    }
    return(false);
}

// ---- the windows' text

typedef struct {
    const char* name;
    int window;
    int dx, dy;                 // where its first cell is, from the window's content
    int cols, rows;             // rows: 0 for as many as the window is tall
} grid;

static const grid GRIDS[] = {
    {"forth", FORTH, 2, 2, 50, 16},
    {"calc", CALC, 6, 4, 14, 5},
    {"editor", EDITOR, 2, 2, 64, 16},
    {"names", WORDS, 2, 0, 13, 0},          // the Words window's list,
    {"source", WORDS, 112, 0, 45, 0},       // and the definition beside it
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

// The rows a window's text has: as many as it says, or as many lines as
// its content is tall.
static int grid_rows(const grid* g) {
    int rows = g->rows ? g->rows : (rect[g->window].h - 20) / ROWS;
    return(rows < MOST_LINES ? rows : MOST_LINES);
}

static void read_grid(const char* name, int row, char* text) {
    const grid* g = grid_named(name);
    read_row(grid_x(g), grid_y(g) + row * 16, g->cols, text);
}

// ---- HexaPaint's canvas, drawn in as paint.fs draws in it

// The patterns along the bottom of its window, in their order there.
static const char* PATTERNS[][2] = { {"display.fs", "p-white"}, {"display.fs", "p-black"},
                                     {"display.fs", "p-dkgray"}, {"display.fs", "p-gray"},
                                     {"display.fs", "p-ltgray"}, {"paint.fs", "p-brick"},
                                     {"paint.fs", "p-diag"}, {"paint.fs", "p-dots"} };
enum { WHITE, BLACK };
#define PATTERNS_THERE ((int)(sizeof(PATTERNS) / sizeof(PATTERNS[0])))
static pattern patterns[PATTERNS_THERE];

// The tools, two to a row down the left of its window.
enum { PENCIL, BRUSH, ERASER, LINE, RECT, RECT_FILLED, OVAL, OVAL_FILLED };

static int cw, ch;              // the canvas, in pixels;
static uint8_t* canvas;         // its pixels as they should be, 1 if black;
static uint8_t* shaped;         // and the same with a shape that is being pulled out
static uint8_t* drawn;          // the one being drawn in
static const pattern* fill;     // what a run is filled with,
static int pen;                 // and how many pixels across and down a dot is
static int tool, in_hand = BLACK;       // the tool and the pattern in hand
static int from_x, from_y, to_x, to_y;  // a shape that is being pulled out

// The pixels of a row from x0 up to x1, in the pattern.
static void span(int x0, int x1, int y) {
    if (y < 0 || y >= ch) return;
    for(int x=high(x0, 0); x < low(x1, cw); x++) drawn[y * cw + x] = pattern_pixel(fill, x, y);
}

static void dot(int x, int y) {
    for(int row=y; row < y + pen; row++) span(x, x + pen, row);
}

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

// The rectangle two corners make, and an oval in it: half its width and
// height, its middle, and how far it reaches at each row from the middle.
static int bx0, by0, bx1, by1;
static int half_w, half_h, mid_x, mid_y;
static int* reach;              // a row each, from the middle to the edge and one more
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

// A shape's outline, which is black and a pixel wide.
static void outline(int how, int x0, int y0, int x1, int y1) {
    fill = &patterns[BLACK], pen = 1;
    box(x0, y0, x1, y1);
    if (how == LINE) line_of_dots(x0, y0, x1, y1);
    else if (how < OVAL) rect_frame();
    else oval_frame();
}

// What a tool does, pulled from a place to another with a pattern in hand.
// The pencil, the brush and the eraser leave a line of dots.
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

// A place of the canvas, on the screen: the tools are 45 across.
static int canvas_x(int x) { return(content_x(PAINT) + 45 + x); }
static int canvas_y(int y) { return(content_y(PAINT) + y); }

// The pixels of the canvas on the screen that are not as they should be,
// but for a box round a place if one is given, and for what is under a
// window if one is given.
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

// ---- what is done

static char names[MOST_LINES][COLS + 1];        // the Words window's list, when it was last looked for names in
static bool both;                               // and whether both of those looked for were in it

static int listed(const char* name) {   // the row a name is on, or -1
    for(int row=0; row < grid_rows(grid_named("names")); row++) {
        if (!strcmp(names[row], name)) return(row);
    }
    return(-1);
}

// Numbers after a word, with commas between or signs before.
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

// A window being dragged (grab:): which, where the pointer pressed on its
// title, how far it was moved, and the screen just before the press.
static struct { int w, x, y, dx, dy; uint8_t before[SCREEN]; } grabbed;

// What a check does, a word at a time:
//
//   'text'             typed, a key a character; \r is the return key
//   ret down backspace pgdn ...    a key, by its name
//   ^s                 a key with Ctrl held
//   front:NAME         a window's title is clicked, which brings it to the front
//   cycle:NAME         ^W, which brings the window at the back, until it is that one
//   drag:NAME+DX+DY    a window is dragged by its title
//   grab:NAME+DX+DY    the same, but the button is still down
//   drop               and it goes up
//   click:NAME+X+Y     a click in a window, from the top left of its content;
//   click:NAME-X-Y     or back from the bottom right of its frame
//   point:X,Y          the pointer is put at a place of the screen
//   at:NAME+X+Y        or of a window, from the top left of its content
//   away               the pointer is put where it is off everything
//   menu               the title in the bar is clicked, which brings the menu down
//   pick:NAME          a window's line of the menu is clicked
//   hold:NAME          the button goes down on the title and up on a window's line
//   over:NAME          the pointer is put on a window's line of the menu
//   pmenu              the picture's title in the bar is clicked, which brings its menu down
//   picture:LINE       a line of the picture's menu is picked (atkinson, ordered, blue, finer,
//                      coarser, darker or lighter), and the picture looked at until it is so
//   find:A,B           Right, until the Words window lists both names
//   name:A             a name in the Words window's list is clicked
//   tool:N pattern:N   HexaPaint's tool or pattern is clicked
//   pull:X0,Y0,X1,Y1   the button down at a place of the canvas, the pointer to another, the button up
//   press:X0,Y0,X1,Y1  the same, and the button is still down
//   release            and it goes up
//
// %W and %H in what is typed are the screen's width and height.
static void does(const char* said) {
    for(const char* at = said; ; ) {
        while (*at == ' ') at++;
        if (!*at) return;
        // a word of it, with room for all that is said and for each %W or
        // %H as a number, which is no more digits than three a byte
        static char* word;
        const size_t room = strlen(said) * (1 + 3 * sizeof(int)) + 1;
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
            desk_type(word);
            desk_turns(4);
            at += len + 2;
            continue;
        }
        len = strcspn(at, " ");
        memcpy(word, at, len);
        word[len] = '\0';
        at += len;

        const char* of = strchr(word, ':');     // what the word is done to: a name, or numbers, or both
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
        } else if (!strncmp(word, "front:", 6)) {
            int w = window(of, name);
            title_of(w, 0, 0, &x, &y);
            click(x, y);
            to_front(w);
        } else if (!strncmp(word, "cycle:", 6)) {
            int w = window(of, name);
            for(int turn=0; turn < WINDOWS && in_front() != w; turn++) {
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
            // the two names, in what was said: the first ends where the comma was
            size_t comma = strcspn(of, ",");
            char* first = word + (of - word);
            const char* second = of[comma] ? of + comma + 1 : "";
            first[comma] = '\0';
            // a page of the list at a time, until both are on one, or a
            // page down shows the page there was: that was the last.  Two
            // that are next to each other may be the last of a page and the
            // first of the next: when one is on the last line and the other
            // is not listed, the choice goes down a line at a time, which
            // moves the list a line once the choice is on its last line.
            static char before[MOST_LINES][COLS + 1];
            const int rows = grid_rows(grid_named("names"));
            bool last = false;
            for(int page=0; !last; page++) {
                look("10-list");
                memcpy(before, names, sizeof(names));
                for(int row=0; row < rows; row++) read_grid("names", row, names[row]);
                both = listed(first) >= 0 && listed(second) >= 0;
                for(int line=0; !both && line < rows && (listed(first) == rows - 1 || listed(second) == rows - 1 ||
                                                         (line && (listed(first) >= 0 || listed(second) >= 0))); line++) {
                    desk_key("down", NULL);
                    look("10-list");
                    for(int row=0; row < rows; row++) read_grid("names", row, names[row]);
                    both = listed(first) >= 0 && listed(second) >= 0;
                }
                last = both || (page && !memcmp(before, names, sizeof(names)));
                if (!last) desk_key("right", NULL);
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

// ---- what else is to be so

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

// The picture as it is to be shown.  A window smaller than its picture
// shows it at half its size, each pixel the gray of four, to the nearest.
// If that is still more than the window shows, it shows the middle: the
// rows in the middle, and across from the middle or the start of the cell
// of the display that is in.
static uint8_t* dither_black;   // a byte a pixel of the picture, 1 if black;
static int pw, ph;              // how big it is, at the size it is shown;
static int kw, kh, px0, py0;    // and the part of it that the window shows
static bool half;               // whether it is shown at half its size
static uint8_t* shown_black;    // the picture as it is to be shown now: as dithered, or as the menu has it

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

// The pixels of the picture, between two corners of the screen, that are
// not as it is to be shown; and where they are in the picture as shown:
// the first, and the least and most of each way.
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

// The viewer's bitmap on the disk, from DISK_VIEW_BLOCK: as many whole rows
// to a block as fit, laid out as the display is.  Every pixel is compared
// with the part of the dither the window shows.
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

// The picture's menu (the end of forth/view.fs), as the checks pick its lines: how
// the picture is drawn (dithered, through Bayer's mask, or through blue
// noise), how many of its pixels a side a sample of the gray is as drawn,
// as a shift, and what is added to the mask's thresholds.  A sample is 4
// by 4 of the picture's pixels to start with.
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

// Where the picture's title starts in the bar: after the windows'.
static int picture_title(void) { return(text_width("hexaforth", &sans) + 16); }

static int picture_line(const char* name) {
    for(int line=0; line < PICTURE_LINE_COUNT; line++) if (!strcmp(name, PICTURE_LINES[line])) return(line);
    fprintf(stderr, "deskcheck: the picture's menu has no line %s\n", name);
    exit(EXIT_FAILURE);
}

static void picture_picked(int line) {
    picture();
    if (pic_d < 0) pic_d = half ? 1 : 2;
    if (line < 3) pic_mode = line;
    else if (line == 3) pic_d = high(pic_d - 1, 0);
    else if (line == 4) pic_d = low(pic_d + 1, 2);
    else pic_br = high(low(pic_br + (line == 5 ? 1 : -1), 15), -15);
}

// The picture as the menu has it drawn: kept as gray at its own size, a
// sample to 1 << (d + half) of its pixels a side, in 4 bits, and drawn
// through the mask, a sample to 1 << d pixels a side.  Blue noise is of 64
// by 64, or 32 by 32 where a cell is 16 bits: its thresholds as what makes
// the disk makes them (util/bluenoise.h), and not as the disk has them.
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
    static uint8_t* masks[65];      // by n, made once
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

// The picture is made again by the viewer's task while the desktop goes
// on: it is looked at until it is as picked, which is when the task has
// made its last rows and left the ring, or until it has had so many turns.
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

// The menu down, with a mark by how the picture is drawn and by no other line.
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

// While a window is grabbed, the screen is as it was before the press,
// except on the one-pixel frame of the window moved by how far the pointer
// went, where it is inverted wherever the gray pattern has ink (wm.fs,
// frame-xor).  The pointer, before and after it moved, and the disk light
// are left out.
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
    snprintf(why, WHY, "its title is not at %d, %d", rect[VIEWER].x + 8, rect[VIEWER].y + 2);
    return(shown_in_front(VIEWER));
}

static bool listed_both(char* why) {
    snprintf(why, WHY, "the list starts %s, %s, %s, %s", names[0], names[1], names[2], names[3]);
    return(both);
}

// The first character in bold of the definition: its row and column.
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

// The pointer is put on a word of the definition, and the box that says
// what instruction it is is read.  The box is a line down, or on the last
// two lines over the word, and starts under the word unless that would
// take it past the edge of the window, so its text is looked for from
// there leftwards.  Is what it says the machine's form of an instruction?
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

// The definition as it is, to look in and to see that it is so again; and
// the screen as it is, to see what of it changes.
static char source[MOST_LINES][COLS + 1];
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

// A word that is an instruction by itself says so too: the dup that the
// first line of dither-start has.
static bool word_told(char* why) {
    const char* dup = strstr(source[1], "dup");
    snprintf(why, WHY, "the first line has no dup: %s", source[1]);
    return(dup && told(1, (int)(dup - source[1]), "10-hover-word", "^[0-9A-F]{4} T->IN IN-> ->T d\\+1", why));
}

// A window that shows part of a text ends its first row with [PGUP] when
// more of the text is above, and its last with [PGDN] when more is below.
static const char PGUP[] = "[PGUP]", PGDN[] = "[PGDN]";

static bool ends_with(const char* text, const char* end) {
    size_t n = strlen(text), e = strlen(end);
    return(n >= e && !strcmp(text + n - e, end));
}

// The first n columns of two rows, where a row that is shorter has spaces.
static bool same_to(const char* a, const char* b, int n) {
    for(int col=0; col < n; col++) {
        char ca = *a ? *a++ : ' ', cb = *b ? *b++ : ' ';
        if (ca != cb) return(false);
    }
    return(true);
}

// The marks the Words window's definition has are where more of it is:
// [PGUP] if its first line, the colon's, is not shown, and [PGDN] if its
// last, the semicolon's, is not.
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

// The definition from further down: a window less a line, so that its
// last row is now its first, if more of it is still below; or else as far
// as fills the window, with its semicolon on the last row.  The rows are
// compared but for the columns a mark may be in.
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

// The marks the Forth window has: [PGUP] and [PGDN], as they are to be.
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

// The only cell of the Forth window that is all black is the cursor, and
// the last row has nothing on it.
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
    int cols = low(g->cols, (rect[VIEWER].x - grid_x(g)) / 8);     // of the Forth window, left of the viewer
    char text[3][COLS + 1] = { "", "", "" };
    for(int row=0; cols > 0 && row < 3; row++) read_row(grid_x(g), grid_y(g) + (12 + row) * 16, cols, text[row]);
    snprintf(why, WHY, "%d pixels of the picture differ; %d columns can be seen, and rows 12 to 14 are: %s | %s | %s",
             wrong, cols, text[0], text[1], text[2]);
    return(!wrong && cols > 2 && !strcmp(text[0], "18") && !strcmp(text[1], "19") && !strcmp(text[2], " ok"));
}

// The disk light at the right of the bar changes whenever blocks are read
// or written, so it is left out.  Its size is disk-corner in wm.fs.
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
        if (strstr(text, "? -")) error = true;          // an error is ? and its number
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

// The last line is a number, and then ok.
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

// The last line ends with how many blocks were read, and then ok.
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

// The menu's lines that are on the screen, each where its window's is to
// be: how many of them, and whether they are all of them.
static int menu_lines(void) {
    int lines = 0;
    for(int at=0; at < WINDOWS; at++) lines += shows(TITLES[order[WINDOWS - 1 - at]], 12, 20 + 16 * at, &sans, false);
    return(lines);
}

static bool bar_plain(void) { return(shows("hexaforth", 8, 2, &sans, false)); }

static bool menu_down(char* why) {
    snprintf(why, WHY, "%d of its %d lines are where they should be", menu_lines(), WINDOWS);
    return(menu_lines() == WINDOWS && shows("hexaforth", 8, 2, &sans, true));
}

static bool line_shown(char* why) {
    int at = menu_line(WORDS);
    snprintf(why, WHY, "%d lines are plain, and line %d is to be the other", menu_lines(), at);
    return(shows(TITLES[WORDS], 12, 20 + 16 * at, &sans, true) && menu_lines() == WINDOWS - 1);
}

static bool picked(int n, char* why) {
    snprintf(why, WHY, "%d lines of the menu are still there, and %s is%s in front", menu_lines(), NAMED[n],
             shown_in_front(n) ? "" : " not");
    return(shown_in_front(n) && !menu_lines());
}

static bool calc_picked(char* why) { return(picked(CALC, why) && bar_plain()); }
static bool words_held(char* why) { return(picked(WORDS, why)); }
static bool menu_away(char* why) { return(picked(FORTH, why)); }

static bool menu_whole(char* why) {
    snprintf(why, WHY, "%d of its %d lines are where they should be", menu_lines(), WINDOWS);
    return(menu_lines() == WINDOWS);
}

static bool menu_gone(char* why) {
    snprintf(why, WHY, "%d lines of the menu are still there", menu_lines());
    return(!menu_lines() && bar_plain());
}

static bool canvas_right(char* why) {
    int wrong = canvas_wrong(canvas, false, 0, 0, -1);
    snprintf(why, WHY, "%d pixels of the canvas differ", wrong);
    return(!wrong);
}

static bool paint_front(char* why) {
    return(canvas_right(why) && shown_in_front(PAINT));
}

// The pointer is on the shape's corner, so that is left out.
static bool shape_shown(char* why) {
    memcpy(shaped, canvas, (size_t)cw * ch);
    drawn = shaped;
    outline(tool, from_x, from_y, to_x, to_y);
    int wrong = canvas_wrong(shaped, true, to_x, to_y, -1);
    snprintf(why, WHY, "%d pixels of the canvas differ", wrong);
    return(!wrong);
}

static uint64_t disk_cell(const uint8_t* bytes, size_t n) {     // a cell of the disk's, its lowest byte first
    uint64_t cell = 0;
    for(int idx=CELL_BYTES - 1; idx >= 0; idx--) cell = cell << 8 | bytes[n * CELL_BYTES + idx];
    return(cell);
}

// The canvas as the disk has it.  A block says it is there, and its size;
// then its rows, as many whole rows to a block as a block holds, a row a
// whole number of cells with its pixels as the display has them.
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

// The calculator, which is in front, is pulled to where it is over the
// middle of what was drawn, and back.
static bool from_disk(char* why) {
    int dx = canvas_x(100) - rect[CALC].x, dy = content_y(PAINT) - 5 - rect[CALC].y;
    int x, y, under_it = 0, seen = 0;
    title_of(CALC, 0, 0, &x, &y);
    drag(x, y, x + dx, y + dy);
    rect[CALC].x += dx, rect[CALC].y += dy;
    for(int cy=0; cy < ch; cy++) {              // of the drawing, what the window is over
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

// ---- the checks, run

// The first line of block 1 of the machine's disk, as it was when the
// desktop came up: what the editor opens on.  It is the disk's to say: a
// disk the machine starts from has there what its boot loads.
static char first_line[DISK_LINE + 1];

static void block1_read(void) {
    uint8_t bytes[DISK_BLOCK];
    desk_block(DISK_SOURCE_BLOCK, 1, bytes);
    int wide = DISK_LINE;
    while (wide && (bytes[wide - 1] == ' ' || !bytes[wide - 1])) wide--;
    memcpy(first_line, bytes, wide);
    first_line[wide] = '\0';
}

// A text a check has, with what the machine says of itself in it.
static void with_names(const char* text, char* out, size_t len) {
    size_t n = 0;
    for(; *text && n + 1 < len; text++) {
        const char* is = text[0] != '%' ? NULL : text[1] == '1' ? machine->newest[0] :
                         text[1] == '2' ? machine->newest[1] : text[1] == 'e' ? machine->ending :
                         text[1] == 'b' ? first_line : NULL;
        if (is) n += snprintf(out + n, len - n, "%s", is), text++;
        else out[n++] = *text;
    }
    out[n < len ? n : len - 1] = '\0';
}

static bool is_seen(const seen* s, char* why) {
    char text[COLS + 1], want[COLS + 1];
    read_grid(s->grid, s->row, text);
    with_names(s->text, want, sizeof(want));
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

// What is read from the Forth before anything is looked at: the fonts and
// the patterns.
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
    boot = desk_boot();
    look(NULL);
    block1_read();
    if (!windows_start()) return(false);
    // The canvas is HexaPaint's content, less its tools down the left and
    // its patterns along the bottom.
    cw = rect[PAINT].w - 2 - 45, ch = rect[PAINT].h - 20 - 21;
    canvas = calloc((size_t)cw * ch, 1);
    shaped = calloc((size_t)cw * ch, 1);
    return(true);
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
        char why[WHY] = "";
        does(c->does);
        if (c->does[0] || c->shot) look(c->shot);
        bool right = true;
        for(int n=0; n < 3 && c->sees[n].grid; n++) right = right && is_seen(&c->sees[n], why);
        if (right && c->also) right = c->also(why);
        checks++;
        if (!right) failed++;
        printf("%s %s%s%s\n", right ? "ok  " : "FAIL", c->label, right ? "" : ": ", right ? "" : why);
    }
    if (!checks) failed++;      // nothing was checked
    return(failed);
}
