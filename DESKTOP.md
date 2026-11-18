# The desktop

How to start the desktop, how to use it, and how to write a program for it.
`forth/wm.fs` is the window manager and describes itself at its top;
`forth/desk.fs` opens the windows.

![The desktop, with HexaPaint in front](doc/desktop.png)

The target `screenshot` makes that picture, from a screen saved by the
desktop's test.

## Starting it

    cmake --build build/cmake --target demo

boots the small AArch64 kernel on QEMU with a display, a disk, a keyboard
and a pointer. The kernel runs the word `cold` when it starts, and `cold`
runs `desktop`, which brings up QEMU's devices and runs `windows`, which
opens the windows and runs the event loop. Whatever is typed at QEMU's
serial console also goes to the window in front.

A machine with other devices brings them up itself and then runs `windows`.
A machine with only a console uses the console's keys.

The reference VM has no display. The desktop's test drives the desktop on it
and saves a screen after each step (`build/64/desktop/`), and the tools
`screen-64` and `png` make a picture of the display from the memory the VM
writes out when it stops (the target `seen` builds them).

## Using it

- Clicking on a window brings it to the front. The window in front gets the
  keys
- Pressing on a window's title, dragging, and letting go moves the window
  by as far as the pointer moved
- Pressing on `hexaforth`, at the top left, drops down a menu of the
  windows, the front one first. Letting go on a line brings that window to
  the front. Letting go on the title leaves the menu open, and the next click
  either picks a line or closes the menu
- `^W` brings the window at the back to the front
- `^Q` leaves the desktop for the serial prompt

At the right of the menu bar is the disk light: a floppy whose spindle
turns while blocks are being read or written, with an up arrow beside it
after a read and a down arrow after a write. The arrow goes away once the
disk has been idle for a few turns of the event loop.

The windows:

- **Forth**: a prompt. It runs as its own task, so its stack is kept from
  one line to the next and the desktop keeps going while a line runs
- **Editor**: one block of the disk, 16 lines of 64 characters. Typing
  overwrites, the cursor keys move, PgUp and PgDn change block, `^S` writes
  the block, and `^L` writes it and loads it as Forth
- **Mandrill**: a picture read from a TIFF on the disk and dithered. A task
  does it a row at a time, and the picture appears down the window. Once
  it is dithered, the bitmap is written to the disk and its memory freed;
  the window is then repainted from the disk, reading only the blocks it
  needs. The Picture menu in the bar redraws it in other ways: dithered,
  or kept as gray and drawn through Bayer's mask or blue noise, finer or
  coarser, darker or lighter
- **Calculator**: digits make a number; Enter or the space bar pushes it, or
  duplicates the top if no number is being typed; `+ - * /` work on the top
  two; `n` negates, `x` swaps, `d` drops, Backspace removes a digit. The
  buttons send the same keys
- **Words**: the dictionary on the left and a decompiled definition on the
  right. Up and Down move through the dictionary, and PgUp and PgDn move by
  a page. Clicking a name shows it, clicking an underlined word follows it,
  and Backspace goes back. Left and Right scroll a long definition. With the
  pointer on a word, a box shows which instruction it is, by its fields
- **HexaPaint**: the tools are down the left: pencil and brush, eraser and
  line, rectangle and filled rectangle, oval and filled oval. The patterns
  are along the bottom. The drawing is saved on the disk

The editor and the picture are only opened if the machine has a disk.

## Writing a program for it

A window is made by `window`:

    window ( x y w h title draw key click -- win )

`x y w h` are the window's position on the screen and its size, including
the frame and title. `title` is a counted string starting on a cell
boundary. The other three are the window's words, and `click` may be 0:

    draw   ( win -- )         paint what is inside the frame
    key    ( c win -- )       a key, while this window is in front
    click  ( x y win -- )     the button went down inside the frame

The `x y` of a click are in the window's own coordinates, the same ones
`draw` paints in: measured from the top left inside the frame. A window
that takes no keys uses `ignore-key`.

In the drawing words, a port is a record of how drawing is done: where the
origin is, what it is clipped to, the pattern, the font. This is a
different thing from a port of the machine (`MODEL.md`). The screen has
one, and each window has its own, which the window manager switches to
before it runs `draw`.

### Drawing

The event loop runs `draw` to paint whatever part of the window is out of
date and visible. Before running it, the window manager has made the inside
of the frame white, set the clip to the area to be painted, and put the
origin at the top left inside the frame, below the title. `draw` paints the
whole window, in the window's own coordinates, and anything that doesn't
need painting is clipped away. The inside is 2 pixels narrower than the
window and 20 pixels shorter.

The drawing words, all in `display.fs` and `gfx.fs`. Where one takes a
flag, true is white and false is black:

    fill-rect   ( flag x y w h -- )
    frame-rect  ( flag x y w h -- )     a one-pixel outline
    hline       ( flag x y n -- )
    pat-rect    ( x y w h -- )          in the current pattern: p-gray pattern !
    draw-char   ( c x y -- )
    draw-text   ( addr u x y -- )       text at a byte address
    draw-chars  ( a i u x y -- )        u characters starting at character i of the cells at a
    copy-bits   ( bitmap sx sy dx dy w h -- )
    plot        ( flag x y -- )

Each of the three window words takes `win` off the stack, whether or not it
uses it.

`fill-rect`, `frame-rect` and `hline` leave plain white or black as the
pattern, so set the pattern again before the next `pat-rect`. The patterns
are `p-white`, `p-black`, `p-gray`, `p-ltgray` and `p-dkgray`. `pat-rect`
draws in the current mode: `copy-mode`, which a window's port starts in,
`or-mode`, `xor-mode` or `bic-mode`.

`mono font !` and `sans font !` choose the font. A port starts with the
system's plain font, which on the desktop is `mono`, so that is what `draw`
gets. Both fonts are 16 pixels high. A `mono` character is 8 pixels wide; in
`sans` each character has its own width, and `chars-width ( a i u -- n )`
and `text-width ( addr u -- n )` give the width of some text. The `x y` of
text is its top left. In `draw`, text adds its black to what is already
there and leaves the rest alone, since the window has just been made white:
`text-or` is true. `0 text-or !` makes text paint its white as well, which
is what text over a pattern needs.

What `.` and `emit` print can be drawn by sending it to a buffer first:
`sb-begin`, the printing words, `sb-end`, and then `sb-draw ( x y -- )`. The
buffer holds 63 characters, and any more are dropped.

### Marking what has changed

A program draws only in `draw`. Code that changes something marks what is
out of date, and the event loop repaints it:

    win-inval ( x y w h win -- )        a rectangle inside the frame

When a `key` or `click` word returns, its whole window is marked out of
date, unless the word called `win-inval` itself. In that case only what it
marked is repainted, and it may mark several rectangles: they add up. The
calculator's keys change its numbers but not its buttons, so `calc-key`
marks just the numbers' rectangle, and the buttons on screen are left as
they are.

### The pointer, the front window, and per-window data

- To follow the pointer while the button is down, `click` stores a word of
  `( x y win -- )` in `'track`. It is run every time the pointer moves, and
  once more when the button goes up, with `held` false. HexaPaint draws
  this way
- To be told whenever the pointer moves, a program stores a word of
  `( -- )` in `'moved`. The Words window shows its box this way
- To draw over its window straight onto the screen, as the pointer is
  drawn, a program stores a word of `( on? -- )` in `'overlay`. It is
  run with false before anything is painted, to take off what it drew,
  and with true after, to put it back. To change what it shows, it calls
  `overlays-off`, changes it, and calls `overlays-on`. It draws in its
  window with `win-port ( win -- )`, which shows only where the window
  can be seen, and goes back with `screen-port @ port!`.
  `keep-bits ( bitmap x y w h -- )` keeps the screen's cells under a
  rectangle and `put-bits` puts them back. The Words window's box is
  drawn this way, so moving it does not paint the window again
- To be told when its window comes to the front, a program stores a word of
  `( win -- )` in the window's `win-front ( win -- a )`; to be told when
  another window comes in front of it, one in `win-back ( win -- a )`. Each
  is 0 until a word is stored there. HexaPaint reads its canvas from the
  disk when it comes to the front, and saves it and gives back its memory
  when it goes behind, this way
- `win-data ( win -- a )` is a cell of the window's record for the
  program's own use. The picture viewer keeps its record there

### Long-running work

The event loop runs each of a window's words to completion, so a slow one
holds up the whole desktop. Such work is given to a task (`spawn`), which
does a piece of it and then calls `pause`. `view.fs` reads and dithers its
picture this way.

### Memory

`halloc ( u -- a )` allocates u bytes, in whole cells, and returns the
address of a cell. `hgive ( a cells -- )` gives it back. `hreuse` is
`halloc` that reuses memory that was given back, if there is enough of it.

### An example

`doc/hello.fs` is a window that counts the keys it gets:

    create t-hello  34 parse Hello" s,
    variable presses

    : hello-draw ( win -- )
       drop  false 8 8 120 40 frame-rect  presses @ '0' + 16 20 draw-char ;

    : hello-key ( c win -- )
       swap 'q' = if drop  presses @ .  nwin @ .  8000 execute then
       >r  1 presses +!  0 0 136 56 r> win-inval ;

    display-init  0 nwin !
    40 60 200 120 t-hello ' hello-draw ' hello-key 0 window drop
    event-loop
    abcq

`34 parse` takes the text up to the next `"`, whose character code is 34,
and `s,` lays it down with its length in front: that is the title. Numbers
typed at the prompt are decimal.

It is the only window, so it doesn't need `windows`: `display-init` makes
the screen's port and switches to it, and `0 nwin !` says there are no
windows yet. `event-loop` marks the whole screen out of date, and then on
each pass takes a key if one is waiting and repaints what is out of date.
The reference VM always reports a key waiting, so there the first key is
taken before the first paint.

What is typed after `event-loop` goes to the window as keys: `a`, `b` and
`c` are counted, and `q` ends it. The nucleus has no word that halts, so
the program jumps to an all-zero instruction, which does halt (`MODEL.md`):
`8000 execute` goes to a spot on page 1 that is past the end of its code on
every machine. The reference VM doesn't stop when it runs out of input, so a
file typed at it has to halt it.

    cmake --build build/cmake --target hello

types that file at the reference VM, prints what it said (which ends with
`3 1`), and leaves a picture of the display in `build/hello/hello.png`. To
do the same by hand with your own file, once `hello` or `seen` has built the
tools, with `/tmp/mine` standing for a directory of your own:

    build/cmake/hexaforth-64 build/64/nuc.hex build/64/disk.img /tmp/mine/memory.out < /tmp/mine/mine.fs
    build/cmake/screen-64 /tmp/mine/memory.out /tmp/mine/mine.pbm
    build/cmake/png /tmp/mine/mine.pbm /tmp/mine/mine.png

The VM is given the image, a disk, and where to write its memory when it
stops. It opens the disk for writing as well, so a program that writes
blocks should be given a copy of `disk.img`.

### A machine's devices

The desktop needs three things from a machine, each a word that the
machine's driver stores in a cell. `desk.fs` does this for QEMU's devices in
`desktop`, and a machine with other devices does the same and then runs
`windows`.

    'blk-read   ( buf blk# -- )    read block blk#, 1,024 bytes, into the cell buf
    'blk-write  ( buf blk# -- )    write it from there
    'poll       ( -- )             ask the keyboard and the pointer for new events

With no disk the two block words are left at 0, and the editor and the
picture aren't opened. With no `'poll`, the console's keys are used.

`'poll` passes each event it gets to `on-event ( type code value -- )`, in
the form Linux input devices use:

- type 1 is a key or a button. `code` is the Linux key code, and `value` is
  1 for down and 0 for up. Code `0x110` is the pointer's button. The two
  shift keys and the two control keys are tracked, and any other key going
  down is turned into a character by `keymap.fs`, whose rows are those
  codes
- type 3 is the pointer's position, one axis at a time: `code` 0 is
  horizontal and anything else is vertical, and `value` is in 32,768ths of
  the screen
- type 0 means the device has reported everything it has for now. A word
  that is following the pointer is called then

### Adding a program to the system

- The program is a file in `forth/`, and `system.fs` includes it.
  `system.fs` says at its top which code page each file goes on, for a
  64K-cell machine and for a larger one. The target `examine-16` shows how
  much of each page is left
- A word that `desk.fs` names needs a header (`header name : name`)
- `windows`, in `desk.fs`, makes the window. Where it goes is a line in each
  of the two tables there: `big-desk` for a 768 by 576 screen and
  `small-desk` for a smaller one
- A variable must be within the 4,096 cells one instruction can reach. A
  table or a buffer goes between `bulk` and `unbulk`
- `check` runs the desktop's test on every machine. To have a window tested,
  give it a line in the table in `test/deskcheck.c`

`calc.fs` is the shortest program that has keys, a click and its own
rectangle marked out of date. `view.fs` has a task and `win-data`.
`paint.fs` has `'track`, `win-front`, `win-back` and the disk.
