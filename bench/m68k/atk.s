| AtkRow: a row of the dither that forth/atkinson.fs makes, for the 68000.
|
| This is mine.  It is written as HToneRow of MacPaint is written, which is
| Bill Atkinson's: called as Pascal calls, a frame and the registers saved,
| the row's pixels shifted into a word that has a flag bit in it to say
| when it is full, and the word then put into the bitmap with OR.  The
| dither is not HToneRow's.  It is the one with six neighbours:
|
|             *  1  1
|          1  1  1
|             1
|
| A pixel is white if its gray and what it is owed come to more than 127,
| and an eighth of what it is then out by, rounded down, is owed to each of
| the six.  Nothing is clipped.
|
| What a pixel is owed is kept so that a pixel costs memory as little as
| it can.  The two before it on its own row are in registers.  The three
| on the row above gave it one sum between them, which was added to its
| word of that row when the last of the three was done; and the one two
| rows above stored what it gave, which nothing had to be added to.  So a
| pixel reads its gray and one word, adds one sum to a word of the row
| below, and stores one word in the row below that.
|
| PROCEDURE AtkRow(grays:               QDPtr;
|                  thisRow,nextRow:     QDPtr;
|                  lastRow:             QDPtr;
|                  dstBits:             BitMap;
|                  vStart,hSize:        INTEGER);
|
|   grays     a byte a pixel, white the high value
|   thisRow   a word a pixel: what the rows above owe this row
|   nextRow   the same for the row below, which this row adds to;
|             the word before its first is written too
|   lastRow   for the row below that, which this row sets
|
|  A6 OFFSETS OF PARAMETERS AFTER LINK:

        .equ    paramSize, 24
        .equ    grays, paramSize+8-4            | Pointer
        .equ    thisRow, grays-4                | Pointer
        .equ    nextRow, thisRow-4              | Pointer
        .equ    lastRow, nextRow-4              | Pointer
        .equ    dstBits, lastRow-4              | Pointer
        .equ    vStart, dstBits-2               | Integer
        .equ    hSize, vStart-2                 | Integer

| A BitMap, from GrafTypes.a.
        .equ    baseAddr, 0
        .equ    rowBytes, 4
        .equ    bounds, 6
        .equ    top, 0

        .text
        .globl  AtkRow, AtkEnd
AtkRow:
        link    a6,#0                           | ALLOCATE STACK FRAME
        movem.l d3-d7/a2-a4,-(sp)               | SAVE REGS
        move.l  dstBits(a6),a3                  | get dst bitmap
        move.w  hSize(a6),d1                    | get horizontal size
        ble.w   GOHOME                          | quit if no pixels
        move.l  baseAddr(a3),a0                 | get baseAddr
        move.w  vStart(a6),d0                   | get vert start
        sub.w   bounds+top(a3),d0               | make it global
        mulu.w  rowBytes(a3),d0                 | times rowBytes for offset
        adda.l  d0,a0                           | add vertical offset

        move.l  grays(a6),a1                    | point to the grays
        move.l  thisRow(a6),a2                  | what this row is owed
        move.l  nextRow(a6),a3                  | what the row below is owed,
        subq.l  #2,a3                           | which a pixel adds to a pixel back
        move.l  lastRow(a6),a4                  | and the row below that
        move.w  #0x8000,d3                      | D3 is its own negative: NEG sets X and leaves it
        move.w  #255,d4                         | white
        moveq   #127,d7                         | the most that is black
        moveq   #0,d5                           | what the pixel before gave
        moveq   #0,d6                           | and that with what the one before it gave
        moveq   #1,d0                           | the flag bit, in the data word
        bra.s   MORE                            | go to main loop start

NXTPIXEL:
        clr.w   d2                              | get ready for byte
        move.b  (a1)+,d2                        | D2 := this pixel's gray
        add.w   (a2)+,d2                        | and what the rows above owe it
        add.w   d6,d2                           | and the two pixels before
        cmp.w   d7,d2                           | is it more than 127 ?
        bgt.s   ItsWhite                        | yes, a white pixel
ItsBlack:
        neg.w   d3                              | Set the X flag
        addx.w  d0,d0                           | shift x-bit in, flag bit out
        bra.s   Shifted
ItsWhite:
        sub.w   d4,d2                           | it is out by what it is short of white
        add.w   d0,d0                           | shift a zero in, flag bit out
Shifted:
        bcc.s   NoWrite                         | br if not time to write
        or.w    d0,(a0)+                        | or a word into dst
        moveq   #1,d0                           | and reset flag bit

NoWrite:
        asr.w   #3,d2                           | an eighth of what it is out by
        move.w  d2,(a4)+                        | to the pixel two rows below
        add.w   d2,d6                           | with the two before: the three the pixel
        add.w   d6,(a3)+                        | below the one before is owed by
        add.w   d2,d5                           | this and the one before,
        move.w  d5,d6                           | which the next pixel is owed,
        move.w  d2,d5                           | and this one by itself

MORE:   dbra    d1,NXTPIXEL                     | loop for all pixels

        add.w   d6,(a3)                         | the last pixel below has only two over it

CLEANUP:
        add.w   d0,d0                           | shift left
        bcc.w   CLEANUP                         | till flag bit pops out
        or.w    d0,(a0)                         | then or the word into dst

GOHOME: movem.l (sp)+,d3-d7/a2-a4               | RESTORE REGS
        unlk    a6                              | RELEASE STACK FRAME
        move.l  (sp)+,a0                        | POP RETURN ADDR
        adda.w  #paramSize,sp                   | STRIP PARAMS
        jmp     (a0)                            | AND RETURN
AtkEnd:
