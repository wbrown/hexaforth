# The desktop

How to start the desktop, how it is worked, and how a program for it is
written.  `forth/wm.fs` is the window manager and says the same of itself
at its top; `forth/desk.fs` opens the windows.

![The desktop, with HexaPaint in front](doc/desktop.png)

That picture is made by the target `screenshot`, from a screen the desktop's
test leaves.

## Starting it

    cmake --build build/cmake --target demo

boots the small AArch64 kernel on QEMU with a display, a disk, a keyboard
and a pointer.  The kernel runs the word `cold` when it starts, and `cold`
runs `desktop`: it brings up QEMU's devices and runs `windows`, which opens
the windows and runs the event loop.  What is typed at QEMU's serial console
goes to the window in front as well.

A machine with other devices brings them up itself and runs `windows`.  A
machine with a console and nothing else is given the console's keys.

The reference VM shows no display.  The desktop's test works the desktop on
it and keeps a screen after each step (`build/64/desktop/`), and the tools
`screen-64` and `png` make a picture of the display from the memory the VM
writes when it stops (the target `seen` builds them).

## Working it

- A press on a window brings it to the front.  The window in front has the
  keys
- A press on a window's title, a pull, and letting go moves the window by
  as far as the pointer went
- A press on `hexaforth`, at the top left, brings down a menu of the
  windows, the one in front first.  Letting go on a line brings that window
  to the front.  Letting go on the title leaves the menu down, and the next
  press picks a line or takes the menu away
- `^W` brings the window at the back to the front
- `^Q` leaves the desktop for the serial prompt

The windows:

- **Forth**: a prompt.  It is a task of its own, so its stack is kept from
  one line to the next and the desktop goes on while a line runs
- **Editor**: one block of the disk, 16 lines of 64.  Typing overwrites, the
  cursor keys move, PgUp and PgDn change block, `^S` writes the block, and
  `^L` writes it and loads it as Forth
- **Mandrill**: a picture read from a TIFF on the disk and dithered.  A
  task does it a row at a time, and the picture comes down the window
- **Calculator**: digits make a number; Enter or the space bar pushes it, or
  copies the top if no number is being typed; `+ - * /` work on the top two;
  `n` negates, `x` swaps, `d` drops, Backspace takes a digit away.  The
  buttons send the same keys
- **Words**: the dictionary on the left and a definition, decompiled, on the
  right.  Up and Down move through the dictionary and PgUp and PgDn by a
  page.  A press on a name shows it, a press on an underlined word follows
  it, and Backspace comes back.  Left and Right scroll a long definition.
  With the pointer on a word, a box says what instruction it is, by its
  fields
- **HexaPaint**: the tools are down the left: pencil and brush, eraser and
  line, rectangle and filled rectangle, oval and filled oval.  The patterns
  are along the bottom.  The drawing is kept on the disk

The editor and the picture are opened if the machine has a disk.

## A program for it

A window is made by `window`:

    window ( x y w h title draw key click -- win )

`x y w h` are where the window is on the screen and how big, frame and
title and all.  `title` is a counted string that starts where a cell does.
The other three are the window's words, and `click` may be 0:

    draw   ( win -- )         paint what is inside the frame
    key    ( c win -- )       a key, while this window is in front
    click  ( x y win -- )     the button has gone down inside the frame

`x y` of a click are in the window's own coordinates, as what `draw` paints
is: from the top left inside the frame.  A window that takes no keys has
`ignore-key`.

A port, in the drawing words, is a record of how drawing is done: where
the origin is, what it is clipped to, the pattern, the font.  It is another
thing than a port of the machine (`MODEL.md`).  The screen has one, and
each window one of its own, which the window manager takes up before it
runs `draw`.

### Drawing

`draw` is run by the event loop, to paint what of the window is out of date
and can be seen.  When it is run the window manager has made the inside of
the frame white, set the clip to what is to be painted, and put the origin
at the top left inside the frame, under the title.  `draw` paints all of
its window, in the window's own coordinates, and what need not be painted is
clipped away.  The inside is 2 pixels less wide than the window and 20 less
high.

The words that draw, all in `display.fs` and `gfx.fs`.  Where one takes a
flag, true is white and false is black:

    fill-rect   ( flag x y w h -- )
    frame-rect  ( flag x y w h -- )     an outline one pixel thick
    hline       ( flag x y n -- )
    pat-rect    ( x y w h -- )          in the pattern: p-gray pattern !
    draw-char   ( c x y -- )
    draw-text   ( addr u x y -- )       text at the address of a byte
    draw-chars  ( a i u x y -- )        u characters from character i of the cells at a
    copy-bits   ( bitmap sx sy dx dy w h -- )
    plot        ( flag x y -- )

Each of the three window words takes `win` off the stack, whether or not it
uses it.

`fill-rect`, `frame-rect` and `hline` leave their plain white or black as
the pattern, so a pattern is set again before the next `pat-rect`.  The
patterns are `p-white`, `p-black`, `p-gray`, `p-ltgray` and `p-dkgray`.
`pat-rect` draws as the mode says: `copy-mode`, which a window's port
starts in, `or-mode`, `xor-mode` or `bic-mode`.

`mono font !` and `sans font !` choose the font, and `draw` is given `mono`.
Both are 16 pixels high.  A character of `mono` is 8 wide; in `sans` each is
as wide as it is, and `chars-width ( a i u -- n )` and
`text-width ( addr u -- n )` say how wide some text is.  The `x y` of text
is its top left.  In `draw` text adds its black to what is there and leaves
the rest, since the window has just been made white: `text-or` is true, and
`0 text-or !` has text paint its white as well, which is what text over a
pattern wants.

What `.` and `emit` print is drawn by putting it in a buffer first:
`sb-begin`, the words that print, `sb-end`, and then `sb-draw ( x y -- )`.
The buffer holds 63 characters, and more are dropped.

### Saying what has changed

A program draws only in `draw`.  What changes something says what is out of
date, and the event loop paints it:

    win-inval ( x y w h win -- )        a rectangle inside the frame

When a `key` or a `click` word returns, the whole of its window is put out
of date, unless the word called `win-inval` itself.  Then what it said is
all that is, and it may say more than one rectangle: they add up.  The
calculator's keys change its numbers and leave its
buttons, so `calc-key` puts the numbers' rectangle out of date and the
buttons are left as they are on the screen.

### The pointer, the front, and what a window keeps

- To follow the pointer while the button is down, `click` stores a word in
  `'track`, of `( x y win -- )`.  It is run each time the pointer has moved,
  and once more when the button goes up, when `held` is false.  HexaPaint
  draws so
- To be told that the pointer has moved at all, a program stores a word of
  `( -- )` in `'moved`.  The Words window shows its box so
- To be told that another window has come to the front, a program gives a
  word of `( -- )` to `on-front`.  There is room for three.  HexaPaint
  writes its canvas to the disk so
- `win-data ( win -- a )` is a cell of the window's record for whoever draws
  it.  The picture's viewer keeps its record there

### Work that takes a while

The event loop runs each of a window's words to its end, so one that takes
long holds the desktop.  Such work is given to a task (`spawn`), which does
a part of it and says `pause`.  `view.fs` reads and dithers its picture so.

### Memory

`halloc ( u -- a )` gives room for u bytes, in whole cells, and a is the
address of a cell.  `hgive ( a cells -- )` gives it back.  `hreuse` is
`halloc` that takes memory that was given back if there is enough of it.

### An example

`doc/hello.fs` is a window that counts the keys it is given:

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

`34 parse` takes the text up to the next `"`, whose code is 34, and `s,`
lays it down with its count before it: that is the title.  Numbers at the
prompt are decimal.

It is the only window, so it does without `windows`: `display-init` makes
the screen's port and takes it up, and `0 nwin !` says there are no windows
yet.  `event-loop` puts the whole screen out of date, and then, each time
round, takes a key if one is waiting and paints what is out of date.  The
reference VM says a key is always waiting, so there the first key is taken
before the first paint.

What is typed after `event-loop` is the window's keys: `a`, `b` and `c` are
counted, and `q` ends it.  The nucleus has no word that halts, so the
program goes to an instruction of nothing, which does (`MODEL.md`):
`8000 execute` goes to a place on page 1 that is past its code on every
machine there is.  The reference VM does not stop at the end of what it is
given to read, so a file that is typed at it has to halt it.

    cmake --build build/cmake --target hello

types that file at the reference VM, prints what it said, which ends `3 1`,
and leaves a picture of the display in `build/hello/hello.png`.  By hand,
for a file of one's own, once `hello` or `seen` has built the tools, and
with `/tmp/mine` standing for a directory of one's own:

    build/cmake/hexaforth-64 build/64/nuc.hex build/64/disk.img /tmp/mine/memory.out < /tmp/mine/mine.fs
    build/cmake/screen-64 /tmp/mine/memory.out /tmp/mine/mine.pbm
    build/cmake/png /tmp/mine/mine.pbm /tmp/mine/mine.png

The VM is given the image, a disk, and where to write its memory when it
stops.  It opens the disk to write to it as well, so a program that writes
blocks is given a copy of `disk.img`.

### A machine's devices

The desktop asks three things of a machine, each a word that the machine's
driver stores in a cell.  `desk.fs` does it for QEMU's devices in `desktop`,
and a machine with others does the same and then runs `windows`.

    'blk-read   ( buf blk# -- )    read block blk#, 1,024 bytes, to the cell buf
    'blk-write  ( buf blk# -- )    write it from there
    'poll       ( -- )             ask the keyboard and the pointer for what they have

With no disk the two block words are left at 0, and the editor and the
picture are not opened.  With no `'poll` the console's keys are taken.

`'poll` hands each thing it is told to `on-event ( type code value -- )`,
which takes them as Linux's input devices say them:

- type 1 is a key or a button.  `code` is the Linux key code and `value` is
  1 for down and 0 for up.  Code `0x110` is the pointer's button.  The two
  shift keys and the two control keys are kept track of, and any other key
  going down is made a character by `keymap.fs`, whose rows are those codes
- type 3 is where the pointer is, an axis at a time: `code` 0 is across and
  any other is down, and `value` is in 32,768ths of the screen
- type 0 says the device has said all it has for now.  A word that is
  following the pointer is told then

### Putting a program in the system

- The program is a file of `forth/`, and `system.fs` includes it.
  `system.fs` says at its top which code page each file is on, for a machine
  of 64K cells and for a larger one.  The target `examine-16` says how much
  of each page is left
- A word that `desk.fs` names has a header (`header name : name`)
- `windows`, in `desk.fs`, makes the window.  Where it goes is a line of
  each of the two tables there: `big-desk` for a screen of 768 by 576 and
  `small-desk` for a smaller one
- A variable is within the 4,096 cells one instruction reaches.  A table or
  a buffer is between `bulk` and `unbulk`
- `check` runs the desktop's test on every machine.  A window that is to be
  tested is given a line in the table of `test/deskcheck.c`

`calc.fs` is the shortest program that has keys, a click and its own
rectangle put out of date.  `view.fs` has a task and `win-data`.  `paint.fs`
has `'track`, `on-front` and the disk.
