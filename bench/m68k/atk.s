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
        subq.w  #1,d1                           | dbra runs one more than it is given

| A pixel, three times over, so that no value is moved from register to
| register: w is worked in, q is what the two before gave and p what the
| one before gave.  After a pixel q is what the next is owed (p and this
| one) and is in p's register, and p is this one, in w's: so the next
| copy has them in other registers.  Each path tests the flag bit itself.
        .macro  pixel w,q,p,k,n
P\k:    clr.w   \w                              | get ready for byte
        move.b  (a1)+,\w                        | this pixel's gray
        add.w   (a2)+,\w                        | and what the rows above owe it
        add.w   \q,\w                           | and the two pixels before
        cmp.w   d7,\w                           | is it more than 127 ?
        bgt.s   W\k                             | yes, a white pixel
        neg.w   d3                              | black: set the X flag
        addx.w  d0,d0                           | shift x-bit in, flag bit out
        bcs.s   B\k                             | br if time to write
T\k:    asr.w   #3,\w                           | an eighth of what it is out by
        move.w  \w,(a4)+                        | to the pixel two rows below
        add.w   \w,\q                           | with the two before: the three the pixel
        add.w   \q,(a3)+                        | below the one before is owed by
        add.w   \w,\p                           | this and the one before
        dbra    d1,P\n                          | loop for all pixels
        add.w   \p,(a3)                         | the last pixel below has only two over it
        bra.w   CLEANUP
W\k:    sub.w   d4,\w                           | it is out by what it is short of white
        add.w   d0,d0                           | shift a zero in, flag bit out
        bcc.s   T\k                             | br if not time to write
B\k:    or.w    d0,(a0)+                        | or a word into dst
        moveq   #1,d0                           | and reset flag bit
        bra.s   T\k
        .endm

        pixel   d2,d6,d5,0,1
        pixel   d6,d5,d2,1,2
        pixel   d5,d2,d6,2,0

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
